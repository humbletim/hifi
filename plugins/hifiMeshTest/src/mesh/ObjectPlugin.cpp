#include "ObjectPlugin.h"
#include "ObjectProxy.h"

ObjectProxy::Pointer MeshEntityPlugin::getObjectProxy(const QUuid&uuid) const {
    std::lock_guard<std::mutex> locker(_lock);
    if (meshes.find(uuid) != meshes.end()) {
        return meshes.at(uuid);
    }
    return nullptr;
}

ObjectProxy::Pointer MeshEntityPlugin::createObjectProxy(const QUuid&uuid, const QVariantMap& parameters) {
    std::lock_guard<std::mutex> locker(_lock);
    if (meshes.find(uuid) != meshes.end()) {
        return meshes.at(uuid);
    }
    auto result = std::make_shared<MeshEntityProxy>(shared_from_this());
    result->objectID = uuid;
    result->initialize(parameters);
    return meshes[uuid] = result;
}

bool MeshEntityPlugin::destroyObjectProxy(const QUuid& uuid)  {
    std::lock_guard<std::mutex> locker(_lock);
    if (meshes.find(uuid) != meshes.end()) {
        meshes.erase(uuid);
        return true;
    }
    return false;
}
