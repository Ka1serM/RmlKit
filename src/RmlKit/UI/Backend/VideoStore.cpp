#include "RmlKit/UI/Backend/VideoStore.h"

#include <utility>

namespace RmlKit::UI {

void VideoStore::put(const std::string& name, std::shared_ptr<const std::vector<std::byte>> video) {
    videos.insert_or_assign(name, std::move(video));
}

void VideoStore::remove(const std::string& name) {
    videos.erase(name);
}

std::shared_ptr<const std::vector<std::byte>> VideoStore::find(const std::string& name) const {
    const auto found = videos.find(name);
    return found == videos.end() ? nullptr : found->second;
}

}
