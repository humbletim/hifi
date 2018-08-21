#ifndef hifi_GraphicsScriptingUtil_h
#define hifi_GraphicsScriptingUtil_h

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
#include <RegisteredMetaTypes.h>
#include "Forward.h"

class Extents;
class AABox;
namespace gpu {
    class Element;
}
Q_DECLARE_LOGGING_CATEGORY(graphics_scripting)

HIFI_DECLARE_CUSTOM_METATYPE(Extents)
HIFI_DECLARE_CUSTOM_METATYPE(AABox)
HIFI_DECLARE_CUSTOM_METATYPE(gpu::BufferView)
HIFI_DECLARE_CUSTOM_METATYPE(gpu::Element)
HIFI_DECLARE_CUSTOM_METATYPE(std::vector<std::string>)

namespace scriptable {
    using MappedQObject = std::pair<const QObject*, const QMetaObject*>;
}
HIFI_DECLARE_CUSTOM_METATYPE(scriptable::MappedQObject)

namespace scriptable {
    inline bool jsThrowError(QScriptEngine* engine, const QString& error, const QLoggingCategory &cat() = &graphics_scripting) {
        if (auto context = engine ? engine->currentContext() : nullptr) {
            context->throwError(error);
            return true;
        } else {
            qCWarning(cat) << "scriptable::jsThrowError (invalid JS context -- logging exception instead):" << error;
            return false;
        }
    }
    template <typename T> T jsAssert(QScriptEngine* engine, T truthy, const QString& message) {
        return truthy ? truthy : (jsThrowError(engine, message), T());
    }

    struct JSVectorAdapter {
        const QVariantMap& qt;
        const QScriptValue& js;
        JSVectorAdapter(const QVariantMap& qt, const QScriptValue& js) : qt(qt), js(js) {}
        static const std::map<QString, QStringList> ALIASES;
        static QString pluralizeAttributeName(const QString& alias);
        static QString singularizeAttributeName(const QString& alias);
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

inline QDebug operator<<(QDebug dbg, graphics::MeshPointer mesh) { return dbg << mesh.get(); }

#endif // hifi_GraphicsScriptingUtil_h
