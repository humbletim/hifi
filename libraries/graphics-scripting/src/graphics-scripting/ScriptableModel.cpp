//
//  ScriptableModel.cpp
//  libraries/graphics-scripting
//
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "GraphicsScriptingUtil.h"
#include "GraphicsScriptingInterface.h"
#include "ScriptableModel.h"
#include "ScriptableMesh.h"

#include <QtScript/QScriptEngine>

#include <Extents.h>
#include <graphics/Material.h>
#include <graphics/TextureMap.h>
#include <image/Image.h>

// #define SCRIPTABLE_MESH_DEBUG 1

js::Graphics::MaterialLayer::MaterialLayer(const graphics::MaterialLayer& materialLayer) :
    material(materialLayer.material), priority(materialLayer.priority) {}

js::Graphics::Material& js::Graphics::Material::operator=(const js::Graphics::Material& material) {
    name = material.name;
    model = material.model;
    opacity = material.opacity;
    roughness = material.roughness;
    metallic = material.metallic;
    scattering = material.scattering;
    unlit = material.unlit;
    emissive = material.emissive;
    albedo = material.albedo;
    emissiveMap = material.emissiveMap;
    albedoMap = material.albedoMap;
    opacityMap = material.opacityMap;
    metallicMap = material.metallicMap;
    specularMap = material.specularMap;
    roughnessMap = material.roughnessMap;
    glossMap = material.glossMap;
    normalMap = material.normalMap;
    bumpMap = material.bumpMap;
    occlusionMap = material.occlusionMap;
    lightmapMap = material.lightmapMap;
    scatteringMap = material.scatteringMap;

    return *this;
}

js::Graphics::Material::Material(const graphics::MaterialPointer& material) :
    name(material->getName().c_str()),
    model(material->getModel().c_str()),
    opacity(material->getOpacity()),
    roughness(material->getRoughness()),
    metallic(material->getMetallic()),
    scattering(material->getScattering()),
    unlit(material->isUnlit()),
    emissive(material->getEmissive()),
    albedo(material->getAlbedo())
{
    auto map = material->getTextureMap(graphics::Material::MapChannel::EMISSIVE_MAP);
    if (map && map->getTextureSource()) {
        emissiveMap = map->getTextureSource()->getUrl().toString();
    }

    map = material->getTextureMap(graphics::Material::MapChannel::ALBEDO_MAP);
    if (map && map->getTextureSource()) {
        albedoMap = map->getTextureSource()->getUrl().toString();
        if (map->useAlphaChannel()) {
            opacityMap = albedoMap;
        }
    }

    map = material->getTextureMap(graphics::Material::MapChannel::METALLIC_MAP);
    if (map && map->getTextureSource()) {
        if (map->getTextureSource()->getType() == image::TextureUsage::Type::METALLIC_TEXTURE) {
            metallicMap = map->getTextureSource()->getUrl().toString();
        } else if (map->getTextureSource()->getType() == image::TextureUsage::Type::SPECULAR_TEXTURE) {
            specularMap = map->getTextureSource()->getUrl().toString();
        }
    }

    map = material->getTextureMap(graphics::Material::MapChannel::ROUGHNESS_MAP);
    if (map && map->getTextureSource()) {
        if (map->getTextureSource()->getType() == image::TextureUsage::Type::ROUGHNESS_TEXTURE) {
            roughnessMap = map->getTextureSource()->getUrl().toString();
        } else if (map->getTextureSource()->getType() == image::TextureUsage::Type::GLOSS_TEXTURE) {
            glossMap = map->getTextureSource()->getUrl().toString();
        }
    }

    map = material->getTextureMap(graphics::Material::MapChannel::NORMAL_MAP);
    if (map && map->getTextureSource()) {
        if (map->getTextureSource()->getType() == image::TextureUsage::Type::NORMAL_TEXTURE) {
            normalMap = map->getTextureSource()->getUrl().toString();
        } else if (map->getTextureSource()->getType() == image::TextureUsage::Type::BUMP_TEXTURE) {
            bumpMap = map->getTextureSource()->getUrl().toString();
        }
    }

    map = material->getTextureMap(graphics::Material::MapChannel::OCCLUSION_MAP);
    if (map && map->getTextureSource()) {
        occlusionMap = map->getTextureSource()->getUrl().toString();
    }

    map = material->getTextureMap(graphics::Material::MapChannel::LIGHTMAP_MAP);
    if (map && map->getTextureSource()) {
        lightmapMap = map->getTextureSource()->getUrl().toString();
    }

    map = material->getTextureMap(graphics::Material::MapChannel::SCATTERING_MAP);
    if (map && map->getTextureSource()) {
        scatteringMap = map->getTextureSource()->getUrl().toString();
    }
}

js::Graphics::MaterialLayer& js::Graphics::MaterialLayer::operator=(const js::Graphics::MaterialLayer& materialLayer) {
    material = materialLayer.material;
    priority = materialLayer.priority;

    return *this;
}

