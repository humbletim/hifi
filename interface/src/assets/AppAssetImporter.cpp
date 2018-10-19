//
//  AppAssetImporter.cpp
//  interface/src/assets
//
//  Migrated from Application.* on 2018-10-24.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "AppAssetImporter.h"
#include <MappingRequest.h>
#include <AssetUpload.h>
#include <OffscreenUi.h>
#include <QtWidgets/QMessageBox>
#include "AvatarHashMap.h"

namespace { QLoggingCategory asset_import{ "hifi.interface.import" }; }

AppAssetImporter::AppAssetImporter(QObject* parent) : QObject(parent) {
    // Auto-update and close adding asset to world info message box.
    static const int ADD_ASSET_TO_WORLD_INFO_TIMEOUT_MS = 5000;
    _addAssetToWorldInfoTimer.setInterval(ADD_ASSET_TO_WORLD_INFO_TIMEOUT_MS); // 5s, Qt::CoarseTimer acceptable
    _addAssetToWorldInfoTimer.setSingleShot(true);
    connect(&_addAssetToWorldInfoTimer, &QTimer::timeout, this, &AppAssetImporter::addAssetToWorldInfoTimeout);
    static const int ADD_ASSET_TO_WORLD_ERROR_TIMEOUT_MS = 8000;
    _addAssetToWorldErrorTimer.setInterval(ADD_ASSET_TO_WORLD_ERROR_TIMEOUT_MS); // 8s, Qt::CoarseTimer acceptable
    _addAssetToWorldErrorTimer.setSingleShot(true);
    connect(&_addAssetToWorldErrorTimer, &QTimer::timeout, this, &AppAssetImporter::addAssetToWorldErrorTimeout);

    auto nodeList = DependencyManager::get<NodeList>();
    const DomainHandler& domainHandler = nodeList->getDomainHandler();
    connect(qApp, &QApplication::aboutToQuit, this, &AppAssetImporter::addAssetToWorldMessageClose);
    connect(&domainHandler, &DomainHandler::domainURLChanged, this, &AppAssetImporter::addAssetToWorldMessageClose);
    connect(&domainHandler, &DomainHandler::redirectToErrorDomainURL, this, &AppAssetImporter::addAssetToWorldMessageClose);
}

void AppAssetImporter::addAssetToWorldFromURL(QString url) {
    qInfo(asset_import) << "Download model and add to world from" << url << QThread::currentThread();
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [=]{ addAssetToWorldFromURL(url); });
        return;
    }

    QString filename;
    if (url.contains("filename")) {
        filename = url.section("filename=", 1, 1);  // Filename is in "?filename=" parameter at end of URL.
    }
    if (url.contains("poly.google.com/downloads")) {
        filename = url.section('/', -1);
        if (url.contains("noDownload")) {
            filename.remove(".zip?noDownload=false");
        } else {
            filename.remove(".zip");
        }

    }

    if (!DependencyManager::get<NodeList>()->getThisNodeCanWriteAssets()) {
        QString errorInfo = "You do not have permissions to write to the Asset Server.";
        qWarning(asset_import) << "Error downloading model: " + errorInfo;
        addAssetToWorldError(filename, errorInfo);
        return;
    }

    addAssetToWorldInfo(filename, "Downloading model file " + filename + ".");

    auto request = DependencyManager::get<ResourceManager>()->createResourceRequest(nullptr, QUrl(url));
    connect(request, &ResourceRequest::finished, this, &AppAssetImporter::addAssetToWorldFromURLRequestFinished);
    request->send();
}

