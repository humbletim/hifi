//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include <QPointer>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QUuid>
#include <QtCore/QVariant>
#include <QtCore/QVector>
#include <QtScript/QScriptValue>
#include <QtScript/QScriptable>
#include <memory>
#include <glm/glm.hpp>

#include "Forward.h"
#include "GraphicsScriptingUtil.h"
#include <graphics/BufferViewHelpers.h>
#include <graphics/Geometry.h>
#include <Extents.h>

namespace scriptable {
    /**jsdoc
     * @typedef {object} Graphics.Mesh
     * @property {Graphics.MeshPart[]} parts - Array of submesh part references.
     * @property {string[]} attributeNames - Vertex attribute names (color, normal, etc.)
     * @property {number} numParts - The number of parts contained in the mesh.
     * @property {number} numIndices - Total number of vertex indices in the mesh.
     * @property {number} numVertices - Total number of vertices in the Mesh.
     * @property {number} numAttributes - Number of currently defined vertex attributes.
     * @property {boolean} valid
     * @property {boolean} strong
     * @property {object} extents
     * @property {object} bufferFormats
     */
    class ScriptableMesh : public QObject, public QScriptable {
        Q_OBJECT
        Q_PROPERTY(glm::uint32 numParts READ getNumParts)
        Q_PROPERTY(glm::uint32 numAttributes READ getNumAttributes)
        Q_PROPERTY(glm::uint32 numVertices READ getNumVertices)
        Q_PROPERTY(glm::uint32 numIndices READ getNumIndices)
        Q_PROPERTY(std::vector<std::string> attributeNames READ getAttributeNames)
        Q_PROPERTY(QVector<scriptable::ScriptableMeshPartPointer> parts READ getMeshParts)
        Q_PROPERTY(bool valid READ isValid)
        Q_PROPERTY(Extents extents READ getMeshExtents)
        Q_PROPERTY(QVariantMap bufferFormats READ getBufferFormats)
        Q_PROPERTY(QString name READ getDisplayName)
        Q_PROPERTY(QString modelName READ getModelName)
    public:
        ScriptableMesh(const graphics::MeshPointer& meshBase) : QObject(nullptr), QScriptable(), _nativeObject(meshBase) {}

        scriptable::ScriptableMeshPointer getNativeObject() const {
            return _nativeObject ? _nativeObject : qscriptvalue_cast<scriptable::ScriptableMeshPointer>(thisObject());
        }
        graphics::MeshPointer getMeshPointer() const { return getNativeObject(); }
        virtual bool isValid() const { return (bool)getNativeObject(); }
        glm::uint32 getNumParts() const;
        glm::uint32 getNumVertices() const;
        glm::uint32 getNumAttributes() const;
        glm::uint32 getNumIndices() const;
        std::vector<std::string> getAttributeNames() const;
        QVector<scriptable::ScriptableMeshPartPointer> getMeshParts() const;
        Extents getMeshExtents() const;

        operator bool() const { return isValid();  }
        int getSlotNumber(const QString& attributeName) const;
        QVariantMap getBufferFormats() const;

    public slots:

        std::vector<glm::uint32> getIndices() const;
        glm::uint32 addAttribute(const QString& attributeName, const QVariant& defaultValue = QVariant());
        glm::uint32 fillAttribute(const QString& attributeName, const QVariant& value);
        bool removeAttribute(const QString& attributeName);
        scriptable::ScriptableMeshPointer cloneMesh() const;
        scriptable::ScriptableMeshPointer dedupeVertices(float epsilon = 1.0e-6f, bool resetNormals = false);
        QVariantList queryVertexAttributes(QVariant selector) const;
        QVariantMap getVertexAttributes(glm::uint32 vertexIndex) const;
        bool setVertexAttributes(glm::uint32 vertexIndex, const QVariantMap& attributeValues);
        QVariant getVertexProperty(glm::uint32 vertexIndex, const QString& attributeName) const;
        bool setVertexProperty(glm::uint32 vertexIndex, const QString& attributeName, const QVariant& value);

        scriptable::ScriptableMeshPointer scaleToFit(float unitScale);
        scriptable::ScriptableMeshPointer recenter(const glm::vec3& = glm::vec3(NAN));
        scriptable::ScriptableMeshPointer translate(const glm::vec3& translation);
        scriptable::ScriptableMeshPointer scale(const glm::vec3& scale, const glm::vec3& origin = glm::vec3(NAN));
        scriptable::ScriptableMeshPointer rotateVec3Degrees(const glm::vec3& eulerAngles, const glm::vec3& origin = glm::vec3(NAN));
        scriptable::ScriptableMeshPointer rotateDegrees(float x, float y, float z, const glm::vec3& origin = glm::vec3(NAN));
        scriptable::ScriptableMeshPointer rotate(const glm::quat& rotation, const glm::vec3& origin = glm::vec3(NAN));
        scriptable::ScriptableMeshPointer transform(const glm::mat4& transform);

        QString toString() const;
        QVariant toVariant() const;
        QString getDisplayName() const { return getNativeObject() ? QString::fromStdString(getNativeObject()->displayName) : objectName(); }
        QString getModelName() const { return getNativeObject() ? QString::fromStdString(getNativeObject()->modelName) : QString(); }

        bool isValidIndex(glm::uint32 vertexIndex, const QString& attributeName = QString()) const;

        glm::uint32 updateVertexAttributes(QScriptValue callback);
        glm::uint32 forEachVertex(QScriptValue callback);

        QString toOBJ();
    protected:
        graphics::MeshPointer _nativeObject;
        template <typename T> T jsAssert(T value, const QString& error) const { return scriptable::jsAssert(engine(), value, error); }
    };

}

Q_DECLARE_METATYPE(scriptable::ScriptableMeshPointer)
Q_DECLARE_METATYPE(QVector<scriptable::ScriptableMeshPointer>)