js::Graphics::Model& js::Graphics::Model::operator=(const js::Graphics::Model& other) {
    provider = other.provider;
    objectID = other.objectID;
    for (const auto& mesh : other.meshes) {
        append(mesh);
    }
    materialLayers = other.materialLayers;
    materialNames = other.materialNames;
    return *this;
}

js::Graphics::Model::~Model() {
#ifdef SCRIPTABLE_MESH_DEBUG
    qCDebug(graphics_scripting) << "~js::Graphics::Model" << this;
    // makes cleanup order more deterministic to help with debugging
    for (auto& m : meshes) {
        m->strongMesh.reset();
    }
    meshes.clear();
#endif
    materialLayers.clear();
    materialNames.clear();
}

void js::Graphics::Model::append(graphics::MeshPointer mesh) {
    qDebug() << "append" << mesh.get();
    meshes.emplace_back(new js::Graphics::Mesh( provider, sharedFromThis(), mesh, this /*parent*/ ));
}

void js::Graphics::Model::append(const js::Graphics::MeshPointer& mesh) {
    qDebug() << "append" << mesh.data();
    if (mesh->provider.lock().get() != provider.lock().get()) {
        qCDebug(graphics_scripting) << "warning: appending mesh from different provider..." << mesh->provider.lock().get() << " != " << provider.lock().get();
    }
    meshes.push_back(mesh);
}

void js::Graphics::Model::appendMaterial(const graphics::MaterialLayer& materialLayer, int shapeID, std::string materialName) {
    materialLayers[QString::number(shapeID)].push_back(js::Graphics::MaterialLayer(materialLayer));
    materialLayers["mat::" + QString::fromStdString(materialName)].push_back(js::Graphics::MaterialLayer(materialLayer));
}

void js::Graphics::Model::appendMaterials(const std::unordered_map<std::string, graphics::MultiMaterial>& materialsToAppend) {
    auto materialsToAppendCopy = materialsToAppend;
    for (auto& multiMaterial : materialsToAppendCopy) {
        while (!multiMaterial.second.empty()) {
            materialLayers[QString(multiMaterial.first.c_str())].push_back(js::Graphics::MaterialLayer(multiMaterial.second.top()));
            multiMaterial.second.pop();
        }
    }
}

void js::Graphics::Model::appendMaterialNames(const std::vector<std::string>& names) {
    for (auto& name : names) {
        materialNames.append(QString::fromStdString(name));
    }
}

QString js::Graphics::ModelPrototype::toString() const {
    qDebug()<< "ModelPrototype::toString" << thisObject().toVariant();
    auto self= getNativeObject();
    return !self ? QString("[Model nullptr]") : QString("[Model%1%2 numMeshes=%3]")
        .arg(self->objectID.isNull() ? "" : " objectID="+self->objectID.toString())
        .arg(self->objectName().isEmpty() ? "" : " name=" +self->objectName())
        .arg(self->meshes.size());
}

js::Graphics::ModelPointer js::Graphics::ModelPrototype::cloneModel(const QVariantMap& options) {
    js::Graphics::ModelPointer clone;
    if (auto self = getNativeObject()) {
        // copy constructor will create a clone of the overall model (including material data)
        clone = js::Graphics::ModelPointer::create(new js::Graphics::Model(*self));
        // we then need to decouple the mesh pointers by creating deep clones of each one
        clone->meshes.clear();
        for (const auto &mesh : getConstMeshes()) {
            clone->meshes.push_back(GraphicsScriptingInterface::cloneMesh(mesh));
        }
    }
    return clone;
}

const js::Graphics::Meshes js::Graphics::ModelPrototype::getConstMeshes() const {
    if (auto self = getNativeObject()) {
        return self->meshes;
    }
    return js::Graphics::Meshes();
}

js::Graphics::Meshes js::Graphics::ModelPrototype::getMeshes() {
    if (auto self = getNativeObject()) {
        return self->meshes;
    } else {
        qCDebug(graphics_scripting) << "getMeshes -- invalid native object" << getNativeObject();
    }
    return js::Graphics::Meshes();
}

Extents js::Graphics::ModelPrototype::getModelExtents() const {
    Extents extents;
    if (auto self = getNativeObject()) {
        for (auto& mesh : self->meshes) {
            if (auto nativeMesh = mesh->getMeshPointer()) {
                extents.add(nativeMesh->evalPartsBound(0, (int)nativeMesh->getNumParts()));;
            }
        }
    } else {
        qCDebug(graphics_scripting) << "getModelExtents -- invalid native object" << getNativeObject();
    }
    return extents;
}

#if 0
// FIXME: remove this after testing that ::reduceAll approach works as replacement
glm::uint32 js::Graphics::ModelPrototype::updateVertexAttributes(QScriptValue callback) {
    glm::uint32 result = 0;
    if (auto self = getNativeObject()) {
        for (auto& mesh : self->meshes) {
            js::Graphics::MeshPrototype wrapped{ mesh };
            result += wrapped.updateVertexAttributes(callback);
            if (engine()->hasUncaughtException()) {
                break;
            }
        }
    }
    return result;
}
#endif
