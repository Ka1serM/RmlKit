#include "RmlKit/UI/Backend/LinkDrag.h"

#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <SDL3/SDL.h>

#include "RmlKit/Core/Error.h"
#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Models/Model.h"

#if PLATFORM_LINUX
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>

#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <wayland-client.h>
#elif PLATFORM_WINDOWS
#include <ole2.h>
#include <shlobj.h>
#include <wrl/client.h>
#endif

namespace RmlKit::UI {

#if PLATFORM_LINUX

//! The drag has its own queue and thread: the main loop sleeps until an SDL event while a drop target asks for the link.
class WaylandDragSource {
public:
    WaylandDragSource(wl_display* display, wl_surface* surface) : display(display), surface(surface) {
        queue = wl_display_create_queue(display);
        displayOnQueue = static_cast<wl_display*>(wl_proxy_create_wrapper(display));
        wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(displayOnQueue), queue);
        registry = wl_display_get_registry(displayOnQueue);
        wl_registry_add_listener(registry, &registryListener, this);
        wl_display_roundtrip_queue(display, queue);
        if (!seat)
            fatal("The Wayland compositor has no wl_seat version 5, so the app cannot drag a link.");
        if (!manager)
            fatal("The Wayland compositor has no wl_data_device_manager version 3, so the app cannot drag a link.");
        wl_seat_add_listener(seat, &seatListener, this);
        device = wl_data_device_manager_get_data_device(manager, seat);
        wl_data_device_add_listener(device, &deviceListener, this);
        wl_display_roundtrip_queue(display, queue);
        stopEvent = eventfd(0, EFD_CLOEXEC);
        if (stopEvent < 0)
            fatal(std::format("eventfd failed: {}", std::strerror(errno)));
        thread = std::thread([this] { dispatch(); });
    }

    ~WaylandDragSource() {
        const std::uint64_t stop = 1;
        if (write(stopEvent, &stop, sizeof(stop)) != sizeof(stop))
            std::terminate();
        thread.join();
        close(stopEvent);
        if (source)
            wl_data_source_destroy(source);
        wl_data_device_release(device);
        if (pointer)
            wl_pointer_release(pointer);
        wl_seat_release(seat);
        wl_data_device_manager_destroy(manager);
        wl_registry_destroy(registry);
        wl_proxy_wrapper_destroy(displayOnQueue);
        wl_event_queue_destroy(queue);
    }

    WaylandDragSource(const WaylandDragSource&) = delete;
    WaylandDragSource& operator=(const WaylandDragSource&) = delete;

    std::expected<void, Core::Error> start(std::string link) {
        const std::uint32_t serial = pressSerial.load();
        if (serial == 0)
            return Core::fail("drag", "The drag has no press: Wayland did not report the pointer button.");
        const std::scoped_lock lock(mutex);
        if (source)
            wl_data_source_destroy(source);
        this->link = std::move(link);
        source = wl_data_device_manager_create_data_source(manager);
        wl_data_source_add_listener(source, &sourceListener, this);
        wl_data_source_offer(source, "text/plain;charset=utf-8");
        wl_data_source_offer(source, "text/plain");
        wl_data_source_set_actions(source, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
        wl_data_device_start_drag(device, source, surface, nullptr, serial);
        wl_display_flush(display);
        return {};
    }

private:
    //! Wayland reads only when all prepared threads read, so this thread and SDL's main loop take turns.
    void dispatch() {
        while (true) {
            while (wl_display_prepare_read_queue(display, queue) != 0)
                wl_display_dispatch_queue_pending(display, queue);
            wl_display_flush(display);
            pollfd events[] = {{wl_display_get_fd(display), POLLIN, 0}, {stopEvent, POLLIN, 0}};
            if (poll(events, 2, -1) < 0 || events[1].revents) {
                wl_display_cancel_read(display);
                if (events[1].revents)
                    return;
                continue;
            }
            if (events[0].revents & POLLIN)
                wl_display_read_events(display);
            else
                wl_display_cancel_read(display);
            wl_display_dispatch_queue_pending(display, queue);
        }
    }

    void send(const char* mimeType, const int fd) {
        std::string text;
        {
            const std::scoped_lock lock(mutex);
            if (std::string_view(mimeType).starts_with("text/plain"))
                text = link;
        }
        for (std::size_t written = 0; written < text.size();) {
            const ssize_t count = write(fd, text.data() + written, text.size() - written);
            if (count < 0 && errno == EINTR)
                continue;
            if (count <= 0)
                break;
            written += static_cast<std::size_t>(count);
        }
        close(fd);
    }

    void finish(wl_data_source* finished) {
        const std::scoped_lock lock(mutex);
        if (finished != source)
            return;
        wl_data_source_destroy(source);
        source = nullptr;
    }

    static WaylandDragSource& self(void* data) { return *static_cast<WaylandDragSource*>(data); }

    static const wl_registry_listener registryListener;
    static const wl_seat_listener seatListener;
    static const wl_pointer_listener pointerListener;
    static const wl_data_device_listener deviceListener;
    static const wl_data_source_listener sourceListener;

    wl_display* display;
    wl_surface* surface;
    wl_event_queue* queue = nullptr;
    wl_display* displayOnQueue = nullptr;
    wl_registry* registry = nullptr;
    wl_seat* seat = nullptr;
    wl_pointer* pointer = nullptr;
    wl_data_device_manager* manager = nullptr;
    wl_data_device* device = nullptr;
    std::atomic<std::uint32_t> pressSerial = 0;
    std::mutex mutex;
    wl_data_source* source = nullptr;
    std::string link;
    int stopEvent = -1;
    std::thread thread;
};

const wl_registry_listener WaylandDragSource::registryListener{
    .global =
        [](void* data, wl_registry* registry, const std::uint32_t name, const char* interface, const std::uint32_t version) {
            WaylandDragSource& drag = self(data);
            const std::string_view announced(interface);
            if (announced == wl_seat_interface.name && !drag.seat && version >= 5)
                drag.seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 5));
            if (announced == wl_data_device_manager_interface.name && !drag.manager && version >= 3)
                drag.manager = static_cast<wl_data_device_manager*>(wl_registry_bind(registry, name, &wl_data_device_manager_interface, 3));
        },
    .global_remove = [](void*, wl_registry*, std::uint32_t) {},
};

