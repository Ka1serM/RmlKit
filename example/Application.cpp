#include "Application.h"

#include <chrono>
#include <format>
#include <stop_token>
#include <thread>

#include "RmlKit/UI/Backend/Dispatcher.h"
#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Backend/MessageBoxService.h"

namespace Example {
namespace {

using RmlKit::UI::ReportLevel;

constexpr const char* platformName() {
    if (PLATFORM_WEB)
        return "Web";
    if (PLATFORM_WINDOWS)
        return "Windows";
    return "Linux";
}

}

Application::Application() : model(getModel()) {
    demo.platform = platformName();
    auto& constructor = model.getConstructor();
    auto fields = constructor.RegisterStruct<Demo>();
    fields.RegisterMember("count", &Demo::count);
    fields.RegisterMember("name", &Demo::name);
    fields.RegisterMember("working", &Demo::working);
    fields.RegisterMember("percent", &Demo::percent);
    fields.RegisterMember("platform", &Demo::platform);
    constructor.Bind("demo", &demo);

    model.bindEvent("demoAdd", &Application::onAdd, this);
    model.bindEvent("demoGreet", &Application::onGreet, this);
    model.bindEvent("demoReport", &Application::onReport, this);
    model.bindEvent("demoStartJob", &Application::onStartJob, this);
    model.bindEvent("demoStopJob", &Application::onStopJob, this);
}

Application::~Application() {
    closeDocuments();
}

std::expected<void, RmlKit::Core::Error> Application::startup() {
    getRouter().open();
    getRouter().showUrlView();
    model.report(ReportLevel::Info, "example", std::format("Started on {}.", demo.platform));
    return {};
}

std::expected<void, RmlKit::Core::Error> Application::shutdown() {
    return {};
}

void Application::onAdd(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
    if (arguments.size() != 1)
        RmlKit::UI::fatal("demoAdd takes one argument: the step");
    demo.count += arguments[0].Get<int>();
    model.dirty("demo");
}

void Application::onGreet(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
    const Rml::String name = demo.name.empty() ? Rml::String("stranger") : demo.name;
    RmlKit::UI::MessageBoxService::show({.title = "Hello", .text = std::format("Hello, {}. The counter is at {}.", name, demo.count)});
}

void Application::onReport(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
    const Rml::String level = arguments.empty() ? Rml::String() : arguments[0].Get<Rml::String>();
    if (level == "warning")
        model.report(ReportLevel::Warning, "example", "This is a warning.");
    else if (level == "error")
        model.report(ReportLevel::Error, "example", "This is an error.");
    else
        model.report(ReportLevel::Info, "example", "This is a note.");
}

//! The job runs on a worker thread. It hands each update to the UI thread with Dispatcher::postUi, which owns the model.
void Application::onStartJob(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
    if (demo.working)
        return;
    demo.working = true;
    demo.percent = 0;
    model.dirty("demo");
    const int id = ++job;
    RmlKit::UI::Dispatcher::run([this, id](const std::stop_token stop) {
        for (int percent = 1; percent <= 100; ++percent) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            if (stop.stop_requested() || id != job.load())
                return;
            RmlKit::UI::Dispatcher::postUi([this, id, percent] {
                if (id != job.load())
                    return;
                demo.percent = percent;
                if (percent == 100) {
                    demo.working = false;
                    model.report(ReportLevel::Done, "example", "The job finished.");
                }
                model.dirty("demo");
            });
        }
    });
}

void Application::onStopJob(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
    if (!demo.working)
        return;
    demo.working = false;
    ++job;
    model.report(ReportLevel::Warning, "example", std::format("The job stopped at {}%.", demo.percent));
    model.dirty("demo");
}

}
