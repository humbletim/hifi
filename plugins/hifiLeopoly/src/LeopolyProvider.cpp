//
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <mutex>

#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QLoggingCategory>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QThread>
#include <QtCore/QtPlugin>

#include <QJsonObject>

#include <QtScript/QScriptable>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>

#include <plugins/RuntimePlugin.h>
#include <plugins/InputPlugin.h>

#include <RegisteredMetaTypes.h>
#include <SharedUtil.h>
#include <model/Geometry.h>
#include <shared/MeshProxy.h>
#include <model-networking/SimpleMeshProxy.h>
#include <shared/ScriptInitializerMixin.h>
#include <shared/ReadWriteLockable.h>
#include <DependencyManager.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "LeopolyManager.h"

Q_LOGGING_CATEGORY(leopoly_log, ID)

#include "SculptEngine.h"
#include "SculptEngineStub.h"

#ifdef Q_OS_WIN
   #include "SculptLib.h"
#endif

class LeopolyManager;
class SculptEngineDebug;
namespace leopoly {
    using API = QSharedPointer<ISculptEngine>;
    using DEBUGAPI = QSharedPointer<SculptEngineDebug>;
    using PLUGIN = QSharedPointer<LeopolyManager>;
    const int INVALID_CLIENTID{ -1 };
    const auto& INVALID_TOOLNAME{ "INVALID" };
    const auto& INVALID_TOOL{ static_cast<ISculptEngine::Tool>(-1) };

    template <typename T, typename U = int> QVariantMap toEnumVariantMap(QMap<QString, T> input) {
        QVariantMap output;
        for(const auto& kv : input.toStdMap()) {
            output[kv.first] = (U)kv.second;
            output[kv.first.toUpper()] = (U)kv.second;
            output[QString("%1").arg((U)kv.second)] = kv.first;
        }
        return output;
    }
    const auto& Tool = toEnumVariantMap<ISculptEngine::Tool>({
            { INVALID_TOOLNAME, INVALID_TOOL },
            { "sculpt", ISculptEngine::Tool::SculptBrush },
            { "pull", ISculptEngine::Tool::PullBrush },
            { "push", ISculptEngine::Tool::PushBrush },
            { "smooth", ISculptEngine::Tool::SmoothBrush },
            { "edge", ISculptEngine::Tool::EdgeBrush },
            { "flatten", ISculptEngine::Tool::FlattenBrush },
            { "paint", ISculptEngine::Tool::PaintBrush },
        });
    const auto& CursorRaycastMode = toEnumVariantMap<ISculptEngine::CursorRaycastMode>({
            { "none", ISculptEngine::CursorRaycastMode::None },
            { "continuous", ISculptEngine::CursorRaycastMode::Continuous },
            { "until_action_start", ISculptEngine::CursorRaycastMode::UntilActionStart },
        });
    const auto& VertexAttribType = toEnumVariantMap<ISculptEngine::VertexAttribType>({
            { "float", ISculptEngine::VertexAttribType::Float },
            { "vec2", ISculptEngine::VertexAttribType::Float2 },
            { "vec3", ISculptEngine::VertexAttribType::Float3 },
            { "vec4", ISculptEngine::VertexAttribType::Float4 },
            { "u8vec4", ISculptEngine::VertexAttribType::R8G8B8A8_Unorm },
        });
    const auto& VertexAttribSizes = toEnumVariantMap<size_t>({
            { "float", sizeof(float) },
            { "vec2", sizeof(glm::vec2) },
            { "vec3", sizeof(glm::vec3) },
            { "vec4", sizeof(glm::vec4) },
            { "u8vec4", sizeof(uint8_t) * 4 },
        });
}

class SculptEngineDebug : public SculptEngineStub {
public:
    virtual bool importMesh(
        int numVertices,
        const float* vertices,
        const unsigned char* vertexColors,
        int numTriangles,
        const int* indices,
        int packetTriLimit) override {
        qCDebug(leopoly_log) << "importMesh" << numVertices << numTriangles << packetTriLimit;
        return SculptEngineStub::importMesh(numVertices, vertices, vertexColors, numTriangles, indices, packetTriLimit);
    }

    virtual bool exportMesh(
        int* numVertices,
        float* vertices,
        float* normals,
        unsigned char* colors,
        int* numTriangles,
        int* indices) const override {
        qCDebug(leopoly_log) << "exportMesh";
        return SculptEngineStub::exportMesh(numVertices, vertices, normals, colors, numTriangles, indices);
    }
    // debug methods
    int clearGeometryPackets() {
        auto beforeCount = packets.size();
        packets.clear();
        changedPackets.clear();
        return beforeCount;
    }
    int generateDebugPackets(int index, mesh::MeshProxyPointer meshProxy) {
        qCDebug(leopoly_log) << "generateDebugPackets" << index << meshProxy;
        if (auto mesh = meshProxy ? meshProxy->getMeshPointer() : nullptr) {
            auto positions = mesh->getVertexBuffer();
            auto indices = mesh->getIndexBuffer();
#ifdef __linux__
            packets[index] = {
                .numVerts = (int)positions.getNumElements(),
                .numInds = (int)indices.getNumElements(),
                .posStream = {
                    .vertexBuffer = (const void*)positions._buffer->getData(),
                    .byteOffset = (int)positions._offset,
                    .byteStride = positions._stride,
                    .type = positions._element.getType() == gpu::FLOAT ? (
                        positions._element.getSize() == 16 ? ISculptEngine::VertexAttribType::Float4 :
                        positions._element.getSize() == 12 ? ISculptEngine::VertexAttribType::Float3 :
                        positions._element.getSize() == 8 ? ISculptEngine::VertexAttribType::Float2 :
                        ISculptEngine::VertexAttribType::Float ) : ISculptEngine::VertexAttribType::R8G8B8A8_Unorm,
                    ._reserved = 0
                },
                .normalStream = {},
                .colorStream = {},
                .indexBuffer = (const unsigned short*)indices._buffer->getData(),
            };
#endif
            changedPackets[index] = true;
        }
        return 1;
    }
    QJsonObject toJson() {
        QVariantList _changedPackets;
        for(const auto &kv : changedPackets.toStdMap()) {
            _changedPackets << kv.second;
        }
        return {
            { "clientId", QJsonValue((int)_clientId) },
            { "tool", QJsonValue((int)_tool) },
            { "mode", QJsonValue((int)_mode) },
            { "radius", QJsonValue(_radius) },
            { "strength", QJsonValue(_strength) },
            { "paintColor", QJsonValue::fromVariant(vec3toVariant(glm::vec3(_color))) },
            { "enableMirror", QJsonValue::fromVariant(vec3toVariant(glm::vec3(_enableMirror))) },
            { "cursorPosition", QJsonValue::fromVariant(vec3toVariant(glm::vec3(_cursorPosition))) },
            { "cursorRotation", QJsonValue::fromVariant(quatToVariant(glm::quat(_cursorRotation))) },
            { "_changedPackets", QJsonValue::fromVariant(_changedPackets) },
        };
    }
};

