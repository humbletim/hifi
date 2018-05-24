#pragma once

#include <mutex>
#include <QtCore/QUuid>
#include <QtCore/QHash>
#include <object-plugins/Forward.h>
#include <UUIDHasher.h>

class MeshEntityProxy;
using plugins::object::Provider;

class MeshEntityPlugin : public Provider, public std::enable_shared_from_this<MeshEntityPlugin> {
    using ObjectProxy = plugins::object::ObjectProxy;
public:
    std::unordered_map< QUuid, std::shared_ptr<MeshEntityProxy> > meshes;
    mutable std::mutex _lock;
    virtual ObjectProxy::Pointer createObjectProxy(const QUuid&uuid, const QVariantMap& parameters = QVariantMap()) override;
    virtual ObjectProxy::Pointer getObjectProxy(const QUuid&uuid) const override;
    virtual bool destroyObjectProxy(const QUuid&uuid) override;
};
