//
//  GraphicsScriptingInterface.cpp
//  libraries/graphics-scripting/src
//
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <glm/glm.hpp>
// FIXME: what's the proper way to convince glm that c++11 stl is available?
#undef GLM_HAS_CXX11_STL
#define GLM_HAS_CXX11_STL 1
#include <glm/gtx/hash.hpp>

#include "GraphicsScriptingInterface.h"
#include "GraphicsScriptingUtil.h"
#include "OBJWriter.h"
#include "RegisteredMetaTypes.h"
#include "ScriptEngineLogging.h"
#include "ScriptableMesh.h"
#include "ScriptableMeshPart.h"
#if !defined(Q_OS_ANDROID)
#include <GeometryCache.h>
#endif
#include <GeometryUtil.h>
#include <EntityScriptingInterface.h>
#include <QHash>
#include <QMetaType>
#include <QUuid>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>
#include <QtScript/QScriptValueIterator>
#include <graphics/BufferViewHelpers.h>
#include <graphics/GpuHelpers.h>
#include <shared/QtHelpers.h>
#include <shared/Scriptable.h>
#include <SpatiallyNestable.h>

#include <Extents.h>
#include <AABox.h>
#include <object-plugins/Forward.h>

#include <unordered_map>
#include <string>
#include <vector>

Q_DECLARE_OPERATORS_FOR_FLAGS (js::Graphics::RenderFlags)
Q_DECLARE_METATYPE(AABox)
Q_DECLARE_METATYPE(Extents)

GraphicsScriptingInterface::GraphicsScriptingInterface(QObject* parent) : QObject(parent), QScriptable() {
}

void GraphicsScriptingInterface::jsThrowError(const QString& error) {
    if (context()) {
        context()->throwError(error);
    } else {
        qCWarning(graphics_scripting) << "GraphicsScriptingInterface::jsThrowError (without valid JS context):" << error;
    }
}

js::Graphics::MeshPointer GraphicsScriptingInterface::getMeshForShape(const QString& shapeString, const glm::vec3& color) {
#if defined(Q_OS_ANDROID)
    jsThrowError("getMeshForShape unavailable on Android; use Graphics.getModel(uuid) w/visible Shape Entity or Overlay uuid instead.");
    return nullptr;
#else
    auto geometryCache = DependencyManager::get<GeometryCache>();
    auto shape = geometryCache->shapeFromString(shapeString);
    if ((int)shape < 0) {
        jsThrowError(QString("invalid shapeName: '%1' (%2)").arg(shapeString).arg((int)shape));
        return nullptr;
    }
    auto mesh = geometryCache->meshFromShape(shape, color);
    return dedupeVertices(js::Graphics::MeshPointer::create(mesh));
#endif
}

bool GraphicsScriptingInterface::overrideRenderFlags(QUuid uuid, js::Graphics::RenderFlags flagsToSet, js::Graphics::RenderFlags flagsToClear) {
    auto provider = getModelProvider(uuid);
    if (!provider) {
        return false;
    }
    return provider->overrideRenderFlags(flagsToSet, flagsToClear);
}

bool GraphicsScriptingInterface::canUpdateModel(QUuid uuid, int meshIndex, int partNumber) {
    auto provider = getModelProvider(uuid);
    return provider && provider->canReplaceModelMeshPart(meshIndex, partNumber);
}

bool GraphicsScriptingInterface::updateModel(QUuid uuid, const js::Graphics::ModelPointer& model, int meshIndex, int partIndex) {
    auto provider = getModelProvider(uuid);
    if (!provider) {
        jsThrowError("provider unavailable");
        return false;
    }

    if (!provider->canReplaceModelMeshPart(meshIndex, partIndex)) {
        jsThrowError("provider does not support updating mesh parts");
        return false;
    }

    return provider->replaceScriptableModelMeshPart(model, meshIndex, partIndex);
}

