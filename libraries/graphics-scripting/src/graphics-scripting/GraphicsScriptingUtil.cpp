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

using buffer_helpers::glmVecToVariant;

Q_LOGGING_CATEGORY(graphics_scripting, "hifi.scripting.graphics")
Q_DECLARE_METATYPE(QByteArray*)

namespace scriptable {

// common IFS property aliases for js::Graphics::newMesh
const QMap<QString, QStringList> JSVectorAdapter::ALIASES{
    { "indices", QStringList{ "indices", "indexes", "index", }},
    { "positions", QStringList{ "positions", "position", "vertices", "vertexPositions", }},
    { "normals", QStringList{ "normals", "normal", "vertexNormals", }},
    { "colors", QStringList{ "colors", "color", "vertexColors", }},
    { "texCoords0", QStringList{ "texCoords0", "texCoord0", "uvs", "vertexUVs", "texcoord", "texCoord",  "vertexTextureCoords" }},
};

template<> QVariant toVariant(const glm::mat4& mat4) {
    const auto ptr = glm::value_ptr(mat4);
    QVariant v;
    v.setValue(QVector<float>::fromStdVector(std::vector<float>{ptr, ptr + 16}));
    return v;
};

template<> QVariant toVariant(const Extents& box) {
    return QVariantMap{
        { "center", glmVecToVariant(box.minimum + (box.size() / 2.0f)) },
        { "minimum", glmVecToVariant(box.minimum) },
        { "maximum", glmVecToVariant(box.maximum) },
        { "dimensions", glmVecToVariant(box.size()) },
    };
}

template<> QVariant toVariant(const AABox& box) {
    return QVariantMap{
        { "brn", glmVecToVariant(box.getCorner()) },
        { "tfl", glmVecToVariant(box.calcTopFarLeft()) },
        { "center", glmVecToVariant(box.calcCenter()) },
        { "minimum", glmVecToVariant(box.getMinimumPoint()) },
        { "maximum", glmVecToVariant(box.getMaximumPoint()) },
        { "dimensions", glmVecToVariant(box.getDimensions()) },
    };
}

template<> QVariant toVariant(const gpu::Element& element) {
    return QVariantMap{
        { "type", gpu::toString(element.getType()) },
        { "semantic", gpu::toString(element.getSemantic()) },
        { "dimension", gpu::toString(element.getDimension()) },
        { "scalarCount", element.getScalarCount() },
        { "byteSize", element.getSize() },
        { "BYTES_PER_ELEMENT", element.getSize() / element.getScalarCount() },
     };
}


QScriptValue toTypedArray(QScriptValue global, const QByteArray& bytes, const QString& typedArrayName) {
    auto ArrayBuffer = global.property("ArrayBuffer");
    auto TypedArray = global.property(typedArrayName);
    auto buffer = ArrayBuffer.construct(QScriptValueList{ bytes.size() });
    if (QByteArray* output = qscriptvalue_cast<QByteArray*>(buffer.data())) {
        ::memcpy(output->data(), bytes.constData(), output->size());
    }
    return TypedArray.construct(QScriptValueList{ buffer });
}

template <typename T, typename U> QByteArray convertBytes(const QByteArray& bytes) {
    const std::vector<T> input = buffer_helpers::variantToVector<T>(bytes);
    std::vector<U> output;
    output.reserve(input.size());
    std::transform(input.begin(), input.end(), std::back_inserter(output), [](T x) { return static_cast<U>(x); });
    return { (const char*)output.data(), (int)(output.size() * sizeof(U)) };
}

template <typename T> QByteArray coerceJSTypedArray(QScriptValue value, const QString& typedArrayName, const QString& property) {
    auto engine = value.engine();
    if (!engine) {
        return QByteArray();
    }
    auto global = engine->globalObject();
    auto context = engine->currentContext();
    auto ArrayBuffer = global.property("ArrayBuffer");
    auto Uint8Array = global.property("Uint8Array");
    auto Uint16Array = global.property("Uint16Array");
    auto Uint32Array = global.property("Uint32Array");
    auto Int8Array = global.property("Int8Array");
    auto Int16Array = global.property("Int16Array");
    auto Int32Array = global.property("Int32Array");
    auto Float32Array = global.property("Float32Array");
    auto TypedArray = global.property(typedArrayName);
    auto buffer = value.property("buffer");
    auto bytes = qscriptvalue_cast<QByteArray>(buffer);
    if (buffer.instanceOf(ArrayBuffer)) {
        if (value.instanceOf(TypedArray)) {
            // already in the desired native format
            return bytes;
        } else if (value.instanceOf(Uint8Array)) {
            return convertBytes<glm::uint8, T>(bytes);
        } else if (value.instanceOf(Int8Array)) {
            return convertBytes<glm::int8, T>(bytes);
        } else if (value.instanceOf(Uint16Array)) {
            return convertBytes<glm::uint16, T>(bytes);
        } else if (value.instanceOf(Int16Array)) {
            return convertBytes<glm::int16, T>(bytes);
        } else if (value.instanceOf(Uint32Array)) {
            return convertBytes<glm::uint32, T>(bytes);
        } else if (value.instanceOf(Int32Array)) {
            return convertBytes<glm::int32, T>(bytes);
        } else if (value.instanceOf(Float32Array)) {
            return convertBytes<glm::float32, T>(bytes);
        } else {
            context->throwError(QString("unsupported TypedArray (%1) for property '%2'")
                                .arg(value.property("constructor").data().toString()).arg(property));
            return QByteArray();
        }
    } else {
        if (value.instanceOf(ArrayBuffer)) {
            context->throwError(QString("please pass JS TypedArray (not ArrayBuffer) for property '%1'").arg(property));
            return QByteArray();
        }
    }
    context->throwError(QString("coerceJSTypedArray -- unrecognized value %1").arg(value.toString()));
    return QByteArray();
}

