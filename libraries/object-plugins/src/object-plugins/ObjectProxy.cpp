#include "ObjectProxy.h"

namespace plugins { namespace object {

ObjectRay::ObjectRay(const glm::vec3& origin, const glm::vec3& direction, const QVariantMap& metadata)
    : origin(origin), direction(direction), metadata(metadata) {}

ObjectRay::operator bool() const {
    return !(glm::any(glm::isnan(origin))||glm::any(glm::isnan(direction)));
}

QVariantMap ObjectRay::toVariantMap() const {
    return {
        { "origin", vec3toVariant(origin), },
        { "direction", vec3toVariant(direction), },
        { "metadata", metadata, },
    };
}

}}


#include <EntityItemProperties.h>

namespace {
    inline QVariant toVariant(const glm::vec3& v) { return vec3toVariant(v); }
    inline QVariant toVariant(const glm::quat& v) { return quatToVariant(v); }
    //inline QVariant toVariant(const QUrl& v) { return v; }
    inline QVariant toVariant(const QString& v) { return v; }
    inline QVariant toVariant(const float v) { return v; }
    inline QVariant toVariant(const bool v) { return v; }
    inline QVariant toVariant(const xColor& v) { return toVariant(toGlm(v)); }
    template <typename T> inline QVariant toVariant(const QVector<T>& v) {
        QVariant var;
        var.setValue<QVector<T>>(v);
        return var;
    }
}

// TODO: flush this out and somehow dedupe with EntityItemPropertiesToScriptValue
QVariantMap EntityItemPropertiesToVariantMap(const EntityItemProperties& properties) {
#define FORWARD_PROP(property, unused, getter) if (properties.property##Changed()) map[#property] = toVariant(properties.getter());
    QVariantMap map;
    FORWARD_PROP(name, string, getName);
    FORWARD_PROP(visible, bool, getVisible);
    FORWARD_PROP(position, vec3, getPosition);
    FORWARD_PROP(rotation, quat, getRotation);
    FORWARD_PROP(registrationPoint, vec3, getRegistrationPoint);
    FORWARD_PROP(color, vec3, getColor);
    FORWARD_PROP(dimensions, vec3, getDimensions);
    FORWARD_PROP(alpha, float, getAlpha);
    FORWARD_PROP(userData, string, getUserData);
    FORWARD_PROP(modelURL, url, getModelURL);
    FORWARD_PROP(textures, string, getTextures);
    FORWARD_PROP(jointRotations, array, getJointRotations);
    FORWARD_PROP(jointTranslations, string, getJointTranslations);
#undef FORWARD_PROP
    return map;
    // alternative strategey...
    // QScriptEngine scriptEngine;
    // return properties.copyToScriptValue(&scriptEngine, skipDefaults, true).toVariant().toMap();
}
