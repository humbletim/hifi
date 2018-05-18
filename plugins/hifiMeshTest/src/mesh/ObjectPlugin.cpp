#include "ObjectPlugin.h"
#include "ObjectProxy.h"

ObjectProxy::Pointer MeshEntityPlugin::getObjectProxy(const QUuid&uuid) {
    std::lock_guard<std::mutex> locker(_lock);
    if (meshes.find(uuid) != meshes.end()) {
        return meshes[uuid];
    }
    return nullptr;
}

ObjectProxy::Pointer MeshEntityPlugin::openObjectProxy(const QUuid&uuid)  {
    std::lock_guard<std::mutex> locker(_lock);
    if (meshes.find(uuid) != meshes.end()) {
        return meshes[uuid];
    }
    auto result = std::make_shared<MeshEntityProxy>(shared_from_this());
    result->objectID = uuid;
    result->initialize();
    return meshes[uuid] = result;
}

bool MeshEntityPlugin::closeObjectProxy(const QUuid& uuid)  {
    std::lock_guard<std::mutex> locker(_lock);
    if (meshes.find(uuid) != meshes.end()) {
        meshes.erase(uuid);
        return true;
    }
    return false;
}
