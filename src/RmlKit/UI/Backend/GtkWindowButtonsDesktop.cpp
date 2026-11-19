#include "RmlKit/UI/Backend/GtkWindowButtons.h"

#if PLATFORM_LINUX

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string_view>
#include <utility>

#include <dlfcn.h>
#include <plutovg.h>

namespace RmlKit::UI {

namespace {

struct GdkRGBA {
    float red, green, blue, alpha;
};

struct GtkBorder {
    std::int16_t left, right, top, bottom;
};

struct GraphenePoint {
    float x, y;
};

constexpr int horizontal = 0;
constexpr int vertical = 1;
constexpr int packEnd = 1;
constexpr int stateActive = 1 << 0;
constexpr int statePrelight = 1 << 1;
constexpr int stateBackdrop = 1 << 6;
constexpr int cairoArgb32 = 0;
//! The images hold two pixels per dp, as the @2 assets of a theme do.
constexpr int imageScale = 2;

//! The GTK, GDK, GSK, GObject and cairo functions that the drawing uses, found in the libraries GTK loads with it.
struct Gtk {
    void* library = nullptr;
    void (*disableSetlocale)();
    int (*initCheck)();
    void (*adwInit)();
    void* (*windowNew)();
    void (*windowDestroy)(void*);
    void (*windowSetTitlebar)(void*, void*);
    void* (*headerBarNew)();
    void (*headerBarSetShowTitleButtons)(void*, int);
    void (*headerBarPackEnd)(void*, void*);
    void* (*windowControlsNew)(int);
    void (*windowControlsSetDecorationLayout)(void*, const char*);
    void* (*firstChild)(void*);
    void* (*nextSibling)(void*);
    int (*hasCssClass)(void*, const char*);
    void (*addCssClass)(void*, const char*);
    void (*removeCssClass)(void*, const char*);
    void (*setStateFlags)(void*, int, int);
    void (*unsetStateFlags)(void*, int);
    void (*measure)(void*, int, int, int*, int*, int*, int*);
    void* (*styleContext)(void*);
    void (*contextMargin)(void*, GtkBorder*);
    void (*contextBorder)(void*, GtkBorder*);
    void (*contextPadding)(void*, GtkBorder*);
    void (*contextColor)(void*, GdkRGBA*);
    void* (*snapshotNew)();
    void* (*snapshotFreeToNode)(void*);
    void (*snapshotSave)(void*);
    void (*snapshotRestore)(void*);
    void (*snapshotTranslate)(void*, const GraphenePoint*);
    void (*renderBackground)(void*, void*, double, double, double, double);
    void (*renderFrame)(void*, void*, double, double, double, double);
    void (*nodeDraw)(void*, void*);
    void (*nodeUnref)(void*);
    void* (*displayDefault)();
    void* (*iconThemeForDisplay)(void*);
    void* (*lookupIcon)(void*, const char*, const char**, int, int, int, int);
    void (*snapshotSymbolic)(void*, void*, double, double, const GdkRGBA*, std::size_t);
    const char* (*imageIconName)(void*);
    void (*objectUnref)(void*);
    void* (*surfaceCreate)(int, int, int);
    void* (*cairoCreate)(void*);
    void (*cairoScale)(void*, double, double);
    void (*cairoDestroy)(void*);
    void (*surfaceFlush)(void*);
    unsigned char* (*surfaceData)(void*);
    int (*surfaceStride)(void*);
    void (*surfaceDestroy)(void*);

    template <class Function>
    bool find(Function& function, const char* name) {
        function = reinterpret_cast<Function>(dlsym(library, name));
        return function != nullptr;
    }

