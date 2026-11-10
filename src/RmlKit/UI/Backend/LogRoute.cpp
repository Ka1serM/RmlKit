#include "RmlKit/UI/Backend/LogRoute.h"

#include <atomic>
#include <string>

#if RMLKIT_VIDEO
extern "C" {
#include <libavutil/log.h>
}
#endif

#include "RmlKit/UI/Backend/Dispatcher.h"
#include "RmlKit/UI/Models/Model.h"

namespace RmlKit::UI {

namespace {

#if RMLKIT_VIDEO
std::atomic<Model*> ffmpegModel = nullptr;
#endif

ReportLevel reportLevel(const Rml::Log::Type type) {
    switch (type) {
    case Rml::Log::LT_ALWAYS:
    case Rml::Log::LT_ERROR:
    case Rml::Log::LT_ASSERT:
        return ReportLevel::Error;
    case Rml::Log::LT_WARNING:
        return ReportLevel::Warning;
    default:
        return ReportLevel::Info;
    }
}

#if RMLKIT_VIDEO
ReportLevel reportLevel(const int ffmpegLevel) {
    if (ffmpegLevel <= AV_LOG_ERROR)
        return ReportLevel::Error;
    if (ffmpegLevel <= AV_LOG_WARNING)
        return ReportLevel::Warning;
    return ReportLevel::Info;
}
#endif

}

void LogSystemInterface::SetMouseCursor(const Rml::String& cursorName) {
    if (cursorName == "ew-resize")
        return static_cast<void>(SDL_SetCursor(horizontalResize.get()));
    if (cursorName == "ns-resize")
        return static_cast<void>(SDL_SetCursor(verticalResize.get()));
    SystemInterface_SDL::SetMouseCursor(cursorName);
}

bool LogSystemInterface::LogMessage(const Rml::Log::Type type, const Rml::String& message) {
    if (!model)
        return SystemInterface_SDL::LogMessage(type, message);
    model->report(reportLevel(type), "ui", message);
    return true;
}

LogRoute::LogRoute(LogSystemInterface& system, Model& model) : system(system) {
    system.setModel(&model);
#if RMLKIT_VIDEO
    ffmpegModel = &model;
    av_log_set_callback(logFfmpeg);
#endif
}

LogRoute::~LogRoute() {
#if RMLKIT_VIDEO
    av_log_set_callback(av_log_default_callback);
    ffmpegModel = nullptr;
#endif
    system.setModel(nullptr);
}

#if RMLKIT_VIDEO
void LogRoute::logFfmpeg(void* context, const int level, const char* format, va_list arguments) {
    if (level > av_log_get_level())
        return;
    Model* const model = ffmpegModel;
    if (!model)
        return av_log_default_callback(context, level, format, arguments);
    //! FFmpeg writes the "[aac @ 0x...] " prefix only at the start of a line.
    thread_local int printPrefix = 1;
    char text[1024];
    av_log_format_line2(context, level, format, arguments, text, sizeof(text), &printPrefix);
    std::string line(text);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();
    if (line.empty())
        return;
    Dispatcher::postUi([model, level, line = std::move(line)]() mutable { model->report(reportLevel(level), "video", std::move(line)); });
}
#endif

}
