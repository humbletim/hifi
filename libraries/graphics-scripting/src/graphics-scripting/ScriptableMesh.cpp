//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Forward.h"

#include "ScriptableMesh.h"
#include "ScriptableMeshPart.h"

#include "GraphicsScriptingInterface.h"
#include "GraphicsScriptingUtil.h"
#include "OBJWriter.h"
#include <BaseScriptEngine.h>
#include <QtScript/QScriptValue>
#include <RegisteredMetaTypes.h>
#include <queue>
#include <unordered_set>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <graphics/BufferViewHelpers.h>
#include <graphics/GpuHelpers.h>
#include <graphics/Geometry.h>

QString js::Graphics::MeshPrototype::toString() const {
    if (isValid()) {
        const QString TOSTRING_TEMPLATE{ "[ScriptableMesh%1 numParts=%2 numVertices=%3]" };
        auto objectName = getNativeObject()->objectName();
        auto name = objectName.isEmpty() ? "" : " name=" + objectName;
        return TOSTRING_TEMPLATE.arg(name).arg(getNumParts()).arg(getNumVertices());
    }
    return "[ScriptableMesh nullptr]";
}

std::vector<js::Graphics::MeshPartPointer> js::Graphics::MeshPrototype::getMeshParts() const {
    std::vector<js::Graphics::MeshPartPointer> out;
    for (glm::uint32 i = 0; i < getNumParts(); i++) {
        out.emplace_back(new js::Graphics::MeshPart(getNativeObject(), i));
    }
    return out;
}

glm::uint32 js::Graphics::MeshPrototype::getNumIndices() const {
    if (auto mesh = getMeshPointer()) {
        return (glm::uint32)mesh->getNumIndices();
    }
    return 0;
}

glm::uint32 js::Graphics::MeshPrototype::getNumVertices() const {
    if (auto mesh = getMeshPointer()) {
        return (glm::uint32)mesh->getNumVertices();
    }
    return 0;
}

namespace {
    using IndexPair = std::pair<float, glm::uint32>;
    struct NearestFirst {
        bool operator()(const IndexPair& a, const IndexPair& b) {
            return a.first > b.first;
        }
    };
    void filterIndices(const graphics::MeshPointer& mesh,
                       const std::vector<glm::uint32>& rawIndices,
                       std::vector<glm::uint32>& result,
                       std::function<float(const glm::vec3& p)> distanceFunc) {

        std::priority_queue<IndexPair, std::vector<IndexPair>, NearestFirst> distances;
        const auto positions = buffer_helpers::mesh::getBufferView(mesh, gpu::Stream::POSITION);

        auto maxIndex = positions.getNumElements() - 1;
        std::unordered_set<glm::uint32> uniqueIndices;
        for (auto i : rawIndices) {
            if (i <= maxIndex) {
                uniqueIndices.insert(i);
            }
        }

        for (auto i : uniqueIndices) {
            float distance = distanceFunc(positions.get<glm::vec3>(i));
            if (!glm::isnan(distance)) {
                distances.push(std::make_pair(distance, i));
            }
        }

        glm::uint32 length = (glm::uint32)distances.size();
        result.resize(length);
        for (glm::uint32 i = 0; i < length; i++) {
            result[i] = distances.top().second;
            distances.pop();
        }
    }
}

std::vector<glm::uint32> js::Graphics::MeshPrototype::findRayVertexIndices(const glm::vec3& start, const glm::vec3& end, float epsilon,
                                                                           const std::vector<glm::uint32>& indices) const {
    std::vector<glm::uint32> result;
    if (!isValid()) {
        return result;
    }
    const float epsilon2 = epsilon*epsilon;
    const glm::vec3& ray{  start - end };
    const float scale = glm::length2(ray);
    if (scale == 0.0f) {
        context()->throwError("start and end are same point");
        return result;
    }
    const auto& rawIndices = indices.empty() ? getIndices() : indices;
    auto distanceMapper = [&](const glm::vec3& p) {
        const glm::vec3& cross{ glm::cross(p - start, ray ) };
        if ((glm::length2(cross) / scale) <= epsilon2) {
            return glm::distance2(p, start);
        }
        return NAN;
    };
    filterIndices(getMeshPointer(), rawIndices, result, distanceMapper);
    return result;
}

