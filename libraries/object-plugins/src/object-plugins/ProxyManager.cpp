#include "ProxyManager.h"

namespace plugins { namespace object {

PluginURIs ProxyManager::getAvailablePluginURIs() const {
    std::lock_guard<std::mutex> locker(_lock);
    PluginURIs uris;
    for (const auto& kv : _providers) {
        uris.push_back(kv.first);
    }
    return uris;
}

std::vector<QUuid> ProxyManager::getAssignedObjectIDs(const PluginURI& uri) const {
    std::lock_guard<std::mutex> locker(_lock);
    std::vector<QUuid> result;
    for (const auto& kv : _assigned) {
        if (uri.empty() || uri == kv.second->pluginURI) {
            result.push_back(kv.first);
        }
    }
    return result;
}

bool ProxyManager::registerProvider(const PluginURI& pluginURI, Provider::Pointer provider) {
    std::lock_guard<std::mutex> locker(_lock);
    if (provider) {
        provider->pluginURI = pluginURI;
        _providers[pluginURI] = provider;
        return true;
    }
    return true;
}

bool ProxyManager::unregisterProvider(const PluginURI& pluginURI, Provider::Pointer provider) {
    std::lock_guard<std::mutex> locker(_lock);
    auto it = _providers.find(pluginURI);
    if (it != _providers.end()) {
        if (!provider || it->second == provider) {
            _providers.erase(it);
            return true;
        }
    }
    return false;
}

Provider::Pointer ProxyManager::getProvider(const PluginURI& pluginURI) const{
    std::lock_guard<std::mutex> locker(_lock);
    Provider::Pointer result;
    auto it = _providers.find(pluginURI);
    if (it != _providers.end()) {
        result = it->second;
    }
    return result;
}

bool ProxyManager::assignProvider(const QUuid& uuid, Provider::Pointer provider) {
    std::lock_guard<std::mutex> locker(_lock);
    bool existed = _assigned.find(uuid) != _assigned.end();
    _assigned[uuid] = provider;
    return existed;
}

bool ProxyManager::unassignProvider(const QUuid& uuid, Provider::Pointer provider) {
    std::lock_guard<std::mutex> locker(_lock);
    auto it = _assigned.find(uuid);
    if (it != _assigned.end()) {
        if (!provider || it->second == provider) {
            auto tmp = it->second;
            _assigned.erase(it);
            qDebug() << "unassignProvider -- destroying object proxy" << uuid;
            return tmp->destroyObjectProxy(uuid);
        }
    }
    return false;
}

Provider::Pointer ProxyManager::getProvider(const QUuid& uuid) const {
    std::lock_guard<std::mutex> locker(_lock);
    auto it = _assigned.find(uuid);
    if (it != _assigned.end()) {
        return it->second;
    }
    return nullptr;
}


}}
