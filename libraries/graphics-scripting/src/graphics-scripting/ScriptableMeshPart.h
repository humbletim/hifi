//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#include "ScriptableMesh.h"
#include <limits>

namespace js { namespace Graphics {
    /**jsdoc
     * @typedef {object} Graphics.MeshPart
     * @property {boolean} valid
     * @property {number} partIndex - The part index (within the containing Mesh).
     * @property {number} firstVertexIndex
     * @property {number} baseVertexIndex
     * @property {number} lastVertexIndex
     * @property {Graphics.Topology} topology - element interpretation (currently only 'triangles' is supported).
     * @property {number} numIndices - Number of vertex indices that this mesh part refers to.
     * @property {number} numVerticesPerFace - Number of vertices per face (eg: 3 when topology is 'triangles').
     * @property {number} numFaces - Number of faces represented by the mesh part (numIndices / numVerticesPerFace).
     * @property {object} extents
     */

    class MeshPartPrototype : public QObject, public QScriptable {
        Q_OBJECT
        Q_PROPERTY(bool valid READ isValid)
        Q_PROPERTY(glm::uint32 partIndex MEMBER partIndex CONSTANT)
        Q_PROPERTY(glm::uint32 numIndices READ getNumIndices WRITE setNumIndices)
        Q_PROPERTY(int numVerticesPerFace READ getTopologyLength)
        Q_PROPERTY(glm::uint32 numFaces READ getNumFaces)

        // NOTE: making read-only for now (see also GraphicsScriptingInterface::newMesh and MeshPartPayload::drawCall)
        Q_PROPERTY(graphics::Mesh::Topology topology READ getTopology)

        Q_PROPERTY(Extents extents READ getPartExtents)

        // WIP
        Q_PROPERTY(glm::uint32 firstVertexIndex READ getFirstVertexIndex WRITE setFirstVertexIndex)
        Q_PROPERTY(glm::uint32 baseVertexIndex READ getBaseVertexIndex WRITE setBaseVertexIndex)
        Q_PROPERTY(glm::uint32 lastVertexIndex READ getLastVertexIndex WRITE setLastVertexIndex)

    public:
        MeshPartPrototype(js::Graphics::MeshPartPointer meshPartBase) : QObject(), QScriptable(), _nativeObject(meshPartBase) {}
        QSharedPointer<MeshPrototype> getParentMeshProxy() const {
            return QSharedPointer<MeshPrototype>::create(getNativeObject() ? getNativeObject()->parentMesh : nullptr);
        }
        js::Graphics::MeshPartPointer getNativeObject() const {
            return _nativeObject ? _nativeObject : qscriptvalue_cast<js::Graphics::MeshPartPointer>(thisObject());
        }
        bool isValid() const { auto mesh = getMeshPointer(); return mesh && partIndex < mesh->getNumParts(); }

        static auto constexpr BIGGEST_INDEX_POSSIBLE = std::numeric_limits<glm::uint32>::max();
    public slots:
        std::vector<glm::uint32> getIndices( glm::uint32 pos = 0, glm::uint32 len = BIGGEST_INDEX_POSSIBLE) const;
        bool setIndices(const std::vector<glm::uint32>& indices);

        std::vector<glm::uint32> getFace(glm::uint32 faceIndex) const;

        bool replaceMeshPartData(js::Graphics::MeshPartPointer source, const std::vector<std::string>& attributeNames = std::vector<std::string>());
        js::Graphics::MeshPartPointer cloneMeshPart();

        QString toOBJ();

        bool isValidIndex(glm::uint32 vertexIndex, const QString& attributeName = QString()) const;
        QString toString() const;
    public:
        glm::uint32 partIndex{ 0 };

    protected:
        const graphics::Mesh::Part& getMeshPart() const;
        graphics::MeshPointer getMeshPointer() const { return getParentMeshProxy() ? getParentMeshProxy()->getMeshPointer() : nullptr; }

        bool setTopology(graphics::Mesh::Topology topology);
        graphics::Mesh::Topology getTopology() const { return isValid() ? getMeshPart()._topology : graphics::Mesh::Topology(); }
        glm::uint32 getTopologyLength() const;
        glm::uint32 getNumIndices() const { return isValid() ? getMeshPart()._numIndices : 0; }
        bool setNumIndices(glm::uint32 numIndices) { return setLastVertexIndex(getFirstVertexIndex() + numIndices); }

        bool setFirstVertexIndex(glm::uint32 vertexIndex);
        glm::uint32 getFirstVertexIndex() const { return isValid() ? getMeshPart()._startIndex : 0; }
        bool setLastVertexIndex(glm::uint32 vertexIndex);
        glm::uint32 getLastVertexIndex() const { return isValid() ? getFirstVertexIndex() + getNumIndices() - 1 : 0; }
        bool setBaseVertexIndex(glm::uint32 vertexIndex);
        glm::uint32 getBaseVertexIndex() const { return isValid() ? getMeshPart()._baseVertex : 0; }

        glm::uint32 getNumFaces() const { return getNumIndices() / getTopologyLength(); }
        Extents getPartExtents() const;

        js::Graphics::MeshPartPointer _nativeObject;
    };
}}

Q_DECLARE_METATYPE(js::Graphics::MeshPartPointer)
Q_DECLARE_METATYPE(std::vector<js::Graphics::MeshPartPointer>)