std::vector<glm::uint32> js::Graphics::MeshPrototype::findNearbyVertexIndices(const glm::vec3& origin, float epsilon,
                                                                              const std::vector<glm::uint32>& indices) const {
    std::vector<glm::uint32> result;
    if (!isValid()) {
        return result;
    }
    const auto epsilon2 = epsilon*epsilon;
    const auto& rawIndices = indices.empty() ? getIndices() : indices;
    auto distanceMapper = [&](const glm::vec3& position) {
        const glm::vec3& delta{ position - origin };
        if (glm::length2(delta) <= epsilon2) {
            return glm::length2(delta);
        }
        return NAN;
    };
    filterIndices(getMeshPointer(), rawIndices, result, distanceMapper);
    return result;
}

std::vector<glm::uint32> js::Graphics::MeshPrototype::getIndices() const {
    if (auto mesh = getMeshPointer()) {
        return buffer_helpers::bufferToVector<glm::uint32>(mesh->getIndexBuffer()).toStdVector();
    }
    return std::vector<glm::uint32>();
}


glm::uint32 js::Graphics::MeshPrototype::getNumAttributes() const {
    if (auto mesh = getMeshPointer()) {
        return (glm::uint32)mesh->getNumAttributes() + 1;
    }
    return 0;
}
std::vector<std::string> js::Graphics::MeshPrototype::getAttributeNames() const {
    std::vector<std::string> result;
    if (auto mesh = getMeshPointer()) {
        for (const auto& a : buffer_helpers::ATTRIBUTES.toStdMap()) {
            auto bufferView = buffer_helpers::mesh::getBufferView(mesh, a.second);
            if (bufferView.getNumElements() > 0) {
                result.emplace_back(a.first.toStdString());
            }
        }
    }
    return result;
}

QVariantMap js::Graphics::MeshPrototype::getVertexAttributes(glm::uint32 vertexIndex) const {
    if (!isValidIndex(vertexIndex)) {
        return QVariantMap();
    }
    return buffer_helpers::mesh::getVertexAttributes(getMeshPointer(), vertexIndex).toMap();
}

bool js::Graphics::MeshPrototype::setVertexAttributes(glm::uint32 vertexIndex, const QVariantMap& attributes) {
    for (const auto& name : attributes.keys()) {
        if (!isValidIndex(vertexIndex, name)) {
            return false;
        }
    }
    return buffer_helpers::mesh::setVertexAttributes(getMeshPointer(), vertexIndex, attributes);
}

int js::Graphics::MeshPrototype::getSlotNumber(const QString& attributeName) const {
    if (auto mesh = getMeshPointer()) {
        return buffer_helpers::ATTRIBUTES.value(attributeName, -1);
    }
    return -1;
}

QVariantMap js::Graphics::MeshPrototype::getBufferFormats() const {
    auto mesh = getMeshPointer();
    if (!mesh) {
        return QVariantMap();
    }
    auto bufferView = mesh->getIndexBuffer();
    QVariantMap result = {
        { "index", QVariantMap{
            { "length", (glm::uint32)bufferView.getNumElements() },
            { "byteLength", (glm::uint32)bufferView._size },
            { "offset", (glm::uint32) bufferView._offset },
            { "stride", (glm::uint32)bufferView._stride },
            { "element", scriptable::toVariant(bufferView._element) },
        }
    }};

    for (const auto& a : buffer_helpers::ATTRIBUTES.toStdMap()) {
        auto bufferView = buffer_helpers::mesh::getBufferView(mesh, a.second);
        if (!bufferView.getNumElements()) {
            continue;
        }
        result[a.first] = QVariantMap{
            { "slot", a.second },
            { "length", (glm::uint32)bufferView.getNumElements() },
            { "byteLength", (glm::uint32)bufferView._size },
            { "offset", (glm::uint32) bufferView._offset },
            { "stride", (glm::uint32)bufferView._stride },
            { "element", scriptable::toVariant(bufferView._element) },
        };
    }
    return result;
}

