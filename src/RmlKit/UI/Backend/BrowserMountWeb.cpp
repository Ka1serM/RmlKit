//! The read-only mounts of files picked in the browser, see web/files.js. A read waits for a reader worker, which copies the
//! bytes of the picked File into wasm memory, so a mounted file costs no memory until it is read.

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#include <emscripten/wasmfs.h>
#include <fcntl.h>
#include <unistd.h>

//! WasmFS has no public backend API: this depends on the own headers of the pinned Emscripten.
#include <backend.h>
#include <wasmfs.h>

namespace RmlKit::UI {

namespace {

constexpr std::int32_t idle = 0;
constexpr std::int32_t requested = 1;
constexpr std::int32_t done = 2;

//! Shared with the reader worker in web/files.js: the layout must match its Int32Array and Float64Array views.
struct ReadSlot {
    std::atomic<std::int32_t> state = idle;
    std::int32_t file = 0;
    std::uint32_t buffer = 0;
    std::uint32_t size = 0;
    std::int32_t result = 0;
    std::int32_t padding = 0;
    double offset = 0;
};
static_assert(sizeof(ReadSlot) == 32 && offsetof(ReadSlot, offset) == 24);

class Readers {
public:
    explicit Readers(const std::size_t count) : slots(std::make_unique<ReadSlot[]>(count)) {
        for (std::size_t index = 0; index < count; ++index)
            free.push_back(index);
    }

    ReadSlot& slot(const std::size_t index) { return slots[index]; }

    std::int32_t read(const std::int32_t file, const double offset, std::uint8_t* buffer, const std::uint32_t size) {
        std::size_t index;
        {
            std::unique_lock lock(mutex);
            available.wait(lock, [this] { return !free.empty(); });
            index = free.back();
            free.pop_back();
        }
        ReadSlot& slot = slots[index];
        slot.file = file;
        slot.offset = offset;
        slot.buffer = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(buffer));
        slot.size = size;
        slot.state.store(requested);
        emscripten_futex_wake(&slot.state, 1);
        while (slot.state.load() != done)
            emscripten_futex_wait(&slot.state, requested, INFINITY);
        const std::int32_t result = slot.result;
        slot.state.store(idle);
        {
            const std::scoped_lock lock(mutex);
            free.push_back(index);
        }
        available.notify_one();
        return result;
    }

private:
    std::unique_ptr<ReadSlot[]> slots;
    std::mutex mutex;
    std::condition_variable available;
    std::vector<std::size_t> free;
};

//! Small reads, such as a parser's, go through 64 KiB chunks, so each does not wait for a worker.
class ChunkCache {
public:
    static constexpr std::uint32_t chunkSize = 64 << 10;

    std::int32_t read(Readers& readers, const std::int32_t file, const off_t fileSize, const off_t offset, std::uint8_t* buffer, const std::uint32_t size) {
        if (size > largeRead)
            return readers.read(file, static_cast<double>(offset), buffer, size);
        std::uint32_t copied = 0;
        while (copied < size) {
            const off_t at = offset + copied;
            const Key key{&readers, file, static_cast<std::uint64_t>(at / chunkSize)};
            const auto inChunk = static_cast<std::uint32_t>(at % chunkSize);
            const std::uint32_t take = std::min(size - copied, chunkSize - inChunk);
            if (!copyCached(key, inChunk, buffer + copied, take) && !load(key, fileSize, inChunk, buffer + copied, take))
                return -1;
            copied += take;
        }
        return static_cast<std::int32_t>(copied);
    }

private:
    static constexpr std::uint32_t largeRead = 4 * chunkSize;
    static constexpr std::size_t chunkCount = 256;

    struct Key {
        Readers* readers;
        std::int32_t file;
        std::uint64_t chunk;
        bool operator==(const Key&) const = default;
    };
    struct KeyHash {
        std::size_t operator()(const Key& key) const {
            return std::hash<std::uint64_t>()(key.chunk * 0x9E3779B97F4A7C15ull ^ static_cast<std::uint64_t>(key.file) << 40
                                              ^ reinterpret_cast<std::uintptr_t>(key.readers));
        }
    };
    struct Slot {
        Key key{};
        std::uint64_t lastUse = 0;
    };

    bool copyCached(const Key& key, const std::uint32_t inChunk, std::uint8_t* buffer, const std::uint32_t take) {
        const std::scoped_lock lock(mutex);
        const auto found = slotOf.find(key);
        if (found == slotOf.end())
            return false;
        slots[found->second].lastUse = ++useClock;
        std::memcpy(buffer, data.get() + found->second * chunkSize + inChunk, take);
        return true;
    }

