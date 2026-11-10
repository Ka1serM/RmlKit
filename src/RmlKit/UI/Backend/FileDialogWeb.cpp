#include "RmlKit/UI/Backend/FileDialog.h"

#include <filesystem>
#include <utility>

#include <RmlUi/Core/Log.h>
#include <emscripten/emscripten.h>

#include "RmlKit/Core/Utf8Path.h"
#include "RmlKit/UI/Backend/Fatal.h"

extern "C" void rmlkit_choose(const char* kind, const char* extension);

namespace RmlKit::UI {

namespace {

FileDialog::Chosen pending;
bool pendingReportsCancel = false;

void begin(FileDialog::Chosen onChosen, const bool reportsCancel) {
    if (pending)
        fatal("A browser file dialog is already open.");
    pending = std::move(onChosen);
    pendingReportsCancel = reportsCancel;
}

}

void FileDialog::openFolder(SDL_Window*, const FolderKind kind, const std::filesystem::path&, Chosen onChosen) {
    if (kind == FolderKind::Export)
        fatal("The web build has no export folder: its Export gives a ZIP download.");
    begin(std::move(onChosen), false);
    rmlkit_choose(kind == FolderKind::GameInstall ? "game" : "replays", "");
}

void FileDialog::openFile(SDL_Window*, const std::span<const FileFilter> filters, const std::filesystem::path&, Chosen onChosen) {
    begin(std::move(onChosen), true);
    rmlkit_choose("files", filters.empty() ? "" : filters.front().extension);
}

}

extern "C" EMSCRIPTEN_KEEPALIVE void rmlkit_dialog_chosen(const char* path) {
    RmlKit::UI::FileDialog::Chosen chosen = std::move(RmlKit::UI::pending);
    chosen(RmlKit::Core::pathFromUtf8(path));
}

extern "C" EMSCRIPTEN_KEEPALIVE void rmlkit_dialog_cancelled() {
    RmlKit::UI::FileDialog::Chosen chosen = std::move(RmlKit::UI::pending);
    if (RmlKit::UI::pendingReportsCancel)
        chosen({});
}

extern "C" EMSCRIPTEN_KEEPALIVE void rmlkit_dialog_error(const char* message) {
    RmlKit::UI::pending = {};
    Rml::Log::Message(Rml::Log::LT_ERROR, "The browser file dialog failed: %s", message);
}
