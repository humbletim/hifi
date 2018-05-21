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
#include "ObjectProxy.h"
#include <UUIDHasher.h>

namespace plugins { namespace object {
    using PluginURI = std::string;
    using PluginURIs = std::vector<PluginURI>;

    class Provider {
    public:
        using Pointer = std::shared_ptr<Provider>;

        PluginURI pluginURI;

        virtual ObjectProxy::Pointer createObjectProxy(const QUuid&uuid, const QVariantMap& parameters = QVariantMap()) = 0;
        virtual ObjectProxy::Pointer getObjectProxy(const QUuid&uuid) const = 0;
        virtual bool destroyObjectProxy(const QUuid&uuid) = 0;
    };

    class ProxyManager  : public QObject, public Dependency {
        Q_OBJECT
    public:
        using Pointer = QSharedPointer<ProxyManager>;

        PluginURIs getAvailablePluginURIs() const;
        std::vector<QUuid> getAssignedObjectIDs(const PluginURI& uri = PluginURI()) const;

        // provider registration
        bool registerProvider(const PluginURI& pluginURI, Provider::Pointer provider);
        bool unregisterProvider(const PluginURI& pluginURI, Provider::Pointer provider = nullptr);
        Provider::Pointer getProvider(const PluginURI& pluginURI) const;

        // object-provider registration
        bool assignProvider(const QUuid& uuid, Provider::Pointer provider);
        bool unassignProvider(const QUuid& uuid, Provider::Pointer provider = nullptr);
        Provider::Pointer getProvider(const QUuid& uuid) const;

    protected:
        mutable std::mutex _lock;
        std::unordered_map<PluginURI, Provider::Pointer> _providers;
        std::unordered_map<QUuid, Provider::Pointer> _assigned;
    };

    template <typename T, typename F>
    T withManager(F function) {
        if (!DependencyManager::isSet<ProxyManager>()) {
            return T();
        }
        return function(DependencyManager::get<ProxyManager>());
    }

    inline std::vector<QUuid> getAssignedObjectIDs(const PluginURI& uri = PluginURI()) {
        return withManager<std::vector<QUuid>>([&](ProxyManager::Pointer manager) {
            return manager->getAssignedObjectIDs(uri);
        });
    }

    inline PluginURIs getAvailablePluginURIs() {
        return withManager<PluginURIs>([](ProxyManager::Pointer manager) {
            return manager->getAvailablePluginURIs();
        });
    }

    inline PluginURI getObjectPluginURI(const QUuid& uuid) {
        return withManager<PluginURI>([&](ProxyManager::Pointer manager) {
            if (auto plugin = manager->getProvider(uuid)) {
                return plugin->pluginURI;
            }
            return PluginURI();
        });
    }

    inline ObjectProxy::Pointer getObjectProxy(const QUuid& uuid, const PluginURI& uri = PluginURI()) {
        return withManager<ObjectProxy::Pointer>([&](ProxyManager::Pointer manager) {
            auto provider = uri.empty() ? manager->getProvider(uuid) : manager->getProvider(uri);
            if (provider) {
                return provider->getObjectProxy(uuid);
            }
            return ObjectProxy::Pointer();
        });
    }

    inline ObjectProxy::Pointer createObjectProxy(const QUuid& uuid, const PluginURI& uri, const QVariantMap& parameters = QVariantMap()) {
        return withManager<ObjectProxy::Pointer>([&](ProxyManager::Pointer manager) {
            if (auto plugin = manager->getProvider(uri)) {
                if (auto proxy = plugin->createObjectProxy(uuid, parameters)) {
                    manager->assignProvider(uuid, plugin);
                    return proxy;
                }
            }
            return ObjectProxy::Pointer();
        });
    }

    inline bool removeObjectProxy(const QUuid& uuid, const PluginURI& uri = PluginURI()) {
         return withManager<bool>([&](ProxyManager::Pointer manager) {
             return manager->unassignProvider(uuid, manager->getProvider(uri));
         });
    }

}}
