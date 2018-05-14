//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QVariant>
#include <QtCore/QUuid>
#include <QSharedPointer>
#include <memory>
#include <unordered_map>
#include <functional>

#include <DependencyManager.h>
#include "MeshObjectPlugin.h"

namespace plugins { namespace entity {
    struct ProxyResolver {
    public:
        using Pointer = std::shared_ptr<ProxyResolver>;
        static ObjectProxy::Pointer INVALID_PROXY;
        QString pluginURI;
        virtual ObjectProxy::Pointer openObjectProxy(const QUuid&uuid) = 0;
        virtual ObjectProxy::Pointer getObjectProxy(const QUuid&uuid) = 0;
        virtual bool closeObjectProxy(const QUuid&uuid) = 0;
    };

    class ProxyManager : public QObject, public Dependency {
        Q_OBJECT
    public:

        using Pointer = QSharedPointer<ProxyManager>;
        bool setPluginResolver(const std::string& pluginURI, ProxyResolver::Pointer resolver);
        ProxyResolver::Pointer getPluginResolver(const std::string& pluginURI) const;
        std::vector<std::string> getRegisteredPlugins() const;
        bool clearObject(const QUuid& uuid);
        ProxyResolver::Pointer getResolver(const QUuid& uuid) const;
        ObjectProxy::Pointer getObject(const QUuid& uuid) const;
        bool associate(const QUuid& uuid, ProxyResolver::Pointer resolver);
        std::vector<std::string> getAvailablePlugins() const;
        std::vector<std::string> getAssigned() const;

        static bool resetObjectPlugin(const QUuid& uuid) {
            if (!DependencyManager::isSet<ProxyManager>()) {
                return false;
            }
            return DependencyManager::get<ProxyManager>()->clearObject(uuid);
        }

        static std::vector<std::string> getAssignedPlugins() {
            if (!DependencyManager::isSet<ProxyManager>()) {
                return std::vector<std::string>();
            }
            return DependencyManager::get<ProxyManager>()->getAssigned();
        }

        static std::vector<std::string> getPluginURIs() {
            if (!DependencyManager::isSet<ProxyManager>()) {
                return std::vector<std::string>();
            }
            return DependencyManager::get<ProxyManager>()->getAvailablePlugins();
        }

        static std::string getPluginURI(const QUuid& uuid) {
            if (!DependencyManager::isSet<ProxyManager>()) {
                return "";
            }
            auto resolver = DependencyManager::get<ProxyManager>()->getResolver(uuid);
            return resolver ? resolver->pluginURI.toStdString() : "";
        }

        static ProxyResolver::Pointer findPlugin(const std::string& uri) {
            if (!DependencyManager::isSet<ProxyManager>()) {
                return nullptr;
            }
            return DependencyManager::get<ProxyManager>()->getPluginResolver(uri);
        }

        static ObjectProxy::Pointer getObjectPlugin(const QUuid& uuid, const std::string& uri) {
            if (!DependencyManager::isSet<ProxyManager>()) {
                return nullptr;
            }
            if (auto plugin = DependencyManager::get<ProxyManager>()->findPlugin(uri)) {
                return plugin->getObjectProxy(uuid);
            }
            return nullptr;
        }
        static ObjectProxy::Pointer createObjectPlugin(const QUuid& uuid, const std::string& uri, const QVariantMap& parameters) {
            if (!DependencyManager::isSet<ProxyManager>()) {
                return nullptr;
            }
            auto manager = DependencyManager::get<ProxyManager>();
            if (auto plugin = manager->findPlugin(uri)) {
                if (auto proxy = plugin->openObjectProxy(uuid)) {
                    manager->associate(uuid, plugin);
                    return proxy;
                }
            }
            return nullptr;
        }

    protected:
        mutable std::mutex _lock;
        std::unordered_map<std::string, ProxyResolver::Pointer> _resolvers;
        QMap<QUuid, ProxyResolver::Pointer> _assigned;
    };

}}
