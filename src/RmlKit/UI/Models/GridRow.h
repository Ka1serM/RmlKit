#pragma once

#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace RmlKit::UI {

template <typename Item>
struct GridRow {
    float height = 0;
    float widthPercent = 0;
    bool header = false;
    Rml::String icon;
    Rml::String title;
    Rml::String countText;
    bool collapsed = false;
    std::vector<Item> items;
    int emptyColumns = 0;
    float emptyGaps = 0;
};

template <typename Item>
struct GridModel {
    int itemCount = 0;
    std::vector<GridRow<Item>> rows;
    Rml::String scrollTo;
};

template <typename Item>
void registerGridTypes(Rml::DataModelConstructor& model) {
    if (auto handle = model.RegisterStruct<GridRow<Item>>()) {
        handle.RegisterMember("height", &GridRow<Item>::height);
        handle.RegisterMember("widthPercent", &GridRow<Item>::widthPercent);
        handle.RegisterMember("header", &GridRow<Item>::header);
        handle.RegisterMember("icon", &GridRow<Item>::icon);
        handle.RegisterMember("title", &GridRow<Item>::title);
        handle.RegisterMember("countText", &GridRow<Item>::countText);
        handle.RegisterMember("collapsed", &GridRow<Item>::collapsed);
        handle.RegisterMember("items", &GridRow<Item>::items);
        handle.RegisterMember("emptyColumns", &GridRow<Item>::emptyColumns);
        handle.RegisterMember("emptyGaps", &GridRow<Item>::emptyGaps);
    }
    model.RegisterArray<std::vector<GridRow<Item>>>();
    if (auto handle = model.RegisterStruct<GridModel<Item>>()) {
        handle.RegisterMember("itemCount", &GridModel<Item>::itemCount);
        handle.RegisterMember("rows", &GridModel<Item>::rows);
        handle.RegisterMember("scrollTo", &GridModel<Item>::scrollTo);
    }
}

}