const wl_seat_listener WaylandDragSource::seatListener{
    .capabilities =
        [](void* data, wl_seat* seat, const std::uint32_t capabilities) {
            WaylandDragSource& drag = self(data);
            if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) || drag.pointer)
                return;
            drag.pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(drag.pointer, &pointerListener, data);
        },
    .name = [](void*, wl_seat*, const char*) {},
};

//! Version 5: the headers of Wayland 1.20 (Ubuntu 22.04) have no members for the events of later versions.
const wl_pointer_listener WaylandDragSource::pointerListener{
    .enter = [](void*, wl_pointer*, std::uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t) {},
    .leave = [](void*, wl_pointer*, std::uint32_t, wl_surface*) {},
    .motion = [](void*, wl_pointer*, std::uint32_t, wl_fixed_t, wl_fixed_t) {},
    .button =
        [](void* data, wl_pointer*, const std::uint32_t serial, std::uint32_t, std::uint32_t, const std::uint32_t state) {
            if (state == WL_POINTER_BUTTON_STATE_PRESSED)
                self(data).pressSerial = serial;
        },
    .axis = [](void*, wl_pointer*, std::uint32_t, std::uint32_t, wl_fixed_t) {},
    .frame = [](void*, wl_pointer*) {},
    .axis_source = [](void*, wl_pointer*, std::uint32_t) {},
    .axis_stop = [](void*, wl_pointer*, std::uint32_t, std::uint32_t) {},
    .axis_discrete = [](void*, wl_pointer*, std::uint32_t, std::int32_t) {},
};

const wl_data_device_listener WaylandDragSource::deviceListener{
    .data_offer = [](void*, wl_data_device*, wl_data_offer* offer) { wl_data_offer_destroy(offer); },
    .enter = [](void*, wl_data_device*, std::uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t, wl_data_offer*) {},
    .leave = [](void*, wl_data_device*) {},
    .motion = [](void*, wl_data_device*, std::uint32_t, wl_fixed_t, wl_fixed_t) {},
    .drop = [](void*, wl_data_device*) {},
    .selection = [](void*, wl_data_device*, wl_data_offer*) {},
};

const wl_data_source_listener WaylandDragSource::sourceListener{
    .target = [](void*, wl_data_source*, const char*) {},
    .send = [](void* data, wl_data_source*, const char* mimeType, const int fd) { self(data).send(mimeType, fd); },
    .cancelled = [](void* data, wl_data_source* source) { self(data).finish(source); },
    .dnd_drop_performed = [](void*, wl_data_source*) {},
    .dnd_finished = [](void* data, wl_data_source* source) { self(data).finish(source); },
    .action = [](void*, wl_data_source*, std::uint32_t) {},
};