bool js::Graphics::MeshPrototype::removeAttribute(const QString& attributeName) {
    auto attribute = isValid() ? getSlotNumber(attributeName) : -1;
    if (attribute < 0) {
        return false;
    }
    if (attribute == gpu::Stream::POSITION) {
        context()->throwError("cannot remove .position attribute");
        return false;
    }
    if (buffer_helpers::mesh::getBufferView(getMeshPointer(), attribute).getNumElements()) {
        getMeshPointer()->removeAttribute(attribute);
        return true;
    }
    return false;
}

glm::uint32 js::Graphics::MeshPrototype::addAttribute(const QString& attributeName, const QVariant& defaultValue) {
    auto attribute = isValid() ? getSlotNumber(attributeName) : -1;
    if (attribute < 0) {
        return 0;
    }
    auto mesh = getMeshPointer();
    auto numVertices = getNumVertices();
    auto names = getAttributeNames();
    if (std::find(names.begin(), names.end(), attributeName.toStdString()) == names.end()) {
        QVector<QVariant> values;
        values.fill(defaultValue, numVertices);
        mesh->addAttribute(attribute, buffer_helpers::newFromVector(values, gpu::Stream::getDefaultElements()[attribute]));
        return values.size();
    } else {
        auto bufferView = buffer_helpers::mesh::getBufferView(mesh, attribute);
        auto current = (glm::uint32)bufferView.getNumElements();
        if (current < numVertices) {
            bufferView = buffer_helpers::resized(bufferView, numVertices);
            for (glm::uint32 i = current; i < numVertices; i++) {
                buffer_helpers::setValue<QVariant>(bufferView, i, defaultValue);
            }
            return numVertices - current;
        } else if (current > numVertices) {
            qCDebug(graphics_scripting) << QString("addAttribute current=%1 > numVertices=%2").arg(current).arg(numVertices);
            return 0;
        }
    }
    return 0;
}

glm::uint32 js::Graphics::MeshPrototype::fillAttribute(const QString& attributeName, const QVariant& value) {
    auto attribute = isValid() ? getSlotNumber(attributeName) : -1;
    if (attribute < 0) {
        return 0;
    }
    auto mesh = getMeshPointer();
    auto numVertices = getNumVertices();
    QVector<QVariant> values;
    values.fill(value, numVertices);
    mesh->addAttribute(attribute, buffer_helpers::newFromVector(values, gpu::Stream::getDefaultElements()[attribute]));
    return true;
}

AABox js::Graphics::MeshPrototype::getMeshExtents() const {
    auto mesh = getMeshPointer();
    return mesh ? mesh->evalPartsBound(0, (int)mesh->getNumParts()) : AABox();
}

glm::uint32 js::Graphics::MeshPrototype::getNumParts() const {
    if (auto mesh = getMeshPointer()) {
        return (glm::uint32)mesh->getNumParts();
    }
    return 0;
}

QVariantList js::Graphics::MeshPrototype::queryVertexAttributes(QVariant selector) const {
    QVariantList result;
    const auto& attributeName = selector.toString();
    if (!isValidIndex(0, attributeName)) {
        return result;
    }
    auto attribute = getSlotNumber(attributeName);
    const auto& bufferView = buffer_helpers::mesh::getBufferView(getMeshPointer(), static_cast<gpu::Stream::Slot>(attribute));
    glm::uint32 numElements = (glm::uint32)bufferView.getNumElements();
    for (glm::uint32 i = 0; i < numElements; i++) {
        result << buffer_helpers::getValue<QVariant>(bufferView, i, qUtf8Printable(attributeName));
    }
    return result;
}

