#pragma once

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QVariant>
#include <QtCore/QUuid>
#include <QSharedPointer>
#include <memory>
#include <unordered_map>

#include <DependencyManager.h>
#include <SpatiallyNestable.h>

#include "graphics/Material.h"
#include "graphics/TextureMap.h"

namespace graphics {
    class Mesh;
    using MeshPointer = std::shared_ptr<Mesh>;
}
class Model;
using ModelPointer = std::shared_ptr<Model>;
namespace gpu {
    class BufferView;
}
class QScriptEngine;

namespace scriptable {
    class ScriptableModelBase;
    using ScriptableModelBasePointer = QSharedPointer<ScriptableModelBase>;
    using ScriptableModelPointer = ScriptableModelBasePointer;

    class ModelProvider;
    using ModelProviderPointer = std::shared_ptr<scriptable::ModelProvider>;

    using ScriptableMeshPointer = graphics::MeshPointer;
    using ScriptableMeshes = QVector<scriptable::ScriptableMeshPointer>;

    /**jsdoc
     * @typedef {object} Graphics.Material
     * @property {string} name
     * @property {string} model
     * @property {number} opacity
     * @property {number} roughness
     * @property {number} metallic
     * @property {number} scattering
     * @property {boolean} unlit
     * @propety {Vec3} emissive
     * @propety {Vec3} albedo
     * @property {string} emissiveMap
     * @property {string} albedoMap
     * @property {string} opacityMap
     * @property {string} metallicMap
     * @property {string} specularMap
     * @property {string} roughnessMap
     * @property {string} glossMap
     * @property {string} normalMap
     * @property {string} bumpMap
     * @property {string} occlusionMap
     * @property {string} lightmapMap
     * @property {string} scatteringMap
     */
    class ScriptableMaterial {
    public:
        ScriptableMaterial() {}
        ScriptableMaterial(const graphics::MaterialPointer& material);
        ScriptableMaterial(const ScriptableMaterial& material) { *this = material; }
        ScriptableMaterial& operator=(const ScriptableMaterial& material);

        QString name;
        QString model;
        float opacity;
        float roughness;
        float metallic;
        float scattering;
        bool unlit;
        glm::vec3 emissive;
        glm::vec3 albedo;
        QString emissiveMap;
        QString albedoMap;
        QString opacityMap;
        QString metallicMap;
        QString specularMap;
        QString roughnessMap;
        QString glossMap;
        QString normalMap;
        QString bumpMap;
        QString occlusionMap;
        QString lightmapMap;
        QString scatteringMap;
    };

    /**jsdoc
     * @typedef {object} Graphics.MaterialLayer
     * @property {Graphics.Material} material - This layer's material.
     * @property {number} priority - The priority of this layer.  If multiple materials are applied to a mesh part, only the highest priority layer is used.
     */
    class ScriptableMaterialLayer {
    public:
        ScriptableMaterialLayer() {}
        ScriptableMaterialLayer(const graphics::MaterialLayer& materialLayer) : material(materialLayer.material), priority(materialLayer.priority) {}
        ScriptableMaterialLayer(const ScriptableMaterialLayer& materialLayer) { *this = materialLayer; }
        ScriptableMaterialLayer& operator=(const ScriptableMaterialLayer& materialLayer);

        ScriptableMaterial material;
        quint16 priority;
    };
    typedef QHash<QString, QVector<scriptable::ScriptableMaterialLayer>> MultiMaterialMap;

    // abstract container for holding one or more references to mesh pointers
    class ScriptableModelBase : public QObject {
        Q_OBJECT
    public:
        ModelProviderPointer provider;
        QUuid objectID; // spatially nestable ID
        ScriptableMeshes meshes;
        MultiMaterialMap materialLayers;
        QVector<QString> materialNames;

        ScriptableModelBase(QObject* parent = nullptr) : QObject(parent) {}
        ScriptableModelBase(const ScriptableModelBase& other) :
            QObject(other.parent()) { *this = other; }
        ScriptableModelBase& operator=(const ScriptableModelBase& other);
        virtual ~ScriptableModelBase();

        void append(graphics::MeshPointer mesh);
        void appendMaterial(const graphics::MaterialLayer& materialLayer, int shapeID, std::string materialName);
        void appendMaterials(const std::unordered_map<std::string, graphics::MultiMaterial>& materialsToAppend);
        void appendMaterialNames(const std::vector<std::string>& names);
        // TODO: in future containers for these could go here
        // QVariantMap shapes;
        // QVariantMap armature;
    };

    // mixin class for Avatar/Entity/Overlay Rendering that expose their in-memory graphics::Meshes
    class ModelProvider {
    public:
        NestableType modelProviderType;
        virtual scriptable::ScriptableModelBase getScriptableModel() = 0;
        virtual bool canReplaceModelMeshPart(int meshIndex, int partIndex) { return false; }
        virtual bool replaceScriptableModelMeshPart(scriptable::ScriptableModelBasePointer model, int meshIndex, int partIndex) { return false; }
    };

    // mixin class for resolving UUIDs into a corresponding ModelProvider
    class ModelProviderFactory : public QObject, public Dependency {
        Q_OBJECT
    public:
        virtual scriptable::ModelProviderPointer lookupModelProvider(const QUuid& uuid) = 0;
    signals:
        void modelAddedToScene(const QUuid& objectID, NestableType nestableType, const ModelPointer& sender);
        void modelRemovedFromScene(const QUuid& objectID, NestableType nestableType, const ModelPointer& sender);
    };

    class MeshPart : public QObject {
        Q_OBJECT
    public:
        MeshPart(graphics::MeshPointer mesh, int partIndex) :
            QObject(nullptr), parentMesh(mesh), partIndex(partIndex) {}
        graphics::MeshPointer parentMesh;
        int partIndex{ -1 };
    };
    using MeshPartPointer = QSharedPointer<MeshPart>;
    using ScriptableMeshPartPointer = MeshPartPointer;
}