class LeopolyAPI;

class LeopolyManager : public InputPlugin, public Dependency {
public:
    // InputPlugin overrides
    bool isSupported() const override { return true; }
    const QString getName() const override { return ID; }
    void init() override {
        qCDebug(leopoly_log) << "LeopolyProvider::init..." << getInstance();
    }
    void deinit() override { qCDebug(leopoly_log) << "LeopolyManager::deinit"; }
    void pluginFocusOutEvent() override { qCDebug(leopoly_log) << "LeopolyManager::pluginFocusOutEvent"; }
    void pluginUpdate(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) override {
        if (!_isEnabled) {
            return;
        }
        qCDebug(leopoly_log) << "LeopolyManager::pluginUpdate";
    }

    // instance helper methods
    std::atomic<bool> _isEnabled{ false };
    bool isEnabled() const { return _isEnabled; }
    void setEnabled(bool enable) { _isEnabled = enable; }
    static QSharedPointer<LeopolyAPI> getScriptableAPI();
    static QSharedPointer<LeopolyManager> getInstance();
    static leopoly::API _leopoly;
    static leopoly::API getLeopoly() {
        if (_leopoly) {
            return _leopoly;
        }
#if defined(Q_OS_WIN) && !defined(USE_LEOPOLY_STUBS)
        _leopoly = leopoly::API(leoSculptCreate(), leoSculptDestroy);
#else
        _leopoly = leopoly::API(new SculptEngineStub);
#endif
        return _leopoly;
    }

    // static leopoly::API getLeopolyStub() {
    //     static SculptEngineStub stub;
    //     return leopoly::API(&stub, [](ISculptEngine*) {});
    // }

    static leopoly::DEBUGAPI getLeopolyDebug() {
        return leopoly::DEBUGAPI(static_cast<SculptEngineDebug*>(getLeopoly().data()), [](SculptEngineDebug*) {});
    }

};
leopoly::API LeopolyManager::_leopoly;

namespace leopoly {
    template <typename T, typename TT> QSharedPointer<TT> instance() { return QSharedPointer<TT>(); }
    template<typename T>
    struct Bar { static QSharedPointer<T> foo(); };
    template<> inline QSharedPointer<ISculptEngine> Bar<ISculptEngine>::foo() { return LeopolyManager::getLeopoly(); }
    template<> inline QSharedPointer<SculptEngineDebug> Bar<SculptEngineDebug>::foo() { return LeopolyManager::getLeopolyDebug(); }
    template<> inline QSharedPointer<LeopolyManager> Bar<LeopolyManager>::foo() { return LeopolyManager::getInstance(); }
    template <typename X> QSharedPointer<X> instance() { return Bar<X>::foo(); }
}

