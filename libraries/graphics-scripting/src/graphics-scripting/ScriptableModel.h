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

namespace js { namespace Graphics {
    /**jsdoc
     * @typedef {object} Graphics.Model
     * @property {Uuid} objectID - UUID of corresponding inworld object (if model is associated)
     * @property {number} numMeshes - The number of submeshes contained in the model.
     * @property {Graphics.Mesh[]} meshes - Array of submesh references.
     * @property {Object.<string,Graphics.MaterialLayer[]>} materialLayers - Map of materials layer lists.  You can look up a material layer list by mesh part number or by material name.
     * @property {string[]} materialNames - Array of all the material names used by the mesh parts of this model, in order (e.g. materialNames[0] is the name of the first mesh part's material).
     */

    class ModelPrototype : public QObject, public QScriptable {
        Q_OBJECT
        Q_PROPERTY(QUuid objectID READ getObjectID)
        Q_PROPERTY(glm::uint32 numMeshes READ getNumMeshes)
        Q_PROPERTY(js::Graphics::Meshes meshes READ getMeshes)
        Q_PROPERTY(bool valid READ isValid)
        Q_PROPERTY(js::Graphics::MultiMaterialMap materialLayers READ getMaterialLayers)
        Q_PROPERTY(QVector<QString> materialNames READ getMaterialNames)
        Q_PROPERTY(Extents extents READ getModelExtents)

    public:
        ModelPrototype(const js::Graphics::ModelPointer& nativeModelBase) : QObject(), QScriptable(), _nativeObject(nativeModelBase) {}
        js::Graphics::Meshes getMeshes();
        const js::Graphics::Meshes getConstMeshes() const;

        js::Graphics::MultiMaterialMap getMaterialLayers() { return isValid() ? getNativeObject()->materialLayers : js::Graphics::MultiMaterialMap(); }
        QVector<QString> getMaterialNames() { return isValid() ? getNativeObject()->materialNames : QVector<QString>(); }

        // invoke a MeshPrototype method across all meshes
        template<typename M, typename ...Args>
        js::Graphics::ModelPointer invokeAll(M method, Args &&...args) {
            auto self = getNativeObject();
            if (self) {
                auto engine = this->engine();
                auto meshes = self->meshes;
                for (auto& mesh : self->meshes) {
                    js::Graphics::MeshPrototype wrapped{ mesh };
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
                    js::Graphics::MeshPrototype wrapped{ mesh };
                    result += (wrapped.*method)(std::forward<Args>(args)...);
                    if (engine && engine->hasUncaughtException()) {
                        break;
                    }
                }
            }
            return result;
        }

    public slots:
        js::Graphics::ModelPointer cloneModel(const QVariantMap& options = QVariantMap());
        QString toString() const;

        // convenience methods to apply aggregate operations across all meshes
        js::Graphics::ModelPointer scaleToFit(float unitScale) {
            return invokeAll(&MeshPrototype::scaleToFit, unitScale);
        }
        js::Graphics::ModelPointer recenter(const glm::vec3& origin = glm::vec3(NAN)) {
            return invokeAll(&MeshPrototype::recenter, origin);
        }
        js::Graphics::ModelPointer translate(const glm::vec3& translation) {
            return invokeAll(&MeshPrototype::translate, translation);
        }
        js::Graphics::ModelPointer scale(const glm::vec3& scale, const glm::vec3& origin = glm::vec3(NAN)) {
            return invokeAll(&MeshPrototype::scale, scale, origin);
        }
        js::Graphics::ModelPointer rotateVec3Degrees(const glm::vec3& eulerAngles, const glm::vec3& origin = glm::vec3(NAN)) {
            return invokeAll(&MeshPrototype::rotateVec3Degrees, eulerAngles, origin);
        }
        js::Graphics::ModelPointer rotateDegrees(float x, float y, float z, const glm::vec3& origin = glm::vec3(NAN)) {
            return invokeAll(&MeshPrototype::rotateDegrees, x, y, z, origin);
        }
        js::Graphics::ModelPointer rotate(const glm::quat& rotation, const glm::vec3& origin = glm::vec3(NAN)) {
            return invokeAll(&MeshPrototype::rotate, rotation, origin);
        }
        js::Graphics::ModelPointer transform(const glm::mat4& transform) {
            return invokeAll(&MeshPrototype::transform, transform);
        }
        glm::uint32 updateVertexAttributes(QScriptValue callback) {
            return countAll(&MeshPrototype::updateVertexAttributes, callback);
        }
        glm::uint32 forEachVertex(QScriptValue callback) {
            return countAll(&MeshPrototype::forEachVertex, callback);
        }

    protected:
        glm::uint32 getNumMeshes() { if (auto self = getNativeObject()) { return (glm::uint32)self->meshes.size(); } return 0; }
        QUuid getObjectID() const { return getNativeObject() ? getNativeObject()->objectID : QUuid(); }
        Extents getModelExtents() const;
        js::Graphics::ModelPointer _nativeObject;
        js::Graphics::ModelPointer getNativeObject() const { return _nativeObject ? _nativeObject : qscriptvalue_cast<js::Graphics::ModelPointer>(thisObject()); }
        bool isValid() const { return (bool)getNativeObject(); }
    };

}}

Q_DECLARE_METATYPE(js::Graphics::ModelPointer)
Q_DECLARE_METATYPE(std::vector<js::Graphics::ModelPointer>)
Q_DECLARE_METATYPE(js::Graphics::Material)
Q_DECLARE_METATYPE(js::Graphics::MaterialLayer)
Q_DECLARE_METATYPE(js::Graphics::MultiMaterialMap)