    bool load(const Key& key, const off_t fileSize, const std::uint32_t inChunk, std::uint8_t* buffer, const std::uint32_t take) {
        thread_local const std::unique_ptr<std::uint8_t[]> staging = std::make_unique<std::uint8_t[]>(chunkSize);
        const auto start = static_cast<off_t>(key.chunk * chunkSize);
        const auto length = static_cast<std::uint32_t>(std::min<off_t>(chunkSize, fileSize - start));
        if (key.readers->read(key.file, static_cast<double>(start), staging.get(), length) != static_cast<std::int32_t>(length))
            return false;
        std::memcpy(buffer, staging.get() + inChunk, take);
        const std::scoped_lock lock(mutex);
        if (slotOf.contains(key))
            return true;
        const std::size_t slot = takeSlot();
        slots[slot] = {key, ++useClock};
        slotOf.emplace(key, slot);
        std::memcpy(data.get() + slot * chunkSize, staging.get(), length);
        return true;
    }

    std::size_t takeSlot() {
        if (used < chunkCount)
            return used++;
        const auto oldest = std::ranges::min_element(slots, {}, &Slot::lastUse);
        slotOf.erase(oldest->key);
        return static_cast<std::size_t>(oldest - slots.begin());
    }

    std::mutex mutex;
    std::unique_ptr<std::uint8_t[]> data = std::make_unique<std::uint8_t[]>(chunkCount * chunkSize);
    std::array<Slot, chunkCount> slots{};
    std::unordered_map<Key, std::size_t, KeyHash> slotOf;
    std::size_t used = 0;
    std::uint64_t useClock = 0;
};

ChunkCache& chunkCache() {
    static ChunkCache cache;
    return cache;
}

class BlobFile final : public wasmfs::DataFile {
public:
    BlobFile(const wasmfs::backend_t backend, std::shared_ptr<Readers> readers, const std::int32_t index, const off_t size, const double modified)
        : DataFile(S_IRUSR | S_IRGRP | S_IROTH, backend), readers(std::move(readers)), index(index), size(size) {
        mtime = modified;
    }

private:
    int open(wasmfs::oflags_t) override { return 0; }
    int close() override { return 0; }

    ssize_t read(std::uint8_t* buffer, const std::size_t length, const off_t offset) override {
        if (offset >= size)
            return 0;
        const auto count = static_cast<std::uint32_t>(std::min<off_t>({static_cast<off_t>(length), size - offset, 1 << 30}));
        const std::int32_t result = chunkCache().read(*readers, index, size, offset, buffer, count);
        return result < 0 ? -EIO : result;
    }

    ssize_t write(const std::uint8_t*, std::size_t, off_t) override { return -EROFS; }
    int setSize(off_t) override { return -EROFS; }
    int flush() override { return 0; }
    off_t getSize() override { return size; }

    std::shared_ptr<Readers> readers;
    std::int32_t index;
    off_t size;
};

//! WasmFS creates a file through a backend without arguments, so add() sets the file of the next createFile().
class MountBackend final : public wasmfs::Backend {
public:
    explicit MountBackend(const std::size_t readerCount) : readers(std::make_shared<Readers>(readerCount)) {}

    Readers& getReaders() { return *readers; }

    int add(const std::filesystem::path& path, const std::int32_t index, const off_t size, const double modified) {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
            return -error.value();
        next = {index, size, modified};
        const int descriptor = wasmfs_create_file(path.c_str(), 0444, reinterpret_cast<::backend_t>(static_cast<wasmfs::Backend*>(this)));
        if (descriptor < 0)
            return descriptor;
        ::close(descriptor);
        return 0;
    }

    std::shared_ptr<wasmfs::DataFile> createFile(mode_t) override { return std::make_shared<BlobFile>(this, readers, next.index, next.size, next.modified); }
    std::shared_ptr<wasmfs::Directory> createDirectory(mode_t) override { return nullptr; }
    std::shared_ptr<wasmfs::Symlink> createSymlink(std::string) override { return nullptr; }

private:
    struct NextFile {
        std::int32_t index = 0;
        off_t size = 0;
        double modified = 0;
    };

    std::shared_ptr<Readers> readers;
    NextFile next;
};

std::vector<MountBackend*> mounts;
std::vector<std::filesystem::path> mountPaths;

}

}

using RmlKit::UI::mountPaths;
using RmlKit::UI::mounts;

extern "C" EMSCRIPTEN_KEEPALIVE void rmlkit_mount_remove(const char* path) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
}

extern "C" EMSCRIPTEN_KEEPALIVE int rmlkit_mount_open(const char* path, const int readers) {
    std::error_code error;
    std::filesystem::create_directories(path, error);
    auto backend = std::make_unique<RmlKit::UI::MountBackend>(static_cast<std::size_t>(readers));
    mounts.push_back(backend.get());
    mountPaths.emplace_back(path);
    wasmfs::wasmFS.addBackend(std::move(backend));
    return static_cast<int>(mounts.size() - 1);
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t rmlkit_mount_slot(const int mount, const int reader) {
    return static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&mounts[static_cast<std::size_t>(mount)]->getReaders().slot(static_cast<std::size_t>(reader))));
}

extern "C" EMSCRIPTEN_KEEPALIVE int rmlkit_mount_add(const int mount, const char* relativePath, const int index, const double size, const double modified) {
    const auto at = static_cast<std::size_t>(mount);
    return mounts[at]->add(mountPaths[at] / relativePath, index, static_cast<off_t>(size), modified);
}
