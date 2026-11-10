#pragma once

#include <expected>
#include <optional>

#include <RmlUi/Core/CallbackTexture.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Geometry.h>

#include "RmlKit/Core/Error.h"
#include "RmlKit/Media/VideoPlayer.h"
#include "RmlKit/UI/Backend/VideoStore.h"
#include "RmlKit/UI/Backend/VideoTexture.h"
#include "RmlKit/UI/Controls/Maximizer.h"

namespace RmlKit::UI {

struct VideoVolume {
    float level = 1.0f;
    bool muted = false;
};

class VideoElement final : public Rml::Element, public Rml::EventListener {
public:
    VideoElement(const Rml::String& tag, VideoConverter& converter, const VideoStore& videos, VideoVolume& volume);
    ~VideoElement() override;

    void ProcessEvent(Rml::Event& event) override;

protected:
    void OnUpdate() override;
    void OnRender() override;
    void OnResize() override;
    void OnAttributeChange(const Rml::ElementAttributes& changedAttributes) override;
    void OnPropertyChange(const Rml::PropertyIdSet& changedProperties) override;

private:
    void open(const Rml::String& source);
    void close();
    std::expected<void, Core::Error> handlePlayerEvent(Rml::Event& event);
    void stop(const Core::Error& error);
    void generateGeometry(Rml::RenderManager& renderManager);
    Rml::Element* findControl(const Rml::Event& event, const Rml::String& className);
    std::expected<void, Core::Error> setPaused(bool paused);
    std::expected<void, Core::Error> applyVolume();
    void setMaximized(bool maximized);
    std::expected<void, Core::Error> finishScrub();
    void updateControls();

    VideoConverter& converter;
    const VideoStore& videos;
    VideoVolume& volume;
    std::optional<Media::VideoPlayer> player;
    Rml::String source;
    Rml::CallbackTexture texture;
    std::optional<VideoTexture> videoTexture;
    Rml::Geometry geometry;
    bool geometryDirty = true;

    Rml::Element* scrubTrack = nullptr;
    double scrubTime = 0.0;
    bool playAfterScrub = false;
    bool pausedWhileHidden = false;
    Rml::Element* volumeTrack = nullptr;
    Rml::String shownTimeText;
    float shownFillPercent = -1.0f;
    float shownVolumePercent = -1.0f;
    Maximizer maximizer;
};

class VideoElementInstancer final : public Rml::ElementInstancer {
public:
    VideoElementInstancer(VideoConverter& converter, const VideoStore& videos) : converter(converter), videos(videos) {}

    Rml::ElementPtr InstanceElement(Rml::Element* parent, const Rml::String& tag, const Rml::XMLAttributes& attributes) override;
    void ReleaseElement(Rml::Element* element) override;

private:
    VideoConverter& converter;
    const VideoStore& videos;
    VideoVolume volume;
};

}
