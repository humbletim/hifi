## Object Plugins Interface

_(work in progress)_

`libraries/object-plugins` provides a framework for extending Interface objects using optional plugin DLLs.

It builds on the existing `plugins/` architecture, adding support for extensions that can hook into individual Entity types and offer features like *customized rendering* and *procedural mesh* support.

Design goals:
* Compartmentalize custom rendering and raycasting aspects of the mesh sculpting project.
* Enable object plugin prototypes to be used with standard Beta builds (ie: optional post-installed DLLs).
* Remain 100% backwards-compatible and protocol-neutral for now (automatic fallback when a plugin is missing).
* Support parallel R&D for upcoming dynamically-provisioned, certified, native marketplace apps.

### Mapping Plugins to Entities

Currently object plugins utilize a "surrogate" Entity as a placeholder, which is then potentially upgraded based on plugin availability.

#### Explicit registration and association:

- **C++ side**: object plugin DLL registers itself using a dynamic *pluginURI*:

```cpp
auto proxyManager = DependencyManager::get<plugins::object::ProxyManager>();
proxyManager->setPluginResolver(PROXY_PLUGIN_URI, std::make_shared<MyObjectProxyPlugin>());
```

- **JS side**: a script associates a specific Entity with that *pluginURI*:

```javascript
if (Graphics.setRenderPlugin(entityID, PROXY_PLUGIN_URI)) {
    print(entityID + ' will now use ' + PROXY_PLUGIN_URI + ' for rendering and interactions.');
}
// note: above Entity would only be affected in the local octree --
//   everyone else continues to see the stock entity per server.
````

> [plugins/hifiMeshTest](https://github.com/humbletim/hifi/tree/hifiMeshTest/plugins/hifiMeshTest) provides an example plugin.
> When the plugin DLL is present, it registers as a provider for the pluginURI `plugin://entity::Shape::MeshTest` --
> which can then be activated (for an existing Shape Entity) using the Graphics API.

### `libraries/entities` and `libraries/entities-renderer` hooks

Object plugins currently hook into the following class hierarchy:

> *EntityItem -> ShapeEntityItem -> RenderableShapeEntityItem*

* EntityItem:
    - New virtual `getEntityProxy()` method which subclasses override to indicate a bound object plugin.
* [ShapeEntityItem.cpp](../entities/src/ShapeEntityItem.cpp) / [RenderableShapeEntityItem.cpp](../entities-renderer/src/RenderableShapeEntityItem.cpp):
    - (When a valid plugin has been assigned) A subset of entity methods are automatically redirected to the plugin provider.

### plugins::object::ObjectProxy reference

The [ObjectProxy](src/object-plugins/MeshObjectPlugin.h) base class provides a template for object plugins.  All methods are currently considered optional.

A new object plugin can be created by implementing the system-level plugins/ scaffolding along with this interface to provide Entity-level functionality.

```cpp
class ObjectProxy {
public:
    using Pointer = std::shared_ptr<ObjectProxy>;
    QUuid objectID;
    js::Graphics::RenderFlags flags;

    ObjectProxy(js::Graphics::RenderFlags flags = js::Graphics::RenderFlag::NONE) : flags(flags) {}
    virtual ~ObjectProxy() {}
    virtual QString toString() const;

    virtual bool update(const render::ScenePointer& scene, render::Transaction& transaction, float dt);

    virtual bool setProperties(const QVariantMap& properties);
    virtual QVariantMap getProperties();

    virtual void debugRender(render::Args* args);
    virtual void setMaterial(graphics::MaterialPointer material);

    std::function<void(const QVariant& message)> postMessage = [](const QVariant&){};
    virtual void messageReceived(const QVariant& message);

    virtual bool recomputeAABox(AABox& box) const;
    virtual bool recomputeDimensions(glm::vec3& box) const;
    virtual bool findRayIntersection(const MeshRay& meshRay, IntersectionResultRef& result);

    virtual void preload() {}
    virtual void unload() {}
};
```

Note: when handling custom AABox and Dimensions the stock calculated values are provided as input -- which a plugin can then optionally modify. Eg:
```c++
class MyObjectProxy : public ObjectProxy {
public:
    virtual bool recomputeAABox(AABox& box) const override {
        box.embiggen(10.0f); // allow incoming ray cast tests beyond the host entity's dimensions
        return true;
    }
};
```

-------

//eof
