#include "RmlKit/UI/Backend/FileShell.h"

#include <format>
#include <memory>
#include <string>

#include <SDL3/SDL.h>

#include "RmlKit/Core/Utf8Path.h"

#if PLATFORM_WINDOWS
#include <ole2.h>
#include <shlobj.h>
#include <wrl/client.h>
#elif PLATFORM_LINUX
#include <string_view>
#endif

namespace RmlKit::UI {

#if PLATFORM_WINDOWS

namespace {

std::expected<void, Core::Error> check(const HRESULT result, const char* code, const std::filesystem::path& file, const char* call) {
    if (FAILED(result))
        return Core::fail(code, std::format("{}: {} failed: 0x{:08x}", Core::utf8(file), call, static_cast<unsigned long>(result)));
    return {};
}

class OleScope {
public:
    OleScope() : result(OleInitialize(nullptr)) {}
    ~OleScope() {
        if (SUCCEEDED(result))
            OleUninitialize();
    }
    OleScope(const OleScope&) = delete;
    OleScope& operator=(const OleScope&) = delete;

    HRESULT getResult() const { return result; }

private:
    HRESULT result;
};

struct ItemListDeleter {
    void operator()(ITEMIDLIST* list) const { ILFree(list); }
};
using ItemList = std::unique_ptr<ITEMIDLIST, ItemListDeleter>;

}

std::expected<void, Core::Error> revealFile(const std::filesystem::path& file) {
    const OleScope ole;
    if (auto initialized = check(ole.getResult(), "file.reveal", file, "OleInitialize"); !initialized)
        return initialized;
    ITEMIDLIST* parsed = nullptr;
    if (auto found = check(SHParseDisplayName(file.c_str(), nullptr, &parsed, 0, nullptr), "file.reveal", file, "SHParseDisplayName"); !found)
        return found;
    const ItemList item(parsed);
    return check(SHOpenFolderAndSelectItems(item.get(), 0, nullptr, 0), "file.reveal", file, "SHOpenFolderAndSelectItems");
}

std::expected<void, Core::Error> copyFileToClipboard(const std::filesystem::path& file) {
    const OleScope ole;
    if (auto initialized = check(ole.getResult(), "file.copy", file, "OleInitialize"); !initialized)
        return initialized;
    ITEMIDLIST* parsed = nullptr;
    if (auto found = check(SHParseDisplayName(file.c_str(), nullptr, &parsed, 0, nullptr), "file.copy", file, "SHParseDisplayName"); !found)
        return found;
    const ItemList item(parsed);
    const ItemList folder(ILClone(item.get()));
    ILRemoveLastID(folder.get());
    LPCITEMIDLIST child = ILFindLastID(item.get());
    Microsoft::WRL::ComPtr<IDataObject> data;
    if (auto created = check(SHCreateDataObject(folder.get(), 1, &child, nullptr, IID_PPV_ARGS(&data)), "file.copy", file, "SHCreateDataObject"); !created)
        return created;
    if (auto set = check(OleSetClipboard(data.Get()), "file.copy", file, "OleSetClipboard"); !set)
        return set;
    return check(OleFlushClipboard(), "file.copy", file, "OleFlushClipboard");
}

#elif PLATFORM_LINUX

namespace {

std::string fileUri(const std::filesystem::path& file) {
    std::string uri = "file://";
    for (const char character : Core::utf8(file)) {
        const auto byte = static_cast<unsigned char>(character);
        const bool kept = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') || byte == '/' || byte == '-'
                          || byte == '.' || byte == '_' || byte == '~';
        if (kept)
            uri.push_back(character);
        else
            uri += std::format("%{:02X}", byte);
    }
    return uri;
}

std::expected<void, Core::Error> runProgram(const char* const* arguments, const char* code, const std::filesystem::path& file) {
    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetPointerProperty(properties, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, const_cast<char**>(arguments));
    SDL_SetNumberProperty(properties, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(properties, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process* process = SDL_CreateProcessWithProperties(properties);
    SDL_DestroyProperties(properties);
    if (!process)
        return Core::fail(code, std::format("{}: {} does not start: {}", Core::utf8(file), arguments[0], SDL_GetError()));
    std::size_t size = 0;
    int exitCode = 0;
    void* output = SDL_ReadProcess(process, &size, &exitCode);
    SDL_DestroyProcess(process);
    const std::string text = output ? std::string(static_cast<const char*>(output), size) : std::string();
    SDL_free(output);
    if (exitCode != 0)
        return Core::fail(code, std::format("{}: {} failed with {}: {}", Core::utf8(file), arguments[0], exitCode, text));
    return {};
}

struct ClipboardFile {
    std::string uriList;
    std::string gnomeFiles;
};

const void* SDLCALL provideClipboardFile(void* userdata, const char* mimeType, std::size_t* size) {
    const auto& copied = *static_cast<const ClipboardFile*>(userdata);
    const std::string& text = std::string_view(mimeType) == "text/uri-list" ? copied.uriList : copied.gnomeFiles;
    *size = text.size();
    return text.data();
}

void SDLCALL releaseClipboardFile(void* userdata) {
    delete static_cast<ClipboardFile*>(userdata);
}

}

std::expected<void, Core::Error> revealFile(const std::filesystem::path& file) {
    const std::string items = "array:string:" + fileUri(file);
    const char* const arguments[] = {"dbus-send",
                                     "--session",
                                     "--print-reply",
                                     "--dest=org.freedesktop.FileManager1",
                                     "/org/freedesktop/FileManager1",
                                     "org.freedesktop.FileManager1.ShowItems",
                                     items.c_str(),
                                     "string:",
                                     nullptr};
    return runProgram(arguments, "file.reveal", file);
}

std::expected<void, Core::Error> copyFileToClipboard(const std::filesystem::path& file) {
    const std::string uri = fileUri(file);
    const char* const mimeTypes[] = {"text/uri-list", "x-special/gnome-copied-files"};
    //! SDL owns the data from here and releases it with releaseClipboardFile.
    auto copied = std::make_unique<ClipboardFile>(uri + "\r\n", "copy\n" + uri);
    if (!SDL_SetClipboardData(provideClipboardFile, releaseClipboardFile, copied.release(), mimeTypes, std::size(mimeTypes)))
        return Core::fail("file.copy", std::format("{}: SDL_SetClipboardData failed: {}", Core::utf8(file), SDL_GetError()));
    return {};
}

#endif

}
