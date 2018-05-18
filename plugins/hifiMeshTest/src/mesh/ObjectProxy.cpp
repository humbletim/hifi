#include "ObjectProxy.h"
#include "ObjectPlugin.h"
#include <graphics/BufferViewHelpers.h>
#include <glm/gtc/epsilon.hpp>
#include <GeometryCache.h>

namespace {
    QVariantMap onlyChanged(const QVariantMap& properties, const QVariantMap& previousValues) {
        auto a = properties;
        auto b = previousValues;
        auto c = QVariantMap();
        for (const auto& k : a.toStdMap()) {
            if (!b.contains(k.first) || b[k.first] != a[k.first]) {
                //qDebug() << "proxy->setProperty" << k.first << k.second;
                c[k.first] = k.second;
            }
        }
        return c;
    }
}

bool MeshEntityProxy::setProperties(const QVariantMap& map) {
    QWriteLocker locker(&lock);
    for(const auto& k : onlyChanged(map, properties).toStdMap()) {
        const auto& value = k.second;
        const auto& key = k.first;
        if (key != "renderTransform") qDebug() << "MeshEntityProxy::setProperty" << k.first << k.second;
        properties[key] = value;
        if (key == "visible" && !value.toBool()) {
            if (_renderItem) {
                qDebug() << "MeshEntityProxy::visible false -- clearing renderItem";
                _renderItem.reset();
            }
        }
        if (key != "renderTransform") {
            flags.setFlag(js::Graphics::RenderFlag::DIRTY);
        }
    }
    return true;
}

bool MeshEntityProxy::findRayIntersection(const MeshRay& entityRay, IntersectionResultRef& result)  {
    const QVariantMap& details = entityRay.metadata;
    //qDebug() << objectID << "MeshEntityProxy.findRayIntersection" << QThread::currentThread();

    QReadLocker locker(&lock);
    auto modelFrameBox = _triangles.getBounds();
    if (modelFrameBox.findRayIntersection(entityRay.origin, entityRay.direction,
                                          result.distance, result.face, result.surfaceNormal)) {
        result.extraInfo["MeshEntityProxy"] = objectID;
        bool precisionPicking = details["precisionPicking"].toBool();
        bool allowBackface = details["allowBackface"].toBool();
        if (!precisionPicking) {
            return true;
        }

        Triangle triangle;
        if (_triangles.findRayIntersection(
                entityRay.origin, entityRay.direction,
                result.distance, result.face, triangle,
                precisionPicking, allowBackface)) {
            result.surfaceNormal = triangle.getNormal();
            result.extraInfo["subMeshIndex"] = 0;
            result.extraInfo["partIndex"] = 0;
            result.extraInfo["subMeshName"] = "MeshEntityProxy";
            result.extraInfo["triangleIndex"] = _triangles.indexOf(triangle);
            return true;
        }        
    }
    return false;
}

MeshEntityProxy::~MeshEntityProxy() {
    qDebug() << "~MeshEntityProxy";
    unload();
}
void MeshEntityProxy::preload() {
    qDebug() << "MeshEntityProxy::preload" << objectID;
}
void MeshEntityProxy::unload() {
    QWriteLocker locker(&lock);
    if (!objectID.isNull()) {
        qDebug() << "MeshEntityProxy::unload" << objectID;
        if (_plugin) {
            _plugin->meshes.erase(objectID);
        }
        _plugin.reset();
        _renderItem.reset();
        _mesh.reset();
        _triangles.clear();
    }
    objectID = QUuid();
}

void MeshEntityProxy::recalculateTriangleSet() {
    QWriteLocker locker(&lock);
    _triangles.clear();
    auto mesh = _mesh;
    if (!mesh) {
        return;
    }
    const gpu::BufferView& partBuffer = mesh->getPartBuffer();
    const int partCount = (int)mesh->getNumParts();
    auto positions = buffer_helpers::mesh::attributeToVector<glm::vec3>(mesh, gpu::Stream::POSITION);
    for (int j = 0; j < partCount; j++) {
        for (int partIndex = 0; partIndex < partCount; partIndex++) {
            const graphics::Mesh::Part& part = partBuffer.get<graphics::Mesh::Part>(partIndex);
            //qDebug() << "MeshEntityProxy::recalculateTriangleSet" << partIndex;
            auto indices = buffer_helpers::bufferToVector<glm::uint32>(mesh->getIndexBuffer(), "mesh.indices");
            auto face = [&](glm::uint32 i0, glm::uint32 i1, glm::uint32 i2) {
                Triangle tri{ positions[indices[i0]], positions[indices[i1]], positions[indices[i2]] };
                _triangles.insert(tri);
            };
            glm::uint32 len = part._startIndex + part._numIndices;
            if (part._topology == graphics::Mesh::QUADS) {
                for (glm::uint32 idx = part._startIndex; idx+3 < len; idx += 4) {
                    face(idx+0, idx+1, idx+3);
                    face(idx+1, idx+2, idx+3);
                }
            } else if (part._topology == graphics::Mesh::TRIANGLES) {
                for (glm::uint32 idx = part._startIndex; idx+2 < len; idx += 3) {
                    face(idx+0, idx+1, idx+2);
                }
            } else {
                qDebug() << "MeshEntityProxy::recalculateTriangleSet -- unhandled part topology" << partIndex << part._topology;
            }
        }
    }
    //qDebug() << "//MeshEntityProxy::recalculateTriangleSet #tris" << _triangles.size();
}

