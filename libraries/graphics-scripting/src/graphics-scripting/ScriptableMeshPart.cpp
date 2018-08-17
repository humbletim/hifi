//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Forward.h"

#include "ScriptableMeshPart.h"
#include "ScriptableMesh.h"

#include "GraphicsScriptingUtil.h"
#include "GraphicsScriptingInterface.h"
#include "OBJWriter.h"
#include <BaseScriptEngine.h>
#include <QtScript/QScriptValue>
#include <RegisteredMetaTypes.h>
#include <Extents.h>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <graphics/BufferViewHelpers.h>
#include <graphics/GpuHelpers.h>
#include <graphics/Geometry.h>

QString scriptable::ScriptableMeshPart::toString() const {
    if (isValid()) {
        const QString TOSTRING_TEMPLATE{ "[MeshPart%1 partIndex=%2 numIndices=%3]" };
        auto objectName = getNativeObject()->objectName();
        auto name = objectName.isEmpty() ? "" : " name=" + objectName;
        return TOSTRING_TEMPLATE.arg(name).arg(partIndex).arg(getNumIndices());
    }
    return "[MeshPart nullptr]";
}

QVariant scriptable::ScriptableMeshPart::toVariant() const {
    return QVariant::fromValue<scriptable::MappedQObject>({ this, &staticMetaObject });
}

QString scriptable::ScriptableMeshPart::toOBJ() {
    if (!getMeshPointer()) {
        if (context()) {
            context()->throwError(QString("null mesh"));
        } else {
            qCWarning(graphics_scripting) << "toOBJ: null mesh";
        }
        return QString();
    }
    return writeOBJToString({ getMeshPointer() });
}

bool scriptable::ScriptableMeshPart::isValidIndex(glm::uint32 vertexIndex, const QString& attributeName) const {
    return isValid() && scriptable::ScriptableMesh(getMeshPointer()).isValidIndex(vertexIndex, attributeName);
}

bool scriptable::ScriptableMeshPart::replaceMeshPartData(scriptable::ScriptableMeshPartPointer src, const std::vector<std::string>& attributeNames) {
    auto target = getMeshPointer();
    auto srcMesh = std::make_shared<scriptable::ScriptableMesh>(src->parentMesh);

    auto source = srcMesh ? srcMesh->getMeshPointer() : nullptr;
    if (!target || !source) {
        if (context()) {
            context()->throwError("ScriptableMeshPart::replaceMeshData -- expected dest and src to be valid mesh proxy pointers");
        } else {
            qCWarning(graphics_scripting) << "ScriptableMeshPart::replaceMeshData -- expected dest and src to be valid mesh proxy pointers";
        }
        return false;
    }

    QVector<QString> attributes;
    {
        const auto& names = attributeNames.empty() ? srcMesh->getAttributeNames() : attributeNames;
        // TODO: the code down below expects QVector<QString> -- at some point should refactor to use std::vector<std::string>
        std::vector<QString> tmp;
        std::transform(names.begin(), names.end(), std::back_inserter(tmp), QString::fromStdString);
        attributes = QVector<QString>::fromStdVector(tmp);
    }

    // remove attributes only found on target mesh, unless user has explicitly specified the relevant attribute names
    if (attributes.isEmpty()) {
        auto attributeViews = buffer_helpers::mesh::getAllBufferViews(target);
        for (const auto& a : attributeViews) {
            auto attribute = buffer_helpers::ATTRIBUTES[a.first];
            if (!attributes.contains(a.first)) {
                target->removeAttribute(attribute);
            }
        }
    }

    target->setVertexBuffer(buffer_helpers::clone(source->getVertexBuffer()));
    target->setIndexBuffer(buffer_helpers::clone(source->getIndexBuffer()));
    target->setPartBuffer(buffer_helpers::clone(source->getPartBuffer()));

    for (const auto& a : attributes) {
        auto attribute = buffer_helpers::ATTRIBUTES[a];
        if (attribute == gpu::Stream::POSITION) {
            continue;
        }
        auto& input = source->getAttributeBuffer(attribute);
        if (input.getNumElements() == 0) {
            target->removeAttribute(attribute);
        } else {
            target->addAttribute(attribute, buffer_helpers::clone(input));
        }
    }
    return true;
}

scriptable::ScriptableMeshPartPointer scriptable::ScriptableMeshPart::cloneMeshPart() {
    if (auto mesh = getMeshPointer()) {
        if (auto clone = buffer_helpers::mesh::clone(mesh)) {
            return scriptable::ScriptableMeshPartPointer::create(clone, partIndex);
        }
    }
    return nullptr;
}