    bool load() {
        library = dlopen("libadwaita-1.so.0", RTLD_LAZY | RTLD_LOCAL);
        if (!library)
            library = dlopen("libgtk-4.so.1", RTLD_LAZY | RTLD_LOCAL);
        if (!library)
            return false;
        if (!find(adwInit, "adw_init"))
            adwInit = nullptr;
        return find(disableSetlocale, "gtk_disable_setlocale") && find(initCheck, "gtk_init_check") && find(windowNew, "gtk_window_new")
               && find(windowDestroy, "gtk_window_destroy") && find(windowSetTitlebar, "gtk_window_set_titlebar") && find(headerBarNew, "gtk_header_bar_new")
               && find(headerBarSetShowTitleButtons, "gtk_header_bar_set_show_title_buttons") && find(headerBarPackEnd, "gtk_header_bar_pack_end")
               && find(windowControlsNew, "gtk_window_controls_new") && find(windowControlsSetDecorationLayout, "gtk_window_controls_set_decoration_layout")
               && find(firstChild, "gtk_widget_get_first_child") && find(nextSibling, "gtk_widget_get_next_sibling")
               && find(hasCssClass, "gtk_widget_has_css_class") && find(addCssClass, "gtk_widget_add_css_class")
               && find(removeCssClass, "gtk_widget_remove_css_class") && find(setStateFlags, "gtk_widget_set_state_flags")
               && find(unsetStateFlags, "gtk_widget_unset_state_flags") && find(measure, "gtk_widget_measure")
               && find(styleContext, "gtk_widget_get_style_context") && find(contextMargin, "gtk_style_context_get_margin")
               && find(contextBorder, "gtk_style_context_get_border") && find(contextPadding, "gtk_style_context_get_padding")
               && find(contextColor, "gtk_style_context_get_color") && find(snapshotNew, "gtk_snapshot_new")
               && find(snapshotFreeToNode, "gtk_snapshot_free_to_node") && find(snapshotSave, "gtk_snapshot_save")
               && find(snapshotRestore, "gtk_snapshot_restore") && find(snapshotTranslate, "gtk_snapshot_translate")
               && find(renderBackground, "gtk_snapshot_render_background") && find(renderFrame, "gtk_snapshot_render_frame")
               && find(nodeDraw, "gsk_render_node_draw") && find(nodeUnref, "gsk_render_node_unref") && find(displayDefault, "gdk_display_get_default")
               && find(iconThemeForDisplay, "gtk_icon_theme_get_for_display") && find(lookupIcon, "gtk_icon_theme_lookup_icon")
               && find(snapshotSymbolic, "gtk_symbolic_paintable_snapshot_symbolic") && find(imageIconName, "gtk_image_get_icon_name")
               && find(objectUnref, "g_object_unref") && find(surfaceCreate, "cairo_image_surface_create") && find(cairoCreate, "cairo_create")
               && find(cairoScale, "cairo_scale") && find(cairoDestroy, "cairo_destroy") && find(surfaceFlush, "cairo_surface_flush")
               && find(surfaceData, "cairo_image_surface_get_data") && find(surfaceStride, "cairo_image_surface_get_stride")
               && find(surfaceDestroy, "cairo_surface_destroy");
    }
};

struct Box {
    double x, y, width, height;
};

struct Size {
    int width, height;
};

Size naturalSize(Gtk& gtk, void* widget) {
    int width = 0;
    int height = 0;
    gtk.measure(widget, horizontal, -1, nullptr, &width, nullptr, nullptr);
    gtk.measure(widget, vertical, width, nullptr, &height, nullptr, nullptr);
    return {width, height};
}

//! Horizontal margin, border and padding of a widget, which its measured size holds around its children.
int horizontalFrame(Gtk& gtk, void* widget) {
    GtkBorder margin{}, border{}, padding{};
    void* context = gtk.styleContext(widget);
    gtk.contextMargin(context, &margin);
    gtk.contextBorder(context, &border);
    gtk.contextPadding(context, &padding);
    return margin.left + margin.right + border.left + border.right + padding.left + padding.right;
}

//! Draws the background and the frame of a widget into its margin box and returns its content box.
Box renderBox(Gtk& gtk, void* snapshot, void* widget, const Box area) {
    GtkBorder margin{}, border{}, padding{};
    void* context = gtk.styleContext(widget);
    gtk.contextMargin(context, &margin);
    gtk.contextBorder(context, &border);
    gtk.contextPadding(context, &padding);
    const Box frame{area.x + margin.left, area.y + margin.top, area.width - margin.left - margin.right, area.height - margin.top - margin.bottom};
    gtk.renderBackground(snapshot, context, frame.x, frame.y, frame.width, frame.height);
    gtk.renderFrame(snapshot, context, frame.x, frame.y, frame.width, frame.height);
    return {frame.x + border.left + padding.left, frame.y + border.top + padding.top, frame.width - border.left - border.right - padding.left - padding.right,
            frame.height - border.top - border.bottom - padding.top - padding.bottom};
}

void renderButton(Gtk& gtk, void* snapshot, void* button, const Size size, const char* iconName) {
    const Box content = renderBox(gtk, snapshot, button, {0.0, 0.0, double(size.width), double(size.height)});
    void* image = gtk.firstChild(button);
    if (!image)
        return;
    const Size imageSize = naturalSize(gtk, image);
    const Box imageArea{content.x + (content.width - imageSize.width) * 0.5, content.y + (content.height - imageSize.height) * 0.5, double(imageSize.width),
                        double(imageSize.height)};
    const Box icon = renderBox(gtk, snapshot, image, imageArea);
    const char* name = iconName ? iconName : gtk.imageIconName(image);
    const int iconSize = static_cast<int>(std::floor(std::min(icon.width, icon.height)));
    if (!name || iconSize <= 0)
        return;
    void* paintable = gtk.lookupIcon(gtk.iconThemeForDisplay(gtk.displayDefault()), name, nullptr, iconSize, imageScale, 0, 0);
    if (!paintable)
        return;
    GdkRGBA color{};
    gtk.contextColor(gtk.styleContext(image), &color);
    gtk.snapshotSave(snapshot);
    const GraphenePoint offset{float(icon.x + (icon.width - iconSize) * 0.5), float(icon.y + (icon.height - iconSize) * 0.5)};
    gtk.snapshotTranslate(snapshot, &offset);
    gtk.snapshotSymbolic(paintable, snapshot, iconSize, iconSize, &color, 1);
    gtk.snapshotRestore(snapshot);
    gtk.objectUnref(paintable);
}

void writePng(void* closure, void* data, const int size) {
    static_cast<std::string*>(closure)->append(static_cast<const char*>(data), static_cast<std::size_t>(size));
}

//! Draws the render node into a PNG with imageScale pixels per dp.
std::optional<std::string> encode(Gtk& gtk, void* node, const Size size) {
    const int width = size.width * imageScale;
    const int height = size.height * imageScale;
    void* surface = gtk.surfaceCreate(cairoArgb32, width, height);
    void* cairo = gtk.cairoCreate(surface);
    gtk.cairoScale(cairo, imageScale, imageScale);
    gtk.nodeDraw(node, cairo);
    gtk.cairoDestroy(cairo);
    gtk.surfaceFlush(surface);
    plutovg_surface_t* image = plutovg_surface_create(width, height);
    const unsigned char* source = gtk.surfaceData(surface);
    const int sourceStride = gtk.surfaceStride(surface);
    unsigned char* target = plutovg_surface_get_data(image);
    const int targetStride = plutovg_surface_get_stride(image);
    for (int y = 0; y < height; ++y)
        std::memcpy(target + std::ptrdiff_t(y) * targetStride, source + std::ptrdiff_t(y) * sourceStride, std::size_t(width) * 4);
    gtk.surfaceDestroy(surface);
    std::string png;
    const bool written = plutovg_surface_write_to_png_stream(image, writePng, &png);
    plutovg_surface_destroy(image);
    return written ? std::optional(std::move(png)) : std::nullopt;
}

struct State {
    const char* suffix;
    int buttonFlags;
    bool backdrop;
    const char* selector;
};

constexpr std::array<State, 5> states{{
    {"", 0, false, ""},
    {"-hover", statePrelight, false, ":hover"},
    {"-active", statePrelight | stateActive, false, ":active"},
    {"-backdrop", 0, true, ""},
    {"-backdrop-hover", statePrelight, true, ":hover"},
}};

}

std::optional<GtkWindowButtons> renderGtkWindowButtons() {
    //! GTK 3 and GTK 4 cannot share a process, and a file dialog or a plugin may have loaded GTK 3.
    if (dlopen("libgtk-3.so.0", RTLD_LAZY | RTLD_NOLOAD))
        return std::nullopt;
    static Gtk gtk;
    if (!gtk.load())
        return std::nullopt;
    //! GTK would otherwise set the C locale from the environment, and number parsing in RmlUi depends on it.
    gtk.disableSetlocale();
    //! The window is never shown, so GTK need not connect to the accessibility bus.
    setenv("GTK_A11Y", "none", 0);
    if (!gtk.initCheck())
        return std::nullopt;
    if (gtk.adwInit)
        gtk.adwInit();
    void* window = gtk.windowNew();
    void* header = gtk.headerBarNew();
    gtk.headerBarSetShowTitleButtons(header, 0);
    gtk.windowSetTitlebar(window, header);
    void* controls = gtk.windowControlsNew(packEnd);
    gtk.headerBarPackEnd(header, controls);
    gtk.windowControlsSetDecorationLayout(controls, ":minimize,maximize,close");

    GtkWindowButtons result;
    std::string rules;
    int count = 0;
    int buttonsWidth = 0;
    Size largest{0, 0};
    for (void* button = gtk.firstChild(controls); button; button = gtk.nextSibling(button)) {
        const char* name = gtk.hasCssClass(button, "close") ? "close" : gtk.hasCssClass(button, "minimize") ? "minimize" : gtk.hasCssClass(button, "maximize") ? "maximize" : nullptr;
        if (!name)
            continue;
        const Size size = naturalSize(gtk, button);
        if (size.width <= 0 || size.height <= 0)
            continue;
        ++count;
        buttonsWidth += size.width;
        largest = {std::max(largest.width, size.width), std::max(largest.height, size.height)};
        const bool maximize = std::string_view(name) == "maximize";
        for (const bool maximized : {false, true}) {
            if (maximized && !maximize)
                continue;
            if (maximized)
                gtk.addCssClass(window, "maximized");
            const std::string file = maximized ? "restore" : name;
            for (const State& state : states) {
                if (state.backdrop)
                    gtk.setStateFlags(window, stateBackdrop, 0);
                gtk.setStateFlags(button, state.buttonFlags, 0);
                void* snapshot = gtk.snapshotNew();
                renderButton(gtk, snapshot, button, size, maximized ? "window-restore-symbolic" : nullptr);
                void* node = gtk.snapshotFreeToNode(snapshot);
                gtk.unsetStateFlags(button, stateActive | statePrelight);
                gtk.unsetStateFlags(window, stateBackdrop);
                std::optional<std::string> png = node ? encode(gtk, node, size) : std::nullopt;
                if (node)
                    gtk.nodeUnref(node);
                if (!png)
                    continue;
                const std::string image = file + state.suffix + ".png";
                result.files.emplace(image, std::move(*png));
                rules += std::format(".window-buttons.gtk{}{} .title-button.{}{} {{ width: {}dp; height: {}dp; decorator: image(/UI/Platform/WindowButtons/{}); }}\n",
                                     maximized ? ".maximized" : "", state.backdrop ? ".unfocused" : "", name, state.selector, size.width, size.height, image);
            }
            if (maximized)
                gtk.removeCssClass(window, "maximized");
        }
    }
    const int frame = horizontalFrame(gtk, controls);
    const int gap = count > 1 ? std::max(0, (naturalSize(gtk, controls).width - frame - buttonsWidth) / (count - 1)) : 0;
    gtk.windowDestroy(window);
    if (count == 0)
        return std::nullopt;
    result.files.emplace("style.rcss", std::format(".window-buttons.gtk {{ gap: {}dp; padding: 0 {}dp; min-height: {}dp; }}\n"
                                                   ".window-buttons.gtk .title-button {{ border-radius: 0; background-color: transparent; }}\n"
                                                   ".window-buttons.gtk .title-button svg {{ display: none; }}\n{}",
                                                   gap, std::max(frame / 2, 8), largest.height, rules));
    return result;
}

}

#else

namespace RmlKit::UI {

std::optional<GtkWindowButtons> renderGtkWindowButtons() {
    return std::nullopt;
}

}

#endif