// Scripting Interface
class LeopolyAPI : public QObject, QScriptable, ReadWriteLockable {
    Q_OBJECT
    Q_PROPERTY(bool supported READ isSupported)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    //Q_PROPERTY(mesh::ModelProxy inputModel MEMBER inputModel NOTIFY inputModelChanged)
    Q_PROPERTY(mesh::MeshProxyPointer inputMesh MEMBER inputMesh NOTIFY inputMeshChanged)
    //Q_PROPERTY(int inputModelIndex MEMBER inputModelIndex NOTIFY inputModelChanged)
    Q_PROPERTY(mesh::MeshProxyPointer outputMesh READ exportMeshSync)
    Q_PROPERTY(mesh::ModelProxy outputModel READ getOutputModelSync)
    //Q_PROPERTY(int outputModelIndex MEMBER outputModelIndex NOTIFY outputModelChanged)
    Q_PROPERTY(QJsonObject _json READ toJson)
    Q_PROPERTY(QVariantMap metadata READ getMetaData WRITE setMetaData NOTIFY metadataChanged)
    Q_PROPERTY(int clientId READ getClientId WRITE setClientId NOTIFY clientIdChanged)
    Q_PROPERTY(int tool READ getTool WRITE setTool NOTIFY toolChanged)
    Q_PROPERTY(QString toolName READ getToolName WRITE setTool NOTIFY toolNameChanged)
    Q_PROPERTY(int cursorMode READ getCursorMode WRITE setCursorMode NOTIFY cursorModeChanged)
    Q_PROPERTY(QString cursorModeName READ getCursorModeName WRITE setCursorMode NOTIFY cursorModeNameChanged)
    Q_PROPERTY(double radius READ getRadius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(double strength READ getStrength WRITE setStrength NOTIFY strengthChanged)
    Q_PROPERTY(glm::vec3 color READ getColor WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(glm::vec3 mirror READ getMirror WRITE setMirror NOTIFY mirrorChanged)
    Q_PROPERTY(glm::vec3 position READ getCursorPosition WRITE setCursorPosition NOTIFY positionChanged)
    Q_PROPERTY(glm::quat rotation READ getCursorRotation WRITE setCursorRotation NOTIFY rotationChanged)
    Q_PROPERTY(QVariantMap cursor READ getCursorData WRITE setCursorData NOTIFY cursorChanged)

    Q_PROPERTY(QVariantList actionPoses READ getActionPoses)
    Q_PROPERTY(QVariantList geometryStatus READ getGeometryStatus)
    Q_PROPERTY(int packetCount READ getNumGeometryPackets)
    Q_PROPERTY(QVariantList geometryPackets READ getGeometryPackets)
    Q_PROPERTY(int activeTool READ getActiveTool WRITE setActiveTool NOTIFY activeToolChanged)
    Q_PROPERTY(QString activeToolName READ getActiveToolName WRITE setActiveTool NOTIFY activeToolNameChanged)

    Q_PROPERTY(qint32 maxVertexCount MEMBER _maxVertexCount NOTIFY maxVertexCountChanged)
    Q_PROPERTY(qint32 maxTriangleCount MEMBER _maxTriangleCount NOTIFY maxTriangleCountChanged)
public:
    static void registerMetaTypes(QScriptEngine* engine) {}

    template <typename API>
    bool _withLeopolyRead(std::function<void(QSharedPointer<API>)> fun) const {
        if (auto api = leopoly::instance<API>()) {
            auto& lock = getLock();
            if (lock.tryLockForRead(1000)) {
                fun(api);
                lock.unlock();
                return true;
            } else {
                qDebug() << __FUNCTION__ << "=============================================================";
                qDebug() << __FUNCTION__ << "=============================================================";
                qDebug() << __FUNCTION__ << "could not get read lock" << QThread::currentThread();
                qDebug() << __FUNCTION__ << "=============================================================";
                qDebug() << __FUNCTION__ << "=============================================================";
            }
        }
        return false;
    }

    template <typename API, typename T> T withLeopolyRead(std::function<T(QSharedPointer<API>)> fun) const {
        T value;
        _withLeopolyRead<API>([&](QSharedPointer<API> api) { value = fun(api); });
        return value;
    }
    template <typename API, typename T, T defaultValue> T withLeopolyRead(std::function<T(QSharedPointer<API>)> fun) const {
        T value = defaultValue;
        _withLeopolyRead<API>([&](QSharedPointer<API> api) { value = fun(api); });
        return value;
    }
    template <typename T, T defaultValue> T withLeopolyRead(std::function<T(leopoly::API)> fun) const {
        T value = defaultValue;
        _withLeopolyRead<ISculptEngine>([&](leopoly::API api) { value = fun(api); });
        return value;
    }
    template <typename T> T withLeopolyRead(std::function<T(leopoly::API)> fun) const {
        T value;
        _withLeopolyRead<ISculptEngine>([&](leopoly::API api) { value = fun(api); });
        return value;
    }
    template <typename T> T withLeopolyRead(std::function<T()> fun) const {
        T value;
        _withLeopolyRead<ISculptEngine>([&](leopoly::API api) { value = fun(); });
        return value;
    }
    template <typename T, T defaultValue> T withLeopolyRead(std::function<T()> fun) const {
        T value = defaultValue;
        _withLeopolyRead<ISculptEngine>([&](leopoly::API api) { value = fun(); });
        return value;
    }

    template <typename API>
    void withLeopolyWrite(std::function<void(QSharedPointer<API>)> fun) {
        if (auto api = leopoly::instance<API>()) {
            auto& lock = getLock();
            if (lock.tryLockForWrite(1000)) {
                fun(api);
                lock.unlock();
            } else {
                qDebug() << __FUNCTION__ << "=============================================================";
                qDebug() << __FUNCTION__ << "=============================================================";
                qDebug() << __FUNCTION__ << "could not get write lock" << QThread::currentThread();
                qDebug() << __FUNCTION__ << "=============================================================";
                qDebug() << __FUNCTION__ << "=============================================================";
            }
        }
    }
    template <typename API, typename T, T defaultValue> T withLeopolyWrite(std::function<T(QSharedPointer<API>)> fun) {
        T value = defaultValue;
        withLeopolyWrite<API>([&](QSharedPointer<API> api) { value = fun(api); });
        return value;
    }

    LeopolyAPI(QObject* parent = nullptr) : QObject(parent) {
        withWriteLock([=]{
            setObjectName(LeopolyManager::getInstance()->getName());
            setProperty("CursorRaycastMode", leopoly::CursorRaycastMode);
            setProperty("VertexAttribType", leopoly::VertexAttribType);
            setProperty("VertexAttribSizes", leopoly::VertexAttribSizes);
            setProperty("Tool", leopoly::Tool);
        });
    }

public slots:
    void _updateActiveTool(ISculptEngine::Tool newActiveTool) {
        qDebug() << __FUNCTION__;
        if (getActiveTool() != (int)newActiveTool) {
            withLeopolyWrite<ISculptEngine>([=](leopoly::API leopoly) { lastActiveTool = newActiveTool; });
            emit activeToolChanged((int)newActiveTool);
            emit activeToolNameChanged(leopoly::Tool.key((int)newActiveTool, leopoly::INVALID_TOOLNAME));
        }
    }

    bool startAction() {
        qCDebug(leopoly_log) << __FUNCTION__;
        if (!inputMesh) {
            qCDebug(leopoly_log) << "WARNING: startAction called without inputMesh";
            return false;
        }
        if (getActiveTool() != (int)leopoly::INVALID_TOOL) {
            qCDebug(leopoly_log) << "WARNING: startAction called when previous action still active:" << getActiveToolName();
        }
        if (getTool() == (int)leopoly::INVALID_TOOL) {
            qCDebug(leopoly_log) << "WARNING: startAction called without valid tool:" << getToolName();
            stopAction();
            return false;
        }
        auto reallyStarted = withLeopolyWrite<ISculptEngine, bool, false>([this](QSharedPointer<ISculptEngine> leopoly){
            return leopoly->startAction();
        });
        qDebug() << __FUNCTION__ << "..._" << reallyStarted;
        _updateActiveTool(reallyStarted ? static_cast<ISculptEngine::Tool>(getTool()) : leopoly::INVALID_TOOL);
        return reallyStarted;
    }

    int update() {
        withLeopolyWrite<ISculptEngine>([this](leopoly::API leopoly){ leopoly->update(); });
        int updated = _triggerGeometryPacketUpdates();
        setGeometryChanged(false);
        return updated;
    }

    void stopAction() {
        qDebug() << __FUNCTION__;
        withLeopolyWrite<ISculptEngine>([this](leopoly::API leopoly) {
            qDebug() << __FUNCTION__ << "..stopAction.locked";
            if (lastActiveTool == leopoly::INVALID_TOOL) {
                qCDebug(leopoly_log) << "WARNING: stopAtion called when no action was started:" << getActiveToolName();
            }
            leopoly->stopAction();
        });
        _updateActiveTool(leopoly::INVALID_TOOL);
    }

    bool importMesh(mesh::MeshProxyPointer meshProxy) {
        inputMeshChanged(inputMesh = meshProxy);
        mesh::MeshPointer mesh = meshProxy ? meshProxy->getMeshPointer() : nullptr;
        if (!mesh)  {
            return false;
        }

        QVector<float> vertices;
        QVector<int> indices;
        QVector<unsigned char> colors;

        const gpu::BufferView& partBuffer = mesh->getPartBuffer();
        const gpu::BufferView& indexBuffer = mesh->getIndexBuffer();
        const gpu::BufferView& vertexBuffer = mesh->getVertexBuffer();

        const gpu::BufferView& colorsBufferView = mesh->getAttributeBuffer(gpu::Stream::COLOR);
        gpu::BufferView::Index numColors = (gpu::BufferView::Index)colorsBufferView.getNumElements();
        gpu::BufferView::Index colorIndex = 0;

        int vertexCount = 0;
        gpu::BufferView::Iterator<const glm::vec3> vertexItr = vertexBuffer.cbegin<const glm::vec3>();
        while (vertexItr != vertexBuffer.cend<const glm::vec3>()) {
            glm::vec3 v = *vertexItr;
            vertices << v[0];
            vertices << v[1];
            vertices << v[2];

            if (colorIndex < numColors) {
                glm::vec3 color = colorsBufferView.get<glm::vec3>(colorIndex);
                colors << (unsigned char)(color[0] * 255.0f);
                colors << (unsigned char)(color[1] * 255.0f);
                colors << (unsigned char)(color[2] * 255.0f);
                colors << (unsigned char)(0xff);
                colorIndex++;
            }
            vertexItr++;
            vertexCount++;
        }

        model::Index partCount = (model::Index)mesh->getNumParts();
        for (int partIndex = 0; partIndex < partCount; partIndex++) {
            const model::Mesh::Part& part = partBuffer.get<model::Mesh::Part>(partIndex);

            const bool shorts = indexBuffer._element == gpu::Element::INDEX_UINT16;
            auto face = [&](uint32_t i0, uint32_t i1, uint32_t i2) {
                uint32_t index0, index1, index2;
                if (shorts) {
                    index0 = indexBuffer.get<uint16_t>(i0);
                    index1 = indexBuffer.get<uint16_t>(i1);
                    index2 = indexBuffer.get<uint16_t>(i2);
                } else {
                    index0 = indexBuffer.get<uint32_t>(i0);
                    index1 = indexBuffer.get<uint32_t>(i1);
                    index2 = indexBuffer.get<uint32_t>(i2);
                }

                indices << index0;
                indices << index1;
                indices << index2;
            };

            uint32_t len = part._startIndex + part._numIndices;
            if (part._topology == model::Mesh::QUADS) {
                for (uint32_t idx = part._startIndex; idx+3 < len; idx += 4) {
                    face(idx+0, idx+1, idx+3);
                    face(idx+1, idx+2, idx+3);
                }
            } else if (part._topology == model::Mesh::TRIANGLES) {
                for (uint32_t idx = part._startIndex; idx+2 < len; idx += 3) {
                    face(idx+0, idx+1, idx+2);
                }
            }
        }
        auto numVertices = vertices.size() / 3;
        auto numTriangles = indices.size() / 3;
        auto packetTriLimit = _maxTriangleCount;
        qCDebug(leopoly_log) << "importMesh" << "numVertices:" << numVertices << "numTriangles:" << numTriangles << "packetTriLimit:" << packetTriLimit;
        bool result = false;
        withLeopolyWrite<ISculptEngine>([&](leopoly::API leopoly){
            result = leopoly->importMesh(numVertices, vertices.constData(), colors.constData(), numTriangles, indices.constData(), packetTriLimit);
        });
        return result;
    }

    Q_INVOKABLE mesh::ModelProxy getOutputModelSync() {
        auto scriptableMeshes = scriptable::Model();
        scriptableMeshes.metadata = {
            { "type", "leopoly" },
            { "entityID",  getMetaData().value("entityID") },
            { "lastModified", QDateTime::currentDateTime().toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate) },
        };
        scriptableMeshes << exportMeshSync()->getMeshPointer();
        return SimpleModelProxy::fromScriptableModel(scriptableMeshes);
    }

    Q_INVOKABLE QScriptValue exportMeshProxy() {
        return engine()->toScriptValue(exportMeshSync());
    }

    Q_INVOKABLE mesh::MeshProxyPointer exportMeshSync() {
        QVector<glm::vec3> vertices;
        QVector<glm::vec3> normals;
        QVector<glm::vec3> colors;
        QVector<uint32_t> indices;

        QVector<glm::u8vec4> byteColors;
        QVector<int> intIndices;

        int numVertices = 0;
        int numTriangles = 0;

        bool result = withLeopolyRead<bool>([&](leopoly::API leopoly) {
            if (leopoly->getNumGeometryPackets() <= 0) {
                return false;
            }
            bool result = leopoly->exportMesh(&numVertices, nullptr, nullptr, nullptr, &numTriangles, nullptr);
            int numIndices = numTriangles * 3;
            qDebug() << "exportMesh -- preflight " << result << numVertices << "(max" << _maxVertexCount << ")" << numTriangles << numIndices;

            vertices.resize(numVertices);
            colors.resize(numVertices);
            byteColors.resize(numVertices);
            normals.resize(numVertices);
            indices.resize(numIndices);
            intIndices.resize(numIndices);
            result = leopoly->exportMesh(&numVertices, (float *)vertices.data(), (float*)normals.data(), (unsigned char*)byteColors.data(), &numTriangles, intIndices.data());
            return result;
        });

        qDebug() << "//exportMesh -- export " << result << numVertices << "(max" << _maxVertexCount << ")" << numTriangles;
        for (int i = 0; i < numVertices; i++) {
            colors[i] = glm::vec3(1.0f);//glm::vec3(byteColors[i]) / 255.0f;
        }
        for (int i = 0; i < numTriangles; i++) {
            indices << (uint32_t) intIndices[i * 3 + 0];
            indices << (uint32_t) intIndices[i * 3 + 1];
            indices << (uint32_t) intIndices[i * 3 + 2];
        }

        model::MeshPointer mesh(new model::Mesh());
        mesh->displayName = "leopoly::exportMesh";

        // vertices
        auto vertexBuffer = std::make_shared<gpu::Buffer>(vertices.size() * sizeof(glm::vec3), (gpu::Byte*)vertices.data());
        auto vertexBufferPtr = gpu::BufferPointer(vertexBuffer);
        gpu::BufferView vertexBufferView(vertexBufferPtr, 0, vertexBufferPtr->getSize(),
                                         sizeof(glm::vec3), gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ));
        mesh->setVertexBuffer(vertexBufferView);

        // normals
        auto normalBuffer = std::make_shared<gpu::Buffer>(normals.size() * sizeof(glm::vec3), (gpu::Byte*)normals.data());
        auto normalBufferPtr = gpu::BufferPointer(normalBuffer);
        gpu::BufferView normalBufferView(normalBufferPtr, 0, normalBufferPtr->getSize(),
                                         sizeof(glm::vec3), gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ));
        mesh->addAttribute(gpu::Stream::NORMAL, normalBufferView);

