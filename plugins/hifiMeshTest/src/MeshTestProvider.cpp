// example pseudo-InputPlugin DLL (registers plugin:://entity::Shape::Proxy as an object plugin)

#include <QtCore/QDebug>
#include <QtCore/QObject>
#include <QtCore/QThread>

#include <plugins/RuntimePlugin.h>
#include <plugins/InputPlugin.h>

#include <object-plugins/Forward.h>
#include <DependencyManager.h>

#include "MeshTestProvider.h"
#include "mesh/ObjectPlugin.h"
#include "mesh/ObjectProxy.h"

Q_LOGGING_CATEGORY(mesh_test, ID)

namespace {
    const std::string SHAPE_PROXY_PLUGIN_URI{ "plugin://entity::Shape::Proxy" };
    class DummyInputPlugin : public InputPlugin {
    public:
        DummyInputPlugin() : InputPlugin() {}
        // InputPlugin overrides
        bool isSupported() const override { return true; }
        const QString getName() const override { return ID; }
        void pluginUpdate(float deltaTime, const controller::InputCalibrationData& inputCalibrationData) override {}
        void pluginFocusOutEvent() override {}
        void init() override {
            qCDebug(mesh_test) << "DummyInputPlugin::init...";
            auto proxyManager = DependencyManager::get<plugins::entity::ProxyManager>();
            if (proxyManager) {
                qCDebug(mesh_test) << "REGISTERING mesh::ObjectPlugin -> MeshEntityPlugin" << SHAPE_PROXY_PLUGIN_URI.c_str();
                proxyManager->setPluginResolver(SHAPE_PROXY_PLUGIN_URI, std::make_shared<MeshEntityPlugin>());
            }
        }
        void deinit() override { qCDebug(mesh_test) << "DummyInputPlugin::deinit"; }
    };
}

class MeshTestProvider : public QObject, public InputProvider {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID InputProvider_iid FILE "plugin.json")
    Q_INTERFACES(InputProvider)

public:
    std::shared_ptr<DummyInputPlugin> manager;
    MeshTestProvider() : QObject(), manager(new DummyInputPlugin()) {
    }
    virtual InputPluginList getInputPlugins() override {
        // note: need to return std::vector<QSharedPointer<InputPlugin>>, but DependencyManager uses std::shared_ptr.
        // we are a singleton anyhow so this creates an indestructable QSharedPointer instance instead
        InputPluginPointer softPointer{ manager.get(), [=](InputPlugin*) { /* no-op */ }};
        return { softPointer };
    }
    virtual void destroyInputPlugins() override { manager.reset(); }
};
    

#include "MeshTestProvider.moc"
