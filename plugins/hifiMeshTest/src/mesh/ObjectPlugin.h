#pragma once

#include <mutex>
#include <QtCore/QUuid>
#include <QtCore/QHash>
#include <object-plugins/Forward.h>
#include <UUIDHasher.h>

class MeshEntityProxy;
using plugins::entity::ProxyResolver;

class MeshEntityPlugin : public ProxyResolver, public std::enable_shared_from_this<MeshEntityPlugin> {
    using ObjectProxy = plugins::entity::ObjectProxy;
public:
    std::unordered_map< QUuid, std::shared_ptr<MeshEntityProxy> > meshes;
    std::mutex _lock;
    virtual ObjectProxy::Pointer openObjectProxy(const QUuid&uuid) override;
    virtual bool closeObjectProxy(const QUuid& uuid) override;
    virtual ObjectProxy::Pointer getObjectProxy(const QUuid& uuid) override;
};
