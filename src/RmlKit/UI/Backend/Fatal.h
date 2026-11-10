#pragma once

#include <string_view>

namespace RmlKit::UI {

[[noreturn]] void fatal(std::string_view message);

}
