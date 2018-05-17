//
//  Created by Bradley Austin Davis on 2016/05/09
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderableShapeEntityItem.h"

#include <glm/gtx/quaternion.hpp>

#include <gpu/Batch.h>
#include <DependencyManager.h>
#include <StencilMaskPass.h>
#include <GeometryCache.h>
#include <PerfStat.h>

#include "render-utils/simple_vert.h"
#include "render-utils/simple_frag.h"
#include "render-utils/simple_transparent_frag.h"
#include "render-utils/forward_simple_frag.h"
#include "render-utils/forward_simple_transparent_frag.h"

#include "RenderPipelines.h"

#include <graphics/BufferViewHelpers.h>
#include <graphics-scripting/GraphicsScriptingUtil.h>

//#define SHAPE_ENTITY_USE_FADE_EFFECT
#ifdef SHAPE_ENTITY_USE_FADE_EFFECT
#include <FadeEffect.h>
#endif
using namespace render;
using namespace render::entities;

// Sphere entities should fit inside a cube entity of the same size, so a sphere that has dimensions 1x1x1
// is a half unit sphere.  However, the geometry cache renders a UNIT sphere, so we need to scale down.
static const float SPHERE_ENTITY_SCALE = 0.5f;


ShapeEntityRenderer::ShapeEntityRenderer(const EntityItemPointer& entity) : Parent(entity) {
    _procedural._vertexSource = simple_vert::getSource();
    // FIXME: Setup proper uniform slots and use correct pipelines for forward rendering
    _procedural._opaquefragmentSource = simple_frag::getSource();
    // FIXME: Transparent procedural entities only seem to work if they use the opaque pipelines
    //_procedural._transparentfragmentSource = simple_transparent_frag::getSource();
    _procedural._transparentfragmentSource = simple_frag::getSource();
    _procedural._opaqueState->setCullMode(gpu::State::CULL_NONE);
    _procedural._opaqueState->setDepthTest(true, true, gpu::LESS_EQUAL);
    PrepareStencil::testMaskDrawShape(*_procedural._opaqueState);
    _procedural._opaqueState->setBlendFunction(false,
        gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA,
        gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ONE);
    _procedural._transparentState->setCullMode(gpu::State::CULL_BACK);
    _procedural._transparentState->setDepthTest(true, true, gpu::LESS_EQUAL);
    PrepareStencil::testMask(*_procedural._transparentState);
    _procedural._transparentState->setBlendFunction(true,
        gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA,
        gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ONE);
}

bool ShapeEntityRenderer::needsRenderUpdate() const {
    if (_procedural.isEnabled() && _procedural.isFading()) {
        return true;
    }

    return Parent::needsRenderUpdate();
}

bool ShapeEntityRenderer::needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const {
    if (_lastUserData != entity->getUserData()) {
        return true;
    }
    if (_material != entity->getMaterial()) {
        return true;
    }

    if (_shape != entity->getShape()) {
        return true;
    }

    if (_dimensions != entity->getScaledDimensions()) {
        return true;
    }

    if (auto proxy = getEntityProxy()) {
        if (proxy->flags & js::Graphics::RenderFlag::DIRTY) {
            return true;
        }
    }

    return false;
}

void ShapeEntityRenderer::doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) {
    withWriteLock([&] {
        auto userData = entity->getUserData();
        if (_lastUserData != userData) {
            _lastUserData = userData;
            _procedural.setProceduralData(ProceduralData::parse(_lastUserData));
        }

        removeMaterial(_material, "0");
        _material = entity->getMaterial();
        addMaterial(graphics::MaterialLayer(_material, 0), "0");

        _shape = entity->getShape();
        _position = entity->getWorldPosition();
        _dimensions = entity->getScaledDimensions();
        _orientation = entity->getWorldOrientation();
        _renderTransform = getModelTransform();

        if (_shape == entity::Sphere) {
            _renderTransform.postScale(SPHERE_ENTITY_SCALE);
        }

        _renderTransform.postScale(_dimensions);
        if (auto proxy = getEntityProxy()) {
            proxy->setProperties({ { "renderTransform", scriptable::toVariant(_renderTransform.getMatrix()) }});
            if (_pendingMeshPartReplacement) {
                auto replace = _pendingMeshPartReplacement;
                _pendingMeshPartReplacement = { nullptr, -1, -1 };
                if (auto meshProxy = std::dynamic_pointer_cast<plugins::entity::MeshObjectProxy>(proxy)) {
                    meshProxy->replaceScriptableModelMeshPart(replace.model, replace.meshIndex, replace.partIndex);
                }
            }
            const float deltaTime = NAN; // TODO
            _needsRenderUpdate = proxy->update(scene, transaction, deltaTime);
        }
    });
}