std::vector<glm::uint32> scriptable::ScriptableMeshPart::getIndices(glm::uint32 pos, glm::uint32 len) const {
    if (auto mesh = getMeshPointer()) {
        if (pos == BIGGEST_INDEX_POSSIBLE) {
            pos = getFirstVertexIndex();
        }
        if (len == BIGGEST_INDEX_POSSIBLE) {
            len = getNumIndices();
        }
        return buffer_helpers::bufferToVector<glm::uint32>(mesh->getIndexBuffer()).mid(pos, len).toStdVector();
    }
    return std::vector<glm::uint32>();
}

bool scriptable::ScriptableMeshPart::setFirstVertexIndex( glm::uint32 vertexIndex) {
    if (!isValidIndex(vertexIndex)) {
        return false;
    }
    auto& part = getMeshPointer()->getPartBuffer().edit<graphics::Mesh::Part>(partIndex);
    part._startIndex = vertexIndex;
    return true;
}

bool scriptable::ScriptableMeshPart::setBaseVertexIndex( glm::uint32 vertexIndex) {
    if (!isValidIndex(vertexIndex)) {
        return false;
    }
    auto& part = getMeshPointer()->getPartBuffer().edit<graphics::Mesh::Part>(partIndex);
    part._baseVertex = vertexIndex;
    return true;
}

bool scriptable::ScriptableMeshPart::setLastVertexIndex( glm::uint32 vertexIndex) {
    if (!isValidIndex(vertexIndex) || vertexIndex <= getFirstVertexIndex()) {
        return false;
    }
    auto& part = getMeshPointer()->getPartBuffer().edit<graphics::Mesh::Part>(partIndex);
    part._numIndices = vertexIndex - part._startIndex;
    return true;
}

bool scriptable::ScriptableMeshPart::setIndices(const std::vector<glm::uint32>& indices) {
    if (!isValid()) {
        return false;
    }
    glm::uint32 len = (glm::uint32)indices.size();
    if (len != getNumIndices()) {
        context()->throwError(QString("setIndices: currently new indicies must be assign 1:1 across old indicies (indicies.size()=%1, numIndices=%2)")
                              .arg(len).arg(getNumIndices()));
        return false;
    }
    auto mesh = getMeshPointer();
    auto indexBuffer = mesh->getIndexBuffer();

    // first loop to validate all indices are valid
    for (glm::uint32 i = 0; i < len; i++) {
        if (!isValidIndex(indices.at(i))) {
            return false;
        }
    }
    const auto first = getFirstVertexIndex();
    // now actually apply them
    for (glm::uint32 i = 0; i < len; i++) {
        buffer_helpers::setValue(indexBuffer, first + i, indices.at(i));
    }
    return true;
}

const graphics::Mesh::Part& scriptable::ScriptableMeshPart::getMeshPart() const {
    static const graphics::Mesh::Part invalidPart;
    if (!isValid()) {
        qCWarning(graphics_scripting) << "getMeshPart() -- parent mesh is invalid";
        return invalidPart;
    }
    return getMeshPointer()->getPartBuffer().get<graphics::Mesh::Part>(partIndex);
}

// FIXME: how we handle topology will need to be reworked if wanting to support TRIANGLE_STRIP, QUADS and QUAD_STRIP
bool scriptable::ScriptableMeshPart::setTopology(graphics::Mesh::Topology topology) {
    if (!isValid()) {
        return false;
    }
    auto& part = getMeshPointer()->getPartBuffer().edit<graphics::Mesh::Part>(partIndex);
    switch (topology) {
#ifdef DEV_BUILD
    case graphics::Mesh::Topology::POINTS:
    case graphics::Mesh::Topology::LINES:
#endif
    case graphics::Mesh::Topology::TRIANGLES:
        part._topology = topology;
        return true;
    default:
        context()->throwError("changing topology to " + graphics::toString(topology) + " is not yet supported");
        return false;
    }
}

glm::uint32 scriptable::ScriptableMeshPart::getTopologyLength() const {
    switch(getTopology()) {
    case graphics::Mesh::Topology::POINTS: return 1;
    case graphics::Mesh::Topology::LINES: return 2;
    case graphics::Mesh::Topology::TRIANGLES: return 3;
    case graphics::Mesh::Topology::QUADS: return 4;
    default: qCDebug(graphics_scripting) << "getTopologyLength -- unrecognized topology" << getTopology();
    }
    return 0;
}

std::vector<glm::uint32> scriptable::ScriptableMeshPart::getFace(glm::uint32 faceIndex) const {
    switch (getTopology()) {
    case graphics::Mesh::Topology::POINTS:
    case graphics::Mesh::Topology::LINES:
    case graphics::Mesh::Topology::TRIANGLES:
    case graphics::Mesh::Topology::QUADS:
        if (faceIndex < getNumFaces()) {
            return getIndices(faceIndex * getTopologyLength(), getTopologyLength());
        }
    default: return std::vector<glm::uint32>();
    }
}

Extents scriptable::ScriptableMeshPart::getPartExtents() const {
    graphics::Box box;
    if (auto mesh = getMeshPointer()) {
        box = mesh->evalPartBound(partIndex);
    }
    return box;
}
