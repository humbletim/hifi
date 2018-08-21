//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include "Forward.h"
#include "GraphicsScriptingUtil.h"
#include "ScriptableMesh.h"
#include <QtScript/QScriptable>

class QScriptValue;

namespace scriptable {
    /**jsdoc
     * @typedef {object} Graphics.Model
     * @property {Uuid} objectID - UUID of corresponding inworld object (if model is associated)
     * @property {number} numMeshes - The number of submeshes contained in the model.
     * @property {Graphics.Mesh[]} meshes - Array of submesh references.
     * @property {Object.<string,Graphics.MaterialLayer[]>} materialLayers - Map of materials layer lists.  You can look up a material layer list by mesh part number or by material name.
     * @property {string[]} materialNames - Array of all the material names used by the mesh parts of this model, in order (e.g. materialNames[0] is the name of the first mesh part's material).
     */

    class ScriptableModel : public QObject, public QScriptable {
        Q_OBJECT
        Q_PROPERTY(QUuid objectID READ getObjectID)
        Q_PROPERTY(glm::uint32 numMeshes READ getNumMeshes)
        Q_PROPERTY(scriptable::ScriptableMeshes meshes READ getMeshes)
        Q_PROPERTY(scriptable::MultiMaterialMap materialLayers READ getMaterialLayers)
        Q_PROPERTY(QVector<QString> materialNames READ getMaterialNames)
        Q_PROPERTY(Extents extents READ getModelExtents)
        Q_PROPERTY(bool valid READ isValid)

    public:
        ScriptableModel(const scriptable::ScriptableModelBasePointer& nativeModelBase) : QObject(), QScriptable(), _nativeObject(nativeModelBase) {}
        scriptable::ScriptableMeshes getMeshes();
        const scriptable::ScriptableMeshes getConstMeshes() const;

        scriptable::MultiMaterialMap getMaterialLayers() { return getNativeObject() ? getNativeObject()->materialLayers : scriptable::MultiMaterialMap(); }
        QVector<QString> getMaterialNames() { return getNativeObject() ? getNativeObject()->materialNames : QVector<QString>(); }

        // invoke a ScriptableMesh method across all meshes
        template<typename M, typename ...Args>
        scriptable::ScriptableModelPointer invokeAll(M method, Args &&...args) {
            auto self = getNativeObject();
            if (self) {
                auto engine = this->engine();
                auto meshes = self->meshes;
                for (auto& mesh : self->meshes) {
                    scriptable::ScriptableMesh wrapped{ mesh };
                    (wrapped.*method)(std::forward<Args>(args)...);
                    if (engine && engine->hasUncaughtException()) {
                        break;
                    }
                }
            }
            return self;
        }

        // invoke a MeshPrototype method across all meshes while accumulating return values
        template<typename M, typename ...Args>
        glm::uint32 countAll(M method, Args &&...args) {
            glm::uint32 result = 0;
            if (auto self = getNativeObject()) {
                auto engine = this->engine();
                auto meshes = self->meshes;
                for (auto& mesh : self->meshes) {
                    scriptable::ScriptableMesh wrapped{ mesh };
                    result += (wrapped.*method)(std::forward<Args>(args)...);
                    if (engine && engine->hasUncaughtException()) {
                        break;
                    }
                }
            }
            return result;
        }

    public slots:
        scriptable::ScriptableModelPointer cloneModel(const QVariantMap& options = QVariantMap());
        QString toString() const;
        QVariant toVariant() const;

        // convenience methods to apply aggregate operations across all meshes
        scriptable::ScriptableModelPointer scaleToFit(float unitScale) {
            return invokeAll(&ScriptableMesh::scaleToFit, unitScale);
        }
        scriptable::ScriptableModelPointer recenter(const glm::vec3& origin = glm::vec3(NAN)) {
            return invokeAll(&ScriptableMesh::recenter, origin);
        }
        scriptable::ScriptableModelPointer translate(const glm::vec3& translation) {
            return invokeAll(&ScriptableMesh::translate, translation);
        }
        scriptable::ScriptableModelPointer scale(const glm::vec3& scaleFactor, const glm::vec3& origin = glm::vec3(NAN)) {
            return invokeAll(&ScriptableMesh::scale, scaleFactor, origin);
        }
        scriptable::ScriptableModelPointer rotateVec3Degrees(const glm::vec3& eulerAngles, const glm::vec3& origin = glm::vec3(NAN)) {
            return invokeAll(&ScriptableMesh::rotateVec3Degrees, eulerAngles, origin);
        }
        scriptable::ScriptableModelPointer rotateDegrees(float x, float y, float z, const glm::vec3& origin = glm::vec3(NAN)) {
            return invokeAll(&ScriptableMesh::rotateDegrees, x, y, z, origin);
        }
        scriptable::ScriptableModelPointer rotate(const glm::quat& rotation, const glm::vec3& origin = glm::vec3(NAN)) {
            return invokeAll(&ScriptableMesh::rotate, rotation, origin);
        }
        scriptable::ScriptableModelPointer transform(const glm::mat4& transform) {
            return invokeAll(&ScriptableMesh::transform, transform);
        }
        glm::uint32 updateVertexAttributes(QScriptValue callback) {
            return countAll(&ScriptableMesh::updateVertexAttributes, callback);
        }
        glm::uint32 forEachVertex(QScriptValue callback) {
            return countAll(&ScriptableMesh::forEachVertex, callback);
        }
        QString toOBJ();

    protected:
        glm::uint32 getNumMeshes() { if (auto self = getNativeObject()) { return (glm::uint32)self->meshes.size(); } return 0; }
        QUuid getObjectID() const { return getNativeObject() ? getNativeObject()->objectID : QUuid(); }
        Extents getModelExtents() const;
        scriptable::ScriptableModelPointer _nativeObject;
        scriptable::ScriptableModelPointer getNativeObject() const { return _nativeObject ? _nativeObject : qscriptvalue_cast<scriptable::ScriptableModelPointer>(thisObject()); }
        bool isValid() const { return (bool)getNativeObject(); }
    };

}

Q_DECLARE_METATYPE(scriptable::ScriptableModelPointer)
Q_DECLARE_METATYPE(QVector<scriptable::ScriptableModelPointer>)
Q_DECLARE_METATYPE(scriptable::ScriptableMaterial)
Q_DECLARE_METATYPE(scriptable::ScriptableMaterialLayer)
Q_DECLARE_METATYPE(scriptable::MultiMaterialMap)