void ShapeEntityRenderer::doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) {
    withReadLock([&] {
        if (_procedural.isEnabled() && _procedural.isFading()) {
            float isFading = Interpolate::calculateFadeRatio(_procedural.getFadeStartTime()) < 1.0f;
            _procedural.setIsFading(isFading);
        }
        if (entity->getEntityProxy() != _proxy) {
            auto old = _proxy;
            _proxy = entity->getEntityProxy();
            qDebug() << "ShapeEntityRenderer::setEntityProxy" << "_proxy = " << _proxy.get() << "(was:" << old.get() << ")";
            entity->markDirtyFlags(Simulation::DIRTY_SHAPE);
            _needsRenderUpdate = true;
        }
    });
}

bool ShapeEntityRenderer::isTransparent() const {
    if (_procedural.isEnabled() && _procedural.isFading()) {
        return Interpolate::calculateFadeRatio(_procedural.getFadeStartTime()) < 1.0f;
    }

    auto mat = _materials.find("0");
    if (mat != _materials.end()) {
        if (mat->second.top().material) {
            auto matKey = mat->second.top().material->getKey();
            if (matKey.isTranslucent()) {
                return true;
            }
        }
    }

    return Parent::isTransparent();
}

ItemKey ShapeEntityRenderer::getKey() {
    ItemKey::Builder builder;
    builder.withTypeShape().withTypeMeta().withTagBits(render::ItemKey::TAG_BITS_0 | render::ItemKey::TAG_BITS_1);

    withReadLock([&] {
        if (isTransparent()) {
            builder.withTransparent();
        } else if (_canCastShadow) {
            builder.withShadowCaster();
        }
    });

    return builder.build();
}

bool ShapeEntityRenderer::useMaterialPipeline() const {
    bool proceduralReady = resultWithReadLock<bool>([&] {
        return _procedural.isReady();
    });
    if (proceduralReady) {
        return false;
    }

    graphics::MaterialKey drawMaterialKey;
    auto mat = _materials.find("0");
    if (mat != _materials.end() && mat->second.top().material) {
        drawMaterialKey = mat->second.top().material->getKey();
    }

    if (drawMaterialKey.isEmissive() || drawMaterialKey.isUnlit() || drawMaterialKey.isMetallic() || drawMaterialKey.isScattering()) {
        return true;
    }

    // If the material is using any map, we need to use a material ShapeKey
    for (int i = 0; i < graphics::Material::MapChannel::NUM_MAP_CHANNELS; i++) {
        if (drawMaterialKey.isMapChannel(graphics::Material::MapChannel(i))) {
            return true;
        }
    }
    return false;
}

ShapeKey ShapeEntityRenderer::getShapeKey() {
    if (useMaterialPipeline()) {
        graphics::MaterialKey drawMaterialKey;
        if (_materials["0"].top().material) {
            drawMaterialKey = _materials["0"].top().material->getKey();
        }

        bool isTranslucent = drawMaterialKey.isTranslucent();
        bool hasTangents = drawMaterialKey.isNormalMap();
        bool hasLightmap = drawMaterialKey.isLightmapMap();
        bool isUnlit = drawMaterialKey.isUnlit();

        ShapeKey::Builder builder;
        builder.withMaterial();

        if (isTranslucent) {
            builder.withTranslucent();
        }
        if (hasTangents) {
            builder.withTangents();
        }
        if (hasLightmap) {
            builder.withLightmap();
        }
        if (isUnlit) {
            builder.withUnlit();
        }

        return builder.build();
    } else {
        return Parent::getShapeKey();
    }
}