        // colors
        auto colorBuffer = std::make_shared<gpu::Buffer>(colors.size() * sizeof(glm::vec3), (gpu::Byte*)colors.data());
        auto colorBufferPtr = gpu::BufferPointer(colorBuffer);
        gpu::BufferView colorBufferView(colorBufferPtr, 0, colorBufferPtr->getSize(),
                                        sizeof(glm::vec3), gpu::Element(gpu::VEC3, gpu::FLOAT, gpu::XYZ));
        mesh->addAttribute(gpu::Stream::COLOR, colorBufferView);

        // indices (faces)
        auto indexBuffer = std::make_shared<gpu::Buffer>(indices.size() * sizeof(uint32_t), (gpu::Byte*)indices.data());
        auto indexBufferPtr = gpu::BufferPointer(indexBuffer);
        gpu::BufferView indexBufferView(indexBufferPtr, gpu::Element(gpu::SCALAR, gpu::UINT32, gpu::RAW));
        mesh->setIndexBuffer(indexBufferView);

        // parts
        std::vector<model::Mesh::Part> parts;
        parts.emplace_back(model::Mesh::Part((model::Index)0, // startIndex
                                             (model::Index)indices.size(), // numIndices
                                             (model::Index)0, // baseVertex
                                             model::Mesh::TRIANGLES)); // topology
        mesh->setPartBuffer(gpu::BufferView(new gpu::Buffer(parts.size() * sizeof(model::Mesh::Part),
                                                            (gpu::Byte*) parts.data()), gpu::Element::PART_DRAWCALL));
        return mesh::MeshProxyPointer(new SimpleMeshProxy(mesh));
    }

    QByteArray packFrameData() { return QByteArray(); }
    bool applyFrameData(const QByteArray& data) { return false; }

    int generateDebugPackets(int index, mesh::MeshProxyPointer meshProxy) {
        return withLeopolyWrite<SculptEngineDebug, int, 0>([=](leopoly::DEBUGAPI leopolyDebug) {
            return leopolyDebug->generateDebugPackets(index, meshProxy);
        });
    }

