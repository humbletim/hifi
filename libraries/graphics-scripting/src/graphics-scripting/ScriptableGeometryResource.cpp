//
//  ScriptableGeometryResource.cpp
//  libraries/model-networking/src/
//
//  Created by Timothy Dedischew on 04/07/2018.
//  Copyright (c) 2018 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <QScriptEngine>

#include "ScriptableGeometryResource.h"
#include "ScriptableModel.h"
#include "ScriptableMesh.h"

namespace {
    int metaType = qRegisterMetaType<GeometryResource::Pointer>("GeometryResource::Pointer");
}

void ScriptableGeometryResource::registerMetaTypes(QScriptEngine* engine) {
    scriptable::registerPrototype<GeometryResource::Pointer>(engine, new ScriptableGeometryResource());
    Q_UNUSED(metaType);
}

QString ScriptableGeometryResource::getStateName() const {
    if (auto self = safeNativeObject()) {
        return self->isGeometryLoaded() ? "ready" :
            self->isLoaded() ? "loaded" : self->isFailed() ? "failed" : "pending";
    }
    return "unknown";
}

int ScriptableGeometryResource::getNumMeshes() const {
    if (auto self = safeNativeObject()) {
        if (self->isGeometryLoaded()) {
            return (int)self->getMeshes().size();
        }
    }
    return 0;
}

QString ScriptableGeometryResource::toString() const {
    return QString("[ModelResource state=%1 meshes=%2 url=%3]")
        .arg(getStateName())
        .arg(getNumMeshes())
        .arg(getURL().toString());
}

QSharedPointer<GeometryResource> ScriptableGeometryResource::safeNativeObject(bool verify) const {
    if (GeometryResource::Pointer result = qscriptvalue_cast<GeometryResource::Pointer>(thisObject())) {
        return result;
    }
    if (verify) {
        auto error = "invalid GeometryResource::Pointer";
        if (context()) {
            context()->throwError(error);
        } else {
            qCWarning(graphics_scripting) << error;
        }
    }
    return nullptr;
}

QUrl ScriptableGeometryResource::getURL() const {
    if (auto self = safeNativeObject()) {
        return self->getURL();
    }
    return QUrl();
}

js::Graphics::ModelPointer ScriptableGeometryResource::getScriptableModel() const {
    auto jsThrowError = [&](const QString& error) {
        if (context()) {
            context()->throwError(error);
        } else {
            qCWarning(graphics_scripting) << "toModel error (without valid JS context):" << error;
        }
    };

    auto modelWrapper = js::Graphics::ModelPointer::create();
    modelWrapper->setObjectName("resource::" + getURL().toString());

    auto self = safeNativeObject(false);
    if (!self || !self->isGeometryLoaded()) {
        jsThrowError(QString("!isGeometryLoaded (state=%1)").arg(getStateName()));
        return modelWrapper;
    }

    auto geometry = self->getFBXGeometry();

    if (geometry.meshes.empty()) {
        jsThrowError("no meshes available");
    } else {
        int i = 0;
        for (const auto& mesh : geometry.meshes) {
            if (mesh._mesh) {
                modelWrapper->append(mesh._mesh);
            } else {
                jsThrowError(QString("invalid mesh at index: %1").arg(i));
                break;
            }
            i++;
        }
    }
    return modelWrapper;
}

QScriptValue ScriptableGeometryResource::getMeshes() const {
    return engine()->toScriptValue(getNativeMeshes());
}
QScriptValue ScriptableGeometryResource::toModel() const {
    return engine()->toScriptValue(getNativeModel());
}

js::Graphics::Meshes ScriptableGeometryResource::getNativeMeshes() const {
    return getScriptableModel()->meshes;
}

js::Graphics::ModelPointer ScriptableGeometryResource::getNativeModel() const {
    return getScriptableModel();
}
