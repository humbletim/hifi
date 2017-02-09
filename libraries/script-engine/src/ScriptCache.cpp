//
//  ScriptCache.cpp
//  libraries/script-engine/src
//
//  Created by Brad Hefta-Gaub on 2015-03-30
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ScriptCache.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkConfiguration>
#include <QNetworkReply>
#include <QObject>
#include <QThread>
#include <QRegularExpression>
#include <QMetaEnum>

#include <assert.h>
#include <SharedUtil.h>

#include "ScriptEngines.h"
#include "ScriptEngineLogging.h"
#include <QtCore/QTimer>

ScriptCache::ScriptCache(QObject* parent) {
    // nothing to do here...
}

void ScriptCache::clearCache() {
    Lock lock(_containerLock);
    foreach(auto& url, _scriptCache.keys()) {
        qCDebug(scriptengine) << "clearing cache: " << url;
    }
    _scriptCache.clear();
}

void ScriptCache::clearATPScriptsFromCache() {
    Lock lock(_containerLock);
    qCDebug(scriptengine) << "Clearing ATP scripts from ScriptCache";
    for (auto it = _scriptCache.begin(); it != _scriptCache.end();) {
        if (it.key().scheme() == "atp") {
            qCDebug(scriptengine) << "Removing: " << it.key();
            it = _scriptCache.erase(it);
        } else {
            ++it;
        }
    }
}

QString ScriptCache::getScript(const QUrl& unnormalizedURL, ScriptUser* scriptUser, bool& isPending, bool reload) {
    QUrl url = ResourceManager::normalizeURL(unnormalizedURL);
    QString scriptContents;

    Lock lock(_containerLock);
    if (_scriptCache.contains(url) && !reload) {
        qCDebug(scriptengine) << "Found script in cache:" << url.toString();
        scriptContents = _scriptCache[url];
        lock.unlock();
        scriptUser->scriptContentsAvailable(url, scriptContents);
        isPending = false;
    } else {
        isPending = true;
        bool alreadyWaiting = _scriptUsers.contains(url);
        _scriptUsers.insert(url, scriptUser);
        lock.unlock();

        if (alreadyWaiting) {
            qCDebug(scriptengine) << "Already downloading script at:" << url.toString();
        } else {
            auto request = ResourceManager::createResourceRequest(nullptr, url);
            request->setCacheEnabled(!reload);
            connect(request, &ResourceRequest::finished, this, &ScriptCache::scriptDownloaded);
            request->send();
        }
    }
    return scriptContents;
}

void ScriptCache::deleteScript(const QUrl& unnormalizedURL) {
    QUrl url = ResourceManager::normalizeURL(unnormalizedURL);
    Lock lock(_containerLock);
    if (_scriptCache.contains(url)) {
        qCDebug(scriptengine) << "Delete script from cache:" << url.toString();
        _scriptCache.remove(url);
    }
    if (_badScripts.contains(url)) {
        qCDebug(scriptengine) << "Delete script from bad scripts:" << url.toString();
        _badScripts.remove(url);
    }
    // FIXME: this second check might not be necessary, but currently neither addScriptToBadScriptList nor isInBadScriptList apply normalization to the url input
    if (_badScripts.contains(unnormalizedURL)) {
        qCDebug(scriptengine) << "Delete unnormalized script from bad scripts:" << unnormalizedURL.toString();
        _badScripts.remove(unnormalizedURL);
    }
}

void ScriptCache::scriptDownloaded() {
    ResourceRequest* req = qobject_cast<ResourceRequest*>(sender());
    QUrl url = req->getUrl();

    Lock lock(_containerLock);
    QList<ScriptUser*> scriptUsers = _scriptUsers.values(url);
    _scriptUsers.remove(url);

    if (!DependencyManager::get<ScriptEngines>()->isStopped()) {
        if (req->getResult() == ResourceRequest::Success) {
            auto scriptContents = req->getData();
            _scriptCache[url] = scriptContents;
            lock.unlock();
            qCDebug(scriptengine) << "Done downloading script at:" << url.toString();

            foreach(ScriptUser* user, scriptUsers) {
                user->scriptContentsAvailable(url, scriptContents);
            }
        } else {
            lock.unlock();
            qCWarning(scriptengine) << "Error loading script from URL " << url;
            foreach(ScriptUser* user, scriptUsers) {
                user->errorInLoadingScript(url);
            }
        }
    }

    req->deleteLater();
}