protected:
    QVariantMap metadata;
    double lastRadius{ 1.0 };
    double lastStrength{ 1.0 };
    glm::u8vec4 lastColor;
    glm::bvec3 lastMirror;
    ISculptEngine::Tool lastTool{ leopoly::INVALID_TOOL };
    ISculptEngine::Tool lastActiveTool{ leopoly::INVALID_TOOL };
    ISculptEngine::CursorRaycastMode lastCursorMode{ ISculptEngine::CursorRaycastMode::None };
    glm::dvec3 lastPosition;
    glm::dquat lastRotation;
    QMap<int,bool> lastPacketStatus;

    mesh::MeshProxyPointer inputMesh{nullptr};
signals:
    void metadataChanged(QVariantMap metadata);
    void toolChanged(int tool);
    void toolNameChanged(const QString& toolName);
    void enabledChanged(bool enabled);
    void clientIdChanged(int clientId);
    void inputMeshChanged(mesh::MeshProxyPointer inputMesh);
    void outputMeshChanged(mesh::MeshProxyPointer outputMesh);

    void cursorModeChanged(int cursorMode);
    void cursorModeNameChanged(const QString& modeName);
    void radiusChanged(double radius);
    void strengthChanged(double strength);
    void mirrorChanged(glm::vec3 mirror);
    void colorChanged(glm::vec3 color);
    void positionChanged(glm::vec3 pos);
    void rotationChanged(glm::quat rot);
    void cursorChanged(glm::vec3 pos, glm::quat rot);

    void pendingFrameData(const QByteArray& frameData);
    void geometryPacketChanged(int index, const QVariantMap& packet);
    void activeToolChanged(int tool);
    void activeToolNameChanged(const QString& toolName);
    void packetCountChanged(int packetCount);

    void maxVertexCountChanged(qint32 maxVertexCount);
    void maxTriangleCountChanged(qint32 maxTriangleCount);
