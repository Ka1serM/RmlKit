#include "RmlKit/UI/Backend/FileDialog.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <RmlUi/Core/Log.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>

#include "RmlKit/Core/Utf8Path.h"
#include "RmlKit/UI/Backend/Dispatcher.h"

namespace RmlKit::UI {

namespace {

struct Dialog {
    FileDialog::Chosen onChosen;
    bool reportsCancel = false;
    std::vector<SDL_DialogFileFilter> filters;
};

void SDLCALL onResult(void* userdata, const char* const* files, int) {
    std::unique_ptr<Dialog> dialog(static_cast<Dialog*>(userdata));
    if (!files) {
        Dispatcher::postUi([error = std::string(SDL_GetError())] { Rml::Log::Message(Rml::Log::LT_ERROR, "The file dialog failed: %s", error.c_str()); });
        return;
    }
    if (!files[0] && !dialog->reportsCancel)
        return;
    Dispatcher::postUi([onChosen = std::move(dialog->onChosen), chosen = files[0] ? Core::pathFromUtf8(files[0]) : std::filesystem::path()]() mutable {
        onChosen(std::move(chosen));
    });
}

}

void FileDialog::openFolder(SDL_Window* window, FolderKind, const std::filesystem::path& location, Chosen onChosen) {
    const std::string folder = Core::utf8(location);
    auto dialog = std::make_unique<Dialog>(std::move(onChosen));
    SDL_ShowOpenFolderDialog(&onResult, dialog.release(), window, folder.empty() ? nullptr : folder.c_str(), false);
}

void FileDialog::openFile(SDL_Window* window, std::span<const FileFilter> filters, const std::filesystem::path& location, Chosen onChosen) {
    auto dialog = std::make_unique<Dialog>(std::move(onChosen), true);
    dialog->filters.reserve(filters.size());
    for (const FileFilter& filter : filters)
        dialog->filters.push_back({filter.name, filter.extension});
    const std::string folder = Core::utf8(location);
    const int count = static_cast<int>(dialog->filters.size());
    const SDL_DialogFileFilter* nativeFilters = dialog->filters.data();
    SDL_ShowOpenFileDialog(&onResult, dialog.release(), window, nativeFilters, count, folder.empty() ? nullptr : folder.c_str(), false);
}

}
