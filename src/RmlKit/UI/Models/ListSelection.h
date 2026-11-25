#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <RmlUi/Core/Event.h>

namespace RmlKit::UI {

//! How a click in a list changes the selection: Mod adds or removes one item, Shift selects from the anchor, both add the range.
struct ListClick {
    bool toggle = false;
    bool range = false;
};

//! Mod is Cmd on macOS and Ctrl elsewhere, as for the hotkeys.
inline ListClick listClickOf(const Rml::Event& event) {
#if defined(__APPLE__)
    const bool toggle = event.GetParameter<int>("meta_key", 0) != 0;
#else
    const bool toggle = event.GetParameter<int>("ctrl_key", 0) != 0;
#endif
    return {toggle, event.GetParameter<int>("shift_key", 0) != 0};
}

//! The selected items of a list, as stable keys; the last item a click selected is the primary one, which an inspector shows.
template <class Key>
class ListSelection {
public:
    //! The order is the list as shown, so a range follows what the user sees.
    void click(std::span<const Key> order, const std::size_t index, const ListClick how) {
        const Key& clicked = order[index];
        const auto anchorAt = anchor ? std::ranges::find(order, *anchor) : order.end();
        if (how.range && anchorAt != order.end()) {
            if (!how.toggle)
                keys.clear();
            const std::size_t from = static_cast<std::size_t>(anchorAt - order.begin());
            const std::size_t first = std::min(from, index), last = std::max(from, index);
            for (std::size_t at = first; at <= last; ++at)
                add(order[at]);
            add(clicked);
            return;
        }
        anchor = clicked;
        if (!how.toggle) {
            keys.assign(1, clicked);
            return;
        }
        if (std::erase(keys, clicked) == 0)
            keys.push_back(clicked);
    }

    void selectOnly(const Key& key) {
        keys.assign(1, key);
        anchor = key;
    }

    void selectAll(std::span<const Key> order) {
        keys.assign(order.begin(), order.end());
        anchor = order.empty() ? std::nullopt : std::optional<Key>(order.front());
    }

    void set(std::vector<Key> selected) {
        keys = std::move(selected);
        anchor = keys.empty() ? std::nullopt : std::optional<Key>(keys.back());
    }

    void clear() {
        keys.clear();
        anchor.reset();
    }

    //! Drops the keys whose items are gone.
    template <class Predicate>
    void keepIf(Predicate exists) {
        std::erase_if(keys, [&](const Key& key) { return !exists(key); });
        if (anchor && !exists(*anchor))
            anchor.reset();
    }

    bool contains(const Key& key) const { return std::ranges::contains(keys, key); }
    bool empty() const { return keys.empty(); }
    std::size_t size() const { return keys.size(); }
    const std::vector<Key>& items() const { return keys; }
    std::optional<Key> primary() const { return keys.empty() ? std::nullopt : std::optional<Key>(keys.back()); }

private:
    //! Moves a selected key to the end, so the last range item stays primary.
    void add(const Key& key) {
        std::erase(keys, key);
        keys.push_back(key);
    }

    std::vector<Key> keys;
    std::optional<Key> anchor;
};

}