public:
    QJsonObject toJson() const {
        return withLeopolyRead<SculptEngineDebug, QJsonObject>([this](leopoly::DEBUGAPI leopolyDebug) { return leopolyDebug->toJson(); });
    }
    bool isSupported() const {
        return withLeopolyRead<LeopolyManager, bool>([this](leopoly::PLUGIN leopolyPlugin) { return leopolyPlugin->isSupported(); });
    }

    void setMetaData(const QVariantMap& metadata) {
        withLeopolyWrite<ISculptEngine>([=](leopoly::API leopoly) {
            this->metadata = metadata;
        });
        emit metadataChanged(metadata);
    }
    QVariantMap getMetaData() const {
        return withLeopolyRead<QVariantMap>([this]() -> QVariantMap {
            return metadata;
        });
    }

    bool isEnabled() {
        return withLeopolyRead<LeopolyManager, bool>([this](leopoly::PLUGIN leopolyPlugin) { return leopolyPlugin->isEnabled(); });
    }
    void setEnabled(bool enable) {
        if (enable != isEnabled()) {
            withLeopolyWrite<LeopolyManager>([=](leopoly::PLUGIN plugin) { plugin->setEnabled(enable); });
            emit enabledChanged(enable);
        }
    }

    double getRadius() const { return withLeopolyRead<double>([this]{ return lastRadius; }); }
    void setRadius(double radius) {
        if (radius != getRadius()) {
            withLeopolyWrite<ISculptEngine>([=](leopoly::API leopoly) { leopoly->setRadius(lastRadius = radius); });
            emit radiusChanged(radius);
        }
    }
    double getStrength() const { return withLeopolyRead<double>([this]{ return lastStrength;}); }
    void setStrength(double strength) {
        if (strength != getStrength()) {
            withLeopolyWrite<ISculptEngine>([=](leopoly::API leopoly) {
                leopoly->setStrength(lastStrength = strength);
            });
            emit strengthChanged(strength);
        }
    }
    glm::vec4 getColor() { return withLeopolyRead<glm::vec4>([this]{ return glm::vec4(glm::dvec4(lastColor) / 255.0); }); }
    void setColor(glm::vec3 _color) {
        glm::u8vec4 currentColor = withLeopolyRead<ISculptEngine, glm::u8vec4>([=](leopoly::API leopoly){ return lastColor; });
        glm::u8vec4 color = glm::dvec4(_color, 1.0) * 255.0;
        if (color != currentColor) {
            withLeopolyWrite<ISculptEngine>([=](leopoly::API leopoly){
                lastColor = color;
                leopoly->setPaintColor(lastColor.x, lastColor.y, lastColor.z);
            });
            emit colorChanged(_color);
        }
    }

    glm::vec3 getMirror() { return withLeopolyRead<glm::vec3>([this]{ return glm::vec3(lastMirror); }); }
    void setMirror(glm::vec3 mirror) {
        if (getMirror() != mirror) {
            withLeopolyWrite<ISculptEngine>([=](leopoly::API leopoly){
                lastMirror = glm::bvec3(mirror);
                leopoly->enableMirror(lastMirror.x, lastMirror.y, lastMirror.z);
            });
            emit mirrorChanged(getMirror());
        }
    }

    int getTool() { return withLeopolyRead<int>([this]{ return (int)lastTool; }); }
    bool setTool(int tool) {
        int currentTool = getTool();
        if (tool != getTool()) {
            withLeopolyWrite<ISculptEngine>([=](leopoly::API leopoly){
                leopoly->setTool(lastTool = static_cast<ISculptEngine::Tool>(tool));
            });
            currentTool = getTool();
            emit toolChanged(currentTool);
            emit toolNameChanged(leopoly::Tool.key((int)currentTool, leopoly::INVALID_TOOLNAME));
            return true;
        }
        return false;
    }
    int getCursorMode() { return withLeopolyRead<int>([this]{ return (int)lastCursorMode; }); }
    bool setCursorMode(int mode) {
        int current = getCursorMode();
        if (mode != current) {
            withLeopolyWrite<ISculptEngine>([=](leopoly::API leopoly){
                leopoly->setCursorRaycastMode(lastCursorMode = static_cast<ISculptEngine::CursorRaycastMode>(mode));
            });
            emit cursorModeChanged(getCursorMode());
            emit cursorModeNameChanged(getCursorModeName());
            return true;
        }
        return false;
    }
    bool setTool(const QString& toolName) {
        return setTool((int)leopoly::Tool.value(toolName, (int)leopoly::INVALID_TOOL).toInt());
    }
    bool setCursorMode(const QString& modeName) {
        return setCursorMode(leopoly::CursorRaycastMode.value(modeName, -1).toInt());
    }
    QString getCursorModeName() const { return withLeopolyRead<QString>([this]{ return leopoly::CursorRaycastMode.key((int)lastCursorMode, "NONE"); }); }
    QString getToolName() const { return withLeopolyRead<QString>([this]{ return leopoly::Tool.key((int)lastTool, leopoly::INVALID_TOOLNAME); }); }
    QString getActiveToolName() const { return withLeopolyRead<QString>([this]{ return leopoly::Tool.key((int)lastActiveTool, leopoly::INVALID_TOOLNAME); }); }

    QVariantMap getCursorData() const {
        return withLeopolyRead<QVariantMap>([this]() -> QVariantMap {
            return {
                { "position", vec3toVariant(glm::vec3(lastPosition)) },
                { "rotation", quatToVariant(glm::quat(lastRotation)) },
            };
        });
    }

    int getActiveTool() const { return withLeopolyRead<int>([this]{ return (int)lastActiveTool; }); }
    bool setActiveTool(const QString& toolName) {
        return setActiveTool((int)leopoly::Tool.value(toolName, (int)leopoly::INVALID_TOOL).toInt());
    }
    bool setActiveTool(int tool) {
        if (tool != getActiveTool()) {
            auto newTool = static_cast<ISculptEngine::Tool>(tool);;
            auto oldTool = static_cast<ISculptEngine::Tool>(getActiveTool());
            if (oldTool != leopoly::INVALID_TOOL) {
                withLeopolyWrite<ISculptEngine>([](leopoly::API leopoly) { leopoly->stopAction(); });
            }
            if (newTool != oldTool) {
                setTool((int)newTool);
            }
            return startAction();
        }
        return false;
    }
    QVariantList getActionPoses() {
        return withLeopolyRead<QVariantList>([=](leopoly::API leopoly) {
            QVariantList result;
            auto num = leopoly->getNumDerivedActionPoses();
            for(int i =0; i < num; i++) {
                glm::dvec3 pos;
                glm::dquat rot;
                leopoly->getDerivedActionPose(i, glm::value_ptr(pos),glm::value_ptr(rot));
                result << QVariantMap{
                    { "index", i },
                    { "position", vec3toVariant(glm::vec3(pos)) },
                    { "rotation", quatToVariant(glm::quat(rot)) },
                        };
            }
            return result;
        });
    }

    void setCursorData(const QVariantMap& data) {
        setCursorData(vec3FromVariant(data.value("position")), quatFromVariant(data.value("rotation")));
    }
    void setCursorData(const glm::vec3& _position, const glm::quat& _rotation) {
        bool posChanged = false;
        bool rotChanged = false;
        withLeopolyWrite<ISculptEngine>([=, &posChanged, &rotChanged](leopoly::API leopoly) {
            glm::dvec3 position = _position;
            glm::dquat rotation = _rotation;
            if (lastPosition != position) {
                lastPosition = position;
                posChanged = true;
            }
            if (lastRotation != rotation) {
                lastRotation = rotation;
                rotChanged = true;
            }
            leopoly->setCursorPose(glm::value_ptr(position),glm::value_ptr(rotation));
        });
        if (posChanged) {
            emit positionChanged(_position);
        }
        if (rotChanged) {
            emit rotationChanged(_rotation);
        }
        if (posChanged || rotChanged) {
            emit cursorChanged(_position, _rotation);
        }
    }
    glm::vec3 getCursorPosition() { return withLeopolyRead<glm::vec3>([this]{ return lastPosition; }); }
    glm::quat getCursorRotation() { return withLeopolyRead<glm::quat>([this]{ return lastRotation; }); }
    void setCursorPosition(const glm::vec3& position) {
        if (getCursorPosition() != position) {
            withLeopolyWrite<ISculptEngine>([=](leopoly::API leopoly) {
                lastPosition = position;
                leopoly->setCursorPose(glm::value_ptr(lastPosition), glm::value_ptr(lastRotation));
            });
            emit cursorChanged(lastPosition, lastRotation);
        }
    }
    void setCursorRotation(const glm::quat& rotation) {
        if (getCursorRotation() != rotation) {
            withLeopolyWrite<ISculptEngine>([=](leopoly::API leopoly) {
                lastRotation = rotation;
                leopoly->setCursorPose(glm::value_ptr(lastPosition), glm::value_ptr(lastRotation));
            });
            emit cursorChanged(lastPosition, lastRotation);
        }
    }

    int getNumGeometryPackets() {
        return withLeopolyRead<int, 0>([](leopoly::API leopoly) {
            return leopoly->getNumGeometryPackets();
        });
    }

    QVariantList getGeometryStatus() {
        return withLeopolyRead<QVariantList>([this](leopoly::API leopoly) {
            QVariantList result;
            auto packetCount = leopoly->getNumGeometryPackets();
            for(int i = 0; i < packetCount; i++) {
                result << leopoly->isGeometryPacketChanged(i);
            }
            return result;
       });
    }

    QVariantList getGeometryPackets() {
        return withLeopolyRead<QVariantList>([this](leopoly::API leopoly) {
            QVariantList result;
            auto packetCount = leopoly->getNumGeometryPackets();
            for(int i = 0; i < packetCount; i++) {
                result << _getGeometryPacket(i);
            }
            return result;
        });
    }

    void setGeometryChanged(bool changed) {
        withLeopolyWrite<ISculptEngine>([=](leopoly::API leopoly) {
            if (!changed) {
                leopoly->clearGeometryPacketChangeFlags();
            }
            if (!changed) {
                lastPacketStatus.clear();
            } else {
                qCDebug(leopoly_log) << "NOTE: setGeometryChanged(changed) only changed === false is supported";
            }
        });
    }

    int getClientId() {
        return withLeopolyRead<int, leopoly::INVALID_CLIENTID>([this](leopoly::API leopoly) {
            return leopoly->getClientId();
        });
    }
    void setClientId(int clientId) {
        if (clientId != getClientId()) {
            withLeopolyWrite<ISculptEngine>([=](leopoly::API leopoly) {
                leopoly->setClientId(clientId);
            });
            emit clientIdChanged(clientId);
        }
    }
    Q_INVOKABLE QVariantMap _getGeometryPacket(int index) {
        auto packet = withLeopolyRead<ISculptEngine::GeometryPacketData>([=](leopoly::API leopoly) -> ISculptEngine::GeometryPacketData {
            return leopoly->getGeometryPacketData(index);
        });
        QVariant indexBuffer;
        indexBuffer.setValue<QByteArray>(QByteArray((const  char*)packet.indexBuffer, packet.numInds * sizeof(unsigned short)));
        auto typeName = leopoly::VertexAttribType.value(QString("%1").arg((int)packet.posStream.type)).toString();
        uint32_t vertSize = leopoly::VertexAttribSizes.value(typeName).toUInt();
        return {
            { "index", index },
            { "numVerts", packet.numVerts },
            { "numInds", packet.numInds },
            { "indexBuffer", indexBuffer },
            { "posStream", QVariantMap{
                { "_byteLength", packet.numVerts * vertSize },
                { "byteOffset", packet.posStream.byteOffset },
                { "byteStride", packet.posStream.byteStride },
                { "type", typeName },
                { "size", vertSize },
                { "xsize", leopoly::VertexAttribSizes.value(typeName) },
                { "vertexBuffer", QByteArray((const char*)packet.posStream.vertexBuffer, packet.numVerts * vertSize) },
                { "_reserved", packet.posStream._reserved },
           }},
        };
    }

    Q_INVOKABLE int _triggerGeometryPacketUpdates() {
        QMap<int,bool> updated;
        withLeopolyWrite<ISculptEngine>([=, &updated](leopoly::API leopoly) {
            QMap<int,bool> currentStatus;
            auto packetCount = leopoly->getNumGeometryPackets();
            for(int i = 0; i < packetCount; i++) {
                currentStatus[i] = leopoly->isGeometryPacketChanged(i);
                if (lastPacketStatus.value(i) != currentStatus.value(i)) {
                    lastPacketStatus[i] = currentStatus[i];
                    if (currentStatus[i]) {
                        updated[i] = true;
                    }
                }
            }
        });
        for (const auto& kv : updated.toStdMap()) {
            emit geometryPacketChanged(kv.first, _getGeometryPacket(kv.first));
        }
        return updated.size();
    }

