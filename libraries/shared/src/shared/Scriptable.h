//
//  Scriptable.h
//  libraries/shared/src/shared
//
//  Copyright 2018 High Fidelity, Inc.
//
//  Shared utilities for interfacing with QtScript
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

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

namespace scriptable {
    // Extend scriptable::NativePrototype to wrap a native object while exposing only desired methods to JS
    // Suggest using in conjunction with smart pointers for the native type (for automatic memory management).
    // Example:
    //   class ThingPrototype : public QObject, public scriptable::NativePrototype<ThingSmartPointer> {
    //   Q_OBJECT
    //   Q_PROPERTY(int length READ getLength)
    //   Q_PROPERTY(QScriptValue finished READ getFinishedSignal)
    //   public:
    //      ThingSmartPointer getNativeObject() const { return qscriptvalue_cast<ThingSmartPointer>(thisObject()); }
    //      int getLength() const { return callWithResult<int>(&Thing::getLength); }
    //      QScriptValue getFinishedSignal() const { return scriptable::makeSignalProxy<scriptable::SignalProxy0, ThingSmartPointer>(thisObject(), &Thing::finished); }
    // };
    //
    // ... and later when registering with a script engine:
    // void registerMetaTypes(QScriptEngine* engine) {
    //    scriptable::registerPrototype(engine, new ThingPrototype());
    // }
    //
    // ... at this point any C++ JS APIs returning a ThingSmartPointer automatically become scriptable instances
    //     of ThingPrototype on the JS side.

    template <typename T> void registerPrototype(QScriptEngine* engine, QObject* prototype) {
        qScriptRegisterSequenceMetaType<QVector<T>>(engine);
        qScriptRegisterSequenceMetaType<std::vector<T>>(engine);
        engine->setDefaultPrototype(qMetaTypeId<T>(), engine->newQObject(prototype, QScriptEngine::QtOwnership, QScriptEngine::ExcludeDeleteLater));
    }

    template <typename C> class NativePrototype {
    public:
        // implement for acquiring a handle to the native object (ideally a smart pointer)
        virtual C getNativeObject() const = 0;

        // (guarded) native method invoke
        // eg: void close() { call(&Thing::close); }
        template<typename M, typename ...Args>
        void call(M method, Args &&...args) {
            if (auto self = getNativeObject()) {
                (*self.*method)(std::forward<Args>(args)...);
            }
        }
        // (guarded) native invoke with result
        // eg: int getNumVertices() const { return callWithResult<int>(&Thing::getNumVertices); }
        template<typename T, typename M, typename ...Args>
        T callWithResult(M method, Args &&...args) const {
            if (auto self = getNativeObject()) {
                return (*self.*method)(std::forward<Args>(args)...);
            }
            return T{};
        }
    };

    template <typename T> QVariant toVariant(const T& value);
    template<> QVariant toVariant(const std::vector<std::string>& strings);
    template<> QVariant toVariant(const std::string& str);

    // helper that automatically resolves Qt-signal-like scoped JS callbacks
    // ... C++ side: `void MyClass::asyncMethod(..., QScriptValue callback)`
    // ... JS side:
    //     * `API.asyncMethod(..., function(){})`
    //     * `API.asyncMethod(..., scope, function(){})`
    //     * `API.asyncMethod(..., scope, "methodName")`
    QScriptValue jsBindCallback(QScriptValue callback);

    QString toDebugString(QObject* tmp);
    template <typename T> QString toDebugString(std::shared_ptr<T> tmp);

    template <typename T> void registerVariantablePrototype(QScriptEngine * engine) {
        qScriptRegisterMetaType<T>(
            engine,
            [](QScriptEngine* engine, const T& flags) -> QScriptValue {
                return engine->toScriptValue(scriptable::toVariant(flags));
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
    
    class SignalProxy0: public QObject {
        Q_OBJECT
    public:
        template <typename T, typename Func> SignalProxy0(T* source, Func c) : QObject(nullptr) {
            QVariant v; v.setValue<QObject*>(source); setProperty("source", v);
            qDebug() << "SignalProxy0" << source << c;
            QObject::connect(source, c, this, &SignalProxy0::handler);
        }
    signals:
        void handler();
    };
    template <typename S, typename T, typename Func>
    static QScriptValue makeSignalProxy(QScriptValue thiz, Func c) {
        auto source = qscriptvalue_cast<T>(thiz).data();
        auto engine = thiz.engine();
        qDebug() << "makeSignalProxy" << source << engine << c;
        return engine->newQObject(new S(source, c), QScriptEngine::ScriptOwnership).property("handler");
    }
    QScriptValue makeScopedHandlerObject(QScriptValue scopeOrCallback, QScriptValue methodOrName);
    QScriptValue callScopedHandlerObject(QScriptValue handler, QScriptValue err, QScriptValue result);
}
