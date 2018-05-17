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

#include <shared/Scriptable.h>

class Extents;
class AABox;
namespace gpu {
    class Element;
}
Q_DECLARE_LOGGING_CATEGORY(graphics_scripting)

namespace scriptable {
    // TODO: consolidate these with other toVariant helpers
    template<> QVariant toVariant(const Extents& box);
    template<> QVariant toVariant(const AABox& box);
    template<> QVariant toVariant(const gpu::Element& element);
    template<> QVariant toVariant(const glm::mat4& mat4);
    template<> QVariant toVariant(const glm::vec2& value);
    template<> QVariant toVariant(const glm::vec3& value);
    template<> QVariant toVariant(const glm::vec4& value);
    template<> QVariant toVariant(const glm::quat& value);

    // JS TypedArrays are a more efficient way to specify mesh data, but harder to construct by hand on the JS side
    // this adapter provides support for incoming mesh properties in both conventions
    // eg: { positions: [Vec3, Vec3, Vec3] } or { positions: Float32Array }
    QScriptValue toTypedArray(QScriptValue global, const QByteArray& bytes, const QString& typedArrayName);
    template <typename T, typename U> QByteArray convertBytes(const QByteArray& bytes);
    template <typename T> QByteArray coerceJSTypedArray(QScriptValue value, const QString& typedArrayName, const QString& property);

    struct JSVectorAdapter {
        const QVariantMap& qt;
        const QScriptValue& js;
        JSVectorAdapter(const QVariantMap& qt, const QScriptValue& js) : qt(qt), js(js) {}
        static const QMap<QString, QStringList> ALIASES;
        static QString normalizeAlias(QString alias);
        static QString resolveAlias(QString property, QStringList available);
        template <typename T>
        std::vector<T> getVector(const QString& property, const QString& jsTypeName);
    };

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

}