private:
    qint32 _maxVertexCount{ 0xffff }; // 65535
    qint32 _maxTriangleCount{ 20000 }; // from Leopoly SDK SculptEngine.cs example
};

class LeopolyProvider : public QObject, public InputProvider {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID InputProvider_iid FILE "plugin.json")
    Q_INTERFACES(InputProvider)

public:
    LeopolyProvider(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~LeopolyProvider() {}
    // FIXME: This Leopoly API plugin currently pretends to be an "input plugin" so that it can fit within the existing
    // plugin architecture (which lacks something more abstract yet -- like a "ModulePlugin.h" type).  At some point this
    // workaround should be replaced with that future generalized plugin approach.
    virtual InputPluginList getInputPlugins() override {
        InputPlugin* input = static_cast<InputPlugin*>(LeopolyManager::getInstance().data());
        // note: we need to return a std::vector<QSharedPointer<InputPlugin>> -- but DependencyManager uses std::shared_ptr.
        // it's a singleton anyway, so a no-op custom deleter is used here to simply disable the QSharedPointer-side memory cleanup.
        InputPluginPointer softPointer{ input, [=](InputPlugin*) { /* no-op */ }};
        return { softPointer };
    }
    virtual void destroyInputPlugins() override {}
};

QSharedPointer<LeopolyAPI> LeopolyManager::getScriptableAPI() {
    static LeopolyAPI api{ LeopolyManager::getInstance().data() };
    return QSharedPointer<LeopolyAPI>{ &api, [](LeopolyAPI*) {}};
}

QSharedPointer<LeopolyManager> LeopolyManager::getInstance() {
    static std::once_flag once;
    std::call_once(once, [&] {
        auto manager = DependencyManager::set<LeopolyManager>();
#ifdef Q_OS_WIN
        manager->setObjectName("Q_OS_WIN");
#endif
        auto scriptInitializers = DependencyManager::get<ScriptInitializerMixin>();
        scriptInitializers->registerNativeScriptInitializer([](QScriptEngine* engine){
             auto manager = DependencyManager::get<LeopolyManager>();
             qCDebug(leopoly_log) << "LeopolyProvider::scriptInitializer" << engine << manager->getName();
             engine->globalObject().setProperty("LeopolyManager", engine->newQObject(manager.data()));
             LeopolyAPI::registerMetaTypes(engine);
             engine->globalObject().setProperty("Leopoly", engine->newQObject(manager->getScriptableAPI().data()));
        });
    });
    return DependencyManager::get<LeopolyManager>();
}

#include "LeopolyProvider.moc"
