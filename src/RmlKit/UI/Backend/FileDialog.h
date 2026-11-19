#pragma once

#include <filesystem>
#include <functional>
#include <span>

#include <SDL3/SDL_video.h>

#include "RmlKit/Core/StdGaps.h"

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
    //! In the browser the picker takes several files, which mount side by side under /mnt/files, so the chosen file (the first that
    //! matches a filter) reads the files next to it, as on the desktop. A folder mounts with every file in it.
    static void openFile(SDL_Window* window, std::span<const FileFilter> filters, const std::filesystem::path& location, Chosen onChosen);
};

}
