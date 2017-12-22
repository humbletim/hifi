#pragma once

#include "SculptEngine.h"
#include <glm/glm.hpp>

// This is a thin testing implementation of the ISculptEngine interface.
// Currently the Leopoly SDK only supports Windows, so this version is also used as the
// default backend for OS X / Linux builds.
// See: SculptEngineDebug class which provides a higher-level mock implementation based on mesh::MeshProxyPointer

class SculptEngineStub : public ISculptEngine {
public:
    virtual bool importMesh(
        int numVertices,
        const float* vertices,
        const unsigned char* vertexColors,
        int numTriangles,
        const int* indices,
        int packetTriLimit) override {
        return false;
    }

    virtual bool exportMesh(
        int* numVertices,
        float* vertices,
        float* normals,
        unsigned char* colors,
        int* numTriangles,
        int* indices) const override {
        return false;
    }

    // tool actions
    virtual bool setTool(Tool tool) override {
        _tool = tool;
        return true;
    }
    virtual void setCursorRaycastMode(CursorRaycastMode mode) override {
        _mode = mode;
    }
    virtual void setRadius(double radius) override {
        _radius = radius;
    }
    virtual void setStrength(double strength) override {
        _strength = strength;
    }
    virtual void setPaintColor(double red, double green, double blue) override {
        _color = { red, green, blue };
    }

    virtual void enableMirror(bool enableX, bool enableY, bool enableZ) override {
        _enableMirror = { enableX, enableY, enableZ };
    }
    virtual void setCursorPose(double pos[3], double rot[4]) override {
        _cursorPosition = glm::make_vec3(pos);
        _cursorRotation = glm::make_quat(rot);
    }

    virtual bool startAction() override {
        _currentAction = _tool;
        return (int)_currentAction >= 0;
    }
    virtual void update() override {
        return;
    }
    virtual void stopAction() override {
        return;
    }

    virtual bool undo() override {
        return false;
    }
    virtual bool redo() override {
        return false;
    }

    // for derived action pose info retrieval
    virtual int getNumDerivedActionPoses() const override {
        return 1;
    }
    virtual void getDerivedActionPose(int index, double pos[3], double rot[4]) const override {
        memset(pos, 0, sizeof(*pos));
        memset(rot, 0, sizeof(*rot));
        rot[3] = 1.0;
    }

    QMap<int,GeometryPacketData> packets;
    QMap<int,bool> changedPackets;
    // for rendering/updating mesh
    virtual int getNumGeometryPackets() const override {
        return packets.size();
    }
    virtual bool isGeometryPacketChanged(int index) const override {
        return changedPackets.value(index, false);
    }
    virtual GeometryPacketData getGeometryPacketData(int index) const override {
        return packets.value(index);
    }
    virtual void clearGeometryPacketChangeFlags() override {
        changedPackets.clear();
    }

    // for multiuser
    virtual int getClientId() const override {
        return _clientId;
    }
    virtual void setClientId(int clientId) override {
        _clientId = clientId;
    }
    virtual int packFrameData(void* buffer, int bufferSize) const override {
        memset(buffer, 0, bufferSize);
        if ((size_t)bufferSize >= strlen("testing")+1) {
            strcpy((char *)buffer, "testing");
            return strlen((char*)buffer)+1;
        }
        return bufferSize;
    }
    virtual bool applyFrameData(const void* data, int dataSize) override {
        char tmp[1024] = {0};
        strncpy(tmp, (char *)data, glm::min((size_t)dataSize, sizeof(tmp)));
        qCDebug(leopoly_log) << "SculptEngineStub::applyFrameData" << tmp;
        return true;
    }

public:
    int _clientId;
    Tool _tool;
    Tool _currentAction;
    CursorRaycastMode _mode;
    double _radius;
    double _strength;
    glm::dvec3 _color;
    glm::bvec3 _enableMirror;
    glm::dvec3 _cursorPosition;
    glm::dquat _cursorRotation;
};

