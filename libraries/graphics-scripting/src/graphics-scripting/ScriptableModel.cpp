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

#include "GraphicsScriptingUtil.h"
#include "ScriptableMesh.h"
#include "graphics/Material.h"
#include "image/Image.h"

// #define SCRIPTABLE_MESH_DEBUG 1

scriptable::ScriptableMaterial& scriptable::ScriptableMaterial::operator=(const scriptable::ScriptableMaterial& material) {
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

scriptable::ScriptableMaterial::ScriptableMaterial(const graphics::MaterialPointer& material) :
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

scriptable::ScriptableMaterialLayer& scriptable::ScriptableMaterialLayer::operator=(const scriptable::ScriptableMaterialLayer& materialLayer) {
    material = materialLayer.material;
    priority = materialLayer.priority;

    return *this;
}

scriptable::ScriptableModelBase& scriptable::ScriptableModelBase::operator=(const scriptable::ScriptableModelBase& other) {
    provider = other.provider;
    objectID = other.objectID;
    for (const auto& mesh : other.meshes) {
        append(mesh);
    }
    materialLayers = other.materialLayers;
    materialNames = other.materialNames;
    return *this;
}

scriptable::ScriptableModelBase::~ScriptableModelBase() {
    materialLayers.clear();
    materialNames.clear();
}

void scriptable::ScriptableModelBase::append(graphics::MeshPointer mesh) {
    meshes << mesh;
}

void scriptable::ScriptableModelBase::appendMaterial(const graphics::MaterialLayer& materialLayer, int shapeID, std::string materialName) {
    materialLayers[QString::number(shapeID)].push_back(ScriptableMaterialLayer(materialLayer));
    materialLayers["mat::" + QString::fromStdString(materialName)].push_back(ScriptableMaterialLayer(materialLayer));
}

void scriptable::ScriptableModelBase::appendMaterials(const std::unordered_map<std::string, graphics::MultiMaterial>& materialsToAppend) {
    auto materialsToAppendCopy = materialsToAppend;
    for (auto& multiMaterial : materialsToAppendCopy) {
        while (!multiMaterial.second.empty()) {
            materialLayers[QString(multiMaterial.first.c_str())].push_back(ScriptableMaterialLayer(multiMaterial.second.top()));
            multiMaterial.second.pop();
        }
    }
}

void scriptable::ScriptableModelBase::appendMaterialNames(const std::vector<std::string>& names) {
    for (auto& name : names) {
        materialNames.append(QString::fromStdString(name));
    }
}

QString scriptable::ScriptableModel::toString() const {
    auto self = getNativeObject();
    if (!self) {
        return "[Model nullptr]";
    }
    auto numMeshes = self->meshes.size();
    auto name = self->objectName();
    auto objectID = self->objectID;
    return QString("[Model numMeshes=%1%2%3]")
        .arg(numMeshes)
        .arg(name.isEmpty() ? "" : " name=" + name)
        .arg(objectID.isNull() ? "" : " objectID=" + objectID.toString());
}

QVariant scriptable::ScriptableModel::toVariant() const {
    return QVariant::fromValue<scriptable::MappedQObject>({ this, &staticMetaObject });
}

scriptable::ScriptableModelPointer scriptable::ScriptableModel::cloneModel(const QVariantMap& options) {
    scriptable::ScriptableModelPointer clone;
    if (auto self = getNativeObject()) {
        clone = scriptable::ScriptableModelPointer::create(new scriptable::ScriptableModelBase(*self));
        clone->meshes.clear();
        // create a deep copy of each mesh
        for (const auto &mesh : getConstMeshes()) {
            clone->meshes.push_back(buffer_helpers::mesh::clone(mesh));
        }
    }
    return clone;
}

const scriptable::ScriptableMeshes scriptable::ScriptableModel::getConstMeshes() const {
    if (auto self = getNativeObject()) {
        return self->meshes;
    }
    return scriptable::ScriptableMeshes();
}

scriptable::ScriptableMeshes scriptable::ScriptableModel::getMeshes() {
    if (auto self = getNativeObject()) {
        return self->meshes;
    } else {
        qCDebug(graphics_scripting) << "getMeshes -- invalid native object" << getNativeObject();
    }
    return scriptable::ScriptableMeshes();
}

Extents scriptable::ScriptableModel::getModelExtents() const {
    Extents extents;
    if (auto self = getNativeObject()) {
        for (auto& mesh : self->meshes) {
            extents.add(mesh->evalPartsBound(0, (int)mesh->getNumParts()));;
        }
    } else {
        qCDebug(graphics_scripting) << "getModelExtents -- invalid native object" << getNativeObject();
    }
    return extents;
}

QString scriptable::ScriptableModel::toOBJ()  {
    return GraphicsScriptingInterface::exportModelToOBJ(getNativeObject());
}