Q_DECLARE_METATYPE(QFileDevice::FileError)
void AppAssetImporter::addAssetToWorldFromURLRequestFinished() {
    auto request = qobject_cast<ResourceRequest*>(sender());
    auto url = request->getUrl().toString();
    auto result = request->getResult();

    QString filename;
    bool isBlocks = false;

    if (url.contains("filename")) {
        filename = url.section("filename=", 1, 1);  // Filename is in "?filename=" parameter at end of URL.
    }
    if (url.contains("poly.google.com/downloads")) {
        filename = url.section('/', -1);
        if (url.contains("noDownload")) {
            filename.remove(".zip?noDownload=false");
        } else {
            filename.remove(".zip");
        }
        isBlocks = true;
    }

    if (result == ResourceRequest::Success) {
        qInfo(asset_import) << "Downloaded model from" << url;
        QTemporaryDir temporaryDir;
        temporaryDir.setAutoRemove(false);
        if (temporaryDir.isValid()) {
            QString temporaryDirPath = temporaryDir.path();
            QString downloadPath = temporaryDirPath + "/" + filename;
            qInfo(asset_import) << "Download path:" << downloadPath;

            QFile tempFile(downloadPath);
            if (tempFile.open(QIODevice::WriteOnly)) {
                tempFile.write(request->getData());
                addAssetToWorldInfoClear(filename);  // Remove message from list; next one added will have a different key.
                tempFile.close();
                qApp->getFileDownloadInterface()->runUnzip(downloadPath, url, true, false, isBlocks);
            } else {
                QString errorInfo = "Couldn't open temporary file for download";
                qWarning(asset_import) << errorInfo << (int)tempFile.error() << QVariant::fromValue(tempFile.error());
                addAssetToWorldError(filename, errorInfo);
            }
        } else {
            QString errorInfo = "Couldn't create temporary directory for download";
            qWarning(asset_import) << errorInfo;
            addAssetToWorldError(filename, errorInfo);
        }
    } else {
        qWarning(asset_import) << "Error downloading" << url << ":" << request->getResultString();
        addAssetToWorldError(filename, "Error downloading " + filename + " : " + request->getResultString());
    }

    request->deleteLater();
}


QString filenameFromPath(QString filePath) {
    return filePath.right(filePath.length() - filePath.lastIndexOf("/") - 1);
}

void AppAssetImporter::addAssetToWorldUnzipFailure(QString filePath) {
    QString filename = filenameFromPath(QUrl(filePath).toLocalFile());
    qWarning(asset_import) << "Couldn't unzip file" << filePath;
    addAssetToWorldError(filename, "Couldn't unzip file " + filename + ".");
}

void AppAssetImporter::addAssetToWorld(QString path, QString zipFile, bool isZip, bool isBlocks) {
    // Automatically upload and add asset to world as an alternative manual process initiated by showAssetServerWidget().
    QString mapping;
    QString filename = filenameFromPath(path);
    if (isZip || isBlocks) {
        QString assetName = zipFile.section("/", -1).remove(QRegExp("[.]zip(.*)$"));
        QString assetFolder = path.section("model_repo/", -1);
        mapping = "/" + assetName + "/" + assetFolder;
    } else {
        mapping = "/" + filename;
    }

    // Test repeated because possibly different code paths.
    if (!DependencyManager::get<NodeList>()->getThisNodeCanWriteAssets()) {
        QString errorInfo = "You do not have permissions to write to the Asset Server.";
        qWarning(asset_import) << "Error downloading model: " + errorInfo;
        addAssetToWorldError(filename, errorInfo);
        return;
    }

    addAssetToWorldInfo(filename, "Adding " + mapping.mid(1) + " to the Asset Server.");

    addAssetToWorldWithNewMapping(path, mapping, 0, isZip, isBlocks);
}

void AppAssetImporter::addAssetToWorldWithNewMapping(QString filePath, QString mapping, int copy, bool isZip, bool isBlocks) {
    auto request = DependencyManager::get<AssetClient>()->createGetMappingRequest(mapping);

    QObject::connect(request, &GetMappingRequest::finished, this, [=](GetMappingRequest* request) mutable {
        const int MAX_COPY_COUNT = 100;  // Limit number of duplicate assets; recursion guard.
        auto result = request->getError();
        if (result == GetMappingRequest::NotFound) {
            addAssetToWorldUpload(filePath, mapping, isZip, isBlocks);
        } else if (result != GetMappingRequest::NoError) {
            QString errorInfo = "Could not map asset name: "
                + mapping.left(mapping.length() - QString::number(copy).length() - 1);
            qWarning(asset_import) << "Error downloading model: " + errorInfo;
            addAssetToWorldError(filenameFromPath(filePath), errorInfo);
        } else if (copy < MAX_COPY_COUNT - 1) {
            if (copy > 0) {
                mapping = mapping.remove(mapping.lastIndexOf("-"), QString::number(copy).length() + 1);
            }
            copy++;
            mapping = mapping.insert(mapping.lastIndexOf("."), "-" + QString::number(copy));
            addAssetToWorldWithNewMapping(filePath, mapping, copy, isZip, isBlocks);
        } else {
            QString errorInfo = "Too many copies of asset name: "
                + mapping.left(mapping.length() - QString::number(copy).length() - 1);
            qWarning(asset_import) << "Error downloading model: " + errorInfo;
            addAssetToWorldError(filenameFromPath(filePath), errorInfo);
        }
        request->deleteLater();
    });

    request->start();
}

