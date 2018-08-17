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
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QVector>

#define _HIFI_DECLARE_CUSTOM_VARIANT(T)                         \
    template<> bool QVariant::canConvert<T>() const;            \
    template<> T QVariant::value<T>() const;                    \
    template<> QVariant QVariant::fromValue(const T& value);

#define HIFI_DECLARE_CUSTOM_METATYPE(T) Q_DECLARE_METATYPE(T) _HIFI_DECLARE_CUSTOM_VARIANT(T)

#define SCRIPTABLE_PASTE_HELPER(a,b) a ## b
#define SCRIPTABLE_UNIQUE_IDENTIFIER(b) SCRIPTABLE_PASTE_HELPER(metaType, b)
#define _HIFI_CUSTOM_METATYPE(T, CANCONVERT)    \
    namespace { int SCRIPTABLE_UNIQUE_IDENTIFIER(__LINE__) = qRegisterMetaType<T>(#T); } \
    template<> bool QVariant::canConvert<T>() const CANCONVERT
#define HIFI_REGISTER_METATYPE(T) _HIFI_CUSTOM_METATYPE(T, { return userType() == qMetaTypeId<T>(); })
#define HIFI_REGISTER_READONLY_METATYPE(T) _HIFI_CUSTOM_METATYPE(T, { return false; })

namespace scriptable {
    template <typename T> QScriptValue registerPrototype(QScriptEngine* engine, QObject* prototype) {
        qRegisterMetaType<T>();
        qScriptRegisterSequenceMetaType<QVector<T>>(engine);
        qScriptRegisterSequenceMetaType<std::vector<T>>(engine);
        QScriptValue proto = engine->newQObject(prototype, QScriptEngine::QtOwnership, QScriptEngine::ExcludeDeleteLater);
        engine->setDefaultPrototype(qMetaTypeId<T>(), proto);
        return proto;
    }

    template <typename T> void registerReadOnlyVariantType(QScriptEngine * engine) {
        qScriptRegisterMetaType<T>(
            engine,
            [](QScriptEngine* engine, const T& flags) -> QScriptValue {
                return engine->toScriptValue(QVariant::fromValue<T>(flags));
            },
            [](const QScriptValue& value, T& flags) {}
        );
    }
    template <typename T> void registerVariantType(QScriptEngine * engine) {
        qScriptRegisterMetaType<T>(
            engine,
            [](QScriptEngine* engine, const T& flags) -> QScriptValue {
                return engine->toScriptValue(QVariant::fromValue<T>(flags));
            },
            [](const QScriptValue& value, T& flags) {
                flags = value.toVariant().value<T>();
            }
        );
    }

    QScriptValue jsBindCallback(QScriptValue callback);
}
