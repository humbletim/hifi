//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QVariant>
#include <QtCore/QUuid>
#include <QSharedPointer>
#include <memory>
#include <unordered_map>
#include <functional>

#include <DependencyManager.h>
#include <glm/glm.hpp>
#include <BoxBase.h>
#include <RegisteredMetaTypes.h>

// FIXME: relative paths used here to avoid Android cyclic library dependencies
// (note: MeshObjectPlugin.h only depends on the definitions found in these Forward headers)
#include <../../libraries/graphics-scripting/src/graphics-scripting/Forward.h>
#include <../../libraries/render/src/render/Forward.h>

namespace plugins { namespace object {
    class ObjectRay : public MathPick {
    public:
        glm::vec3 origin;
        glm::vec3 direction;
        QVariantMap metadata;
        ObjectRay(const glm::vec3& origin, const glm::vec3& direction, const QVariantMap& metadata = QVariantMap());
        operator bool() const override;
        QVariantMap toVariantMap() const override;
    };
    struct IntersectionResultRef {
        float& distance;
        BoxFace& face;
        glm::vec3& surfaceNormal;
        QVariantMap& extraInfo;
    };

    class ObjectProxy {
    public:
        using Pointer = std::shared_ptr<ObjectProxy>;
        QUuid objectID;
        js::Graphics::RenderFlags flags;
        glm::mat4 entityToWorldMatrix;
        mutable QReadWriteLock lock { QReadWriteLock::Recursive };

        ObjectProxy(js::Graphics::RenderFlags flags = js::Graphics::RenderFlag::NONE) : flags(flags) {}
        virtual ~ObjectProxy() {}
        virtual QString toString() const { return "[ObjectProxy "+objectID.toString()+"]"; }

        virtual bool update(const render::ScenePointer& scene, render::Transaction& transaction, float deltaTime) { return false; }

        virtual bool setProperties(const QVariantMap& properties) { return false; }
        virtual QVariantMap getProperties() { return QVariantMap(); }

        virtual void debugRender(render::Args* args) {}
        virtual void setMaterial(graphics::MaterialPointer material) {}

        std::function<void(const QVariant& message)> postMessage = [](const QVariant&){};
        virtual void messageReceived(const QVariant& message) { }

        virtual bool recomputeAABox(AABox& box) const { return false; }
        virtual bool recomputeDimensions(glm::vec3& box) const { return false; }
        virtual bool findRayIntersection(const ObjectRay& meshRay, IntersectionResultRef& result) { return false; }

        virtual void preload() {}
        virtual void unload() {}
    };

    class MeshObjectProxy : public ObjectProxy, public js::Graphics::ModelProvider {
    public:
        MeshObjectProxy(js::Graphics::RenderFlags flags = js::Graphics::RenderFlag::NONE) : ObjectProxy(flags) {}
        virtual ~MeshObjectProxy() {}
        virtual QString toString() const override { return "[MeshObjectProxy "+objectID.toString()+"]"; }

        virtual js::Graphics::ModelPointer getGraphicsModel() override { return nullptr; }
        virtual bool canReplaceModelMeshPart(int meshIndex, int partIndex) override { return false; }
        virtual bool replaceScriptableModelMeshPart(const js::Graphics::ModelPointer& model, int meshIndex, int partIndex) override { return false; }
        virtual bool overrideRenderFlags(js::Graphics::RenderFlags flagsToSet, js::Graphics::RenderFlags flagsToClear) override { return false; }
    };
}}

// TODO: remove / relocate with graphics-scripting/_backwards-compat.h changes
class EntityItemProperties;
QVariantMap EntityItemPropertiesToVariantMap(const EntityItemProperties& properties);
