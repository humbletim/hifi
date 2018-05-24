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

// inline forward declarations to keep this Forward.h include file lightweight
namespace graphics {
    class Mesh;
    class Material;
    class MaterialLayer;
    class MultiMaterial;
    typedef std::shared_ptr< Material > MaterialPointer;
    using MeshPointer = std::shared_ptr< graphics::Mesh >;
    using WeakMeshPointer = std::weak_ptr< graphics::Mesh >;
}
class Model;
using ModelPointer = std::shared_ptr<Model>;
namespace gpu {
    class BufferView;
}
class QScriptEngine;

namespace js { namespace Graphics {
    Q_NAMESPACE
    enum class RenderFlag : int {
        NONE = 0x0,
        DIRTY = 0x1,
        WIREFRAME = 0x2
    };
    Q_DECLARE_FLAGS( RenderFlags, RenderFlag )
    Q_FLAG_NS(RenderFlag)
}}
Q_DECLARE_METATYPE(js::Graphics::RenderFlags)

// TODO: remove with _backwards-compat.h changes
namespace scriptable {
    class ScriptableModelBase;
}
namespace js { namespace Graphics {
    class Model;
    using ModelPointer = QSharedPointer<Model>;

    class ModelProvider;
    using ModelProviderPointer = std::shared_ptr<ModelProvider>;
    using WeakModelProviderPointer = std::weak_ptr<ModelProvider>;

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
    class Material {
    public:
        Material() {}
        Material(const graphics::MaterialPointer& material);
        Material(const Material& material) { *this = material; }
        Material& operator=(const Material& material);

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
    class MaterialLayer {
    public:
        MaterialLayer() {}
        MaterialLayer(const graphics::MaterialLayer& materialLayer);
        MaterialLayer(const MaterialLayer& materialLayer) { *this = materialLayer; }
        MaterialLayer& operator=(const MaterialLayer& materialLayer);

        Material material;
        quint16 priority;
    };
    typedef QHash<QString, QVector<js::Graphics::MaterialLayer>> MultiMaterialMap;

    class Mesh : public QObject, public QEnableSharedFromThis<Mesh> {
        Q_OBJECT
    public:
        WeakModelProviderPointer provider;
        ModelPointer model;
        graphics::WeakMeshPointer weakMesh;
        graphics::MeshPointer strongMesh;
        Mesh(WeakModelProviderPointer provider, ModelPointer model, graphics::WeakMeshPointer weakMesh, QObject* parent);
        Mesh(graphics::MeshPointer strongMesh);
        Mesh(QObject* parent = nullptr) : QObject(parent), QEnableSharedFromThis<Mesh>() {}
        Mesh(const Mesh& other, QObject* parent = nullptr) :
            QObject(parent), QEnableSharedFromThis<Mesh>() { *this = other; }
        Mesh& operator=(const Mesh& view);
        Q_INVOKABLE const graphics::MeshPointer getMeshPointer() const { return weakMesh.lock(); }
        Q_INVOKABLE const js::Graphics::ModelProviderPointer getModelProviderPointer() const { return provider.lock(); }
        Q_INVOKABLE const js::Graphics::ModelPointer getModelPointer() const { return model; }
    };

    using MeshPointer = QSharedPointer<Mesh>;
    using Meshes = std::vector<js::Graphics::MeshPointer>;

    // abstract container for holding one or more references to mesh pointers
    class Model : public QObject, public QEnableSharedFromThis<Model> {
        Q_OBJECT
    public:
        WeakModelProviderPointer provider;
        QUuid objectID; // spatially nestable ID
        Meshes meshes;
        MultiMaterialMap materialLayers;
        QVector<QString> materialNames;

        Model(QObject* parent = nullptr) : QObject(parent), QEnableSharedFromThis<Model>() {}
        Model(const Model& other) :
            QObject(other.parent()), QEnableSharedFromThis<Model>() { *this = other; }
        Model& operator=(const Model& other);
        virtual ~Model();

        void append(const js::Graphics::MeshPointer& mesh);
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
        virtual js::Graphics::ModelPointer getGraphicsModel();
        virtual scriptable::ScriptableModelBase getScriptableModel() = 0;
        virtual bool canReplaceModelMeshPart(int meshIndex, int partIndex) { return false; }
        virtual bool replaceScriptableModelMeshPart(const js::Graphics::ModelPointer& model, int meshIndex, int partIndex) { return false; }
        virtual bool overrideRenderFlags(RenderFlags flagsToSet, RenderFlags flagsToClear) { return false; }
    };

    // mixin class for resolving UUIDs into a corresponding ModelProvider
    class ModelProviderFactory : public QObject, public Dependency {
        Q_OBJECT
    public:
        virtual js::Graphics::ModelProviderPointer lookupModelProvider(const QUuid& uuid) = 0;
    signals:
        void modelAddedToScene(const QUuid& objectID, NestableType nestableType, const ::ModelPointer& sender);
        void modelRemovedFromScene(const QUuid& objectID, NestableType nestableType, const ::ModelPointer& sender);
    };

    class MeshPart : public QObject, public QEnableSharedFromThis<MeshPart> {
        Q_OBJECT
    public:
        MeshPart(MeshPointer mesh, int partIndex) :
            QObject(nullptr), QEnableSharedFromThis<MeshPart>(), parentMesh(mesh), partIndex(partIndex) {}
        MeshPointer parentMesh;
        int partIndex{ -1 };
    };
    using MeshPartPointer = QSharedPointer<MeshPart>;
}}

// TODO: in later PR remove in favor of applying corresponding API changes to source files
#include "_backwards-compat.h"