js::Graphics::ModelProviderPointer GraphicsScriptingInterface::getModelProvider(QUuid uuid) {
    QString error;
    if (auto appProvider = DependencyManager::get<js::Graphics::ModelProviderFactory>()) {
        if (auto provider = appProvider->lookupModelProvider(uuid)) {
            return provider;
        } else {
            error = "provider unavailable for " + uuid.toString();
        }
    } else {
        error = "appProvider unavailable";
    }
    jsThrowError(error);
    return nullptr;
}

js::Graphics::ModelPointer GraphicsScriptingInterface::newModel(const js::Graphics::Meshes& meshes) {
    auto modelWrapper = js::Graphics::ModelPointer::create();
    modelWrapper->setObjectName("js::model");
    if (meshes.empty()) {
        jsThrowError("expected [meshes] array as first argument");
    } else {
        int i = 0;
        for (const auto& mesh : meshes) {
            if (mesh) {
                modelWrapper->append(mesh);
            } else {
                jsThrowError(QString("invalid mesh at index: %1").arg(i));
                break;
            }
            i++;
        }
    }
    return modelWrapper;
}

js::Graphics::ModelPointer GraphicsScriptingInterface::getModel(QUuid uuid) {
    QString error;
    bool success;
    QString providerType = "unknown";
    if (auto nestable = DependencyManager::get<SpatialParentFinder>()->find(uuid, success).lock()) {
        providerType = SpatiallyNestable::nestableTypeToString(nestable->getNestableType());
        if (auto provider = getModelProvider(uuid)) {
            js::Graphics::ModelPointer modelObject = provider->getGraphicsModel();
            js::Graphics::Model* sanityCheck = modelObject.data();
            if (sanityCheck && modelObject) {
                if (uuid == AVATAR_SELF_ID) {
                    // special case override so that scripts can rely on matching input/output UUIDs
                    modelObject->objectID = AVATAR_SELF_ID;
                }
                if (modelObject->objectID == uuid) {
                    if (modelObject->meshes.size()) {
                        //auto modelWrapper = js::Graphics::ModelPointer::create(modelObject);
                        modelObject->setObjectName(providerType+"::"+uuid.toString()+"::model");
                        return modelObject;
                    } else {
                        error = "no meshes available: " + modelObject->objectID.toString();
                    }
                } else {
                    error = QString("objectID mismatch: %1 (result contained %2 meshes)").arg(modelObject->objectID.toString()).arg(modelObject->meshes.size());
                }
            } else {
                error = "failed to get model object";
            }
        } else {
            error = "model provider unavailable";
        }
    } else {
        error = "model object not found";
    }
    jsThrowError(QString("failed to get meshes from %1 provider for uuid %2 (%3)").arg(providerType).arg(uuid.toString()).arg(error));
    return nullptr;
}

js::Graphics::MeshPointer GraphicsScriptingInterface::newMesh(QScriptValue ifsMeshData) {
    return newMesh(scriptable::JSVectorAdapter{ ifsMeshData.toVariant().toMap(), ifsMeshData });
}
js::Graphics::MeshPointer GraphicsScriptingInterface::newMeshFromVariant(QVariantMap ifsMeshData) {
    return newMesh(scriptable::JSVectorAdapter{ ifsMeshData, argument(0) });
}