void AppAssetImporter::addAssetToWorldUpload(QString filePath, QString mapping, bool isZip, bool isBlocks) {
    qInfo(asset_import) << "Uploading" << filePath << "to Asset Server as" << mapping;
    auto upload = DependencyManager::get<AssetClient>()->createUpload(filePath);
    QObject::connect(upload, &AssetUpload::finished, this, [=](AssetUpload* upload, const QString& hash) mutable {
        if (upload->getError() != AssetUpload::NoError) {
            QString errorInfo = "Could not upload model to the Asset Server.";
            qWarning(asset_import) << "Error downloading model: " + errorInfo;
            addAssetToWorldError(filenameFromPath(filePath), errorInfo);
        } else {
            addAssetToWorldSetMapping(filePath, mapping, hash, isZip, isBlocks);
        }

        // Remove temporary directory created by Clara.io market place download.
        int index = filePath.lastIndexOf("/model_repo/");
        if (index > 0) {
            QString tempDir = filePath.left(index);
            qCDebug(asset_import) << "Removing temporary directory at: " + tempDir;
            QDir(tempDir).removeRecursively();
        }

        upload->deleteLater();
    });

    upload->start();
}

void AppAssetImporter::addAssetToWorldSetMapping(QString filePath, QString mapping, QString hash, bool isZip, bool isBlocks) {
    auto request = DependencyManager::get<AssetClient>()->createSetMappingRequest(mapping, hash);
    connect(request, &SetMappingRequest::finished, this, [=](SetMappingRequest* request) mutable {
        if (request->getError() != SetMappingRequest::NoError) {
            QString errorInfo = "Could not set asset mapping.";
            qWarning(asset_import) << "Error downloading model: " + errorInfo;
            addAssetToWorldError(filenameFromPath(filePath), errorInfo);
        } else {
            // to prevent files that aren't models or texture files from being loaded into world automatically
            if ((filePath.toLower().endsWith(".obj") || filePath.toLower().endsWith(".fbx")) ||
                ((filePath.toLower().endsWith(".jpg") || filePath.toLower().endsWith(".png")) &&
                ((!isBlocks) && (!isZip)))) {
                addAssetToWorldAddEntity(filePath, mapping);
            } else {
                qCDebug(asset_import) << "Zipped contents are not supported entity files";
                addAssetToWorldInfoDone(filenameFromPath(filePath));
            }
        }
        request->deleteLater();
    });

    request->start();
}