    template<> QVariant toVariant(const glm::vec2& value) { return buffer_helpers::glmVecToVariant(value); }
    template<> QVariant toVariant(const glm::vec3& value) { return buffer_helpers::glmVecToVariant(value); }
    template<> QVariant toVariant(const glm::vec4& value) { return buffer_helpers::glmVecToVariant(value); }
    template<> QVariant toVariant(const glm::quat& value) { return buffer_helpers::glmVecToVariant(value); }
 
 QString JSVectorAdapter::normalizeAlias(QString alias) {
        for (const auto& kv : ALIASES.toStdMap()) {
            if (kv.second.contains(alias)) {
                return kv.first;
            }
        }
        return QString();
    }
 QString JSVectorAdapter::resolveAlias(QString property, QStringList available) {
        for (const auto& a : ALIASES[property].toStdList()) {
            if (available.contains(a)) {
                if (property != a) {
                    qCDebug(graphics_scripting) << "using aliased property" << property << "->" << a;
                }
                return a;
            }
        }
        return QString();
    }
    template <typename T>
    std::vector<T> JSVectorAdapter::getVector(const QString& property, const QString& jsTypeName) {
        QString sourceProperty = resolveAlias(property, qt.keys());
        if (!qt.contains(sourceProperty)) {
            return std::vector<T>();
        }
        auto variant = qt.value(sourceProperty);
        if (variant.type() == (QVariant::Type)QMetaType::QVariantList) {
            return buffer_helpers::variantToVector<T>(variant);
        } else {
            auto value = js.property(sourceProperty);
            return buffer_helpers::variantToVector<T>(coerceJSTypedArray<T>(value, jsTypeName, sourceProperty));
        }
    }


    template std::vector<glm::uint32> JSVectorAdapter::getVector<glm::uint32>(const QString& property, const QString& jsTypeName);
    template std::vector<glm::vec2> JSVectorAdapter::getVector<glm::vec2>(const QString& property, const QString& jsTypeName);
    template std::vector<glm::vec3> JSVectorAdapter::getVector<glm::vec3>(const QString& property, const QString& jsTypeName);
    template std::vector<glm::vec4> JSVectorAdapter::getVector<glm::vec4>(const QString& property, const QString& jsTypeName);
}