js::Graphics::MeshPointer GraphicsScriptingInterface::newMesh(scriptable::JSVectorAdapter adapter) {
    // TODO: this is bare-bones way for now to improvise a new mesh from the scripting side
    //  in the future we want to support a formal C++ structure data type here instead

    /**jsdoc
     * @typedef {object} Graphics.IFSData
     * @property {string} [name=""] - mesh name (useful for debugging / debug prints).
     * @property {string} [topology=""]
     * @property {number[]} indices - vertex indices to use for the mesh faces.
     * @property {Vec3[]} vertices - vertex positions (model space)
     * @property {Vec3[]} [normals=[]] - vertex normals (normalized)
     * @property {Vec3[]} [colors=[]] - vertex colors (normalized)
     * @property {Vec2[]} [texCoords0=[]] - vertex texture coordinates (normalized)
     */

    std::string meshName = adapter.qt.value("name").toString().toStdString();
    QString topologyName = adapter.qt.value("topology").toString();

    std::vector<glm::uint32> indices = adapter.getVector<glm::uint32>("indices", "Uint32Array");
    std::vector<glm::vec3> vertices = adapter.getVector<glm::vec3>("positions", "Float32Array");
    std::vector<glm::vec3> normals = adapter.getVector<glm::vec3>("normals", "Float32Array");
    std::vector<glm::vec3> colors = adapter.getVector<glm::vec3>("colors", "Float32Array");
    std::vector<glm::vec2> texCoords0 = adapter.getVector<glm::vec2>("texCoords0", "Float32Array");

    if (engine()->hasUncaughtException()) {
        qDebug() << "hasUncaughtException" << engine()->uncaughtException().toString();
        return nullptr;
    }

    const auto numVertices = vertices.size();
    const auto numIndices = indices.size();
    const auto topology = graphics::TOPOLOGIES.key(topologyName, graphics::Mesh::Topology::TRIANGLES);

    // TODO: support additional topologies (POINTS and LINES ought to "just work" --
    //   if MeshPartPayload::drawCall is updated to actually check the Mesh::Part::_topology value
    //   (TRIANGLE_STRIP, TRIANGLE_FAN, QUADS, QUAD_STRIP may need additional conversion code though)
    static const QStringList acceptableTopologies{ "triangles" };

    // sanity checks
    QString error;
    if (!topologyName.isEmpty() && !acceptableTopologies.contains(topologyName)) {
        error = QString("expected .topology to be %1").arg(acceptableTopologies.join(" | "));
    } else if (!numIndices) {
        error = QString("expected non-empty [uint32,...] array for .indices (got type=%1)").arg(adapter.qt.value("indices").typeName());
    } else if (numIndices % 3 != 0) {
        error = QString("expected .indices to define %1 faces (ie: .length needs to be divisible by %2) length=%3")
            .arg("triangles").arg(3).arg(numIndices);
    } else if (!numVertices) {
        error = "expected non-empty [glm::vec3(),...] array for .positions";
    } else {
        const glm::uint32 maxVertexIndex = (glm::uint32)numVertices;
        int i = 0;
        for (const auto& ind : indices) {
            if (ind >= maxVertexIndex) {
                error = QString("index out of .indices[%1] index=%2 >= maxVertexIndex=%3").arg(i).arg(ind).arg(maxVertexIndex);
                break;
            }
            i++;
        }
    }
    if (!error.isEmpty()) {
        jsThrowError(error);
        return nullptr;
    }

    if (adapter.qt.contains("normals") && normals.size() < numVertices) {
        qCInfo(graphics_scripting) << "newMesh -- expanding .normals to #" << numVertices;
        normals.resize(numVertices);
    }
    if (adapter.qt.contains("colors") && colors.size() < numVertices) {
        qCInfo(graphics_scripting) << "newMesh -- expanding .colors to #" << numVertices;
        colors.resize(numVertices);
    }
    if (adapter.qt.contains("texCoords0") && texCoords0.size() < numVertices) {
        qCInfo(graphics_scripting) << "newMesh -- expanding .texCoords0 to #" << numVertices;
        texCoords0.resize(numVertices);
    }
    if (adapter.qt.contains("texCoords1")) {
        qCWarning(graphics_scripting) << "newMesh - texCoords1 not yet supported; ignoring";
    }

    graphics::MeshPointer mesh(new graphics::Mesh());
    mesh->modelName = "graphics::newMesh";
    mesh->displayName = meshName;

    // TODO: newFromVector does inbound type conversion, but not compression or packing
    //  (later we should autodetect if fitting into gpu::INDEX_UINT16 and reduce / pack normals etc.)
    mesh->setIndexBuffer(buffer_helpers::newFromVector(indices, gpu::Format::INDEX_INT32));
    mesh->setVertexBuffer(buffer_helpers::newFromVector(vertices, gpu::Format::VEC3F_XYZ));
    if (normals.size()) {
        mesh->addAttribute(gpu::Stream::NORMAL, buffer_helpers::newFromVector(normals, gpu::Format::VEC3F_XYZ));
    }
    if (colors.size()) {
        mesh->addAttribute(gpu::Stream::COLOR, buffer_helpers::newFromVector(colors, gpu::Format::VEC3F_XYZ));
    }
    if (texCoords0.size()) {
        mesh->addAttribute(gpu::Stream::TEXCOORD0, buffer_helpers::newFromVector(texCoords0, gpu::Format::VEC2F_UV));
    }
    std::vector<graphics::Mesh::Part> parts{{ 0, (int)indices.size(), 0, topology }};
    mesh->setPartBuffer(buffer_helpers::newFromVector(parts, gpu::Element::PART_DRAWCALL));
    return js::Graphics::MeshPointer::create(mesh);
}

