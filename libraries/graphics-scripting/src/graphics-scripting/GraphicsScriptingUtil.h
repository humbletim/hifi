#pragma once

#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>
#include <QtScript/QScriptable>
#include <QtCore/QPointer>
#include <QtCore/QObject>
#include <QtCore/QLoggingCategory>
#include <QtCore/QVariant>
#include <QDebug>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include <graphics/BufferViewHelpers.h>

class Extents;
class AABox;
namespace gpu {
    class Element;
}
Q_DECLARE_LOGGING_CATEGORY(graphics_scripting)

namespace scriptable {
    // TODO: consolidate these with other toVariant helpers
    QVariant toVariant(const Extents& box);
    QVariant toVariant(const AABox& box);
    QVariant toVariant(const std::vector<std::string>& strings);
    QVariant toVariant(const std::string& str);
    QVariant toVariant(const gpu::Element& element);
    QVariant toVariant(const glm::mat4& mat4);
    inline QVariant toVariant(const glm::vec2& value) { return buffer_helpers::glmVecToVariant(value); }
    inline QVariant toVariant(const glm::vec3& value) { return buffer_helpers::glmVecToVariant(value); }
    inline QVariant toVariant(const glm::vec4& value) { return buffer_helpers::glmVecToVariant(value); }
    inline QVariant toVariant(const glm::quat& value) { return buffer_helpers::glmVecToVariant(value); }

    // helper that automatically resolves Qt-signal-like scoped callbacks
    // ... C++ side: `void MyClass::asyncMethod(..., QScriptValue callback)`
    // ... JS side:
    //     * `API.asyncMethod(..., function(){})`
    //     * `API.asyncMethod(..., scope, function(){})`
    //     * `API.asyncMethod(..., scope, "methodName")`
    QScriptValue jsBindCallback(QScriptValue callback);

    QString toDebugString(QObject* tmp);
    template <typename T> QString toDebugString(std::shared_ptr<T> tmp);

    // JS TypedArrays are a more efficient way to specify mesh data, but harder to construct by hand on the JS side
    // this adapter provides support for incoming mesh properties in both conventions
    // eg: { positions: [Vec3, Vec3, Vec3] } or { positions: Float32Array }
    QScriptValue toTypedArray(QScriptValue global, const QByteArray& bytes, const QString& typedArrayName);
    template <typename T, typename U> QByteArray convertBytes(const QByteArray& bytes);
    template <typename T> QByteArray coerceJSTypedArray(QScriptValue value, const QString& typedArrayName, const QString& property);

    struct JSVectorAdapter {
        const QVariantMap& qt;
        const QScriptValue& js;
        // aliases to simplify use of existing 3rd party javascript libraries when creating meshes
        static const QMap<QString, QStringList> ALIASES;
        static QString normalizeAlias(QString alias) {
            for (const auto& kv : ALIASES.toStdMap()) {
                if (kv.second.contains(alias)) {
                    return kv.first;
                }
            }
            return QString();
        }
        static QString resolveAlias(QString property, QStringList available) {
            for (const auto& a : ALIASES[property].toStdList()) {
                if (available.contains(a)) {
                    qDebug() << "found aliased property" << property << "->" << a;
                    return a;
                }
            }
            return QString();
        }
        template <typename T>
        QVector<T> getVector(const QString& property, const QString& jsTypeName) {
            QString sourceProperty = resolveAlias(property, qt.keys());
            if (!qt.contains(sourceProperty)) {
                return QVector<T>();
            }
            auto variant = qt.value(sourceProperty);
            qDebug() << "...." << sourceProperty << jsTypeName << variant.typeName();
            if (variant.type() == (QVariant::Type)QMetaType::QVariantList) {
                return buffer_helpers::variantToVector<T>(variant);
            } else {
                auto value = js.property(sourceProperty);
                return buffer_helpers::variantToVector<T>(coerceJSTypedArray<T>(value, jsTypeName, sourceProperty));
            }
        }

    };
    template QVector<glm::uint32> JSVectorAdapter::getVector<glm::uint32>(const QString& property, const QString& jsTypeName);
    template QVector<glm::vec2> JSVectorAdapter::getVector<glm::vec2>(const QString& property, const QString& jsTypeName);
    template QVector<glm::vec3> JSVectorAdapter::getVector<glm::vec3>(const QString& property, const QString& jsTypeName);
    template QVector<glm::vec4> JSVectorAdapter::getVector<glm::vec4>(const QString& property, const QString& jsTypeName);

    // TODO: continue looking for standardizable ways to consistently register C++ JS APIs

    // register a DebugEnums type (QMap-based non-flag enums where JS uses the string representation)
    template <typename T> int registerDebugEnum(QScriptEngine* engine, const DebugEnums<T>& debugEnums) {
        static const DebugEnums<T>& instance = debugEnums;
        return qScriptRegisterMetaType<T>(
            engine,
            [](QScriptEngine* engine, const T& topology) -> QScriptValue {
                return instance.value(topology);
            },
            [](const QScriptValue& value, T& topology) {
                topology = instance.key(value.toString());
            }
        );
    }

    template <typename T> void registerPrototype(QScriptEngine* engine, QObject* prototype) {
        qScriptRegisterSequenceMetaType<QVector<T>>(engine);
        qScriptRegisterSequenceMetaType<std::vector<T>>(engine);
        engine->setDefaultPrototype(qMetaTypeId<T>(), engine->newQObject(prototype, QScriptEngine::QtOwnership, QScriptEngine::ExcludeDeleteLater));
    }

    template <typename T> void registerVariantablePrototype(QScriptEngine * engine) {
        qScriptRegisterMetaType<T>(
            engine,
            [](QScriptEngine* engine, const T& flags) -> QScriptValue {
                return engine->toScriptValue(toVariant(flags));
            },
            [](const QScriptValue& value, T& flags) {}
        );
    }

    // register an enum type (declared with Q_DECLARE_FLAGS)
    template <typename T> int registerQFlagsEnum(QScriptEngine* engine) {
        return qScriptRegisterMetaType<T>(
            engine,
            [](QScriptEngine* engine, const T& flags) -> QScriptValue {
                return (int)flags;
            },
            [](const QScriptValue& value, T& flags) {
                flags = static_cast<T>(value.toInt32());
            }
        );
    }
}
