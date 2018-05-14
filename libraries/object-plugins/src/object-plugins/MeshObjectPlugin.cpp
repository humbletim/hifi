#include "MeshObjectPlugin.h"

namespace plugins { namespace entity {
    MeshRay::MeshRay(const glm::vec3& origin, const glm::vec3& direction, const QVariantMap& metadata)
        : origin(origin), direction(direction), metadata(metadata) {}
    MeshRay::operator bool() const {
        return !(glm::any(glm::isnan(origin))||glm::any(glm::isnan(direction)));
    }
    QVariantMap MeshRay::toVariantMap() const {
        return {
            { "origin", vec3toVariant(origin), },
            { "direction", vec3toVariant(direction), },
            { "metadata", metadata, },
        };
    }
}}


