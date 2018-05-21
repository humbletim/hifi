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
    if (_providers.find(pluginURI) != _providers.end()) {
        if (!provider || _providers[pluginURI] == provider) {
            _providers.erase(pluginURI);
            return true;
        }
    }
    return false;
}

Provider::Pointer ProxyManager::getProvider(const PluginURI& pluginURI) const{
    std::lock_guard<std::mutex> locker(_lock);
    Provider::Pointer result;
    if (_providers.find(pluginURI) != _providers.end()) {
        result = _providers.at(pluginURI);
    }
    return result;
}

bool ProxyManager::assignProvider(const QUuid& uuid, Provider::Pointer provider) {
    std::lock_guard<std::mutex> locker(_lock);
    bool result = _assigned.find(uuid) == _assigned.end();
    _assigned[uuid] = provider;
    return result;
}

bool ProxyManager::unassignProvider(const QUuid& uuid, Provider::Pointer provider) {
    std::lock_guard<std::mutex> locker(_lock);
    if (_assigned.find(uuid) != _assigned.end()) {
        if (!provider || _assigned[uuid] == provider) {
            auto tmp = _assigned[uuid];
            _assigned.erase(uuid);
            qDebug() << "unassignProvider -- destroying object proxy" << uuid;
            return tmp->destroyObjectProxy(uuid);
        }
    }
    return false;
}

Provider::Pointer ProxyManager::getProvider(const QUuid& uuid) const {
    std::lock_guard<std::mutex> locker(_lock);
    if (_assigned.find(uuid) != _assigned.end()) {
        return _assigned.at(uuid);
    }
    return nullptr;
}


}}
