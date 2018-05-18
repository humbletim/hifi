// MeshEntityProxy
#pragma once

#include <object-plugins/Forward.h>
#include <graphics/Forward.h>
#include <MeshPartPayload.h>
#include <QUuid>
#include <memory>
#include <ShapeEntityItem.h>
#include <TriangleSet.h>
#include <shared/ReadWriteLockable.h>

class MeshEntityPlugin;
using MeshEntityPluginPointer = std::shared_ptr<MeshEntityPlugin>;

using plugins::entity::ObjectProxy;
using plugins::entity::MeshObjectProxy;
using plugins::entity::MeshRay;
using plugins::entity::IntersectionResultRef;

class MeshEntityProxy : public MeshObjectProxy,
    public std::enable_shared_from_this<MeshObjectProxy> {
public:
    QVariantMap properties;
    struct MeshRenderItem {
        using Pointer = std::shared_ptr<MeshRenderItem>;
        MeshRenderItem(const render::ScenePointer& scene, graphics::MeshPointer mesh, int partIndex = 0, graphics::MaterialPointer = graphics::MaterialPointer());
        ~MeshRenderItem();
        graphics::MeshPointer mesh;
        int partIndex;
        graphics::MaterialPointer material;
        render::ItemID itemID;
        std::shared_ptr<MeshPartPayload::Payload> renderPayload;
        std::shared_ptr<MeshPartPayload> payload;
        render::ScenePointer scene;
    };
#define getset(T, Name, _pvt)                                           \
    T _pvt;                                                             \
    T get##Name() { QReadLocker locker(&lock); return _pvt; } \
    void set##Name(T v) { QWriteLocker locker(&lock);  _pvt = v;}
    
    getset(MeshRenderItem::Pointer, RenderItem, _renderItem);
    getset(graphics::MaterialPointer, EntityMaterial, _material);
    getset(graphics::MeshPointer, Mesh, _mesh);
#undef getset

    MeshEntityProxy(MeshEntityPluginPointer plugin) : _plugin(plugin) {}
    virtual ~MeshEntityProxy();
    virtual QString toString() const override { return QVariant(properties).toString(); }

    virtual bool setProperties(const QVariantMap& properties) override;
    virtual QVariantMap getProperties() override { QReadLocker locker(&lock); return properties; }
        
    virtual void setMaterial(graphics::MaterialPointer material) override { setEntityMaterial(material); }
    virtual bool update(const render::ScenePointer& scene, render::Transaction& transaction, float deltaTime) override;

    virtual bool recomputeAABox(AABox& box) const override;
    virtual bool recomputeDimensions(glm::vec3& box) const override;
    virtual bool findRayIntersection(const MeshRay& meshRay, IntersectionResultRef& result) override;

    virtual void messageReceived(const QVariant& message) override;
    virtual void preload() override;
    virtual void unload() override;

    virtual js::Graphics::ModelPointer getScriptableModel() override;
    virtual bool canReplaceModelMeshPart(int meshIndex, int partIndex) override;
    virtual bool replaceScriptableModelMeshPart(const js::Graphics::ModelPointer& model, int meshIndex, int partIndex) override;
    virtual bool overrideModelRenderFlags(js::Graphics::RenderFlags flagsToSet, js::Graphics::RenderFlags flagsToClear) override;

    bool initialize();
protected:
    class MyTriangleSet : public TriangleSet {
    public:
        int indexOf(const Triangle tri);
    };
    MyTriangleSet _triangles;
    void recalculateTriangleSet();
    MeshEntityPluginPointer _plugin;
};

