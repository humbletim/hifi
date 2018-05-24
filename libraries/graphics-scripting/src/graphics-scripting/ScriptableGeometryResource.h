//
//  ScriptableGeometryResource.h
//  libraries/graphics-scripting/src/graphics-scripting/
//
//  Created by Timothy Dedischew on 04/07/2018.
//  Copyright (c) 2018 High Fidelity, Inc. All rights reserved.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ScriptableGeometryResource_h
#define hifi_ScriptableGeometryResource_h

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtScript/QScriptable>
#include "Forward.h"
#include "GraphicsScriptingUtil.h"
#include <model-networking/ModelCache.h>

/// Scriptable wrapper for working with ModelCache.getModel GeometryResource pointers from JS.
class ScriptableGeometryResource : public QObject, public QScriptable {
    Q_OBJECT
    Q_PROPERTY(QUrl url READ getURL)
    Q_PROPERTY(QString stateName READ getStateName)
    Q_PROPERTY(int numMeshes READ getNumMeshes)

public:
    static void registerMetaTypes(QScriptEngine* engine);
    ScriptableGeometryResource() : QObject(), QScriptable() {}

public slots:
    QString toString() const;
    QString getStateName() const;
    int getNumMeshes() const;
    QUrl getURL() const;
    js::Graphics::Meshes getNativeMeshes() const;
    js::Graphics::ModelPointer getNativeModel() const;
    QScriptValue getMeshes() const;
    QScriptValue toModel() const;

protected:
    GeometryResource::Pointer safeNativeObject(bool verify = true) const;
    js::Graphics::ModelPointer getScriptableModel() const;
};

#endif // hifi_ScriptableGeometryResource_h
