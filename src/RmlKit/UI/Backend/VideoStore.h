#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RmlKit::UI {

class VideoStore {
public:
    void put(const std::string& name, std::shared_ptr<const std::vector<std::byte>> video);
    void remove(const std::string& name);
    std::shared_ptr<const std::vector<std::byte>> find(const std::string& name) const;

private:
    std::unordered_map<std::string, std::shared_ptr<const std::vector<std::byte>>> videos;
};

}