QString GraphicsScriptingInterface::exportModelToOBJ(const js::Graphics::ModelPointer& base, bool dedupeVertices) {
    if (!base) {
        jsThrowError("null model");
        return QString();
    }

    QList<graphics::MeshPointer> meshes;
    int i = -1;
    foreach (auto meshProxy, base->meshes) {
        i++;
        if (!meshProxy || !meshProxy->getMeshPointer()) {
            qCWarning(graphics_scripting) << "exportModelToOBJ -- skipping null mesh at index" << i;
            continue;
        }
        auto meshPtr = dedupeVertices ?
            GraphicsScriptingInterface::dedupeVertices(meshProxy)->getMeshPointer() :
            meshProxy->getMeshPointer();
        if (!meshPtr) {
            qCWarning(graphics_scripting) << "exportModelToOBJ -- skipping null mesh pointer at index" << i;
            continue;
        }
        meshes << meshPtr;
    }
    if (meshes.size()) {
        return writeOBJToString(meshes);
    }
    jsThrowError("no meshes");
    return QString();
}

// js::Graphics::MeshPointer => graphics::MeshPointer
graphics::MeshPointer GraphicsScriptingInterface::getNativeMesh(js::Graphics::MeshPointer meshProxy) {
    graphics::MeshPointer result;
    if (!meshProxy) {
        jsThrowError("expected meshProxy as first parameter");
        return result;
    }
    auto mesh = meshProxy->getMeshPointer();
    if (!mesh) {
        jsThrowError("expected valid meshProxy as first parameter");
        return result;
    }
    return mesh;
}

namespace {
    std::vector<int> metaTypeIds{
        qRegisterMetaType<uint64_t>("uint64_t"),
        qRegisterMetaType<glm::uint32>("glm::uint32"),
        qRegisterMetaType<std::vector<glm::uint32>>("std::vector<glm::uint32>"),
        qRegisterMetaType<std::string>("std::string"),
        qRegisterMetaType<std::vector<std::string>>("std::vector<std::string>"),
        qRegisterMetaType<std::vector<QUuid>>("std::vector<QUuid>"),
        qRegisterMetaType<js::Graphics::Meshes>("js::Graphics::Meshes"),
        qRegisterMetaType<std::vector<js::Graphics::MeshPointer>>("std::vector<js::Graphics::MeshPointer>"),
        qRegisterMetaType<std::vector<js::Graphics::MeshPartPointer>>("std::vector<js::Graphics::MeshPartPointer>"),
        qRegisterMetaType<js::Graphics::MeshPointer>("js::Graphics::MeshPointer"),
        qRegisterMetaType<js::Graphics::ModelPointer>("js::Graphics::ModelPointer"),
        qRegisterMetaType<js::Graphics::MeshPartPointer>("js::Graphics::MeshPartPointer"),
        qRegisterMetaType<js::Graphics::Material>("js::Graphics::Material"),
        qRegisterMetaType<js::Graphics::MaterialLayer>("js::Graphics::MaterialLayer"),
        qRegisterMetaType<QVector<js::Graphics::MaterialLayer>>("QVector<js::Graphics::MaterialLayter>"),
        qRegisterMetaType<js::Graphics::MultiMaterialMap>("js::Graphics::MultiMaterialMap"),
        qRegisterMetaType<graphics::Mesh::Topology>(),
    };
}

