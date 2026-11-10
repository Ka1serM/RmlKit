#include "RmlKit/UI/Backend/MessageBoxService.h"

#include <format>
#include <utility>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>

#include "RmlKit/UI/Backend/Dispatcher.h"
#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Models/Model.h"

namespace RmlKit::UI {

namespace {

constexpr const char* documentPath = "UI/RmlKit/Documents/message-box.rml";
constexpr const char* documentId = "message-box";

}

MessageBoxService::MessageBoxService(Rml::Context& context, Model& model) : context(context), model(model) {
    if (instance)
        fatal("A message box service already exists");
    instance = this;
    model.bindEvent("closeMessageBox", &MessageBoxService::onClose, this);
    model.bindEvent("messageBoxKey", &MessageBoxService::onKey, this);
}

MessageBoxService::~MessageBoxService() {
    if (document)
        document->Close();
    instance = nullptr;
}

MessageBoxService& MessageBoxService::getInstance() {
    if (!instance)
        fatal("The message box service does not exist");
    return *instance;
}

void MessageBoxService::show(Message message) {
    Dispatcher::postUi([message = std::move(message)]() mutable { getInstance().add(std::move(message)); });
}

void MessageBoxService::add(Message message) {
    waiting.push_back(std::move(message));
    if (waiting.size() == 1)
        present();
}

void MessageBoxService::present() {
    const Message& message = waiting.front();
    MessageBoxModel& shown = model.messageBox;
    switch (message.level) {
    case MessageLevel::Info:
        shown.level = "info";
        shown.icon = "info";
        break;
    case MessageLevel::Warning:
        shown.level = "warning";
        shown.icon = "triangle-alert";
        break;
    case MessageLevel::Error:
        shown.level = "error";
        shown.icon = "circle-x";
        break;
    }
    shown.title = message.title;
    shown.text = message.text;
    model.dirty("messageBox");
    if (document)
        return;
    document = context.LoadDocument(documentPath);
    if (!document || document->GetId() != documentId)
        fatal(std::format("{} is not the message box: its <body> needs id=\"{}\"", documentPath, documentId));
    document->Show(Rml::ModalFlag::Modal, Rml::FocusFlag::Auto);
}

void MessageBoxService::close() {
    if (waiting.empty())
        return;
    waiting.pop_front();
    if (!waiting.empty())
        return present();
    //! RmlUi deletes a closed document in its next update, so the element of the event stays alive.
    document->Close();
    document = nullptr;
}

void MessageBoxService::onClose(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
    if (!arguments.empty())
        fatal("closeMessageBox takes no argument");
    close();
}

void MessageBoxService::onKey(Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
    if (!arguments.empty())
        fatal("messageBoxKey takes no argument");
    const int key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
    if (key != Rml::Input::KI_RETURN && key != Rml::Input::KI_NUMPADENTER && key != Rml::Input::KI_ESCAPE)
        return;
    event.StopPropagation();
    close();
}

}
