//
//  AppAssetImporter.h
//  interface/src/assets
//
//  Migrated from Application.* on 2018-10-24.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AppAssetImporter_h
#define hifi_AppAssetImporter_h

#include "Application.h"

#if 0
struct AssetImportContext {
public:
    QString origin;
    QString url;
    bool isZip;
    bool isBlocks;
    QString zipFile;
};
#endif

class AppAssetImporter : public QObject {
    Q_OBJECT
public:
    AppAssetImporter(QObject* parent);
    void addAssetToWorldFromURL(QString url);
    void addAssetToWorld(QString filePath, QString zipFile, bool isZip = false, bool isBlocks = false);
    void addAssetToWorldWithNewMapping(QString filePath, QString mapping, int copy, bool isZip = false, bool isBlocks = false);
    void addAssetToWorldUpload(QString filePath, QString mapping, bool isZip = false, bool isBlocks = false);
    void addAssetToWorldSetMapping(QString filePath, QString mapping, QString hash, bool isZip = false, bool isBlocks = false);
    void addAssetToWorldAddEntity(QString filePath, QString mapping);

    void addAssetToWorldInfo(QString modelName, QString infoText);
    void addAssetToWorldInfoClear(QString modelName);
    void addAssetToWorldInfoDone(QString modelName);
    void addAssetToWorldError(QString modelName, QString errorText);

public slots:
    void onAssetToWorldMessageBoxClosed();
    void addAssetToWorldFromURLRequestFinished();
    void addAssetToWorldUnzipFailure(QString filePath);
    void handleUnzip(QString sourceFile, QStringList destinationFile, bool autoAdd, bool isZip, bool isBlocks);
    void addAssetToWorldMessageClose();
    void addAssetToWorldInfoTimeout();
    void addAssetToWorldErrorTimeout();

protected:
    QQuickItem* _addAssetToWorldMessageBox{ nullptr };
    QStringList _addAssetToWorldInfoKeys;  // Model name
    QStringList _addAssetToWorldInfoMessages;  // Info message
    QTimer _addAssetToWorldInfoTimer;
    QTimer _addAssetToWorldErrorTimer;
};

#endif