namespace scriptable {
    QScriptValue qVectorScriptableMaterialLayerToScriptValue(QScriptEngine* engine, const QVector<js::Graphics::MaterialLayer>& vector) {
        return qScriptValueFromSequence(engine, vector);
    }

    void qVectorScriptableMaterialLayerFromScriptValue(const QScriptValue& array, QVector<js::Graphics::MaterialLayer>& result) {
        qScriptValueToSequence(array, result);
    }

    QScriptValue scriptableMaterialToScriptValue(QScriptEngine* engine, const js::Graphics::Material &material) {
        QScriptValue obj = engine->newObject();
        obj.setProperty("name", material.name);
        obj.setProperty("model", material.model);
        obj.setProperty("opacity", material.opacity);
        obj.setProperty("roughness", material.roughness);
        obj.setProperty("metallic", material.metallic);
        obj.setProperty("scattering", material.scattering);
        obj.setProperty("unlit", material.unlit);
        obj.setProperty("emissive", vec3toScriptValue(engine, material.emissive));
        obj.setProperty("albedo", vec3toScriptValue(engine, material.albedo));
        obj.setProperty("emissiveMap", material.emissiveMap);
        obj.setProperty("albedoMap", material.albedoMap);
        obj.setProperty("opacityMap", material.opacityMap);
        obj.setProperty("metallicMap", material.metallicMap);
        obj.setProperty("specularMap", material.specularMap);
        obj.setProperty("roughnessMap", material.roughnessMap);
        obj.setProperty("glossMap", material.glossMap);
        obj.setProperty("normalMap", material.normalMap);
        obj.setProperty("bumpMap", material.bumpMap);
        obj.setProperty("occlusionMap", material.occlusionMap);
        obj.setProperty("lightmapMap", material.lightmapMap);
        obj.setProperty("scatteringMap", material.scatteringMap);
        return obj;
    }

    void scriptableMaterialFromScriptValue(const QScriptValue &object, js::Graphics::Material& material) {
        // No need to convert from QScriptValue to ScriptableMaterial
    }

    QScriptValue scriptableMaterialLayerToScriptValue(QScriptEngine* engine, const js::Graphics::MaterialLayer &materialLayer) {
        QScriptValue obj = engine->newObject();
        obj.setProperty("material", scriptableMaterialToScriptValue(engine, materialLayer.material));
        obj.setProperty("priority", materialLayer.priority);
        return obj;
    }

    void scriptableMaterialLayerFromScriptValue(const QScriptValue &object, js::Graphics::MaterialLayer& materialLayer) {
        // No need to convert from QScriptValue to ScriptableMaterialLayer
    }

    QScriptValue multiMaterialMapToScriptValue(QScriptEngine* engine, const js::Graphics::MultiMaterialMap& map) {
        QScriptValue obj = engine->newObject();
        for (auto key : map.keys()) {
            obj.setProperty(key, qVectorScriptableMaterialLayerToScriptValue(engine, map[key]));
        }
        return obj;
    }

    void multiMaterialMapFromScriptValue(const QScriptValue& map, js::Graphics::MultiMaterialMap& result) {
        // No need to convert from QScriptValue to MultiMaterialMap
    }

}

QScriptValue GraphicsScriptingInterface::getRenderFlagEnum() const {
    auto meta = engine()->newQMetaObject(&js::Graphics::staticMetaObject);
    // by default Qt global enums are also available; resetting the prototype rescopes so only js::Graphics values
    meta.setPrototype(engine()->newObject());
    return meta;
}