namespace {

//! The Linux build of SDL has only its Wayland video driver.
wl_display* waylandDisplay() {
    void* display = SDL_GetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, nullptr);
    if (!display)
        fatal(std::format("SDL gives no Wayland display: it uses the {} video driver.", SDL_GetCurrentVideoDriver()));
    return static_cast<wl_display*>(display);
}

wl_surface* waylandSurface(SDL_Window* window) {
    void* surface = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
    if (!surface)
        fatal("SDL gives no Wayland surface for the window.");
    return static_cast<wl_surface*>(surface);
}

}

struct LinkDrag::Native {
    explicit Native(SDL_Window* window) : wayland(waylandDisplay(), waylandSurface(window)) {}

    std::expected<void, Core::Error> start(std::string link) { return wayland.start(std::move(link)); }

    WaylandDragSource wayland;
    Rml::String pressedLink;
    float pressX = 0;
    float pressY = 0;
};

#elif PLATFORM_WINDOWS

namespace {

class DropSource final : public IDropSource {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override {
        if (id != IID_IUnknown && id != IID_IDropSource) {
            *object = nullptr;
            return E_NOINTERFACE;
        }
        *object = static_cast<IDropSource*>(this);
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(const BOOL escape, const DWORD keys) override {
        if (escape)
            return DRAGDROP_S_CANCEL;
        if (!(keys & MK_LBUTTON))
            return DRAGDROP_S_DROP;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};

std::expected<void, Core::Error> check(const HRESULT result, const char* call) {
    if (FAILED(result))
        return Core::fail("drag", std::format("{} failed: 0x{:08x}", call, static_cast<unsigned long>(result)));
    return {};
}

}

struct LinkDrag::Native {
    explicit Native(SDL_Window*) {
        if (auto initialized = check(OleInitialize(nullptr), "OleInitialize"); !initialized)
            fatal(initialized.error().message);
    }
    ~Native() { OleUninitialize(); }
    Native(const Native&) = delete;
    Native& operator=(const Native&) = delete;

    std::expected<void, Core::Error> start(const std::string& link) {
        const int length = MultiByteToWideChar(CP_UTF8, 0, link.data(), static_cast<int>(link.size()), nullptr, 0);
        HGLOBAL text = GlobalAlloc(GMEM_MOVEABLE, (static_cast<SIZE_T>(length) + 1) * sizeof(wchar_t));
        if (!text)
            return Core::fail("drag", "GlobalAlloc failed for the dragged link.");
        auto* characters = static_cast<wchar_t*>(GlobalLock(text));
        MultiByteToWideChar(CP_UTF8, 0, link.data(), static_cast<int>(link.size()), characters, length);
        characters[length] = L'\0';
        GlobalUnlock(text);

        Microsoft::WRL::ComPtr<IDataObject> data;
        if (const HRESULT created = SHCreateDataObject(nullptr, 0, nullptr, nullptr, IID_PPV_ARGS(&data)); FAILED(created)) {
            GlobalFree(text);
            return check(created, "SHCreateDataObject");
        }
        FORMATETC format{CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        medium.tymed = TYMED_HGLOBAL;
        medium.hGlobal = text;
        if (const HRESULT set = data->SetData(&format, &medium, TRUE); FAILED(set)) {
            GlobalFree(text);
            return check(set, "IDataObject::SetData");
        }
        DropSource source;
        DWORD effect = DROPEFFECT_NONE;
        return check(DoDragDrop(data.Get(), &source, DROPEFFECT_COPY, &effect), "DoDragDrop");
    }

    Rml::String pressedLink;
    float pressX = 0;
    float pressY = 0;
};

#endif

LinkDrag::LinkDrag(Rml::Context& context, Model& model, SDL_Window* window)
    : context(context), model(model), window(window), native(std::make_unique<Native>(window)) {}

LinkDrag::~LinkDrag() = default;

void LinkDrag::handle(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        if (event.button.button != SDL_BUTTON_LEFT)
            return;
        const Rml::Element* element = linkAt(event.button.x, event.button.y);
        native->pressedLink = element ? element->GetAttribute<Rml::String>("href", "") : "";
        native->pressX = event.button.x;
        native->pressY = event.button.y;
        return;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
        native->pressedLink.clear();
        return;
    case SDL_EVENT_MOUSE_MOTION: {
        if (native->pressedLink.empty())
            return;
        constexpr float dragDistance = 4;
        if (std::hypot(event.motion.x - native->pressX, event.motion.y - native->pressY) < dragDistance)
            return;
        if (auto started = native->start(std::exchange(native->pressedLink, {})); !started)
            model.report(ReportLevel::Error, "drag", started.error().message);
        context.ProcessMouseButtonUp(0, 0);
        return;
    }
    default:
        return;
    }
}

}
