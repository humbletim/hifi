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

#include <QUrl>
#include <QtCore/QObject>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptable>

#include "Forward.h"
#include "ScriptableMesh.h"
#include "ScriptableModel.h"
#include <DependencyManager.h>

namespace scriptable {
    struct JSVectorAdapter;
}

/**jsdoc
 * The Graphics API <em>(experimental)</em> lets you query and manage certain graphics-related structures (like underlying meshes and textures) from scripting.
 * @namespace Graphics
 *
 * @hifi-interface
 * @hifi-client-entity
 */

class GraphicsScriptingInterface : public QObject, public QScriptable, public Dependency {
    Q_OBJECT

    Q_PROPERTY(QScriptValue RenderFlag READ getRenderFlagEnum)
public:
    static void registerMetaTypes(QScriptEngine* engine);
    GraphicsScriptingInterface(QObject* parent = nullptr);
    js::Graphics::MeshPointer newMesh(scriptable::JSVectorAdapter adapter);

public slots:
    /**jsdoc
     * Returns a model reference object associated with the specified UUID ({@link EntityID}, {@link OverlayID}, or {@link AvatarID}).
     *
     * @function Graphics.getModel
     * @param {UUID} entityID - The objectID of the model whose meshes are to be retrieved.
     * @return {Graphics.Model} the resulting Model object
     */
    js::Graphics::ModelPointer getModel(QUuid uuid);

    /**jsdoc
     * Assign a local Render Plugin for the specified UUID.
     *
     * @function Graphics.setRenderPlugin
     * @param {UUID} objectID - The objectID to assign an object proxy to
     * @param {URL} pluginURI - URI (or null to reset current plugin)
     * @return {bool} - true if proxy was successfully assigned
     */
    bool setRenderPlugin(QUuid uuid, std::string pluginURI, QVariantMap parameters = QVariantMap());
    std::string getRenderPlugin(QUuid uuid) const;
    std::vector<std::string> getAvailableRenderPlugins() const;
    std::vector<QUuid> getRenderPluginObjectIDs(std::string pluginURI = "") const;

    /**jsdoc
     * Specify advanced rendering flags for the specified UUID (currently only applies to {@link Shape} Plugins).
     *
     * @function Graphics.overrideRenderFlags
     * @param {UUID} The objectID to be affected.
     * @param {Graphics.RenderFlags} flagsToSet OR'd flags to enable
     * @param {Graphics.RenderFlags} [flagsToClear=0] OR'd flags to disable
     * Example: To enable wireframe display: `Graphics.overrideRenderFlags(uuid, Graphics.RenderFlags.WIREFRAME, 0);`
     */
    /**jsdoc
     * @namespace Graphics.RenderFlags
     * @property NONE {number} invalid flag value (0)
     * @property WIREFRAME {number} render in wireframe mode
     * @property DIRTY {number} indicate underlying object should be refreshed
    */
    bool overrideRenderFlags(QUuid uuid, js::Graphics::RenderFlags flagsToSet, js::Graphics::RenderFlags flagsToClear = js::Graphics::RenderFlag::NONE);

    /**jsdoc
     * Temporarily use an alternative Model object for the specified UUID.
     * The alternative Model is applied to both rendering and interactive ray intersection (ie: picking) tests.
     * For Entities, changes to certain properties (shapeType, modelURL, dimensions, etc.) will revert to the original model.
     *
     * @function Graphics.updateModel
     * @param {UUID} id The objectID of the model whose meshes are to be retrieved.
     * @param {Graphics.Model} The alternative Model object to use for rendering and ray picking.
     * @param {number} [meshIndex=-1] submesh index to update
     * @param {number} [partIndex=-1] submesh part number to update
     * @return {boolean}
     */
    bool updateModel(QUuid uuid, const js::Graphics::ModelPointer& model, int meshIndex = -1, int partIndex = -1);

    /**jsdoc
     * Check whether the given objectID supports scripted model updates.
     * @function Graphics.canUpdateModel
     * @param {Uuid} id
     * @param {number} [meshIndex=-1]
     * @param {number} [partIndex=-1]
     * @returns {boolean}
     */
    bool canUpdateModel(QUuid uuid, int meshIndex = -1, int partIndex = -1);

    /**jsdoc
     * @function Graphics.newModel
     * @param {Graphics.Mesh[]} meshes
     * @returns {Graphics.Model}
     */
    js::Graphics::ModelPointer newModel(const js::Graphics::Meshes& meshes);

    /**jsdoc
     * Create a new Mesh using the specified data buffers.
     *
     * @function Graphics.newMesh
     * @param {Graphics.IFSData} ifsMeshData Index-Faced Set (IFS) arrays used to create the new mesh.
     * @return {Graphics.Mesh} the resulting Mesh object
     */
    /**jsdoc
    * @typedef {object} Graphics.IFSData
    * @property {string} [name] - mesh name (useful for debugging / debug prints).
    * @property {number[]} indices - vertex indices to use for the mesh faces.
    * @property {Vec3[]} vertices - vertex positions (model space)
    * @property {Vec3[]} [normals] - vertex normals (normalized)
    * @property {Vec3[]} [colors] - vertex colors (normalized)
    * @property {Vec2[]} [texCoords0] - vertex texture coordinates (normalized)
    *
    * Advanced:
    * <p>Mesh input arrays can also be specified as native JS TypedArrays
    * (eg: Uint32Array for indices, Float32Array for positions, etc.)<p>
    * <p>To simplify using output from other JS libraries the following aliases are also supported:
    *   <li>indices :: indices | indexes | index</li>
    *   <li>positions ::  positions | position | vertices | vertexPositions</li>
    *   <li>normals :: normals | normal | vertexNormals</li>
    *   <li>colors :: colors | color | vertexColors</li>
    *   <li>texCoords0 :: texCoords0 | texCoord0 | uvs | uv | vertexUVs | texcoord | texCoord | vertexTextureCoords</li>
    * </p>
    */
    js::Graphics::MeshPointer newMesh(QScriptValue ifsMeshData);
    js::Graphics::MeshPointer newMeshFromVariant(QVariantMap ifsMeshData);

    /**jsdoc
     * Returns a mesh object representing the specified primitive {@link Shape}.
     *
     * @function Graphics.getMeshForShape
     * @param {Shape} The shape name
     * @param [color=Vec3.ONE] {Vec3} Default color to apply across shape vertices
     * @return {Graphics.Mesh} the resulting Mesh object
     */
    js::Graphics::MeshPointer getMeshForShape(const QString& shapeName, const glm::vec3& color = glm::vec3(1.0f));

    /**jsdoc
     * @function Graphics.exportModelToOBJ
     * @param {Graphics.Model} model
     * @returns {string}
     */

    QString exportModelToOBJ(const js::Graphics::ModelPointer& in, bool dedupeVertices = false);

    static js::Graphics::MeshPointer cloneMesh(const js::Graphics::MeshPointer& input);
    static js::Graphics::MeshPointer dedupeVertices(const js::Graphics::MeshPointer& input, float epsilon = DEDUPE_EPSILON, bool resetNormals = false);

private:
    static constexpr float DEDUPE_EPSILON = 1e-6f;
    js::Graphics::ModelProviderPointer getModelProvider(QUuid uuid);
    void jsThrowError(const QString& error);
    graphics::MeshPointer getNativeMesh(js::Graphics::MeshPointer meshProxy);
    QScriptValue getRenderFlagEnum() const;
};

Q_DECLARE_METATYPE(NestableType)

#endif // hifi_GraphicsScriptingInterface_h
