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
#include <SpatiallyNestable.h>

#include <object-plugins/Forward.h>

#include <unordered_map>


Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(std::vector<std::string>)
Q_DECLARE_METATYPE(AABox)
Q_DECLARE_METATYPE(Extents)

Q_DECLARE_OPERATORS_FOR_FLAGS (js::Graphics::RenderFlags)

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

bool GraphicsScriptingInterface::overrideModelRenderFlags(QUuid uuid, js::Graphics::RenderFlags flagsToSet, js::Graphics::RenderFlags flagsToClear) {
    auto provider = getModelProvider(uuid);
    if (!provider) {
        return false;
    }
    return provider->overrideModelRenderFlags(flagsToSet, flagsToClear);
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
            js::Graphics::ModelPointer modelObject = provider->getScriptableModel();
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

js::Graphics::MeshPointer GraphicsScriptingInterface::newMesh(const QVariantMap& ifsMeshData) {
    // TODO: this is bare-bones way for now to improvise a new mesh from the scripting side
    //  in the future we want to support a formal C++ structure data type here instead
    std::string meshName = ifsMeshData.value("name").toString().toStdString();
    QString topologyName = ifsMeshData.value("topology").toString();

    scriptable::JSVectorAdapter adapter{ ifsMeshData, context()->argument(0) };
    // auto posit = context()->argument(0).property("position");
    // qDebug() << "POSITIONS" << posit.toString() << qscriptvalue_cast<QByteArray>(posit.property("buffer")).size();

    QVector<glm::uint32> indices = adapter.getVector<glm::uint32>("indices", "Uint32Array");
    QVector<glm::vec3> vertices = adapter.getVector<glm::vec3>("positions", "Float32Array");
    QVector<glm::vec3> normals = adapter.getVector<glm::vec3>("normals", "Float32Array");
    QVector<glm::vec3> colors = adapter.getVector<glm::vec3>("colors", "Float32Array");
    QVector<glm::vec2> texCoords0 = adapter.getVector<glm::vec2>("texCoords0", "Float32Array");

    if (engine()->hasUncaughtException()) {
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
        error = QString("expected non-empty [uint32,...] array for .indices (got type=%1)").arg(ifsMeshData.value("indices").typeName());
    } else if (numIndices % 3 != 0) {
        error = QString("expected .indices to define %1 faces (ie: .length needs to be divisible by %2) length=%3")
            .arg("triangles").arg(3).arg(numIndices);
    } else if (!numVertices) {
        error = "expected non-empty [glm::vec3(),...] array for .positions";
    } else {
        const gpu::uint32 maxVertexIndex = numVertices;
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

    if (ifsMeshData.contains("normals") && normals.size() < numVertices) {
        qCInfo(graphics_scripting) << "newMesh -- expanding .normals to #" << numVertices;
        normals.resize(numVertices);
    }
    if (ifsMeshData.contains("colors") && colors.size() < numVertices) {
        qCInfo(graphics_scripting) << "newMesh -- expanding .colors to #" << numVertices;
        colors.resize(numVertices);
    }
    if (ifsMeshData.contains("texCoords0") && texCoords0.size() < numVertices) {
        qCInfo(graphics_scripting) << "newMesh -- expanding .texCoords0 to #" << numVertices;
        texCoords0.resize(numVertices);
    }
    if (ifsMeshData.contains("texCoords1")) {
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
    QVector<graphics::Mesh::Part> parts = {{ 0, indices.size(), 0, topology }};
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
    QVector<int> metaTypeIds{
        qRegisterMetaType<uint64_t>("uint64_t"),
        qRegisterMetaType<glm::uint32>("glm::uint32"),
        qRegisterMetaType<std::vector<glm::uint32>>("std::vector<glm::uint32>"),
        qRegisterMetaType<std::vector<std::string>>("std::vector<std::string>"),
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
    qScriptRegisterSequenceMetaType<QVector<glm::uint32>>(engine);
    qScriptRegisterSequenceMetaType<std::vector<glm::uint32>>(engine);
    qScriptRegisterSequenceMetaType<std::vector<std::string>>(engine);
    qScriptRegisterSequenceMetaType<js::Graphics::Meshes>(engine);
    qScriptRegisterSequenceMetaType<QVector<js::Graphics::MaterialLayer>>(engine);

    scriptable::registerPrototype<js::Graphics::ModelPointer>(engine, new js::Graphics::ModelPrototype(nullptr));
    scriptable::registerPrototype<js::Graphics::MeshPointer>(engine, new js::Graphics::MeshPrototype(nullptr));
    scriptable::registerPrototype<js::Graphics::MeshPartPointer>(engine, new js::Graphics::MeshPartPrototype(nullptr));
    scriptable::registerVariantablePrototype<std::string>(engine);
    scriptable::registerVariantablePrototype<AABox>(engine);
    scriptable::registerVariantablePrototype<Extents>(engine);

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

bool GraphicsScriptingInterface::setRenderPlugin(QUuid uuid, QString pluginURI, QVariantMap parameters) {
    using plugins::entity::ProxyManager;
    qDebug() << "setRenderPlugin" << uuid << pluginURI << parameters;
    if (!pluginURI.isNull()) {
        if (auto proxy = ProxyManager::createObjectPlugin(uuid, pluginURI.toStdString(), parameters)){ 
            if (!proxy->postMessage) {
                auto entities = DependencyManager::get<EntityScriptingInterface>();
                const EntityItemID entityItemID{ uuid };
                // forward "web" events to EntityScriptingInterface
                proxy->postMessage = [=](const QVariant& message) {
                    emit entities->webEventReceived(entityItemID, message);
                };
            }
            return proxy->setProperties(parameters);
        } else {
            jsThrowError("error opening proxy");
        }
    } else if (!uuid.isNull()) {
        qDebug() << "setRenderPlugin -- resetObjectPlugin" << uuid;
        return ProxyManager::resetObjectPlugin(uuid);
    } else {
        jsThrowError("invalid arguments (URI or UUID invalid)");
    }
    return false;
}

std::vector<std::string> GraphicsScriptingInterface::getAvailablePlugins() const {
    return plugins::entity::ProxyManager::getPluginURIs();
}

std::vector<std::string> GraphicsScriptingInterface::getAssignedPlugins() const {
    return plugins::entity::ProxyManager::getAssignedPlugins();
}

std::string GraphicsScriptingInterface::getRenderPlugin(QUuid uuid) const {
    return plugins::entity::ProxyManager::getPluginURI(uuid);
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
        } else {
            if (hashedVertices.count(position)) {
                remapIndices[i] = hashedVertices[position];
                unique = false;
            }
        }
        if (unique) {
            remapIndices[i] = (glm::uint32)uniqueVerts.size();
            uniqueVerts.push_back(position);
        }
    }

    const auto& indices = buffer_helpers::bufferToVector<glm::uint32>(mesh->getIndexBuffer());
    glm::uint32 numIndices = (glm::uint32)indices.size();
    QVector<glm::uint32> newIndices;
    newIndices.resize(indices.size());
    for (glm::uint32 i = 0; i < numIndices; i++) {
        const auto& index = indices[i];
        if (remapIndices.find(index) != remapIndices.end()) {
            newIndices[i] = remapIndices[index];
        }
    }

    {
        // remove degenerate triangles
        for (int i = 0; i < newIndices.size() - 2;) {
            auto a = newIndices[i+0];
            auto b = newIndices[i+1];
            auto c = newIndices[i+2];
            if (a == b || b == c || a == c) {
                newIndices.remove(i, 3);
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
        auto& view = a.second;
        auto attribute = buffer_helpers::ATTRIBUTES[a.first];
        if (attribute == gpu::Stream::POSITION) {
            continue;
        }
        if (view._element.getSize() == 0) {
            continue;
        }
        if (attribute == gpu::Stream::NORMAL && resetNormals) {
            QVector<glm::vec3> normals;
            normals.resize(numUniqueVerts);
            for (const auto& p : uniqueVerts) {
                normals << glm::normalize(p);
            }
            newMesh->addAttribute(gpu::Stream::NORMAL, buffer_helpers::newFromVector(normals, gpu::Element::VEC3F_XYZ));
            continue;
        }
        if (attribute == gpu::Stream::COLOR || attribute == gpu::Stream::NORMAL) {
            const auto& input = buffer_helpers::mesh::attributeToVector<glm::vec3>(mesh, static_cast<gpu::Stream::InputSlot>(attribute));
            QVector<glm::vec3> output;
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

    QVector<graphics::Mesh::Part> parts = {{ 0, newIndices.size(), 0, graphics::Mesh::Topology::TRIANGLES }};
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
