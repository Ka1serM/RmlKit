#pragma once

#include <deque>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Types.h>

namespace Rml {
class Context;
class ElementDocument;
}

namespace RmlKit::UI {

class Model;

enum class MessageLevel { Info, Warning, Error };

struct Message {
    MessageLevel level = MessageLevel::Info;
    Rml::String title;
    Rml::String text;
};

class MessageBoxService {
public:
    MessageBoxService(Rml::Context& context, Model& model);
    ~MessageBoxService();
    MessageBoxService(const MessageBoxService&) = delete;
    MessageBoxService& operator=(const MessageBoxService&) = delete;

    static void show(Message message);

private:
    static MessageBoxService& getInstance();

    void add(Message message);
    void present();
    void close();
    void onClose(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments);
    void onKey(Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments);

    static inline MessageBoxService* instance = nullptr;

    Rml::Context& context;
    Model& model;
    std::deque<Message> waiting;
    Rml::ElementDocument* document = nullptr;
};

}