MeshEntityProxy::MeshRenderItem::MeshRenderItem(const render::ScenePointer& scene, graphics::MeshPointer mesh, int partIndex, graphics::MaterialPointer material) :
    mesh(mesh), partIndex(partIndex), material(material), scene(scene) {
    //auto scene = AbstractViewStateInterface::instance()->getMain3DScene();
    if (!mesh || !mesh->getNumParts()) {
        qDebug() << "MeshRenderItem -- no parts :(" << mesh.get();
        return;
    }
    render::Transaction transaction;
    itemID = scene->allocateID();

    Transform identity;
    identity.setIdentity();
    Transform offset;
    const graphics::Mesh::Part& part = mesh->getPartBuffer().get<graphics::Mesh::Part>(partIndex);
    qDebug() << "part" << part._startIndex << part._numIndices;
    payload = std::make_shared<MeshPartPayload>(mesh, partIndex, material);
    qDebug() << "MeshPartPayload" << payload.get();
    if (!payload) {
        qDebug() << "!payload";
        return;
    }
    payload->updateTransform(identity, offset);
    renderPayload = std::make_shared<MeshPartPayload::Payload>(payload);
    transaction.resetItem(itemID, renderPayload);
    scene->enqueueTransaction(transaction);
    qDebug() << "//MeshEntityProxy::MeshRenderItem::MeshRenderItem" << itemID;
}

MeshEntityProxy::MeshRenderItem::~MeshRenderItem() {
    qDebug() << "MeshEntityProxy::MeshRenderItem::~MeshRenderItem" << itemID;
    if (scene) {
        render::Transaction transaction;
        transaction.removeItem(itemID);
        scene->enqueueTransaction(transaction);
    }
    render::Item::clearID(itemID);
    material.reset();
    mesh.reset();
    scene.reset();
}

bool MeshEntityProxy::recomputeAABox(AABox& box) const {
    box.embiggen(100.0f);
    return true;
}
bool MeshEntityProxy::recomputeDimensions(glm::vec3& box) const {
    box *= 100.0f;
    return true;
}

void MeshEntityProxy::messageReceived(const QVariant& message) {
    qDebug() << "MeshEntityProxy::messageReceived" << message;
    auto map = message.toMap();
    if (map["method"].toString() == "set") {
        auto props = map["arguments"].toList().value(0).toMap();
        //qDebug() << "MeshEntityProxy::messageReceived::set" << props;
        setProperties(props);
    } else if (map["method"].toString() == "recalculate") {
        recalculateTriangleSet();
        flags.setFlag(js::Graphics::RenderFlag::DIRTY);
    }
}

namespace {
    graphics::MaterialPointer getDefaultMaterial() {
        qDebug() << "using placeholder magenta material";
        auto material = std::make_shared<graphics::Material>();
        material->setModel("--entity--");
        material->setMetallic(0.02f);
        material->setRoughness(0.5f);
        return material;
    }
}