QVariant js::Graphics::MeshPrototype::getVertexProperty(glm::uint32 vertexIndex, const QString& attributeName) const {
    if (!isValidIndex(vertexIndex, attributeName)) {
        return QVariant();
    }
    auto attribute = getSlotNumber(attributeName);
    const auto& bufferView = buffer_helpers::mesh::getBufferView(getMeshPointer(), static_cast<gpu::Stream::Slot>(attribute));
    return buffer_helpers::getValue<QVariant>(bufferView, vertexIndex, qUtf8Printable(attributeName));
}

bool js::Graphics::MeshPrototype::setVertexProperty(glm::uint32 vertexIndex, const QString& attributeName, const QVariant& value) {
    if (!isValidIndex(vertexIndex, attributeName)) {
        return false;
    }
    auto attribute = getSlotNumber(attributeName);
    const auto& bufferView = buffer_helpers::mesh::getBufferView(getMeshPointer(), static_cast<gpu::Stream::Slot>(attribute));
    return buffer_helpers::setValue(bufferView, vertexIndex, value);
}

glm::uint32 js::Graphics::MeshPrototype::forEachVertex(QScriptValue _callback) {
    auto mesh = getMeshPointer();
    if (!mesh) {
        return 0;
    }
    auto scopedHandler = scriptable::jsBindCallback(_callback);

    // destructure so we can still invoke callback scoped, but with a custom signature (obj, i, jsMesh)
    auto scope = scopedHandler.property("scope");
    auto callback = scopedHandler.property("callback");
    auto js = engine() ? engine() : scopedHandler.engine(); // cache value to avoid resolving each iteration
    if (!js) {
        return 0;
    }
    auto meshPart = js ? js->toScriptValue(getNativeObject()) : QScriptValue::NullValue;
    int numProcessed = 0;
    buffer_helpers::mesh::forEachVertex(mesh, [&](glm::uint32 index, const QVariantMap& values) {
        auto result = callback.call(scope, { js->toScriptValue(values), index, meshPart });
        if (js->hasUncaughtException()) {
            js->currentContext()->throwValue(js->uncaughtException());
            return false;
        }
        numProcessed++;
        return true;
    });
    return numProcessed;
}


glm::uint32 js::Graphics::MeshPrototype::updateVertexAttributes(QScriptValue _callback) {
    auto mesh = getMeshPointer();
    if (!mesh) {
        return 0;
    }
    auto scopedHandler = scriptable::jsBindCallback(_callback);

    // destructure so we can still invoke callback scoped, but with a custom signature (obj, i, jsMesh)
    auto scope = scopedHandler.property("scope");
    auto callback = scopedHandler.property("callback");
    auto js = engine() ? engine() : scopedHandler.engine(); // cache value to avoid resolving each iteration
    if (!js) {
        return 0;
    }
    auto meshPart = js ? js->toScriptValue(getNativeObject()) : QScriptValue::NullValue;
    int numProcessed = 0;
    auto attributeViews = buffer_helpers::mesh::getAllBufferViews(mesh);
    buffer_helpers::mesh::forEachVertex(mesh, [&](glm::uint32 index, const QVariantMap& values) {
        auto obj = js->toScriptValue(values);
        auto result = callback.call(scope, { obj, index, meshPart });
        if (js->hasUncaughtException()) {
            js->currentContext()->throwValue(js->uncaughtException());
            return false;
        }
        if (result.isBool() && !result.toBool()) {
            // bail without modifying data if user explicitly returns false
            return true;
        }
        if (result.isObject() && !result.strictlyEquals(obj)) {
            // user returned a new object (ie: instead of modifying input properties)
            obj = result;
        }
        for (const auto& a : attributeViews) {
            const auto& attribute = obj.property(a.first);
            if (attribute.isValid()) {
                buffer_helpers::setValue(a.second, index, attribute.toVariant());
            }
        }
        numProcessed++;
        return true;
    });
    return numProcessed;
}

