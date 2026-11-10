#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

#include "RmlKit/Core/Error.h"

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVIOContext;
struct AVPacket;
struct SDL_AudioStream;
struct SwsContext;

namespace RmlKit::Media {

enum class YuvMatrix { Bt601, Bt709 };

struct VideoImage {
    std::array<const std::uint8_t*, 3> planes;
    std::array<int, 3> strides;
    int width;
    int height;
    YuvMatrix matrix;
    bool fullRange;
};

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();
    VideoPlayer(const VideoPlayer&) = delete;
    VideoPlayer& operator=(const VideoPlayer&) = delete;

    std::expected<void, Core::Error> open(std::shared_ptr<const std::vector<std::byte>> bytes);

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    std::expected<bool, Core::Error> readDueFrame(const std::function<void(const VideoImage&)>& read);
    std::expected<double, Core::Error> getSecondsToNextFrame();
    double getDuration() const { return duration; }
    double getPosition() const { return shownPosition; }
    bool isPaused() const { return paused; }
    std::expected<void, Core::Error> setPaused(bool paused);
    std::expected<void, Core::Error> setVolume(float volume);
    std::expected<void, Core::Error> seek(double seconds);

private:
    struct InputFreer {
        void operator()(AVIOContext* input) const noexcept;
    };
    struct FormatCloser {
        void operator()(AVFormatContext* format) const noexcept;
    };
    struct CodecFreer {
        void operator()(AVCodecContext* codec) const noexcept;
    };
    struct PacketFreer {
        void operator()(AVPacket* packet) const noexcept;
    };
    struct FrameFreer {
        void operator()(AVFrame* frame) const noexcept;
    };
    struct ScalerFreer {
        void operator()(SwsContext* scaler) const noexcept;
    };
    struct AudioStreamDestroyer {
        void operator()(SDL_AudioStream* stream) const noexcept;
    };
    using Packet = std::unique_ptr<AVPacket, PacketFreer>;
    using Frame = std::unique_ptr<AVFrame, FrameFreer>;

    struct PendingPacket {
        Packet packet;
        double loopStart;
    };

    struct QueuedFrame {
        double time = 0.0;
        double position = 0.0;
        Frame frame;
    };

    struct MemoryFile {
        std::shared_ptr<const std::vector<std::byte>> bytes;
        int64_t position = 0;
    };

    static constexpr size_t frameCapacity = 4;

    static int readMemory(void* opaque, uint8_t* buffer, int size);
    static int64_t seekMemory(void* opaque, int64_t offset, int whence);
    std::expected<double, Core::Error> getClock();
    std::expected<double, Core::Error> getQueuedAudioSeconds() const;
    void startWorker();
    void decode(std::stop_token stop);
    std::expected<void, Core::Error> play(std::stop_token stop);
    std::expected<void, Core::Error> readPacket();
    std::expected<void, Core::Error> decodeAudio(const AVPacket* packet);
    std::expected<void, Core::Error> decodeVideo(const PendingPacket& pending, std::stop_token stop);
    std::expected<void, Core::Error> queueFrame(double loopStart, std::stop_token stop);
    std::expected<void, Core::Error> take(AVFrame& queued);
    VideoImage imageOf(const AVFrame& frame) const;
    bool hasFrameSpace();

    MemoryFile file;
    std::unique_ptr<AVIOContext, InputFreer> input;
    std::unique_ptr<AVFormatContext, FormatCloser> format;
    std::unique_ptr<AVCodecContext, CodecFreer> video;
    std::unique_ptr<AVCodecContext, CodecFreer> audio;
    std::unique_ptr<SDL_AudioStream, AudioStreamDestroyer> audioOutput;
    int videoStream = -1;
    int audioStream = -1;
    int width = 0;
    int height = 0;
    double frameInterval = 0.0;
    double videoTimeBase = 0.0;
    int64_t videoStartPts = 0;
    double audioTimeBase = 0.0;
    int64_t audioStartPts = 0;
    int sampleRate = 0;
    int audioFrameBytes = 0;
    double duration = 0.0;
    double clockStart = 0.0;

    int64_t lastPlayedFrames = 0;
    uint64_t lastPlayedNs = 0;
    Frame shown;
    double shownPosition = 0.0;
    bool paused = false;
    double pausedClock = 0.0;

    Packet readBuffer;
    Frame decoded;
    std::unique_ptr<SwsContext, ScalerFreer> scaler;
    std::vector<const void*> audioPlanes;
    std::deque<PendingPacket> pendingVideo;
    double loopStart = 0.0;

    std::atomic<int64_t> audioFramesPut = 0;
    mutable std::mutex mutex;
    std::condition_variable_any frameTaken;
    std::array<QueuedFrame, frameCapacity> frames;
    size_t firstFrame = 0;
    size_t frameCount = 0;
    std::optional<Core::Error> error;
    std::jthread worker;
};

}