void AppAssetImporter::addAssetToWorldAddEntity(QString filePath, QString mapping) {
    EntityItemProperties properties;
    properties.setType(EntityTypes::Model);
    properties.setName(mapping.right(mapping.length() - 1));
    if (filePath.toLower().endsWith(".png") || filePath.toLower().endsWith(".jpg")) {
        QJsonObject textures {
            {"tex.picture", QString("atp:" + mapping) }
        };
        properties.setModelURL("https://hifi-content.s3.amazonaws.com/DomainContent/production/default-image-model.fbx");
        properties.setTextures(QJsonDocument(textures).toJson(QJsonDocument::Compact));
        properties.setShapeType(SHAPE_TYPE_BOX);
    } else {
        properties.setModelURL("atp:" + mapping);
        properties.setShapeType(SHAPE_TYPE_SIMPLE_COMPOUND);
    }
    properties.setCollisionless(true);  // Temporarily set so that doesn't collide with avatar.
    bool grabbable = (Menu::getInstance()->isOptionChecked(MenuOption::CreateEntitiesGrabbable));
    properties.setUserData(grabbable ? GRABBABLE_USER_DATA : NOT_GRABBABLE_USER_DATA);
    auto avatarHashMap = DependencyManager::get<AvatarHashMap>();
    auto myAvatar = avatarHashMap->getAvatarBySessionID(QUuid());
    glm::vec3 positionOffset = myAvatar->getWorldOrientation() * (myAvatar->getSensorToWorldScale() * glm::vec3(0.0f, 0.0f, -2.0f));
    properties.setPosition(myAvatar->getWorldPosition() + positionOffset);
    properties.setRotation(myAvatar->getWorldOrientation());
    properties.setGravity(glm::vec3(0.0f, 0.0f, 0.0f));

    // Note: Model dimensions are not available here -- EntityScriptingInterface::addModelEntity attends to autoresizing later.
    auto entityID = DependencyManager::get<EntityScriptingInterface>()->addModelEntity(
        properties.getName(), properties.getModelURL(), properties.getTextures(),
        properties.getShapeType() == SHAPE_TYPE_BOX ? "box" : "simple-compound",
        properties.getDynamic(), properties.getCollisionless(), grabbable,
        properties.getPosition(), properties.getGravity()
    );

    if (auto entityTreeRenderer = DependencyManager::get<EntityTreeRenderer>()) {
        if (auto entityTree = entityTreeRenderer->getTree()) {
            entityTree->withReadLock([&] {
                EntityItemPointer entity = entityTree->findEntityByEntityItemID(entityID);
                qDebug() << "addAssetToWorldAddEntity...entity pointer" << entity;
                });
        }
    }

    if (entityID == QUuid()) {
        QString errorInfo = "Could not add model " + mapping + " to world.";
        qWarning(asset_import) << "Could not add model to world: " + errorInfo;
        addAssetToWorldError(filenameFromPath(filePath), errorInfo);
    } else {
        // Close progress message box.
        addAssetToWorldInfoDone(filenameFromPath(filePath));
    }
}

void AppAssetImporter::addAssetToWorldInfo(QString modelName, QString infoText) {
    // Displays the most recent info message, subject to being overridden by error messages.

    if (qApp->isAboutToQuit()) {
        return;
    }

    /*
    Cancel info timer if running.
    If list has an entry for modelName, delete it (just one).
    Append modelName, infoText to list.
    Display infoText in message box unless an error is being displayed (i.e., error timer is running).
    Show message box if not already visible.
    */

    _addAssetToWorldInfoTimer.stop();

    addAssetToWorldInfoClear(modelName);

    _addAssetToWorldInfoKeys.append(modelName);
    _addAssetToWorldInfoMessages.append(infoText);

    if (!_addAssetToWorldErrorTimer.isActive()) {
        if (!_addAssetToWorldMessageBox) {
            _addAssetToWorldMessageBox = DependencyManager::get<OffscreenUi>()->createMessageBox(OffscreenUi::ICON_INFORMATION,
                "Downloading Model", "", QMessageBox::NoButton, QMessageBox::NoButton);
            connect(_addAssetToWorldMessageBox, SIGNAL(destroyed()), this, SLOT(onAssetToWorldMessageBoxClosed()));
        }

        _addAssetToWorldMessageBox->setProperty("text", "\n" + infoText);
        _addAssetToWorldMessageBox->setVisible(true);
    }
}

void AppAssetImporter::addAssetToWorldInfoClear(QString modelName) {
    // Clears modelName entry from message list without affecting message currently displayed.

    if (qApp->isAboutToQuit()) {
        return;
    }

    /*
    Delete entry for modelName from list.
    */

    auto index = _addAssetToWorldInfoKeys.indexOf(modelName);
    if (index > -1) {
        _addAssetToWorldInfoKeys.removeAt(index);
        _addAssetToWorldInfoMessages.removeAt(index);
    }
}

void AppAssetImporter::addAssetToWorldInfoDone(QString modelName) {
    // Continues to display this message if the latest for a few seconds, then deletes it and displays the next latest.

    if (qApp->isAboutToQuit()) {
        return;
    }

    /*
    Delete entry for modelName from list.
    (Re)start the info timer to update message box. ... onAddAssetToWorldInfoTimeout()
    */

    addAssetToWorldInfoClear(modelName);
    _addAssetToWorldInfoTimer.start();
}