bool MeshEntityProxy::update(const render::ScenePointer& scene, render::Transaction& transaction, float deltaTime) {
    MeshRenderItem::Pointer renderItem;
    std::shared_ptr<ShapeEntityItem> entity;
    graphics::MaterialPointer material;
    graphics::MeshPointer mesh;
    {
        QReadLocker locker(&lock);
        renderItem = getRenderItem();
        material = getEntityMaterial();
        mesh = getMesh();
    }
    if (!scene) {
        qDebug() << "!scene";
        return false;
    }
    QVariantMap properties = getProperties();

    if (!renderItem) {
        if (!material) {
            material = getDefaultMaterial();
        }
        qDebug() << "creating renderItem" << QThread::currentThread() << mesh.get();
        qDebug() << mesh->getNumParts() << mesh->getNumIndices() << mesh->getNumVertices();
        renderItem = std::make_shared<MeshRenderItem>(scene, mesh, 0, material);
        setRenderItem(renderItem);
    }
    if (!material) {
        material = renderItem->material;
    }

    if (material->getModel() == "--entity--") {
        glm::vec3 color = vec3FromVariant(properties["color"]);
        glm::vec3 albedo = material->getAlbedo();
        if (glm::all(glm::epsilonEqual(color, albedo, glm::epsilon<float>()))) {
            qDebug() << "updating color" << color;
            material->setAlbedo(color);
        }
    }

    if (renderItem->itemID == render::Item::INVALID_ITEM_ID) {
        qDebug() << "renderItem->itemID == Item::INVALID_ITEM_ID";
    }
    {
        QVector<float> renderTransform = properties["renderTransform"].value<QVector<float>>();
        Transform modelTransform{ glm::make_mat4(renderTransform.data()) };

        bool isWireframe = flags.testFlag(js::Graphics::RenderFlag::WIREFRAME);
        bool isVisible = properties["visible"].toBool();
        bool canCastShadow = properties["canCastShadow"].toBool();
        uint8_t viewTagBits = render::ItemKey::TAG_BITS_0 | render::ItemKey::TAG_BITS_1;
        bool isLayeredInFront = properties["isLayeredInFront"].toBool();
        bool isLayeredInHUD = properties["isLayeredInHUD"].toBool();
        bool isGroupCulled = properties["isGroupCulled"].toBool();

        auto itemID = renderItem->itemID;
        Transform collisionMeshOffset;
        collisionMeshOffset.setIdentity();
        renderItem->payload->updateTransform(modelTransform, collisionMeshOffset);
        graphics::MeshPointer newMesh;
        if (renderItem->mesh != mesh) {
            newMesh = renderItem->mesh = mesh;
        }
        auto partIndex = renderItem->partIndex;
        transaction.updateItem<MeshPartPayload>(itemID, [=](MeshPartPayload& data) {
            if (data._isWireframe != isWireframe) {
                qDebug() << "data.setIsWireframe" << isWireframe << "was: " << data._isWireframe;
                postMessage(QVariantMap{
                    { "type", "changed" },
                    { "property", "wireframe" },
                    { "value", isWireframe },
                });
                data.setIsWireframe(isWireframe);
            }
            data.updateKey(isVisible, isLayeredInFront || isLayeredInHUD, canCastShadow, viewTagBits, isGroupCulled);
            if (newMesh) {
                data.updateMeshPart(newMesh, partIndex);
            }
            data.updateTransform(modelTransform, collisionMeshOffset);
            data._drawMaterials = graphics::MultiMaterial();
            if (renderItem->material) {
                data.addMaterial(graphics::MaterialLayer(renderItem->material, 0));
            }
        });
    }
    {
        QWriteLocker locker(&lock);
        bool wasDirty = flags.testFlag(js::Graphics::RenderFlag::DIRTY);
        if (wasDirty) {
            if (0) qDebug() << "WAS DIRTY!!! RESETTING DIRTY FLAG";
            flags.setFlag(js::Graphics::RenderFlag::DIRTY, false);
        }
        return wasDirty;
    }
}

bool MeshEntityProxy::replaceScriptableModelMeshPart(const js::Graphics::ModelPointer& newModel, int meshIndex, int partIndex) {
    if (newModel && newModel->meshes.size()) {
        meshIndex = std::min<int>(std::max<int>(0, meshIndex), (int)newModel->meshes.size()-1);
        setMesh(newModel->meshes.at(meshIndex)->getMeshPointer());
        flags.setFlag(js::Graphics::RenderFlag::DIRTY);
        return true;
    }
    return false;
}

bool MeshEntityProxy::canReplaceModelMeshPart(int meshIndex, int partIndex) {
    return (bool)_mesh;
}

bool MeshEntityProxy::overrideModelRenderFlags(js::Graphics::RenderFlags flagsToSet, js::Graphics::RenderFlags flagsToClear) {
    auto before = flags;
    flags = (before | flagsToSet) & ~(flagsToClear);
    if (!flagsToClear.testFlag(js::Graphics::RenderFlag::DIRTY)) {
        flags.setFlag(js::Graphics::RenderFlag::DIRTY);
    }
    return true;
}

js::Graphics::ModelPointer MeshEntityProxy::getScriptableModel() {
    QReadLocker locker(&lock);
    js::Graphics::ModelPointer result;
    if (_mesh) {
        result = js::Graphics::ModelPointer::create();
        result->objectID = objectID;
        result->append(_mesh);
    }
   return result;
}


int MeshEntityProxy::MyTriangleSet::indexOf(const Triangle tri) {
    int i=0;
    for (const auto& t: _triangles) {
        if (t.v0 == tri.v0 && t.v1 == tri.v1 && t.v2 == tri.v2) {
            return i;
        }
        i++;
    }
    return -1;
}

bool MeshEntityProxy::initialize() {
    auto geometryCache = DependencyManager::get<GeometryCache>();
    if (auto mesh = geometryCache->meshFromShape(GeometryCache::Shape::Icosahedron, glm::vec3(1.0f, 0.0f, 1.0f))) {
        setMesh(mesh);
        {
            QWriteLocker locker(&lock);
            flags |= js::Graphics::RenderFlag::DIRTY;
        }
        recalculateTriangleSet();
        return true;
    }
    return false;
}