void GraphicsScriptingInterface::registerMetaTypes(QScriptEngine* engine) {
    qScriptRegisterSequenceMetaType<std::vector<glm::uint32>>(engine);
    qScriptRegisterSequenceMetaType<std::vector<std::string>>(engine);
    qScriptRegisterSequenceMetaType<std::vector<QUuid>>(engine);
    qScriptRegisterSequenceMetaType<js::Graphics::Meshes>(engine);
    qScriptRegisterSequenceMetaType<QVector<js::Graphics::MaterialLayer>>(engine);

    scriptable::registerPrototype<js::Graphics::ModelPointer>(engine, new js::Graphics::ModelPrototype(nullptr));
    scriptable::registerPrototype<js::Graphics::MeshPointer>(engine, new js::Graphics::MeshPrototype(nullptr));
    scriptable::registerPrototype<js::Graphics::MeshPartPointer>(engine, new js::Graphics::MeshPartPrototype(nullptr));
    scriptable::registerVariantType<std::string>(engine);
    scriptable::registerReadOnlyVariantType<AABox>(engine);
    scriptable::registerReadOnlyVariantType<Extents>(engine);

    scriptable::registerQFlagsEnum<js::Graphics::RenderFlags>(engine);

    scriptable::registerDebugEnum<graphics::Mesh::Topology>(engine, graphics::TOPOLOGIES);
    scriptable::registerDebugEnum<gpu::Type>(engine, gpu::TYPES);
    scriptable::registerDebugEnum<gpu::Semantic>(engine, gpu::SEMANTICS);
    scriptable::registerDebugEnum<gpu::Dimension>(engine, gpu::DIMENSIONS);

    qScriptRegisterMetaType(engine, scriptable::scriptableMaterialToScriptValue, scriptable::scriptableMaterialFromScriptValue);
    qScriptRegisterMetaType(engine, scriptable::scriptableMaterialLayerToScriptValue, scriptable::scriptableMaterialLayerFromScriptValue);
    qScriptRegisterMetaType(engine, scriptable::qVectorScriptableMaterialLayerToScriptValue, scriptable::qVectorScriptableMaterialLayerFromScriptValue);
    qScriptRegisterMetaType(engine, scriptable::multiMaterialMapToScriptValue, scriptable::multiMaterialMapFromScriptValue);

    Q_UNUSED(metaTypeIds);
}

bool GraphicsScriptingInterface::setRenderPlugin(QUuid uuid, std::string pluginURI, QVariantMap parameters) {
    using plugins::object::ProxyManager;
    qDebug() << "setRenderPlugin" << uuid << pluginURI.c_str() << parameters;
    EntityItemPointer entity;
    bool success = false;
    if (auto nestable = DependencyManager::get<SpatialParentFinder>()->find(uuid, success).lock()) {
        entity = std::dynamic_pointer_cast<EntityItem>(nestable);
    }
    if (!pluginURI.empty()) {
        if (auto proxy = plugins::object::createObjectProxy(uuid, pluginURI, parameters)) {
            if (entity) {
                qDebug() << "assigning object proxy to available EntityItem" << entity.get();
                entity->setEntityProxy(proxy);
                success = true;
            } else {
                // pre-stage message handler (on the assumption entity is about to become available)
                if (!proxy->postMessage) {
                    auto entities = DependencyManager::get<EntityScriptingInterface>();
                    const EntityItemID entityItemID{ uuid };
                    // forward "web" events to EntityScriptingInterface
                    proxy->postMessage = [=](const QVariant& message) {
                        emit entities->webEventReceived(entityItemID, message);
                    };
                }
            }
        } else {
            jsThrowError(QString("error creating proxy %1 for %2").arg(pluginURI.c_str()).arg(uuid.toString()));
            success = false;
        }
    } else if (!uuid.isNull()) {
        qDebug() << "setRenderPlugin -- removeObjectProxy" << uuid;
        if (entity) {
            qDebug() << "resetting proxy for EntityItem" << entity.get();
            entity->setEntityProxy(nullptr);
        }
        success = plugins::object::removeObjectProxy(uuid);
    } else {
        jsThrowError("invalid arguments (URI or UUID invalid)");
        success = false;
    }
    return success;
}