// protect against user scripts sending bogus values
bool js::Graphics::MeshPrototype::isValidIndex(glm::uint32 vertexIndex, const QString& attributeName) const {
    if (!isValid()) {
        return false;
    }
    const auto last = getNumVertices() - 1;
    if (vertexIndex > last) {
        if (context()) {
            context()->throwError(QString("vertexIndex=%1 out of range (firstVertexIndex=%2, lastVertexIndex=%3)").arg(vertexIndex).arg(0).arg(last));
        }
        return false;
    }
    if (!attributeName.isEmpty()) {
        auto attribute = getSlotNumber(attributeName);
        if (attribute < 0) {
            if (context()) {
                context()->throwError(QString("invalid attributeName=%1").arg(attributeName));
            }
            return false;
        }
        auto view = buffer_helpers::mesh::getBufferView(getMeshPointer(), static_cast<gpu::Stream::Slot>(attribute));
        if (vertexIndex >= (glm::uint32)view.getNumElements()) {
            if (context()) {
                context()->throwError(QString("vertexIndex=%1 out of range (attribute=%2, numElements=%3)").arg(vertexIndex).arg(attributeName).arg(view.getNumElements()));
            }
            return false;
        }
    }
    return true;
}

namespace {
    template <typename T> gpu::BufferView bufferViewFromScriptValue(
        gpu::Stream::Slot attribute, QScriptValue array, const QString& typedArrayName, int forceSize, gpu::Element type) {
        auto values = scriptable::coerceJSTypedArray<T>(array, typedArrayName, buffer_helpers::ATTRIBUTES.key((int)attribute));
        auto vector = buffer_helpers::variantToVector<T>(values);
        vector.resize(forceSize);
        return buffer_helpers::newFromVector<T>(vector, type);
    }
}

bool js::Graphics::MeshPrototype::attributeFromTypedArray(const QString& attributeName, QScriptValue array) {
    auto mesh = getMeshPointer();

    if (!mesh) {
        return false;
    }
    auto attribute = getSlotNumber(attributeName);
    qDebug() << "bufferView attribute=" << attributeName << (int)attribute;
    gpu::BufferView bufferView;
    switch (attribute) {
    case gpu::Stream::NORMAL:
    case gpu::Stream::COLOR:
    case gpu::Stream::POSITION:
    case gpu::Stream::TANGENT:
        bufferView = bufferViewFromScriptValue<glm::vec3>(attribute, array, "Float32Array", getNumVertices(), gpu::Format::VEC3F_XYZ);
        break;
    case gpu::Stream::TEXCOORD0:
    case gpu::Stream::TEXCOORD1:
    case gpu::Stream::TEXCOORD2:
    case gpu::Stream::TEXCOORD3:
    case gpu::Stream::TEXCOORD4:
        bufferView = bufferViewFromScriptValue<glm::vec2>(attribute, array, "Float32Array", getNumVertices(), gpu::Format::VEC2F_UV);
        break;
    case gpu::Stream::SKIN_CLUSTER_INDEX:
    case gpu::Stream::SKIN_CLUSTER_WEIGHT:
        bufferView = bufferViewFromScriptValue<glm::vec4>(attribute, array, "Float32Array", getNumVertices(), gpu::Format::VEC4F_XYZW);
        break;
    default:
        if (attributeName == "index") {
            auto values = scriptable::coerceJSTypedArray<glm::uint32>(array, "Uint32Array", attributeName);
            //qDebug() << "values" << values;
            auto vector = buffer_helpers::variantToVector<glm::uint32>(values);
            auto maxVertexIndex = getNumVertices();
            int i=0;
            for (auto& index : vector) {
                if (index >= maxVertexIndex) {
                    qDebug() << "forcing new index at " << i << "into valid vertex range [0, " << maxVertexIndex << ")" << "-- was " << index;
                    index = maxVertexIndex - 1;
                }
            }
            bufferView = buffer_helpers::newFromVector<glm::uint32>(vector, gpu::Format::INDEX_INT32);
            break;
        }
    }
    if (engine()->hasUncaughtException()) {
        return false;
    }
    if (bufferView.getNumElements()) {
        qDebug() << "bufferView" << bufferView.getNumElements() << bufferView._buffer->getSize();
        qDebug() << "bufferView[0]" << bufferView.get<glm::vec3>(0);
        if (attributeName == "index") {
            mesh->setIndexBuffer(bufferView);
            int maxIndex = (int)bufferView.getNumElements() - 1;
            for (int i = 0; i < (int)getNumParts(); i++) {
                auto& part = getMeshPointer()->getPartBuffer().edit<graphics::Mesh::Part>(i);
                if (part._startIndex >= maxIndex) {
                    qDebug() << "forcing part._startIndex within index range" << i << "was:" << part._startIndex << "now: " << glm::min(part._startIndex, maxIndex);
                    part._startIndex = glm::min(part._startIndex, maxIndex);
                }
                if ((part._startIndex + part._numIndices) > maxIndex) {
                    qDebug() << "forcing part._numIndices within index range" << i << "was:" << part._numIndices << "now:" << glm::min(part._numIndices, maxIndex - part._startIndex);
                    part._numIndices = glm::min(part._numIndices, maxIndex - part._startIndex);
                }
            }
            return true;
        } else if (attribute == gpu::Stream::POSITION) {
            mesh->setVertexBuffer(bufferView);
        } else {
            mesh->addAttribute(attribute, bufferView);
        }
        return true;
    }
    return false;
}

