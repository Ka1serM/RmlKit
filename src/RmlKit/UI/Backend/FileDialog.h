#pragma once

#include <filesystem>
#include <functional>

#include "RmlKit/Core/StdGaps.h"
#include <span>

#include <SDL3/SDL_video.h>

namespace RmlKit::UI {

struct FileFilter {
    const char* name;
    const char* extension;
};

enum class FolderKind { GameInstall, Replays, Export };

class FileDialog {
public:
    using Chosen = std::move_only_function<void(std::filesystem::path)>;

    static void openFolder(SDL_Window* window, FolderKind kind, const std::filesystem::path& location, Chosen onChosen);
    //! A cancelled file dialog gives an empty path; a cancelled folder dialog calls nothing.
    static void openFile(SDL_Window* window, std::span<const FileFilter> filters, const std::filesystem::path& location, Chosen onChosen);
};

}
