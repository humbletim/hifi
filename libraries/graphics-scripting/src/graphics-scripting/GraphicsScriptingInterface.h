//
//  GraphicsScriptingInterface.h
//  libraries/graphics-scripting/src
//
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_GraphicsScriptingInterface_h
#define hifi_GraphicsScriptingInterface_h

#include <QtCore/QObject>
#include <QUrl>

#include <QtScript/QScriptEngine>
#include <QtScript/QScriptable>

#include "Forward.h"
#include "ScriptableMesh.h"
#include "ScriptableModel.h"
#include <DependencyManager.h>
#include <shared/Scriptable.h>

namespace scriptable {
    struct JSVectorAdapter;
}

/**jsdoc
 * The experimental Graphics API <em>(experimental)</em> lets you query and manage certain graphics-related structures (like underlying meshes and textures) from scripting.
 * @namespace Graphics
 *
 * @hifi-interface
 * @hifi-client-entity
 */

class GraphicsScriptingInterface : public QObject, public QScriptable, public Dependency {
    Q_OBJECT

public:
    static void registerMetaTypes(QScriptEngine* engine);
    GraphicsScriptingInterface(QObject* parent = nullptr);
    scriptable::ScriptableMeshPointer newMesh(scriptable::JSVectorAdapter adapter);

public slots:
    /**jsdoc
     * Returns a model reference object associated with the specified UUID ({@link EntityID}, {@link OverlayID}, or {@link AvatarID}).
     *
     * @function Graphics.getModel
     * @param {UUID} entityID - The objectID of the model whose meshes are to be retrieved.
     * @returns {Graphics.Model} the resulting Model object
     */
    scriptable::ScriptableModelPointer getModel(QUuid uuid);

    /**jsdoc
     * @function Graphics.updateModel
     * @param {Uuid} id
     * @param {Graphics.Model} model
     * @returns {boolean}
     */
    bool updateModel(QUuid uuid, const scriptable::ScriptableModelPointer& model);

    /**jsdoc
     * @function Graphics.canUpdateModel
     * @param {Uuid} id
     * @param {number} [meshIndex=-1]
     * @param {number} [partNumber=-1]
     * @returns {boolean}
     */
    bool canUpdateModel(QUuid uuid, int meshIndex = -1, int partNumber = -1);

    /**jsdoc
     * @function Graphics.newModel
     * @param {Graphics.Mesh[]} meshes
     * @returns {Graphics.Model}
     */
    scriptable::ScriptableModelPointer newModel(const scriptable::ScriptableMeshes& meshes);

    /**jsdoc
     * Create a new Mesh / Mesh Part with the specified data buffers.
     *
     * @function Graphics.newMesh
     * @param {Graphics.IFSData} ifsMeshData Index-Faced Set (IFS) arrays used to create the new mesh.
     * @returns {Graphics.Mesh} the resulting Mesh / Mesh Part object
     */
    scriptable::ScriptableMeshPointer newMesh(QScriptValue ifsMeshData);
    scriptable::ScriptableMeshPointer newMeshFromVariant(QVariantMap ifsMeshData);

    /**jsdoc
     * @function Graphics.exportModelToOBJ
     * @param {Graphics.Model} model
     * @returns {string}
     */
    static QString exportModelToOBJ(const scriptable::ScriptableModelPointer& in);

    static QString pluralizeAttributeName(const QString& attributeName) { return scriptable::JSVectorAdapter::pluralizeAttributeName(attributeName); }
    static QString singularizeAttributeName(const QString& attributeName) { return scriptable::JSVectorAdapter::singularizeAttributeName(attributeName); }

private:
    scriptable::ModelProviderPointer getModelProvider(QUuid uuid);
    void jsThrowError(const QString& error) { scriptable::jsThrowError(engine(), error); }
};

Q_DECLARE_METATYPE(NestableType)

#endif // hifi_GraphicsScriptingInterface_h
