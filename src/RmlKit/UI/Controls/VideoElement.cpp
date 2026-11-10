#include "RmlKit/UI/Controls/VideoElement.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Log.h>
#include <RmlUi/Core/MeshUtilities.h>
#include <RmlUi/Core/Property.h>
#include <RmlUi/Core/PropertyIdSet.h>
#include <RmlUi/Core/RenderBox.h>
#include <RmlUi/Core/RenderManager.h>

namespace RmlKit::UI {

namespace {

constexpr Rml::EventId controlEvents[] = {Rml::EventId::Click, Rml::EventId::Mousedown, Rml::EventId::Mouseup, Rml::EventId::Drag, Rml::EventId::Dragend};

float getTrackFraction(Rml::Element& track, const Rml::Event& event) {
    const float left = track.GetAbsoluteOffset(Rml::BoxArea::Border).x;
    const float width = track.GetBox().GetSize(Rml::BoxArea::Border).x;
    if (width <= 0)
        return 0.0f;
    return std::clamp((event.GetParameter<float>("mouse_x", left) - left) / width, 0.0f, 1.0f);
}

std::string formatTime(const double seconds) {
    const int whole = static_cast<int>(seconds);
    return std::format("{}:{:02}", whole / 60, whole % 60);
}

}

VideoElement::VideoElement(const Rml::String& tag, VideoConverter& converter, const VideoStore& videos, VideoVolume& volume)
    : Rml::Element(tag), converter(converter), videos(videos), volume(volume), maximizer(*this, *this) {
    for (const Rml::EventId event : controlEvents)
        AddEventListener(event, this);
}

VideoElement::~VideoElement() {
    for (const Rml::EventId event : controlEvents)
        RemoveEventListener(event, this);
}

void VideoElement::ProcessEvent(Rml::Event& event) {
    if (maximizer.handleKey(event))
        return;
    if (event.GetId() == Rml::EventId::Click && findControl(event, "video-maximize")) {
        setMaximized(!maximizer.isMaximized());
        return;
    }
    if (!player)
        return;
    if (auto handled = handlePlayerEvent(event); !handled)
        stop(handled.error());
}

std::expected<void, Core::Error> VideoElement::handlePlayerEvent(Rml::Event& event) {
    switch (event.GetId()) {
    case Rml::EventId::Click:
        if (findControl(event, "video-mute")) {
            volume.muted = volume.level > 0.0f && !volume.muted;
            if (volume.level == 0.0f)
                volume.level = 1.0f;
            return applyVolume();
        }
        if (event.GetTargetElement() != this && !findControl(event, "video-toggle"))
            return {};
        return setPaused(!player->isPaused());
    case Rml::EventId::Mousedown:
        if (event.GetParameter<int>("button", 0) != 0)
            return {};
        volumeTrack = findControl(event, "video-volume");
        if (volumeTrack) {
            volume.level = getTrackFraction(*volumeTrack, event);
            volume.muted = false;
            return applyVolume();
        }
        scrubTrack = findControl(event, "video-seek");
        if (!scrubTrack)
            return {};
        playAfterScrub = !player->isPaused();
        if (auto paused = setPaused(true); !paused)
            return paused;
        scrubTime = getTrackFraction(*scrubTrack, event) * player->getDuration();
        updateControls();
        return {};
    case Rml::EventId::Drag:
        if (volumeTrack) {
            volume.level = getTrackFraction(*volumeTrack, event);
            return applyVolume();
        }
        if (!scrubTrack)
            return {};
        scrubTime = getTrackFraction(*scrubTrack, event) * player->getDuration();
        updateControls();
        return {};
    case Rml::EventId::Mouseup:
    case Rml::EventId::Dragend:
        volumeTrack = nullptr;
        if (!scrubTrack)
            return {};
        return finishScrub();
    default:
        return {};
    }
}

void VideoElement::OnUpdate() {
    maximizer.update();
    if (!player)
        return;
    if (!IsVisible(true)) {
        if (player->isPaused())
            return;
        if (auto paused = setPaused(true); !paused)
            return stop(paused.error());
        pausedWhileHidden = true;
        return;
    }
    if (std::exchange(pausedWhileHidden, false))
        if (auto played = setPaused(false); !played)
            return stop(played.error());
    if (videoTexture)
        if (auto shownFrame = player->readDueFrame([this](const Media::VideoImage& frame) { videoTexture->convert(frame); }); !shownFrame)
            return stop(shownFrame.error());
    updateControls();
    const auto wait = player->getSecondsToNextFrame();
    if (!wait)
        return stop(wait.error());
    if (Rml::Context* context = GetContext())
        context->RequestNextUpdate(*wait);
}

void VideoElement::OnRender() {
    if (!player)
        return;
    Rml::RenderManager* renderManager = GetRenderManager();
    if (!renderManager)
        return;
    if (!texture) {
        texture = renderManager->MakeCallbackTexture([this](const Rml::CallbackTextureInterface& callback) {
            videoTexture.emplace(converter, player->getWidth(), player->getHeight());
            callback.SetTextureHandle(videoTexture->getHandle(), {player->getWidth(), player->getHeight()});
            return true;
        });
    }
    if (geometryDirty)
        generateGeometry(*renderManager);
    geometry.Render(GetAbsoluteOffset(Rml::BoxArea::Border), texture);
}

void VideoElement::OnResize() {
    geometryDirty = true;
}

void VideoElement::OnAttributeChange(const Rml::ElementAttributes& changedAttributes) {
    Rml::Element::OnAttributeChange(changedAttributes);
    if (changedAttributes.find("src") == changedAttributes.end())
        return;
    open(GetAttribute<Rml::String>("src", ""));
}

void VideoElement::OnPropertyChange(const Rml::PropertyIdSet& changedProperties) {
    Rml::Element::OnPropertyChange(changedProperties);
    if (changedProperties.Contains(Rml::PropertyId::ImageColor) || changedProperties.Contains(Rml::PropertyId::Opacity))
        geometryDirty = true;
}

void VideoElement::open(const Rml::String& source) {
    close();
    this->source = source;
    if (source.empty())
        return;
    std::shared_ptr<const std::vector<std::byte>> file = videos.find(source);
    if (!file) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "No video is named '%s'.", source.c_str());
        return;
    }
    player.emplace();
    if (auto opened = player->open(std::move(file)); !opened) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "The video '%s' cannot play: %s", source.c_str(), opened.error().message.c_str());
        player.reset();
        return;
    }
    SetClass("loaded", true);
    SetClass("paused", false);
    if (auto applied = applyVolume(); !applied)
        stop(applied.error());
}