void ShapeEntityRenderer::doRender(RenderArgs* args) {
    PerformanceTimer perfTimer("RenderableShapeEntityItem::render");
    Q_ASSERT(args->_batch);

    if (auto proxy = getEntityProxy()) {
        proxy->debugRender(args);
        return;
    }

    gpu::Batch& batch = *args->_batch;

    std::shared_ptr<graphics::Material> mat;
    auto geometryCache = DependencyManager::get<GeometryCache>();
    GeometryCache::Shape geometryShape;
    bool proceduralRender = false;
    glm::vec4 outColor;
    withReadLock([&] {
        geometryShape = geometryCache->getShapeForEntityShape(_shape);
        batch.setModelTransform(_renderTransform); // use a transform with scale, rotation, registration point and translation
        mat = _materials["0"].top().material;
        if (mat) {
            outColor = glm::vec4(mat->getAlbedo(), mat->getOpacity());
            if (_procedural.isReady()) {
                outColor = _procedural.getColor(outColor);
                outColor.a *= _procedural.isFading() ? Interpolate::calculateFadeRatio(_procedural.getFadeStartTime()) : 1.0f;
                _procedural.prepare(batch, _position, _dimensions, _orientation, outColor);
                proceduralRender = true;
            }
        }
    });

    if (!mat) {
        return;
    }

    if (proceduralRender) {
        if (render::ShapeKey(args->_globalShapeKey).isWireframe()) {
            geometryCache->renderWireShape(batch, geometryShape, outColor);
        } else {
            geometryCache->renderShape(batch, geometryShape, outColor);
        }
    } else if (!useMaterialPipeline()) {
        // FIXME, support instanced multi-shape rendering using multidraw indirect
        outColor.a *= _isFading ? Interpolate::calculateFadeRatio(_fadeStartTime) : 1.0f;
        auto pipeline = outColor.a < 1.0f ? geometryCache->getTransparentShapePipeline() : geometryCache->getOpaqueShapePipeline();
        if (render::ShapeKey(args->_globalShapeKey).isWireframe()) {
            geometryCache->renderWireShapeInstance(args, batch, geometryShape, outColor, pipeline);
        } else {
            geometryCache->renderSolidShapeInstance(args, batch, geometryShape, outColor, pipeline);
        }
    } else {
        RenderPipelines::bindMaterial(mat, batch, args->_enableTexturing);
        args->_details._materialSwitches++;

        geometryCache->renderShape(batch, geometryShape);
    }

    const auto triCount = geometryCache->getShapeTriangleCount(geometryShape);
    args->_details._trianglesRendered += (int)triCount;
}

js::Graphics::ModelPointer ShapeEntityRenderer::getScriptableModel()  {
    js::Graphics::ModelPointer result;
    if (auto meshProxy = std::dynamic_pointer_cast<plugins::entity::MeshObjectProxy>(getEntityProxy())) {
        if (auto result = meshProxy->getScriptableModel()) {
            std::lock_guard<std::mutex> lock(_materialsLock);
            result->appendMaterials(_materials);
            return result;
        }
    }
    auto geometryCache = DependencyManager::get<GeometryCache>();
    auto geometryShape = geometryCache->getShapeForEntityShape(_shape);
    glm::vec3 vertexColor{ 1.0f };
    {
        std::lock_guard<std::mutex> lock(_materialsLock);
        if (_materials["0"].top().material) {
            vertexColor = _materials["0"].top().material->getAlbedo();
        }
    }
    if (auto mesh = geometryCache->meshFromShape(geometryShape, vertexColor)) {
        result = js::Graphics::ModelPointer::create();
        result->objectID = getEntity()->getID();
        result->append(mesh);
        {
            std::lock_guard<std::mutex> lock(_materialsLock);
            result->appendMaterials(_materials);
        }
    }
    return result;
}

bool ShapeEntityRenderer::canReplaceModelMeshPart(int meshIndex, int partIndex) {
    if (auto meshProxy = std::dynamic_pointer_cast<plugins::entity::MeshObjectProxy>(getEntityProxy())) {
        return meshProxy->canReplaceModelMeshPart(meshIndex, partIndex);
    }
    return false;
}

bool ShapeEntityRenderer::replaceScriptableModelMeshPart(const js::Graphics::ModelPointer& model, int meshIndex, int partIndex) {
    if (auto meshProxy = std::dynamic_pointer_cast<plugins::entity::MeshObjectProxy>(getEntityProxy())) {
        if (canReplaceModelMeshPart(meshIndex, partIndex)) {
            withWriteLock([=]{ _pendingMeshPartReplacement = { model, meshIndex, partIndex }; });
            _needsRenderUpdate = true;
            return true;
        }
    }
    return false;
}

bool ShapeEntityRenderer::overrideModelRenderFlags(js::Graphics::RenderFlags flagsToSet, js::Graphics::RenderFlags flagsToClear) {
    if (auto meshProxy = std::dynamic_pointer_cast<plugins::entity::MeshObjectProxy>(getEntityProxy())) {
        if (meshProxy->overrideModelRenderFlags(flagsToSet, flagsToClear)) {
            _needsRenderUpdate = true;
            return true;
        }
    }
    return false;
}

void ShapeEntityRenderer::onRemoveFromSceneTyped(const TypedEntityPointer& entity) {
    withWriteLock([this]{
        if (_proxy) {
            qDebug() << "...unloading ObjectProxy" << _proxy.get();
            _proxy->unload();
            _proxy.reset();
        }
    });
    Parent::onRemoveFromSceneTyped(entity);
}

void ShapeEntityRenderer::onAddToSceneTyped(const TypedEntityPointer& entity) {
    withWriteLock([this]{
        if (_proxy) {
            qDebug() << "...preloading ObjectProxy" << _proxy.get();
            _proxy->preload();
        }
    });
    Parent::onAddToSceneTyped(entity);
}
