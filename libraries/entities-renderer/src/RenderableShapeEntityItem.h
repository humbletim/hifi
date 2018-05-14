//
//  Created by Bradley Austin Davis on 2016/05/09
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RenderableShapeEntityItem_h
#define hifi_RenderableShapeEntityItem_h

#include "RenderableEntityItem.h"

#include <procedural/Procedural.h>
#include <ShapeEntityItem.h>

namespace render { namespace entities { 

class ShapeEntityRenderer : public TypedEntityRenderer<ShapeEntityItem> {
    using Parent = TypedEntityRenderer<ShapeEntityItem>;
    using Pointer = std::shared_ptr<ShapeEntityRenderer>;
public:
    ShapeEntityRenderer(const EntityItemPointer& entity);

    virtual plugins::entity::ObjectProxy::Pointer getEntityProxy() const override { return _proxy; }
    virtual js::Graphics::ModelPointer getScriptableModel() override;
    virtual bool canReplaceModelMeshPart(int meshIndex, int partIndex) override;
    virtual bool replaceScriptableModelMeshPart(const js::Graphics::ModelPointer& model, int meshIndex, int partIndex) override;
    virtual bool overrideModelRenderFlags(js::Graphics::RenderFlags flagsToSet, js::Graphics::RenderFlags flagsToClear) override;
protected:
    ItemKey getKey() override;
    ShapeKey getShapeKey() override;
    virtual void onAddToSceneTyped(const TypedEntityPointer& entity) override;
    virtual void onRemoveFromSceneTyped(const TypedEntityPointer& entity) override;

private:
    virtual bool needsRenderUpdate() const override;
    virtual bool needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const override;
    virtual void doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) override;
    virtual void doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) override;
    virtual void doRender(RenderArgs* args) override;
    virtual bool isTransparent() const override;

    bool useMaterialPipeline() const;

    Procedural _procedural;
    QString _lastUserData;
    Transform _renderTransform;
    entity::Shape _shape { entity::Sphere };
    std::shared_ptr<graphics::Material> _material;
    glm::vec3 _position;
    glm::vec3 _dimensions;
    glm::quat _orientation;
    plugins::entity::ObjectProxy::Pointer _proxy;

    struct PendingMeshPartReplacement {
        js::Graphics::ModelPointer model;
        int meshIndex;
        int partIndex;
        operator bool() { return (bool)model; }
    };
    PendingMeshPartReplacement _pendingMeshPartReplacement;
};

} } 
#endif // hifi_RenderableShapeEntityItem_h
