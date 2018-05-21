//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Scriptable.h"
#include "../BaseScriptEngine.h"

namespace scriptable {

QScriptValue makeScopedHandlerObject(QScriptValue scopeOrCallback, QScriptValue methodOrName) {
    return ::makeScopedHandlerObject(scopeOrCallback, methodOrName);
}
QScriptValue callScopedHandlerObject(QScriptValue handler, QScriptValue err, QScriptValue result) {
    return ::callScopedHandlerObject(handler, err, result);
}

template<> std::string fromVariant(const QVariant& value) {
    return value.toString().toStdString();
}
template<> QVariant toVariant(const std::string& str) {
    return QString::fromStdString(str);
}
template<> QVariant toVariant(const std::vector<std::string>& strings) {
    QStringList result;
    result.reserve((int)strings.size());
    for (const auto& s : strings) {
        result << QString::fromStdString(s);
    }
    return result;
}

QScriptValue jsBindCallback(QScriptValue value) {
    if (value.isObject() && value.property("callback").isFunction()) {
        // value is already a bound callback
        return value;
    }
    auto engine = value.engine();
    auto context = engine ? engine->currentContext() : nullptr;
    auto length = context ? context->argumentCount() : 0;
    QScriptValue scope = context ? context->thisObject() : QScriptValue::NullValue;
    QScriptValue method;

    // find position in the incoming JS Function.arguments array (so we can test for the two-argument case)
    for (int i = 0; context && i < length; i++) {
        if (context->argument(i).strictlyEquals(value)) {
            method = context->argument(i+1);
        }
    }
    if (method.isFunction() || method.isString()) {
        // interpret as `API.func(..., scope, function callback(){})` or `API.func(..., scope, "methodName")`
        scope = value;
    } else {
        // interpret as `API.func(..., function callback(){})`
        method = value;
    }
    return ::makeScopedHandlerObject(scope,  method);
}

    
QString toDebugString(QObject* tmp) {
    QString s;
    QTextStream out(&s);
    out << tmp;
    return s;
}
template <typename T> QString toDebugString(std::shared_ptr<T> tmp) {
    return toDebugString(qobject_cast<QObject*>(tmp.get()));
}


}
