#pragma once

#include <atomic>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Types.h>
#include <RmlUi/Core/Variant.h>

#include "RmlKit/UI/Backend/Application.h"

namespace Example {

//! What the views show. The data model reads these fields; a change shows after Model::dirty("demo").
struct Demo {
    int count = 0;
    Rml::String name;
    bool working = false;
    int percent = 0;
    Rml::String platform;
};

class Application final : public RmlKit::UI::Application {
public:
    Application();
    ~Application() override;

    std::expected<void, RmlKit::Core::Error> startup() override;
    std::expected<void, RmlKit::Core::Error> shutdown() override;

private:
    void onAdd(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments);
    void onGreet(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments);
    void onReport(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments);
    void onStartJob(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments);
    void onStopJob(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments);

    RmlKit::UI::Model& model;
    Demo demo;
    //! Each start or stop takes a new number; a job that sees another number has been stopped or replaced.
    std::atomic<int> job = 0;
};

}
