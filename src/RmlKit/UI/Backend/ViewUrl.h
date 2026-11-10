#pragma once

#include <RmlUi/Core/Types.h>

namespace RmlKit::UI {

class Router;

class ViewUrl {
public:
    explicit ViewUrl(Router& router);
    ~ViewUrl();
    ViewUrl(const ViewUrl&) = delete;
    ViewUrl& operator=(const ViewUrl&) = delete;

    Rml::String getView() const;
    void show(const Rml::String& view);
};

}
