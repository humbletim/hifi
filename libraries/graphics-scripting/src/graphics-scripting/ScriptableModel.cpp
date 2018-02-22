//
//  ScriptableModel.cpp
//  libraries/graphics-scripting
//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GraphicsScriptingUtil.h"
#include "ScriptableModel.h"
#include "ScriptableMesh.h"

#include <QtScript/QScriptEngine>
//#include "ScriptableModel.moc"

void scriptable::ScriptableModelBase::mixin(const QVariantMap& modelMetaData) {
    for (const auto& key : modelMetaData.keys()) {
        const auto& value = modelMetaData[key];
        if (metadata.contains(key) && metadata[key].type() == (QVariant::Type)QMetaType::QVariantList) {
            qCDebug(graphics_scripting) << "CONCATENATING" << key << metadata[key].toList().size() << "+" << value.toList().size();
            metadata[key] = metadata[key].toList() + value.toList();
        } else {
            metadata[key] = modelMetaData[key];
        }
    }
}

scriptable::ScriptableModelBase::~ScriptableModelBase() {
#ifdef SCRIPTABLE_MESH_DEBUG
    qCDebug(graphics_scripting) << "~ScriptableModelBase" << this;
#endif
    for (auto& m : meshes) {
        m.ownedMesh.reset();
        //qCDebug(graphics_scripting) << "~~~~ScriptableModelBase" << &m << m.mesh.use_count();
    }
    meshes.clear();
    //qCDebug(graphics_scripting) << "//~ScriptableModelBase" << this;
}

void scriptable::ScriptableModelBase::append(scriptable::WeakMeshPointer mesh, const QVariantMap& metadata) {
    //qCDebug(graphics_scripting) << "+ APPEND WeakMeshPointer" << mesh.lock().get();
    meshes << ScriptableMeshBase{ provider, this, mesh, metadata };
}

void scriptable::ScriptableModelBase::append(const ScriptableMeshBase& mesh, const QVariantMap& modelMetaData) {
    //qCDebug(graphics_scripting) << "+ APPEND ScriptableMeshBase" << &mesh;
    if (mesh.provider.lock().get() != provider.lock().get()) {
        qCDebug(graphics_scripting) << "warning: appending mesh from different provider..." << mesh.provider.lock().get() << " != " << provider.lock().get();
    }
    //if (mesh.model && mesh.model != this) {
    //    qCDebug(graphics_scripting) << "warning: appending mesh from different model..." << mesh.model << " != " << this;
    //}                    
    meshes << mesh;
    mixin(modelMetaData);
}

void scriptable::ScriptableModelBase::append(const ScriptableModelBase& other, const QVariantMap& modelMetaData) {
    //qCDebug(graphics_scripting) << "+ APPEND ScriptableModelBase" << &other;
    for (const auto& mesh : other.meshes) { append(mesh); }
    mixin(other.metadata);
    mixin(modelMetaData);
}


QString scriptable::ScriptableModel::toString() const {
    return QString("[ScriptableModel%1%2]")
        .arg(objectID.isNull() ? "" : " objectID="+objectID.toString())
        .arg(objectName().isEmpty() ? "" : " name=" +objectName());
}

scriptable::ScriptableModelPointer scriptable::ScriptableModel::cloneModel(const QVariantMap& options) {
    scriptable::ScriptableModelPointer clone = scriptable::ScriptableModelPointer(new scriptable::ScriptableModel(*this));
    qCDebug(graphics_scripting) << "clone->getNumMeshes" << clone->getNumMeshes();
    clone->meshes.clear();
    qCDebug(graphics_scripting) << "..clone->getNumMeshes" << clone->getNumMeshes();
    for (const auto &mesh : getConstMeshes()) {
        auto cloned = mesh->cloneMesh(options.value("recalculateNormals").toBool());
        if (auto tmp = qobject_cast<scriptable::ScriptableMeshBase*>(cloned)) {
            qCDebug(graphics_scripting) << "++ APPEND"  << tmp << tmp->ownedMesh.use_count() << tmp->metadata.value("__ownership__") << tmp->metadata.value("__native__");
            clone->meshes << *tmp;
            tmp->deleteLater();
            qCDebug(graphics_scripting) << "//++ APPEND"  << clone->meshes.constLast().ownedMesh.use_count();;
        } else {
            qCDebug(graphics_scripting) << "error cloning mesh" << cloned;
        }
    }
    qCDebug(graphics_scripting) << "//clone->getNumMeshes" << clone->getNumMeshes();
    return clone;
}


const QVector<scriptable::ScriptableMeshPointer> scriptable::ScriptableModel::getConstMeshes() const {
    QVector<scriptable::ScriptableMeshPointer> out;
    for(const auto& mesh : meshes) {
        const scriptable::ScriptableMesh* m = qobject_cast<const scriptable::ScriptableMesh*>(&mesh);
        if (!m) {
            m = scriptable::make_scriptowned<scriptable::ScriptableMesh>(mesh);
        } else {
            qCDebug(graphics_scripting) << "reusing scriptable mesh" << m;
        }
        const scriptable::ScriptableMeshPointer mp = scriptable::ScriptableMeshPointer(const_cast<scriptable::ScriptableMesh*>(m));
        out << mp;
    }
    return out;
}

QVector<scriptable::ScriptableMeshPointer> scriptable::ScriptableModel::getMeshes() {
    QVector<scriptable::ScriptableMeshPointer> out;
    for(auto& mesh : meshes) {
        scriptable::ScriptableMesh* m = qobject_cast<scriptable::ScriptableMesh*>(&mesh);
        if (!m) {
            m = scriptable::make_scriptowned<scriptable::ScriptableMesh>(mesh);
        } else {
            qCDebug(graphics_scripting) << "reusing scriptable mesh" << m;
        }
        scriptable::ScriptableMeshPointer mp = scriptable::ScriptableMeshPointer(m);
        out << mp;
    }
    return out;
}

quint32 scriptable::ScriptableModel::mapAttributeValues(QScriptValue callback) {
    quint32 result = 0;
    QVector<scriptable::ScriptableMeshPointer> in = getMeshes();
    if (in.size()) {
        foreach (scriptable::ScriptableMeshPointer meshProxy, in) {
            result += meshProxy->mapAttributeValues(callback);
        }
    }
    return result;
}

/*namespace {
    QScriptValue modelPointerToScriptValue(QScriptEngine* engine, scriptable::ScriptableModelPointer const &in) {
        return qObjectToScriptValue(engine, in);
    }
    void modelPointerFromScriptValue(const QScriptValue& value, scriptable::ScriptableModelPointer &out) {
        out = scriptable::qpointer_qobject_cast<scriptable::ScriptableModel>(value);
    }
}

namespace scriptable {
    bool registerMetaTypes(QScriptEngine* engine) {
        qScriptRegisterMetaType(engine, modelPointerToScriptValue, modelPointerFromScriptValue);
        return true;
    }
}
*/