QScriptValue js::Graphics::MeshPrototype::attributeToTypedArray(const QString& attributeName) {
    gpu::BufferView bufferView;
    auto sourceProperty = scriptable::JSVectorAdapter::normalizeAlias(attributeName);
    if (attributeName == "index") {
        bufferView = getMeshPointer()->getIndexBuffer();
    } else if (isValidIndex(0, attributeName)) {
        bufferView = buffer_helpers::mesh::getBufferView(getMeshPointer(), static_cast<gpu::Stream::Slot>(getSlotNumber(attributeName)));
    }

    if (!bufferView.getNumElements()) {
        return QScriptValue::NullValue;
    }

    QByteArray bytes{ (const char*)bufferView._buffer->getData(), (int)bufferView._buffer->getSize() };
    QString typedArrayName;
    if (attributeName == "index") {
        typedArrayName = "Uint32Array";
    } else {
        switch(bufferView._element.getType()) {
        case gpu::UINT32: typedArrayName = "Uint32Array"; break;
        case gpu::UINT16: typedArrayName = "Uint16Array"; break;
        case gpu::UINT8: typedArrayName = "Uint8Array"; break;
        case gpu::INT32: typedArrayName = "Int32Array"; break;
        case gpu::INT16: typedArrayName = "Int16Array"; break;
        case gpu::FLOAT: typedArrayName = "Float32Array"; break;
        case gpu::HALF: typedArrayName = "Uint16Array"; break;
        case gpu::NUINT8: typedArrayName = "Uint8Array"; break;
        default: typedArrayName = "Uint8Array"; break;
        }
    }
    auto result = scriptable::toTypedArray(engine()->globalObject(), bytes, typedArrayName);
    result.setProperty("typedArrayName", typedArrayName);
    return result;
}

js::Graphics::MeshPointer js::Graphics::MeshPrototype::recenter(const glm::vec3& origin) {
    if (auto mesh = getMeshPointer()) {
        auto box = mesh->evalPartsBound(0, (int)mesh->getNumParts());
        glm::vec3 center = box.calcCenter();
        glm::vec3 zero = glm::isnan(origin.x) ? glm::vec3() : origin;
        return translate(zero - center);
    }
    return nullptr;
}