void ScriptCache::getScriptContents(const QString& scriptOrURL, contentAvailableCallback contentAvailable, bool forceDownload, int max_retries) {
    #ifdef THREAD_DEBUGGING
    qCDebug(scriptengine) << "ScriptCache::getScriptContents() on thread [" << QThread::currentThread() << "] expected thread [" << thread() << "]";
    #endif
    QUrl unnormalizedURL(scriptOrURL);
    QUrl url = ResourceManager::normalizeURL(unnormalizedURL);

    // attempt to determine if this is a URL to a script, or if this is actually a script itself (which is valid in the
    // entityScript use case)
    if (unnormalizedURL.scheme().isEmpty() &&
            scriptOrURL.simplified().replace(" ", "").contains(QRegularExpression(R"(\(function\([a-z]?[\w,]*\){)"))) {
        contentAvailable(scriptOrURL, scriptOrURL, false, true, "Inline");
        return;
    }

    // give a similar treatment to javacript: urls
    if (unnormalizedURL.scheme() == "javascript") {
        QString contents { scriptOrURL };
        contents.replace(QRegularExpression("^javascript:"), "");
        contentAvailable(scriptOrURL, contents, false, true, "Inline");
        return;
    }

    Lock lock(_containerLock);
    if (_scriptCache.contains(url) && !forceDownload) {
        auto scriptContent = _scriptCache[url];
        lock.unlock();
        qCDebug(scriptengine) << "Found script in cache:" << url.toString();
        contentAvailable(url.toString(), scriptContent, true, true, "Cached");
    } else {
        auto& scriptRequest = _activeScriptRequests[url];
        bool alreadyWaiting = scriptRequest.scriptUsers.size() > 0;
        scriptRequest.scriptUsers.push_back(contentAvailable);

        lock.unlock();

        if (alreadyWaiting) {
            qCDebug(scriptengine) << "Already downloading script at:" << url.toString()
                 << "(retry: " << scriptRequest.numRetries << "; waiters: " << scriptRequest.scriptUsers.size() << ")" ;
        } else {
            scriptRequest.maxRetries = max_retries;
            #ifdef THREAD_DEBUGGING
            qCDebug(scriptengine) << "about to call: ResourceManager::createResourceRequest(this, url); on thread [" << QThread::currentThread() << "] expected thread [" << thread() << "]";
            #endif
            auto request = ResourceManager::createResourceRequest(nullptr, url);
            Q_ASSERT(request);
            request->setCacheEnabled(!forceDownload);
            connect(request, &ResourceRequest::finished, this, [=]{ scriptContentAvailable(max_retries); });
            request->send();
        }
    }
}

void ScriptCache::scriptContentAvailable(int max_retries) {
    qCDebug(scriptengine) << "ScriptCache::scriptContentAvailable(" << max_retries << ")";
    #ifdef THREAD_DEBUGGING
    qCDebug(scriptengine) << "ScriptCache::scriptContentAvailable() on thread [" << QThread::currentThread() << "] expected thread [" << thread() << "]";
    #endif
    ResourceRequest* req = qobject_cast<ResourceRequest*>(sender());
    QUrl url = req->getUrl();

    QString scriptContent;
    std::vector<contentAvailableCallback> allCallbacks;
    QString status = QMetaEnum::fromType<ResourceRequest::Result>().valueToKey(req->getResult());
    bool success { false };

    {
        Q_ASSERT(req->getState() == ResourceRequest::Finished);
        success = req->getResult() == ResourceRequest::Success;

        Lock lock(_containerLock);

        if (_activeScriptRequests.contains(url)) {
            auto& scriptRequest = _activeScriptRequests[url];

            if (success) {
                allCallbacks = scriptRequest.scriptUsers;

                _activeScriptRequests.remove(url);

                _scriptCache[url] = scriptContent = req->getData();
                qCDebug(scriptengine) << "Done downloading script at:" << url.toString();
            } else {
                auto result = req->getResult();
                bool irrecoverable =
                    result == ResourceRequest::AccessDenied ||
                    result == ResourceRequest::InvalidURL ||
                    result == ResourceRequest::NotFound ||
                    scriptRequest.numRetries >= max_retries;

                if (!irrecoverable) {
                    ++scriptRequest.numRetries;

                    int timeout = exp(scriptRequest.numRetries) * ScriptRequest::START_DELAY_BETWEEN_RETRIES;
                    int attempt = scriptRequest.numRetries;
                    qCDebug(scriptengine) << QString("Script request failed [%1]: %2 (will retry %3 more times; attempt #%4 in %5ms...)")
                        .arg(status).arg(url.toString()).arg(max_retries - attempt + 1).arg(attempt).arg(timeout);

                    QTimer::singleShot(timeout, this, [this, url, attempt, max_retries]() {
                        qCDebug(scriptengine) << QString("Retrying script request [%1 / %2]: %3")
                            .arg(attempt).arg(max_retries).arg(url.toString());

                        auto request = ResourceManager::createResourceRequest(nullptr, url);
                        Q_ASSERT(request);

                        // We've already made a request, so the cache must be disabled or it wasn't there, so enabling
                        // it will do nothing.
                        request->setCacheEnabled(false);
                        connect(request, &ResourceRequest::finished, this, [=]{ scriptContentAvailable(max_retries); });
                        request->send();
                    });
                } else {
                    // Dubious, but retained here because it matches the behavior before fixing the threading

                    allCallbacks = scriptRequest.scriptUsers;

                    if (_scriptCache.contains(url))
                        scriptContent = _scriptCache[url];
                    _activeScriptRequests.remove(url);
                    qCWarning(scriptengine) << "Error loading script from URL " << url << "(" << result <<")";

                }
            }
        }
    }

    req->deleteLater();

    if (allCallbacks.size() > 0 && !DependencyManager::get<ScriptEngines>()->isStopped()) {
        foreach(contentAvailableCallback thisCallback, allCallbacks) {
            thisCallback(url.toString(), scriptContent, true, success, status);
        }
    }
}
