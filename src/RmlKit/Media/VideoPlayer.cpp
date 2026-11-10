#include "RmlKit/Media/VideoPlayer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <format>
#include <vector>

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_timer.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace RmlKit::Media {

namespace {

constexpr double audioLeadSeconds = 0.5;
constexpr double maxClockAdvanceSeconds = 0.05;

std::expected<void, Core::Error> check(const int result, const char* operation) {
    if (result >= 0)
        return {};
    char message[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(result, message, sizeof(message));
    return Core::fail("video", std::format("{} failed: {}", operation, message));
}

std::unexpected<Core::Error> sdlFailure(const char* operation) {
    return Core::fail("video", std::format("{} failed: {}", operation, SDL_GetError()));
}

std::unexpected<Core::Error> outOfMemory() {
    return Core::fail("video", "The video player is out of memory.");
}

constexpr int inputBufferSize = 1 << 16;

std::expected<bool, Core::Error> receiveFrame(AVCodecContext* codec, AVFrame* frame) {
    const int result = avcodec_receive_frame(codec, frame);
    if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
        return false;
    return check(result, "avcodec_receive_frame").transform([] { return true; });
}

}

int VideoPlayer::readMemory(void* opaque, uint8_t* buffer, const int size) {
    auto* file = static_cast<MemoryFile*>(opaque);
    const int64_t left = static_cast<int64_t>(file->bytes->size()) - file->position;
    if (left <= 0)
        return AVERROR_EOF;
    const int count = static_cast<int>(std::min<int64_t>(size, left));
    std::memcpy(buffer, file->bytes->data() + file->position, static_cast<size_t>(count));
    file->position += count;
    return count;
}

int64_t VideoPlayer::seekMemory(void* opaque, const int64_t offset, const int whence) {
    auto* file = static_cast<MemoryFile*>(opaque);
    const int64_t size = static_cast<int64_t>(file->bytes->size());
    if (whence == AVSEEK_SIZE)
        return size;
    int64_t target = 0;
    switch (whence & ~AVSEEK_FORCE) {
    case SEEK_SET:
        target = offset;
        break;
    case SEEK_CUR:
        target = file->position + offset;
        break;
    case SEEK_END:
        target = size + offset;
        break;
    default:
        return AVERROR(EINVAL);
    }
    if (target < 0 || target > size)
        return AVERROR(EINVAL);
    file->position = target;
    return target;
}

void VideoPlayer::InputFreer::operator()(AVIOContext* input) const noexcept {
    av_freep(&input->buffer);
    avio_context_free(&input);
}
void VideoPlayer::FormatCloser::operator()(AVFormatContext* format) const noexcept {
    avformat_close_input(&format);
}
void VideoPlayer::CodecFreer::operator()(AVCodecContext* codec) const noexcept {
    avcodec_free_context(&codec);
}
void VideoPlayer::PacketFreer::operator()(AVPacket* packet) const noexcept {
    av_packet_free(&packet);
}
void VideoPlayer::FrameFreer::operator()(AVFrame* frame) const noexcept {
    av_frame_free(&frame);
}
void VideoPlayer::ScalerFreer::operator()(SwsContext* scaler) const noexcept {
    sws_freeContext(scaler);
}
void VideoPlayer::AudioStreamDestroyer::operator()(SDL_AudioStream* stream) const noexcept {
    SDL_DestroyAudioStream(stream);
}

VideoPlayer::VideoPlayer() = default;

std::expected<void, Core::Error> VideoPlayer::open(std::shared_ptr<const std::vector<std::byte>> bytes) {
    file = {std::move(bytes)};
    auto* inputBuffer = static_cast<unsigned char*>(av_malloc(inputBufferSize));
    if (!inputBuffer)
        return outOfMemory();
    input.reset(avio_alloc_context(inputBuffer, inputBufferSize, 0, &file, readMemory, nullptr, seekMemory));
    if (!input) {
        av_free(inputBuffer);
        return outOfMemory();
    }
    AVFormatContext* opened = avformat_alloc_context();
    if (!opened)
        return outOfMemory();
    opened->pb = input.get();
    if (auto result = check(avformat_open_input(&opened, nullptr, nullptr, nullptr), "avformat_open_input"); !result)
        return result;
    format.reset(opened);
    if (auto result = check(avformat_find_stream_info(format.get(), nullptr), "avformat_find_stream_info"); !result)
        return result;

    const AVCodec* videoCodec = nullptr;
    const AVCodec* audioCodec = nullptr;
    videoStream = av_find_best_stream(format.get(), AVMEDIA_TYPE_VIDEO, -1, -1, &videoCodec, 0);
    if (auto result = check(videoStream, "av_find_best_stream (video)"); !result)
        return result;
    audioStream = av_find_best_stream(format.get(), AVMEDIA_TYPE_AUDIO, -1, videoStream, &audioCodec, 0);
    if (auto result = check(audioStream, "av_find_best_stream (audio)"); !result)
        return result;

    const AVStream* videoInfo = format->streams[videoStream];
    video.reset(avcodec_alloc_context3(videoCodec));
    if (!video)
        return outOfMemory();
    if (auto result = check(avcodec_parameters_to_context(video.get(), videoInfo->codecpar), "avcodec_parameters_to_context (video)"); !result)
        return result;
    video->thread_count = 0;
    if (auto result = check(avcodec_open2(video.get(), videoCodec, nullptr), "avcodec_open2 (video)"); !result)
        return result;

    audio.reset(avcodec_alloc_context3(audioCodec));
    if (!audio)
        return outOfMemory();
    if (auto result = check(avcodec_parameters_to_context(audio.get(), format->streams[audioStream]->codecpar), "avcodec_parameters_to_context (audio)");
        !result)
        return result;
    if (auto result = check(avcodec_open2(audio.get(), audioCodec, nullptr), "avcodec_open2 (audio)"); !result)
        return result;
    if (audio->sample_fmt != AV_SAMPLE_FMT_FLTP)
        return Core::fail("video", "The audio decoder does not give planar float samples.");
    const AVStream* audioInfo = format->streams[audioStream];
    audioTimeBase = av_q2d(audioInfo->time_base);
    if (audioInfo->start_time != AV_NOPTS_VALUE)
        audioStartPts = audioInfo->start_time;

    width = videoInfo->codecpar->width;
    height = videoInfo->codecpar->height;
    if (width <= 0 || height <= 0 || width % 2 || height % 2)
        return Core::fail("video", std::format("The video size {} x {} is not a positive even size.", width, height));
    const AVRational frameRate = av_guess_frame_rate(format.get(), format->streams[videoStream], nullptr);
    if (frameRate.num <= 0 || frameRate.den <= 0)
        return Core::fail("video", "The video has no frame rate.");
    frameInterval = av_q2d(av_inv_q(frameRate));
    videoTimeBase = av_q2d(videoInfo->time_base);
    if (videoInfo->start_time != AV_NOPTS_VALUE)
        videoStartPts = videoInfo->start_time;
    if (format->duration <= 0)
        return Core::fail("video", "The video has no duration.");
    duration = static_cast<double>(format->duration) / AV_TIME_BASE;

    sampleRate = audio->sample_rate;
    const int channels = audio->ch_layout.nb_channels;
    audioFrameBytes = channels * static_cast<int>(sizeof(float));
    audioPlanes.resize(static_cast<size_t>(channels));
    const SDL_AudioSpec spec = {SDL_AUDIO_F32, channels, sampleRate};
    audioOutput.reset(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr));
    if (!audioOutput)
        return sdlFailure("SDL_OpenAudioDeviceStream");

    readBuffer.reset(av_packet_alloc());
    decoded.reset(av_frame_alloc());
    shown.reset(av_frame_alloc());
    if (!readBuffer || !decoded || !shown)
        return outOfMemory();
    for (QueuedFrame& queued : frames) {
        queued.frame.reset(av_frame_alloc());
        if (!queued.frame)
            return outOfMemory();
    }

    if (!SDL_ResumeAudioStreamDevice(audioOutput.get()))
        return sdlFailure("SDL_ResumeAudioStreamDevice");
    startWorker();
    return {};
}

VideoPlayer::~VideoPlayer() = default;

std::expected<bool, Core::Error> VideoPlayer::readDueFrame(const std::function<void(const VideoImage&)>& read) {
    const auto clock = getClock();
    if (!clock)
        return std::unexpected(clock.error());
    std::unique_lock lock(mutex);
    if (error)
        return std::unexpected(*error);
    size_t due = 0;
    while (due < frameCount && frames[(firstFrame + due) % frameCapacity].time <= *clock)
        ++due;
    if (due == 0)
        return false;
    QueuedFrame& newest = frames[(firstFrame + due - 1) % frameCapacity];
    shown.swap(newest.frame);
    shownPosition = newest.position;
    firstFrame = (firstFrame + due) % frameCapacity;
    frameCount -= due;
    lock.unlock();
    frameTaken.notify_one();
    read(imageOf(*shown));
    return true;
}

std::expected<double, Core::Error> VideoPlayer::getSecondsToNextFrame() {
    const auto clock = getClock();
    if (!clock)
        return clock;
    std::scoped_lock lock(mutex);
    if (frameCount == 0)
        return frameInterval;
    return std::max(0.0, frames[firstFrame].time - *clock);
}

std::expected<void, Core::Error> VideoPlayer::setPaused(const bool paused) {
    if (paused == this->paused)
        return {};
    if (paused) {
        const auto clock = getClock();
        if (!clock)
            return std::unexpected(clock.error());
        pausedClock = *clock;
        if (!SDL_PauseAudioStreamDevice(audioOutput.get()))
            return sdlFailure("SDL_PauseAudioStreamDevice");
    } else {
        if (!SDL_ResumeAudioStreamDevice(audioOutput.get()))
            return sdlFailure("SDL_ResumeAudioStreamDevice");
        lastPlayedNs = SDL_GetTicksNS();
    }
    this->paused = paused;
    return {};
}

std::expected<void, Core::Error> VideoPlayer::setVolume(const float volume) {
    if (!SDL_SetAudioStreamGain(audioOutput.get(), volume))
        return sdlFailure("SDL_SetAudioStreamGain");
    return {};
}

std::expected<void, Core::Error> VideoPlayer::seek(double seconds) {
    seconds = std::clamp(seconds, 0.0, duration);
    worker.request_stop();
    worker.join();
    firstFrame = 0;
    frameCount = 0;
    pendingVideo.clear();
    avcodec_flush_buffers(video.get());
    avcodec_flush_buffers(audio.get());
    if (!SDL_ClearAudioStream(audioOutput.get()))
        return sdlFailure("SDL_ClearAudioStream");
    const int64_t formatStart = format->start_time == AV_NOPTS_VALUE ? 0 : format->start_time;
    if (auto result = check(av_seek_frame(format.get(), -1, formatStart + std::llround(seconds * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD), "av_seek_frame");
        !result)
        return result;
    clockStart = seconds;
    loopStart = 0.0;
    audioFramesPut = 0;
    lastPlayedFrames = 0;
    lastPlayedNs = SDL_GetTicksNS();
    pausedClock = seconds;
    shownPosition = seconds;
    startWorker();
    return {};
}

std::expected<double, Core::Error> VideoPlayer::getClock() {
    if (paused)
        return pausedClock;
    const int64_t framesPut = audioFramesPut.load();
    const int queuedBytes = SDL_GetAudioStreamQueued(audioOutput.get());
    if (queuedBytes < 0)
        return sdlFailure("SDL_GetAudioStreamQueued");
    const int64_t playedFrames = framesPut - queuedBytes / audioFrameBytes;
    const uint64_t now = SDL_GetTicksNS();
    if (playedFrames > lastPlayedFrames) {
        lastPlayedFrames = playedFrames;
        lastPlayedNs = now;
    }
    const double advance = std::min(static_cast<double>(now - lastPlayedNs) / SDL_NS_PER_SECOND, maxClockAdvanceSeconds);
    return clockStart + static_cast<double>(lastPlayedFrames) / sampleRate + advance;
}

std::expected<double, Core::Error> VideoPlayer::getQueuedAudioSeconds() const {
    const int queuedBytes = SDL_GetAudioStreamQueued(audioOutput.get());
    if (queuedBytes < 0)
        return sdlFailure("SDL_GetAudioStreamQueued");
    return static_cast<double>(queuedBytes / audioFrameBytes) / sampleRate;
}

void VideoPlayer::startWorker() {
    worker = std::jthread([this](const std::stop_token stop) { decode(stop); });
}

void VideoPlayer::decode(const std::stop_token stop) {
    const std::expected<void, Core::Error> result = play(stop);
    if (result)
        return;
    std::scoped_lock lock(mutex);
    error = result.error();
}

std::expected<void, Core::Error> VideoPlayer::play(const std::stop_token stop) {
    while (!stop.stop_requested()) {
        if (!pendingVideo.empty() && hasFrameSpace()) {
            if (auto result = decodeVideo(pendingVideo.front(), stop); !result)
                return result;
            pendingVideo.pop_front();
            continue;
        }
        const auto queuedAudio = getQueuedAudioSeconds();
        if (!queuedAudio)
            return std::unexpected(queuedAudio.error());
        if (pendingVideo.empty() || *queuedAudio < audioLeadSeconds) {
            if (auto result = readPacket(); !result)
                return result;
            continue;
        }
        std::unique_lock lock(mutex);
        const std::stop_callback onStop(stop, [this] { frameTaken.notify_all(); });
        frameTaken.wait(lock, [&] { return stop.stop_requested() || frameCount < frameCapacity; });
    }
    return {};
}

std::expected<void, Core::Error> VideoPlayer::readPacket() {
    const int result = av_read_frame(format.get(), readBuffer.get());
    if (result == AVERROR_EOF) {
        if (auto drained = decodeAudio(nullptr); !drained)
            return drained;
        avcodec_flush_buffers(audio.get());
        pendingVideo.push_back({nullptr, loopStart});
        loopStart = clockStart + static_cast<double>(audioFramesPut.load()) / sampleRate;
        return check(av_seek_frame(format.get(), -1, 0, AVSEEK_FLAG_BACKWARD), "av_seek_frame");
    }
    if (auto read = check(result, "av_read_frame"); !read)
        return read;
    if (readBuffer->stream_index == audioStream)
        if (auto decodedAudio = decodeAudio(readBuffer.get()); !decodedAudio)
            return decodedAudio;
    if (readBuffer->stream_index != videoStream) {
        av_packet_unref(readBuffer.get());
        return {};
    }
    Packet packet(av_packet_alloc());
    if (!packet)
        return outOfMemory();
    av_packet_move_ref(packet.get(), readBuffer.get());
    pendingVideo.push_back({std::move(packet), loopStart});
    return {};
}

std::expected<void, Core::Error> VideoPlayer::decodeAudio(const AVPacket* packet) {
    if (auto sent = check(avcodec_send_packet(audio.get(), packet), "avcodec_send_packet (audio)"); !sent)
        return sent;
    while (true) {
        const auto received = receiveFrame(audio.get(), decoded.get());
        if (!received)
            return std::unexpected(received.error());
        if (!*received)
            return {};
        if (decoded->ch_layout.nb_channels * static_cast<int>(sizeof(float)) != audioFrameBytes)
            return Core::fail("video", "The audio channel count changed during the video.");
        const double time = loopStart + static_cast<double>(decoded->best_effort_timestamp - audioStartPts) * audioTimeBase;
        const int dropped = static_cast<int>(std::clamp(std::ceil((clockStart - time) * sampleRate), 0.0, static_cast<double>(decoded->nb_samples)));
        const int kept = decoded->nb_samples - dropped;
        for (size_t channel = 0; channel < audioPlanes.size(); ++channel)
            audioPlanes[channel] = reinterpret_cast<const float*>(decoded->extended_data[channel]) + dropped;
        if (kept > 0) {
            if (!SDL_PutAudioStreamPlanarData(audioOutput.get(), audioPlanes.data(), static_cast<int>(audioPlanes.size()), kept))
                return sdlFailure("SDL_PutAudioStreamPlanarData");
            audioFramesPut += kept;
        }
        av_frame_unref(decoded.get());
    }
}

std::expected<void, Core::Error> VideoPlayer::decodeVideo(const PendingPacket& pending, const std::stop_token stop) {
    if (auto sent = check(avcodec_send_packet(video.get(), pending.packet.get()), "avcodec_send_packet (video)"); !sent)
        return sent;
    while (true) {
        const auto received = receiveFrame(video.get(), decoded.get());
        if (!received)
            return std::unexpected(received.error());
        if (!*received)
            break;
        if (auto queued = queueFrame(pending.loopStart, stop); !queued)
            return queued;
        av_frame_unref(decoded.get());
    }
    if (!pending.packet)
        avcodec_flush_buffers(video.get());
    return {};
}

std::expected<void, Core::Error> VideoPlayer::queueFrame(const double loopStart, const std::stop_token stop) {
    if (decoded->width != width || decoded->height != height)
        return Core::fail("video", std::format("A frame is {} x {}, not the video size {} x {}.", decoded->width, decoded->height, width, height));
    const double position = static_cast<double>(decoded->best_effort_timestamp - videoStartPts) * videoTimeBase;
    const double time = loopStart + position;
    if (time + frameInterval <= clockStart)
        return {};
    std::unique_lock lock(mutex);
    const std::stop_callback onStop(stop, [this] { frameTaken.notify_all(); });
    frameTaken.wait(lock, [&] { return stop.stop_requested() || frameCount < frameCapacity; });
    if (stop.stop_requested())
        return {};
    QueuedFrame& queued = frames[(firstFrame + frameCount) % frameCapacity];
    lock.unlock();
    if (auto taken = take(*queued.frame); !taken)
        return taken;
    queued.time = time;
    queued.position = position;
    lock.lock();
    ++frameCount;
    return {};
}

std::expected<void, Core::Error> VideoPlayer::take(AVFrame& queued) {
    av_frame_unref(&queued);
    const auto format = static_cast<AVPixelFormat>(decoded->format);
    if (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUVJ420P) {
        av_frame_move_ref(&queued, decoded.get());
        return {};
    }
    if (!scaler) {
        scaler.reset(sws_getContext(width, height, format, width, height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr));
        if (!scaler)
            return Core::fail("video", std::format("sws_getContext found no conversion of {} video frames to YUV 4:2:0.", av_get_pix_fmt_name(format)));
    }
    queued.format = AV_PIX_FMT_YUV420P;
    queued.width = width;
    queued.height = height;
    if (auto buffer = check(av_frame_get_buffer(&queued, 0), "av_frame_get_buffer"); !buffer)
        return buffer;
    if (auto props = check(av_frame_copy_props(&queued, decoded.get()), "av_frame_copy_props"); !props)
        return props;
    sws_scale(scaler.get(), decoded->data, decoded->linesize, 0, height, queued.data, queued.linesize);
    return {};
}

VideoImage VideoPlayer::imageOf(const AVFrame& frame) const {
    const bool bt601 = frame.colorspace == AVCOL_SPC_BT470BG || frame.colorspace == AVCOL_SPC_SMPTE170M;
    return {
        .planes = {frame.data[0], frame.data[1], frame.data[2]},
        .strides = {frame.linesize[0], frame.linesize[1], frame.linesize[2]},
        .width = width,
        .height = height,
        .matrix = bt601 ? YuvMatrix::Bt601 : YuvMatrix::Bt709,
        .fullRange = frame.color_range == AVCOL_RANGE_JPEG || frame.format == AV_PIX_FMT_YUVJ420P,
    };
}

bool VideoPlayer::hasFrameSpace() {
    std::scoped_lock lock(mutex);
    return frameCount < frameCapacity;
}

}
