//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GraphicsScriptingUtil.h"

#include <BaseScriptEngine.h>

#include <graphics/BufferViewHelpers.h>
#include <AABox.h>
#include <Extents.h>
#include <glm/gtc/type_ptr.hpp>

Q_LOGGING_CATEGORY(graphics_scripting, "hifi.scripting.graphics")

HIFI_REGISTER_METATYPE(std::string)
template<> std::string QVariant::value<std::string>() const {
    return toString().toStdString();
}
template<> QVariant QVariant::fromValue(const std::string& value) {
    return QString::fromStdString(value);
}

HIFI_REGISTER_METATYPE(std::vector<std::string>)
template<> std::vector<std::string> QVariant::value<std::vector<std::string>>() const {
    // QVariantList (this) -> std::vector
    const auto& list = toList();
    std::vector<std::string> result;
    result.resize(list.size());
    for (const auto& s : list) {
        result.push_back(s.toString().toStdString());
    }
    return result;
}
template<> QVariant QVariant::fromValue(const std::vector<std::string>& value) {
    // std::vector (value) -> QVariantList
    QVariantList result;
    result.reserve((int)value.size());
    for (const auto& s : value) {
        result << QString::fromStdString(s);
    }
    return result;
}

HIFI_REGISTER_METATYPE(glm::mat4)
template<> QVariant QVariant::fromValue(const glm::mat4& value) {
    const auto ptr = glm::value_ptr(value);
    QVariant v;
    v.setValue(QVector<float>::fromStdVector(std::vector<float>{ptr, ptr + 16}));
    return v;
}
template<> glm::mat4 QVariant::value<glm::mat4>() const {
    QVector<float> floats = value<QVector<float>>();
    return floats.size() == 16 ? glm::make_mat4(floats.data()) : glm::mat4(NAN);
}

template <typename T> QVariant glmVecToVariant(const T& value) { return QVariant::fromValue<T>(value); }

HIFI_REGISTER_READONLY_METATYPE(Extents)
template<> QVariant QVariant::fromValue(const Extents& value) {
    return QVariantMap{
        { "center", glmVecToVariant(value.minimum + (value.size() / 2.0f)) },
        { "minimum", glmVecToVariant(value.minimum) },
        { "maximum", glmVecToVariant(value.maximum) },
        { "dimensions", glmVecToVariant(value.size()) },
    };
}

HIFI_REGISTER_READONLY_METATYPE(AABox)
template<> QVariant QVariant::fromValue(const AABox& value) {
    return QVariantMap{
        { "brn", glmVecToVariant(value.getCorner()) },
        { "tfl", glmVecToVariant(value.calcTopFarLeft()) },
        { "center", glmVecToVariant(value.calcCenter()) },
        { "minimum", glmVecToVariant(value.getMinimumPoint()) },
        { "maximum", glmVecToVariant(value.getMaximumPoint()) },
        { "dimensions", glmVecToVariant(value.getDimensions()) },
    };
}

HIFI_REGISTER_READONLY_METATYPE(gpu::Element)
template<> QVariant QVariant::fromValue(const gpu::Element& value) {
    return QVariantMap{
        { "type", gpu::toString(value.getType()) },
        { "semantic", gpu::toString(value.getSemantic()) },
        { "dimension", gpu::toString(value.getDimension()) },
        { "scalarCount", value.getScalarCount() },
        { "byteSize", value.getSize() },
        { "BYTES_PER_value", value.getSize() / value.getScalarCount() },
     };
}

HIFI_REGISTER_METATYPE(scriptable::MappedQObject)
template<> QVariant QVariant::fromValue(const scriptable::MappedQObject& value) {
    auto object = value.first;
    auto metaObject = value.second;
    QVariantMap result;
    if (!object || !metaObject) {
        return result;
    }
    result["__class__"] = metaObject->className();
    for (int i=0; i < metaObject->propertyCount(); i++) {
        auto prop = metaObject->property(i);
        if (prop.isReadable()) {
            auto name = prop.name();
            result[name] = object->property(name);
        }
    }
    return result;
}
template<> QVariant QVariant::fromValue(const glm::vec2& value) { return buffer_helpers::glmVecToVariant(value); }
template<> QVariant QVariant::fromValue(const glm::vec3& value) { return buffer_helpers::glmVecToVariant(value); }
template<> QVariant QVariant::fromValue(const glm::vec4& value) { return buffer_helpers::glmVecToVariant(value); }
template<> QVariant QVariant::fromValue(const glm::quat& value) { return buffer_helpers::glmVecToVariant(value); }

namespace scriptable {

const std::map<QString, QStringList> JSVectorAdapter::ALIASES{
    { "indices", { "indices", "index", "indexes", }},
    { "positions", { "positions", "position", "vertices", } },
    { "normals", { "normals", "normal", }},
    { "colors", { "colors", "color", }},
    { "texCoords0", { "texcoords0", "texcoord0", "texturecoords", "texcoords", "texcoord", "uvs", "uv" } },
};

// returns the normalized, pluralized camelCase name
QString JSVectorAdapter::pluralizeAttributeName(const QString& alias) {
    auto normalized = alias.toLower().replace("vertex", "");
    for (const auto& kv : ALIASES) {
        if (kv.first == alias || kv.second.contains(alias)) {
            return kv.first;
        }
    }
    return QString();
}

// returns the normalized, singularized internal attribute name
QString JSVectorAdapter::singularizeAttributeName(const QString& attributeName) {
    auto plural = pluralizeAttributeName(attributeName).toLower();
    if (plural == "indices") {
        return "index";
    }
    return plural.replace(QRegExp("s([0-9])$"), "\\1").replace(QRegExp("s$"), "");
};

namespace {
    QString findPropertyByAlias(const QString& alias, const QStringList& propertyNames) {
        auto normalized = JSVectorAdapter::pluralizeAttributeName(alias);
        if (!normalized.isEmpty()) {
            for (const auto& n : propertyNames) {
                if (JSVectorAdapter::pluralizeAttributeName(n) == normalized) {
                    return n;
                }
            }
        }
        return QString();
    }
}

template <typename T>
std::vector<T> JSVectorAdapter::getVector(const QString& property, const QString& jsTypeName) {
    QString sourceProperty = findPropertyByAlias(property, qt.keys());
    if (!qt.contains(sourceProperty)) {
        // qCDebug(graphics_scripting) << "JSVectorAdapter::getVector -- normalized property not found" << sourceProperty << "(" << property << ")" << JSVectorAdapter::pluralizeAttributeName(property) << jsTypeName << qt.keys();
        return std::vector<T>();
    }
    auto variant = qt.value(sourceProperty);
    if (variant.type() == (QVariant::Type)QMetaType::QVariantList) {
        return buffer_helpers::variantToVector<T>(variant);
    } else {
        qDebug() << "JSVectorAdapter::getVector -- discovered " << sourceProperty << " (" << property << ") Object (assuming value is a TypedArray)" << jsTypeName;
        qDebug() << "TODO: TypedArray support moved to separate PR";
        return std::vector<T>();
    }
}

template std::vector<glm::uint32> JSVectorAdapter::getVector<glm::uint32>(const QString& property, const QString& jsTypeName);
template std::vector<glm::vec2> JSVectorAdapter::getVector<glm::vec2>(const QString& property, const QString& jsTypeName);
template std::vector<glm::vec3> JSVectorAdapter::getVector<glm::vec3>(const QString& property, const QString& jsTypeName);
template std::vector<glm::vec4> JSVectorAdapter::getVector<glm::vec4>(const QString& property, const QString& jsTypeName);

}