js::Graphics::MeshPointer js::Graphics::MeshPrototype::scaleToFit(float unitScale) {
    if (auto mesh = getMeshPointer()) {
        auto box = mesh->evalPartsBound(0, (int)mesh->getNumParts());
        auto center = box.calcCenter();
        float maxDimension = box.getLargestDimension();
        return scale(glm::vec3(unitScale / maxDimension), center);
    }
    return {};
}
js::Graphics::MeshPointer js::Graphics::MeshPrototype::translate(const glm::vec3& translation) {
    return transform(glm::translate(translation));
}
js::Graphics::MeshPointer js::Graphics::MeshPrototype::scale(const glm::vec3& scale, const glm::vec3& origin) {
    if (auto mesh = getMeshPointer()) {
        auto box = mesh->evalPartsBound(0, (int)mesh->getNumParts());
        glm::vec3 center = glm::isnan(origin.x) ? box.calcCenter() : origin;
        return transform(glm::translate(center) * glm::scale(scale) * glm::translate(-center));
    }
    return {};
}
js::Graphics::MeshPointer js::Graphics::MeshPrototype::rotateVec3Degrees(const glm::vec3& eulerAngles, const glm::vec3& origin) {
    return rotate(glm::quat(glm::radians(eulerAngles)), origin);
}
js::Graphics::MeshPointer js::Graphics::MeshPrototype::rotateDegrees(float x, float y, float z, const glm::vec3& origin) {
    return rotateVec3Degrees({ x, y, z }, origin);
}
js::Graphics::MeshPointer js::Graphics::MeshPrototype::rotate(const glm::quat& rotation, const glm::vec3& origin) {
    if (auto mesh = getMeshPointer()) {
        auto box = mesh->evalPartsBound(0, (int)mesh->getNumParts());
        glm::vec3 center = glm::isnan(origin.x) ? box.calcCenter() : origin;
        return transform(glm::translate(center) * glm::toMat4(rotation) * glm::translate(-center));
    }
    return {};
}
js::Graphics::MeshPointer js::Graphics::MeshPrototype::transform(const glm::mat4& transform) {
    if (auto mesh = getMeshPointer()) {
        const auto& pos = buffer_helpers::mesh::getBufferView(mesh, gpu::Stream::POSITION);
        const glm::uint32 num = (glm::uint32)pos.getNumElements();
        for (glm::uint32 i = 0; i < num; i++) {
            auto& position = pos.edit<glm::vec3>(i);
            position = transform * glm::vec4(position, 1.0f);
        }
        return getNativeObject();
    }
    return {};
}

js::Graphics::MeshPointer js::Graphics::MeshPrototype::cloneMesh() const {
    if (auto mesh = getMeshPointer()) {
        return GraphicsScriptingInterface::cloneMesh(getNativeObject());
    }
    return nullptr;
}

// implementations for Forward.h

// note: we don't always want the JS side to prevent mesh data from being freed (hence weak pointers unless parented QObject)
js::Graphics::Mesh::Mesh(
    js::Graphics::WeakModelProviderPointer provider, js::Graphics::ModelPointer model, graphics::WeakMeshPointer weakMesh, QObject* parent
    ) : QObject(parent), QEnableSharedFromThis<js::Graphics::Mesh>(), provider(provider), model(model), weakMesh(weakMesh) {
    if (parent) {
        strongMesh = weakMesh.lock();
    }
    auto mesh = getMeshPointer();
    QString name = mesh ? QString::fromStdString(mesh->modelName) : "";
    if (name.isEmpty()) {
        name = mesh ? QString::fromStdString(mesh->displayName) : "";
    }
    auto parentModel = getModelPointer();
    setObjectName(QString("%1#%2").arg(parentModel ? parentModel->objectName() : "").arg(name));
}

js::Graphics::Mesh::Mesh(graphics::MeshPointer strongMesh) :
    js::Graphics::Mesh(js::Graphics::WeakModelProviderPointer(), nullptr, strongMesh, nullptr) {
    this->strongMesh = strongMesh;
    //QObject::connect(this, &QObject::destroyed, this, [this]() { this->strongMesh.reset(); });
}

js::Graphics::Mesh& js::Graphics::Mesh::operator=(const js::Graphics::Mesh& view) {
    provider = view.provider;
    model = view.model;
    weakMesh = view.weakMesh;
    strongMesh = view.strongMesh;
    return *this;
}