void VideoElement::close() {
    videoTexture.reset();
    texture.Release();
    player.reset();
    geometryDirty = true;
    scrubTrack = nullptr;
    volumeTrack = nullptr;
    SetClass("loaded", false);
    SetClass("paused", false);
    if (Rml::Element* fill = QuerySelector(".video-seek-fill"))
        fill->SetProperty(Rml::PropertyId::Width, Rml::Property(0.0f, Rml::Unit::PERCENT));
    if (Rml::Element* clock = QuerySelector(".video-clock"))
        clock->SetInnerRML("");
    shownTimeText.clear();
    shownFillPercent = -1.0f;
    shownVolumePercent = -1.0f;
}

void VideoElement::stop(const Core::Error& error) {
    Rml::Log::Message(Rml::Log::LT_ERROR, "The video '%s' stopped: %s", source.c_str(), error.message.c_str());
    close();
}

void VideoElement::generateGeometry(Rml::RenderManager& renderManager) {
    geometryDirty = false;
    const Rml::RenderBox box = GetRenderBox(Rml::BoxArea::Content);
    const Rml::Vector2f size = box.GetFillSize();
    if (size.x <= 0 || size.y <= 0) {
        geometry = {};
        return;
    }
    const float boxRatio = size.x / size.y;
    const float videoRatio = static_cast<float>(player->getWidth()) / static_cast<float>(player->getHeight());
    Rml::Vector2f quadOffset = box.GetFillOffset();
    Rml::Vector2f quadSize = size;
    Rml::Vector2f extent(1, 1);
    if (IsClassSet("maximized")) {
        if (videoRatio > boxRatio)
            quadSize.y = size.x / videoRatio;
        else
            quadSize.x = size.y * videoRatio;
        quadOffset += (size - quadSize) * 0.5f;
    } else {
        if (videoRatio > boxRatio)
            extent.x = boxRatio / videoRatio;
        else
            extent.y = videoRatio / boxRatio;
    }
    const Rml::Vector2f topLeft = (Rml::Vector2f(1, 1) - extent) * 0.5f;

    const Rml::ComputedValues& computed = GetComputedValues();
    Rml::Mesh mesh;
    Rml::MeshUtilities::GenerateQuad(mesh, quadOffset, quadSize, computed.image_color().ToPremultiplied(computed.opacity()), topLeft, topLeft + extent);
    geometry = renderManager.MakeGeometry(std::move(mesh));
}