std::vector<std::string> GraphicsScriptingInterface::getAvailableRenderPlugins() const {
    return plugins::object::getAvailablePluginURIs();
}

std::vector<QUuid> GraphicsScriptingInterface::getRenderPluginObjectIDs(std::string uri) const {
    return plugins::object::getAssignedObjectIDs(uri);
}

std::string GraphicsScriptingInterface::getRenderPlugin(QUuid uuid) const {
    return plugins::object::getObjectPluginURI(uuid);
}

js::Graphics::MeshPointer GraphicsScriptingInterface::dedupeVertices(const js::Graphics::MeshPointer& input, float epsilon, bool resetNormals) {
    const auto mesh = input->getMeshPointer();
    if (!mesh) {
        qCWarning(graphics_scripting) << "dedupeVertices passed null mesh" << input;
        return nullptr;
    }
    graphics::MeshPointer newMesh(new graphics::Mesh());
    newMesh->modelName = mesh->modelName + " (deduped)";
    newMesh->displayName = mesh->displayName;

    const auto& positions = buffer_helpers::bufferToVector<glm::vec3>(mesh->getVertexBuffer());
    glm::uint32 numPositions = positions.size();
    const auto epsilon2 = epsilon*epsilon;
    bool slowDedupe = epsilon != DEDUPE_EPSILON;
    std::unordered_map<glm::vec3, glm::uint32> hashedVertices;
    std::vector<glm::vec3> uniqueVerts;
    std::unordered_map<glm::uint32,glm::uint32> remapIndices;

    for (glm::uint32 i = 0; i < numPositions; i++) {
        if (i % 16384 == 0) {
            qDebug() << "long running dedupe... i=" << i << " / " << numPositions;
        }
        const auto& position = positions[i];
        bool unique = true;
        if (slowDedupe) {
            const glm::uint32 numUnique = (glm::uint32)uniqueVerts.size();
            for (glm::uint32 j = 0; j < numUnique; j++) {
                if (glm::length2(uniqueVerts[j] - position) <= epsilon2) {
                    remapIndices[i] = j;
                    unique = false;
                    break;
                }
            }
        } else if (hashedVertices.count(position)) {
                remapIndices[i] = hashedVertices[position];
                unique = false;
        }
        if (unique) {
            remapIndices[i] = (glm::uint32)uniqueVerts.size();
            uniqueVerts.push_back(position);
        }
    }

    const auto& indices = buffer_helpers::bufferToVector<glm::uint32>(mesh->getIndexBuffer());
    glm::uint32 numIndices = (glm::uint32)indices.size();
    std::vector<glm::uint32> newIndices;
    newIndices.resize(indices.size());
    for (glm::uint32 i = 0; i < numIndices; i++) {
        const auto& index = indices[i];
        auto it = remapIndices.find(index);
        if (it != remapIndices.end()) {
            newIndices[i] = it->second;
        }
    }

    {
        // remove degenerate triangles
        for (int i = 0; i < (int)newIndices.size() - 2;) {
            auto a = newIndices[i+0];
            auto b = newIndices[i+1];
            auto c = newIndices[i+2];
            if (a == b || b == c || a == c) {
                auto it = newIndices.begin() + i;
                newIndices.erase(it, std::next(it, 3));
                // preserve i for next pass
            } else {
                i += 3;
            }
        }
    }

    newMesh->setIndexBuffer(buffer_helpers::newFromVector(newIndices, { gpu::SCALAR, gpu::UINT32, gpu::INDEX }));
    newMesh->setVertexBuffer(buffer_helpers::newFromVector(uniqueVerts, gpu::Element::VEC3F_XYZ));

    auto attributeViews = buffer_helpers::mesh::getAllBufferViews(mesh);
    glm::uint32 numUniqueVerts = (glm::uint32)uniqueVerts.size();
    for (const auto& a : attributeViews) {
        auto attribute = buffer_helpers::ATTRIBUTES[a.first];
        if (attribute == gpu::Stream::POSITION) {
            continue;
        }
        auto& view = a.second;
        if (view._element.getSize() == 0) {
            continue;
        }
        if (attribute == gpu::Stream::NORMAL && resetNormals) {
            std::vector<glm::vec3> normals;
            normals.reserve(numUniqueVerts);
            for (const auto& p : uniqueVerts) {
                normals.emplace_back(glm::normalize(p));
            }
            newMesh->addAttribute(gpu::Stream::NORMAL, buffer_helpers::newFromVector(normals, gpu::Element::VEC3F_XYZ));
            continue;
        }
        if (attribute == gpu::Stream::COLOR || attribute == gpu::Stream::NORMAL) {
            const auto& input = buffer_helpers::mesh::attributeToVector<glm::vec3>(mesh, static_cast<gpu::Stream::InputSlot>(attribute));
            std::vector<glm::vec3> output;
            output.resize(numUniqueVerts);
            QMap<glm::uint32,bool> seen;
            glm::uint32 numElements = (glm::uint32)input.size();
            for (glm::uint32 i = 0; i < numElements; i++) {
                glm::uint32 fromVertexIndex = i;
                glm::uint32 toVertexIndex = remapIndices.find(fromVertexIndex) != remapIndices.end() ?
                    remapIndices[fromVertexIndex] : fromVertexIndex;
                auto a = input[fromVertexIndex];
                auto b = seen.contains(toVertexIndex) ? output[toVertexIndex] : a;
                auto c = glm::mix(a, b, 0.5f);
                if (attribute == gpu::Stream::NORMAL) {
                    c = glm::normalize(c);
                }
                output[toVertexIndex] = c;
                seen[toVertexIndex] = true;
            }
            newMesh->addAttribute(attribute, buffer_helpers::newFromVector(output, gpu::Element::VEC3F_XYZ));
            continue;
        }
        auto buffer = new gpu::Buffer();
        buffer->resize(view._element.getSize() * numUniqueVerts);
        auto newView = gpu::BufferView(buffer, view._element);

        glm::uint32 numElements = (glm::uint32)view.getNumElements();
        for (glm::uint32 i = 0; i < numElements; i++) {
            glm::uint32 fromVertexIndex = i;
            glm::uint32 toVertexIndex = remapIndices.find(fromVertexIndex) != remapIndices.end() ?
                remapIndices[fromVertexIndex] : fromVertexIndex;
            buffer_helpers::setValue<QVariant>(newView, toVertexIndex, buffer_helpers::getValue<QVariant>(view, fromVertexIndex, "dedupe"));
        }
        newMesh->addAttribute(attribute, newView);
    }

    std::vector<graphics::Mesh::Part> parts{{ 0, (int)newIndices.size(), 0, graphics::Mesh::Topology::TRIANGLES }};
    newMesh->setPartBuffer(buffer_helpers::newFromVector(parts, gpu::Element::PART_DRAWCALL));
    return js::Graphics::MeshPointer::create(newMesh);
}

js::Graphics::MeshPointer GraphicsScriptingInterface::cloneMesh(const js::Graphics::MeshPointer& input) {
    auto mesh = input->getMeshPointer();
    if (!mesh) {
        qCWarning(graphics_scripting) << "ScriptableMesh::cloneMesh -- !meshPointer";
        return nullptr;
    }
    return js::Graphics::MeshPointer::create(buffer_helpers::mesh::clone(mesh));
}

// TODO: remove with _backwards-compat.h
js::Graphics::ModelPointer js::Graphics::ModelProvider::getGraphicsModel() { return (js::Graphics::ModelPointer)getScriptableModel(); }
