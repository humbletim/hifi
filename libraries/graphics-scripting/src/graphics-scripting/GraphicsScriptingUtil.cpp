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

HIFI_REGISTER_METATYPE(std::vector<std::string>)
template<> std::vector<std::string> QVariant::value() const {
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


template <typename T> QVariant glmVecToVariant(const T& value) { return QVariant::fromValue<T>(value); }

HIFI_REGISTER_READONLY_METATYPE(Extents)
template<> QVariant QVariant::fromValue(const Extents& box) {
    return QVariantMap{
        { "center", glmVecToVariant(box.minimum + (box.size() / 2.0f)) },
        { "minimum", glmVecToVariant(box.minimum) },
        { "maximum", glmVecToVariant(box.maximum) },
        { "dimensions", glmVecToVariant(box.size()) },
    };
}

HIFI_REGISTER_READONLY_METATYPE(AABox)
template<> QVariant QVariant::fromValue(const AABox& box) {
    return QVariantMap{
        { "brn", glmVecToVariant(box.getCorner()) },
        { "tfl", glmVecToVariant(box.calcTopFarLeft()) },
        { "center", glmVecToVariant(box.calcCenter()) },
        { "minimum", glmVecToVariant(box.getMinimumPoint()) },
        { "maximum", glmVecToVariant(box.getMaximumPoint()) },
        { "dimensions", glmVecToVariant(box.getDimensions()) },
    };
}

HIFI_REGISTER_READONLY_METATYPE(gpu::Element)
template<> QVariant QVariant::fromValue(const gpu::Element& element) {
    return QVariantMap{
        { "type", gpu::toString(element.getType()) },
        { "semantic", gpu::toString(element.getSemantic()) },
        { "dimension", gpu::toString(element.getDimension()) },
        { "scalarCount", element.getScalarCount() },
        { "byteSize", element.getSize() },
        { "BYTES_PER_ELEMENT", element.getSize() / element.getScalarCount() },
     };
}

HIFI_REGISTER_READONLY_METATYPE(gpu::BufferView)
template<> QVariant QVariant::fromValue(const gpu::BufferView& value) {
    return QVariantMap{
        { "length", (glm::uint32) value.getNumElements() },
        { "byteLength", (glm::uint32) value._size },
        { "offset", (glm::uint32) value._offset },
        { "stride", (glm::uint32) value._stride },
        { "element", QVariant::fromValue(value._element) },
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
