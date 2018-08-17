#pragma once
#include <graphics/Forward.h>
#include <glm/glm.hpp>
#include <vector>

namespace graphics { namespace utils {
    static auto constexpr DEDUPE_EPSILON = 1.0e-6f;
    inline graphics::MeshPointer dedupeVertices(const graphics::MeshPointer& input, float epsilon = DEDUPE_EPSILON, bool resetNormals = false) {
        qDebug() << "TODO: graphics::dedupeVertices";
        return nullptr;
    }
}}