Rml::Element* VideoElement::findControl(const Rml::Event& event, const Rml::String& className) {
    for (Rml::Element* element = event.GetTargetElement(); element && element != this; element = element->GetParentNode())
        if (element->IsClassSet(className))
            return element;
    return nullptr;
}

std::expected<void, Core::Error> VideoElement::setPaused(const bool paused) {
    if (auto set = player->setPaused(paused); !set)
        return set;
    SetClass("paused", paused);
    return {};
}

std::expected<void, Core::Error> VideoElement::applyVolume() {
    const float level = volume.muted ? 0.0f : volume.level;
    if (auto set = player->setVolume(level); !set)
        return set;
    SetClass("muted", level == 0.0f);
    Rml::Element* fill = QuerySelector(".video-volume-fill");
    if (!fill || level * 100.0f == shownVolumePercent)
        return {};
    shownVolumePercent = level * 100.0f;
    fill->SetProperty(Rml::PropertyId::Width, Rml::Property(shownVolumePercent, Rml::Unit::PERCENT));
    return {};
}

void VideoElement::setMaximized(const bool maximized) {
    maximizer.set(maximized);
    geometryDirty = true;
}

std::expected<void, Core::Error> VideoElement::finishScrub() {
    scrubTrack = nullptr;
    if (auto sought = player->seek(scrubTime); !sought)
        return sought;
    if (auto paused = setPaused(!playAfterScrub); !paused)
        return paused;
    updateControls();
    return {};
}

void VideoElement::updateControls() {
    const double duration = player->getDuration();
    const double time = std::min(scrubTrack ? scrubTime : player->getPosition(), duration);
    if (Rml::Element* fill = QuerySelector(".video-seek-fill")) {
        const float percent = static_cast<float>(100.0 * time / duration);
        if (std::abs(percent - shownFillPercent) >= 0.05f) {
            shownFillPercent = percent;
            fill->SetProperty(Rml::PropertyId::Width, Rml::Property(percent, Rml::Unit::PERCENT));
        }
    }
    if (Rml::Element* clock = QuerySelector(".video-clock")) {
        const Rml::String text = std::format("{} / {}", formatTime(time), formatTime(duration));
        if (text != shownTimeText) {
            shownTimeText = text;
            clock->SetInnerRML(text);
        }
    }
}

Rml::ElementPtr VideoElementInstancer::InstanceElement(Rml::Element*, const Rml::String& tag, const Rml::XMLAttributes&) {
    return Rml::ElementPtr(new VideoElement(tag, converter, videos, volume));
}

void VideoElementInstancer::ReleaseElement(Rml::Element* element) {
    delete element;
}

}
