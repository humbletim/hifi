// TODO: remove in later PR once applying corresponding changes to source files
#pragma once
#include <QtCore/QSharedPointer>

namespace scriptable {
    class ScriptableModelBase {
    public:
        QUuid objectID;
        js::Graphics::ModelPointer model;
        ScriptableModelBase(js::Graphics::ModelPointer model = js::Graphics::ModelPointer::create()) : model(model) {}
        template <typename T> void append(const T& value) { model->append(value); }
        template <typename T> void appendMaterials(const T& value) { model->appendMaterials(value); }
        template <typename ...Args> void appendMaterial(Args &&...args) { model->appendMaterial(std::forward<Args>(args)...); }
        template <typename ...Args> void appendMaterialNames(Args &&...args) { model->appendMaterialNames(std::forward<Args>(args)...); }

        operator js::Graphics::ModelPointer() { if (model && !objectID.isNull()) model->objectID = objectID; return model; }
    };
    using ModelProvider = js::Graphics::ModelProvider;
    using ModelProviderPointer = std::shared_ptr<scriptable::ModelProvider>;
    using ModelProviderFactory = js::Graphics::ModelProviderFactory;
}

