#include "ProxyManager.h"

namespace plugins { namespace entity {
    ObjectProxy::Pointer ProxyResolver::INVALID_PROXY{ nullptr };

    bool ProxyManager::setPluginResolver(const std::string& pluginURI, ProxyResolver::Pointer resolver) {
        std::lock_guard<std::mutex> locker(_lock);
        if (resolver) {
            resolver->pluginURI = QString::fromStdString(pluginURI);
            _resolvers[pluginURI] = resolver;
        } else if (_resolvers.find(pluginURI) != _resolvers.end()) {
            _resolvers.erase(pluginURI);
        } else {
            return false;
        }
        return true;
    }
    std::vector<std::string> ProxyManager::getAvailablePlugins() const {
        std::lock_guard<std::mutex> locker(_lock);
        std::vector<std::string> uris;
        for (const auto& kv : _resolvers) {
            uris.push_back(kv.first);
        }
        return uris;
    }

    std::vector<std::string> ProxyManager::getAssigned() const {
        std::lock_guard<std::mutex> locker(_lock);
        std::vector<std::string> result;
        qDebug() << "getAssigned" << _assigned.keys();
        for (const auto& id : _assigned.keys()) {
            result.push_back(id.toString().toStdString());
        }
        return result;
    }

    ProxyResolver::Pointer ProxyManager::getPluginResolver(const std::string& pluginURI) const {
        std::lock_guard<std::mutex> locker(_lock);
        ProxyResolver::Pointer result;
        if (_resolvers.find(pluginURI) != _resolvers.end()) {
            result = _resolvers.at(pluginURI);
        }
        return result;
    }
    std::vector<std::string> ProxyManager::getRegisteredPlugins() const {
        std::lock_guard<std::mutex> locker(_lock);
        std::vector<std::string> keys;
        for (const auto& kv : _resolvers) {
            keys.emplace_back(kv.first);
        }
        return keys;
    }

    bool ProxyManager::clearObject(const QUuid& uuid) {
        std::lock_guard<std::mutex> locker(_lock);
        if (_assigned.contains(uuid)) {
            qDebug() << "clearObject -- closing object proxy" << uuid;
            auto tmp = _assigned[uuid];
            _assigned.remove(uuid);
            return tmp->closeObjectProxy(uuid);
        }
        return false;
    }
    bool ProxyManager::associate(const QUuid& uuid, ProxyResolver::Pointer resolver) {
        std::lock_guard<std::mutex> locker(_lock);
        bool result = !_assigned.contains(uuid);
        qDebug() << "associate" << uuid << resolver->pluginURI;
        _assigned[uuid] = resolver;
        return result;
    }

    ProxyResolver::Pointer ProxyManager::getResolver(const QUuid& uuid) const {
        std::lock_guard<std::mutex> locker(_lock);
        if (_assigned.contains(uuid)) {
            return _assigned[uuid];
        }
        return nullptr;
    }
    ObjectProxy::Pointer ProxyManager::getObject(const QUuid& uuid) const {
        if (auto resolver = getResolver(uuid)) {
            return resolver->getObjectProxy(uuid);
        }
        return nullptr;
    }
}}