void AppAssetImporter::addAssetToWorldInfoTimeout() {
    if (qApp->isAboutToQuit()) {
        return;
    }

    /*
    If list not empty, display last message in list (may already be displayed ) unless an error is being displayed.
    If list empty, close the message box unless an error is being displayed.
    */

    if (!_addAssetToWorldErrorTimer.isActive() && _addAssetToWorldMessageBox) {
        if (_addAssetToWorldInfoKeys.length() > 0) {
            _addAssetToWorldMessageBox->setProperty("text", "\n" + _addAssetToWorldInfoMessages.last());
        } else {
            disconnect(_addAssetToWorldMessageBox);
            _addAssetToWorldMessageBox->setVisible(false);
            _addAssetToWorldMessageBox->deleteLater();
            _addAssetToWorldMessageBox = nullptr;
        }
    }
}

void AppAssetImporter::addAssetToWorldError(QString modelName, QString errorText) {
    // Displays the most recent error message for a few seconds.

    if (qApp->isAboutToQuit()) {
        return;
    }

    /*
    If list has an entry for modelName, delete it.
    Display errorText in message box.
    Show message box if not already visible.
    (Re)start error timer. ... onAddAssetToWorldErrorTimeout()
    */

    addAssetToWorldInfoClear(modelName);

    if (!_addAssetToWorldMessageBox) {
        _addAssetToWorldMessageBox = DependencyManager::get<OffscreenUi>()->createMessageBox(OffscreenUi::ICON_INFORMATION,
            "Downloading Model", "", QMessageBox::NoButton, QMessageBox::NoButton);
        connect(_addAssetToWorldMessageBox, SIGNAL(destroyed()), this, SLOT(onAssetToWorldMessageBoxClosed()));
    }

    _addAssetToWorldMessageBox->setProperty("text", "\n" + errorText);
    _addAssetToWorldMessageBox->setVisible(true);

    _addAssetToWorldErrorTimer.start();
}

void AppAssetImporter::addAssetToWorldErrorTimeout() {
    if (qApp->isAboutToQuit()) {
        return;
    }

    /*
    If list is not empty, display message from last entry.
    If list is empty, close the message box.
    */

    if (_addAssetToWorldMessageBox) {
        if (_addAssetToWorldInfoKeys.length() > 0) {
            _addAssetToWorldMessageBox->setProperty("text", "\n" + _addAssetToWorldInfoMessages.last());
        } else {
            disconnect(_addAssetToWorldMessageBox);
            _addAssetToWorldMessageBox->setVisible(false);
            _addAssetToWorldMessageBox->deleteLater();
            _addAssetToWorldMessageBox = nullptr;
        }
    }
}


void AppAssetImporter::addAssetToWorldMessageClose() {
    // Clear messages, e.g., if Interface is being closed or domain changes.

    /*
    Call if user manually closes message box.
    Call if domain changes.
    Call if application is shutting down.

    Stop timers.
    Close the message box if open.
    Clear lists.
    */

    _addAssetToWorldInfoTimer.stop();
    _addAssetToWorldErrorTimer.stop();

    if (_addAssetToWorldMessageBox) {
        disconnect(_addAssetToWorldMessageBox);
        _addAssetToWorldMessageBox->setVisible(false);
        _addAssetToWorldMessageBox->deleteLater();
        _addAssetToWorldMessageBox = nullptr;
    }

    _addAssetToWorldInfoKeys.clear();
    _addAssetToWorldInfoMessages.clear();
}

void AppAssetImporter::onAssetToWorldMessageBoxClosed() {
    if (_addAssetToWorldMessageBox) {
        // User manually closed message box; perhaps because it has become stuck, so reset all messages.
        qInfo(asset_import) << "User manually closed download status message box";
        disconnect(_addAssetToWorldMessageBox);
        _addAssetToWorldMessageBox = nullptr;
        addAssetToWorldMessageClose();
    }
}


void AppAssetImporter::handleUnzip(QString zipFile, QStringList unzipFile, bool autoAdd, bool isZip, bool isBlocks) {
    if (autoAdd) {
        if (!unzipFile.isEmpty()) {
            for (int i = 0; i < unzipFile.length(); i++) {
                if (QFileInfo(unzipFile.at(i)).isFile()) {
                    qCDebug(asset_import) << "Preparing file for asset server: " << unzipFile.at(i);
                    addAssetToWorld(unzipFile.at(i), zipFile, isZip, isBlocks);
                }
            }
        } else {
            addAssetToWorldUnzipFailure(zipFile);
        }
    } else {
        qApp->showAssetServerWidget(unzipFile.first());
    }
}
