#pragma once

#include <expected>
#include <filesystem>

#include "RmlKit/Core/Error.h"

namespace RmlKit::UI {

std::expected<void, Core::Error> revealFile(const std::filesystem::path& file);
std::expected<void, Core::Error> copyFileToClipboard(const std::filesystem::path& file);

}
