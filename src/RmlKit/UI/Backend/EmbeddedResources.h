#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace RmlKit::UI {

class EmbeddedResources {
public:
    static bool exists(const std::string& path);
    static std::string_view read(const std::string& path);
    static std::vector<std::string> list(const std::string& directory);
    static std::vector<std::string> files(const std::string& directory);
};

}
