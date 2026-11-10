/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @brief
 * VideoSink.cpp
 *
 * Implementation of VideoSink service
 */

#include "VideoSink.h"

#include <binder/IInterface.h>
#include <chrono>
#include <cstdio>
#include <thread>

#define LOG_TAG "VideoSink"
#define LOG_INFO(...) do { std::fprintf(stderr, "I/%s: ", LOG_TAG); std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } while (0)
#define LOG_ERROR(...) do { std::fprintf(stderr, "E/%s: ", LOG_TAG); std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } while (0)

namespace com {
namespace rdk {
namespace hal {
namespace videosink {

namespace {

constexpr int64_t kUndefinedPresentationTime = -1;

android::sp<IVideoSinkEventListener> g_event_listeners[NUM_VIDEO_SINK];
android::sp<IVideoSinkControllerListener> g_controller_listeners[NUM_VIDEO_SINK];
android::sp<IVideoSinkController> g_open_controllers[NUM_VIDEO_SINK];
avclock::IAVClock::Id g_clock_ids[NUM_VIDEO_SINK];
bool g_end_of_stream_signalled[NUM_VIDEO_SINK] = {false, false};
bool g_end_of_stream_callback_sent[NUM_VIDEO_SINK] = {false, false};
bool g_queue_full_reported[NUM_VIDEO_SINK] = {false, false};
bool g_first_frame_rendered[NUM_VIDEO_SINK] = {false, false};
int64_t g_last_rendered_presentation_time[NUM_VIDEO_SINK] = {
    kUndefinedPresentationTime,
    kUndefinedPresentationTime,
};

bool isValidSinkId(int32_t sinkId)
{
    return sinkId >= 0 && sinkId < NUM_VIDEO_SINK;
}

bool isValidAVSourceValue(int32_t value)
{
    switch (static_cast<AVSource>(value))
    {
        case AVSource::AUTO:
        case AVSource::UNKNOWN:
        case AVSource::IP:
        case AVSource::TUNER:
        case AVSource::SYSTEM:
        case AVSource::HDMI_1:
        case AVSource::HDMI_2:
        case AVSource::HDMI_3:
        case AVSource::HDMI_4:
        case AVSource::HDMI_5:
        case AVSource::COMPOSITE_1:
        case AVSource::COMPOSITE_2:
        case AVSource::COMPOSITE_3:
        case AVSource::COMPOSITE_4:
        case AVSource::COMPOSITE_5:
            return true;
        default:
            return false;
    }
}

template <typename Interface>
bool hasSameBinder(const android::sp<Interface>& lhs, const android::sp<Interface>& rhs)
{
    if (lhs == nullptr || rhs == nullptr) {
        return lhs == rhs;
    }
    return android::IInterface::asBinder(lhs) == android::IInterface::asBinder(rhs);
}

void notifyStateChangedLocked(int32_t sinkId, State oldState, State newState)
{
    if (g_event_listeners[sinkId]) {
        g_event_listeners[sinkId]->onStateChanged(oldState, newState);
    }
    if (g_controller_listeners[sinkId]) {
        g_controller_listeners[sinkId]->onStateChanged(oldState, newState);
    }
}

/* rdk-halif-aidl 0.23.0: onFirstFrameRendered() is reported only once a video plane is mapped.
 * The mock has no plane control, so no plane is ever mapped and the callback is not reported. */
static constexpr bool kMockVideoPlaneMapped = false;

void notifyFirstFrameRenderedLocked(int32_t sinkId, int64_t nsPresentationTime)
{
    if (!kMockVideoPlaneMapped)
    {
        LOG_INFO("onFirstFrameRendered not reported for ID %d: no video plane mapped (PTS %ld)", sinkId, nsPresentationTime);
        return;
    }
    if (g_event_listeners[sinkId]) {
        g_event_listeners[sinkId]->onFirstFrameRendered(nsPresentationTime);
    }
    if (g_controller_listeners[sinkId]) {
        g_controller_listeners[sinkId]->onFirstFrameRendered(nsPresentationTime);
    }
}

void notifyEndOfStreamLocked(int32_t sinkId, int64_t nsPresentationTime)
{
    if (g_event_listeners[sinkId]) {
        g_event_listeners[sinkId]->onEndOfStream(nsPresentationTime);
    }
    if (g_controller_listeners[sinkId]) {
        g_controller_listeners[sinkId]->onEndOfStream(nsPresentationTime);
    }
}

void notifyFlushCompleteLocked(int32_t sinkId)
{
    if (g_event_listeners[sinkId]) {
        g_event_listeners[sinkId]->onFlushComplete();
    }
    if (g_controller_listeners[sinkId]) {
        g_controller_listeners[sinkId]->onFlushComplete();
    }
}

void notifyFrameBufferAvailableLocked(int32_t sinkId)
{
    if (!g_queue_full_reported[sinkId] || g_sink_queue_depth[sinkId] >= MAX_QUEUE_DEPTH)
    {
        return;
    }
    g_queue_full_reported[sinkId] = false;
    if (g_controller_listeners[sinkId])
    {
        g_controller_listeners[sinkId]->onFrameBufferAvailable();
    }
}

void clearQueuedFramesLocked(int32_t sinkId)
{
    while (!g_frame_queue[sinkId].empty())
    {
        g_frame_queue[sinkId].pop();
    }
    g_sink_queue_depth[sinkId] = 0;
}

void resetPlaybackStateLocked(int32_t sinkId, bool resetClock)
{
    clearQueuedFramesLocked(sinkId);
    g_end_of_stream_signalled[sinkId] = false;
    g_end_of_stream_callback_sent[sinkId] = false;
    g_queue_full_reported[sinkId] = false;
    g_first_frame_rendered[sinkId] = false;
    g_last_rendered_presentation_time[sinkId] = kUndefinedPresentationTime;
    if (resetClock)
    {
        g_clock_ids[sinkId].value = avclock::IAVClock::Id::UNDEFINED;
    }
}

void completeEndOfStreamIfPossibleLocked(int32_t sinkId)
{
    if (!g_end_of_stream_signalled[sinkId] || g_end_of_stream_callback_sent[sinkId] || g_sink_queue_depth[sinkId] != 0)
    {
        return;
    }

    const int64_t presentationTime =
        g_last_rendered_presentation_time[sinkId] == kUndefinedPresentationTime
            ? kUndefinedPresentationTime
            : g_last_rendered_presentation_time[sinkId];
    g_end_of_stream_callback_sent[sinkId] = true;
    notifyEndOfStreamLocked(sinkId, presentationTime);
}

void consumeQueuedFramesIfPresentingLocked(int32_t sinkId)
{
    if (g_videosink_state[sinkId] != State::STARTED ||
        g_clock_ids[sinkId].value == avclock::IAVClock::Id::UNDEFINED)
    {
        return;
    }

    while (!g_frame_queue[sinkId].empty())
    {
        const int64_t presentationTime = g_frame_queue[sinkId].front().first;
        g_frame_queue[sinkId].pop();
        --g_sink_queue_depth[sinkId];

        if (!g_first_frame_rendered[sinkId])
        {
            g_first_frame_rendered[sinkId] = true;
            notifyFirstFrameRenderedLocked(sinkId, presentationTime);
        }

        g_last_rendered_presentation_time[sinkId] = presentationTime;
    }

    notifyFrameBufferAvailableLocked(sinkId);
    completeEndOfStreamIfPossibleLocked(sinkId);
}

}  // namespace

// Global state
State g_videosink_state[NUM_VIDEO_SINK] = {State::UNKNOWN, State::UNKNOWN};
videodecoder::IVideoDecoder::Id g_videodecoder_id[NUM_VIDEO_SINK];
int32_t g_resource_id[NUM_VIDEO_SINK];
int32_t g_sink_queue_depth[NUM_VIDEO_SINK];
AVSource g_av_source[NUM_VIDEO_SINK];
int32_t g_render_first_frame[NUM_VIDEO_SINK];
std::queue<std::pair<int64_t, int64_t>> g_frame_queue[NUM_VIDEO_SINK]; // nsPresentationTime, frameBufferHandle

std::mutex state_mutex;
std::condition_variable state_cv;

VideoSinkManager::VideoSinkManager()
{
    reset();
    LOG_INFO("VideoSinkManager service initialized");
}

VideoSinkManager::~VideoSinkManager()
{
    LOG_INFO("VideoSinkManager service destroyed");
}

void VideoSinkManager::reset()
{
    std::lock_guard<std::mutex> lock(state_mutex);
    for (int32_t i = 0; i < NUM_VIDEO_SINK; i++)
    {
        g_videosink_state[i] = State::CLOSED;
        g_av_source[i] = AVSource::UNKNOWN;
        g_videodecoder_id[i].value = videodecoder::IVideoDecoder::Id::UNDEFINED;
        g_resource_id[i] = i;
        g_render_first_frame[i] = 0;
        g_event_listeners[i] = nullptr;
        g_controller_listeners[i] = nullptr;
        g_open_controllers[i] = nullptr;
        resetPlaybackStateLocked(i, true);
    }
}

android::binder::Status VideoSinkManager::getVideoSinkIds(std::vector<IVideoSink::Id>* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("getVideoSinkIds: null _aidl_return");
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    _aidl_return->clear();
    for (int32_t i = 0; i < NUM_VIDEO_SINK; i++)
    {
        IVideoSink::Id sinkId;
        sinkId.value = g_resource_id[i];
        _aidl_return->push_back(sinkId);
    }
    return android::binder::Status::ok();
}

android::binder::Status VideoSinkManager::getVideoSink(const IVideoSink::Id& videoSinkId, android::sp<IVideoSink>* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("getVideoSink: null _aidl_return");
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!isValidSinkId(videoSinkId.value))
    {
        LOG_ERROR("getVideoSink: invalid videoSinkId %d", videoSinkId.value);
        *_aidl_return = nullptr;
        return android::binder::Status::ok();
    }
    *_aidl_return = new VideoSink(videoSinkId);
    return android::binder::Status::ok();
}

VideoSink::VideoSink(IVideoSink::Id id) : id(id)
{
    eventListener = nullptr;
    LOG_INFO("VideoSink service initialized for ID %d", id.value);
}

VideoSink::~VideoSink()
{
    LOG_INFO("VideoSink service destroyed for ID %d", id.value);
}

android::binder::Status VideoSink::getCapabilities(videosink::Capabilities* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("getCapabilities: null _aidl_return for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    videosink::Capabilities capabilities;
    capabilities.supportsAVSync = true;
    capabilities.vsyncDisplayLatencyNs = 16000000;
    capabilities.supportsHoldLastFrame = true;
    capabilities.supportsSecure = false;
    *_aidl_return = capabilities;
    return android::binder::Status::ok();
}

android::binder::Status VideoSink::getProperty(videosink::Property property, std::optional<PropertyValue>* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("getProperty: null _aidl_return for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("getProperty: invalid ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    PropertyValue value;
    switch (property)
    {
        case videosink::Property::RESOURCE_ID:
            value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(id.value);
            break;
        case videosink::Property::AV_SOURCE:
            value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(static_cast<int32_t>(g_av_source[id.value]));
            break;
        case videosink::Property::SINK_QUEUE_DEPTH:
            value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(g_sink_queue_depth[id.value]);
            break;
        case videosink::Property::RENDER_FIRST_FRAME:
            value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(g_render_first_frame[id.value]);
            break;
        case videosink::Property::METRIC_FRAMES_RECEIVED:
            value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(g_sink_queue_depth[id.value]); // Mock
            break;
        case videosink::Property::METRIC_FRAMES_PRESENTED:
            value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(g_sink_queue_depth[id.value] / 2); // Mock
            break;
        case videosink::Property::METRIC_FRAMES_DROPPED_LATE:
            value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(0); // Mock
            break;
        case videosink::Property::METRIC_FRAMES_DROPPED_FRC:
            value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(0); // Mock
            break;
        case videosink::Property::METRIC_FRAMES_REPEATED_FRC:
            value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(0); // Mock
            break;
        case videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME:
            value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(0); // Mock
            break;
        case videosink::Property::METRIC_UNDERFLOWED:
            value.value = PropertyValue::Value::make<PropertyValue::Value::intValue>(g_frame_queue[id.value].empty() ? 1 : 0); // Mock
            break;
        default:
            LOG_ERROR("getProperty: invalid property value");
            return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    *_aidl_return = value;
    return android::binder::Status::ok();
}

android::binder::Status VideoSink::getState(State* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("getState: null _aidl_return for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("getState: invalid ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    *_aidl_return = g_videosink_state[id.value];
    return android::binder::Status::ok();
}

// VideoSink::open
android::binder::Status VideoSink::open(const android::sp<videosink::IVideoSinkControllerListener>& videoSinkControllerListener,
                                        android::sp<IVideoSinkController>* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("open: null _aidl_return for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!videoSinkControllerListener)
    {
        LOG_ERROR("open: null videoSinkControllerListener for ID %d", id.value);
        *_aidl_return = nullptr;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("open: invalid ID %d", id.value);
        *_aidl_return = nullptr;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }

    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::CLOSED)
    {
        LOG_ERROR("open: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        *_aidl_return = nullptr;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }

    controllerListener = videoSinkControllerListener;
    g_controller_listeners[id.value] = videoSinkControllerListener;
    g_videodecoder_id[id.value].value = videodecoder::IVideoDecoder::Id::UNDEFINED;
    resetPlaybackStateLocked(id.value, true);

    g_videosink_state[id.value] = State::OPENING;
    LOG_INFO("open: ID %d transitioning CLOSED -> OPENING", id.value);
    notifyStateChangedLocked(id.value, State::CLOSED, State::OPENING);
    state_cv.notify_all();

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    g_videosink_state[id.value] = State::READY;
    LOG_INFO("open: ID %d transitioning OPENING -> READY", id.value);
    notifyStateChangedLocked(id.value, State::OPENING, State::READY);
    state_cv.notify_all();

    android::sp<IVideoSinkController> controller = new VideoSinkController(id);
    g_open_controllers[id.value] = controller;
    *_aidl_return = controller;
    return android::binder::Status::ok();
}

android::binder::Status VideoSink::close(const android::sp<videosink::IVideoSinkController>& videoSinkController, bool* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("close: null _aidl_return for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!videoSinkController)
    {
        LOG_ERROR("close: null videoSinkController for ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("close: invalid ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::READY)
    {
        LOG_ERROR("close: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }
    if (!g_open_controllers[id.value] || !hasSameBinder(g_open_controllers[id.value], videoSinkController))
    {
        LOG_ERROR("close: unrecognized controller for ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::ok();
    }
    g_videosink_state[id.value] = State::CLOSING;
    LOG_INFO("close: ID %d transitioning READY -> CLOSING", id.value);
    notifyStateChangedLocked(id.value, State::READY, State::CLOSING);
    state_cv.notify_all();

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    g_videosink_state[id.value] = State::CLOSED;
    LOG_INFO("close: ID %d transitioning CLOSING -> CLOSED", id.value);
    notifyStateChangedLocked(id.value, State::CLOSING, State::CLOSED);
    state_cv.notify_all();

    controllerListener = nullptr;
    g_controller_listeners[id.value] = nullptr;
    g_open_controllers[id.value] = nullptr;
    g_videodecoder_id[id.value].value = videodecoder::IVideoDecoder::Id::UNDEFINED;
    resetPlaybackStateLocked(id.value, true);
    *_aidl_return = true;
    return android::binder::Status::ok();
}

android::binder::Status VideoSink::registerEventListener(const android::sp<videosink::IVideoSinkEventListener>& videoSinkEventListener,
                                                         bool* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("registerEventListener: null _aidl_return for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!videoSinkEventListener)
    {
        LOG_ERROR("registerEventListener: null videoSinkEventListener for ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("registerEventListener: invalid ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    if (eventListener)
    {
        LOG_ERROR("registerEventListener: listener already registered for ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::ok();
    }
    eventListener = videoSinkEventListener;
    g_event_listeners[id.value] = videoSinkEventListener;
    *_aidl_return = true;
    LOG_INFO("registerEventListener: registered for ID %d", id.value);
    return android::binder::Status::ok();
}

android::binder::Status VideoSink::unregisterEventListener(const android::sp<videosink::IVideoSinkEventListener>& videoSinkEventListener,
                                                           bool* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("unregisterEventListener: null _aidl_return for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!videoSinkEventListener)
    {
        LOG_ERROR("unregisterEventListener: null videoSinkEventListener for ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("unregisterEventListener: invalid ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    if (!eventListener || !hasSameBinder(eventListener, videoSinkEventListener))
    {
        LOG_ERROR("unregisterEventListener: no listener registered for ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::ok();
    }
    eventListener = nullptr;
    g_event_listeners[id.value] = nullptr;
    *_aidl_return = true;
    LOG_INFO("unregisterEventListener: unregistered for ID %d", id.value);
    return android::binder::Status::ok();
}

// VideoSinkController constructor
VideoSinkController::VideoSinkController(IVideoSink::Id id)
    : id(id)
{
    LOG_INFO("VideoSinkController service initialized for ID %d", id.value);
}

VideoSinkController::~VideoSinkController()
{
    LOG_INFO("VideoSinkController service destroyed for ID %d", id.value);
}

android::binder::Status VideoSinkController::setProperty(videosink::Property property,
                                                         const PropertyValue& propertyValue,
                                                         bool* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("setProperty: null _aidl_return for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!propertyValue.value)
    {
        LOG_ERROR("setProperty: null propertyValue for ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NONE);
    }
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("setProperty: invalid ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard<std::mutex> lock(state_mutex);
    switch (property)
    {
        case videosink::Property::AV_SOURCE:
        {
            if (g_videosink_state[id.value] != State::READY)
            {
                LOG_ERROR("setProperty: invalid state %d for AV_SOURCE ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
                *_aidl_return = false;
                return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
            }
            int32_t intValue = propertyValue.value->get<PropertyValue::Value::Tag::intValue>();
            LOG_INFO("setProperty: AV_SOURCE = %d for ID %d", intValue, id.value);
            if (!isValidAVSourceValue(intValue))
            {
                LOG_ERROR("setProperty: invalid AV_SOURCE value %d for ID %d", intValue, id.value);
                *_aidl_return = false;
                return android::binder::Status::ok();
            }
            g_av_source[id.value] = static_cast<AVSource>(intValue);
            break;
        }
        case videosink::Property::RENDER_FIRST_FRAME:
        {
            if (g_videosink_state[id.value] != State::READY)
            {
                LOG_ERROR("setProperty: invalid state %d for RENDER_FIRST_FRAME ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
                *_aidl_return = false;
                return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
            }
            int32_t intValue = propertyValue.value->get<PropertyValue::Value::Tag::intValue>();
            LOG_INFO("setProperty: RENDER_FIRST_FRAME = %d for ID %d", intValue, id.value);
            if (intValue < 0 || intValue > 1)
            {
                LOG_ERROR("setProperty: invalid RENDER_FIRST_FRAME value %d for ID %d", intValue, id.value);
                *_aidl_return = false;
                return android::binder::Status::ok();
            }
            g_render_first_frame[id.value] = intValue;
            break;
        }
        case videosink::Property::RESOURCE_ID:
        case videosink::Property::SINK_QUEUE_DEPTH:
        case videosink::Property::METRIC_FRAMES_RECEIVED:
        case videosink::Property::METRIC_FRAMES_PRESENTED:
        case videosink::Property::METRIC_FRAMES_DROPPED_LATE:
        case videosink::Property::METRIC_FRAMES_DROPPED_FRC:
        case videosink::Property::METRIC_FRAMES_REPEATED_FRC:
        case videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME:
        case videosink::Property::METRIC_UNDERFLOWED:
            LOG_ERROR("setProperty: read-only property %d for ID %d", static_cast<int>(property), id.value);
            *_aidl_return = false;
            return android::binder::Status::fromExceptionCode(android::binder::Status::EX_UNSUPPORTED_OPERATION);
        default:
            LOG_ERROR("setProperty: unsupported property %d for ID %d", static_cast<int>(property), id.value);
            *_aidl_return = false;
            return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NONE);
    }
    *_aidl_return = true;
    return android::binder::Status::ok();
}

android::binder::Status VideoSinkController::setVideoDecoder(const videodecoder::IVideoDecoder::Id& videoDecoderId, bool* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("setVideoDecoder: null _aidl_return for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("setVideoDecoder: invalid ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::READY)
    {
        LOG_ERROR("setVideoDecoder: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }
    /* UNDEFINED (-1) clears the association, EXTERNAL (-2) is a non-HAL source */
    if (videoDecoderId.value < videodecoder::IVideoDecoder::Id::EXTERNAL)
    {
        LOG_ERROR("setVideoDecoder: invalid videoDecoderId %d for ID %d", videoDecoderId.value, id.value);
        *_aidl_return = false;
        return android::binder::Status::ok();
    }

    g_videodecoder_id[id.value] = videoDecoderId;
    *_aidl_return = true;
    LOG_INFO("setVideoDecoder: set ID %d to decoder %d", id.value, videoDecoderId.value);
    return android::binder::Status::ok();
}

android::binder::Status VideoSinkController::getVideoDecoder(videodecoder::IVideoDecoder::Id* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("getVideoDecoder: null _aidl_return for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("getVideoDecoder: invalid ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::READY && g_videosink_state[id.value] != State::STARTED)
    {
        LOG_ERROR("getVideoDecoder: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }
    *_aidl_return = g_videodecoder_id[id.value];
    return android::binder::Status::ok();
}

int32_t availableClocks[] = {0, 1}; // Mock available clock IDs
android::binder::Status VideoSinkController::attachClock(const avclock::IAVClock::Id& clockId)
{
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("attachClock: invalid ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    if (clockId.value < avclock::IAVClock::Id::UNDEFINED)
    {
        LOG_ERROR("attachClock: invalid clockId %d for ID %d", clockId.value, id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }

    if (clockId.value != avclock::IAVClock::Id::UNDEFINED)
    {
        bool clockFound = false;
        for (const int32_t availableClockId : availableClocks)
        {
            if (availableClockId == clockId.value)
            {
                clockFound = true;
                break;
            }
        }

        if (!clockFound)
        {
            LOG_ERROR("attachClock: clockId %d not available for ID %d", clockId.value, id.value);
            return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
        }
    }

    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::READY && g_videosink_state[id.value] != State::STARTED)
    {
        LOG_ERROR("attachClock: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }

    g_clock_ids[id.value] = clockId;
    consumeQueuedFramesIfPresentingLocked(id.value);
    return android::binder::Status::ok();
}

android::binder::Status VideoSinkController::detachClock()
{
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("detachClock: invalid ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }

    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::READY && g_videosink_state[id.value] != State::STARTED)
    {
        LOG_ERROR("detachClock: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }

    g_clock_ids[id.value].value = avclock::IAVClock::Id::UNDEFINED;
    return android::binder::Status::ok();
}

android::binder::Status VideoSinkController::getClock(avclock::IAVClock::Id* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("getClock: null _aidl_return for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("getClock: invalid ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }

    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::READY && g_videosink_state[id.value] != State::STARTED)
    {
        LOG_ERROR("getClock: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }

    *_aidl_return = g_clock_ids[id.value];
    return android::binder::Status::ok();
}

android::binder::Status VideoSinkController::start()
{
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("start: invalid ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::READY)
    {
        LOG_ERROR("start: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }
    /* rdk-halif-aidl 0.23.0: a video decoder (or EXTERNAL) must be associated before start() */
    if (g_videodecoder_id[id.value].value == videodecoder::IVideoDecoder::Id::UNDEFINED)
    {
        LOG_ERROR("start: no video decoder associated for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }
    g_videosink_state[id.value] = State::STARTING;
    LOG_INFO("start: ID %d transitioning READY -> STARTING", id.value);
    g_end_of_stream_signalled[id.value] = false;
    g_end_of_stream_callback_sent[id.value] = false;
    g_first_frame_rendered[id.value] = false;
    notifyStateChangedLocked(id.value, State::READY, State::STARTING);
    state_cv.notify_all();

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    g_videosink_state[id.value] = State::STARTED;
    LOG_INFO("start: ID %d transitioning STARTING -> STARTED", id.value);
    notifyStateChangedLocked(id.value, State::STARTING, State::STARTED);
    consumeQueuedFramesIfPresentingLocked(id.value);
    state_cv.notify_all();
    return android::binder::Status::ok();
}

android::binder::Status VideoSinkController::stop()
{
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("stop: invalid ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::STARTED)
    {
        LOG_ERROR("stop: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }
    g_videosink_state[id.value] = State::STOPPING;
    LOG_INFO("stop: ID %d transitioning STARTED -> STOPPING", id.value);
    notifyStateChangedLocked(id.value, State::STARTED, State::STOPPING);
    state_cv.notify_all();

    resetPlaybackStateLocked(id.value, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    g_videosink_state[id.value] = State::READY;
    LOG_INFO("stop: ID %d transitioning STOPPING -> READY", id.value);
    notifyStateChangedLocked(id.value, State::STOPPING, State::READY);
    state_cv.notify_all();
    return android::binder::Status::ok();
}

android::binder::Status VideoSinkController::queueVideoFrame(int64_t nsPresentationTime, int64_t frameBufferHandle,
                                                             const videodecoder::FrameMetadata& metadata, bool* _aidl_return)
{
    if (!_aidl_return)
    {
        LOG_ERROR("queueVideoFrame: null _aidl_return for ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_NULL_POINTER);
    }
    (void)metadata;
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("queueVideoFrame: invalid ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::STARTED)
    {
        LOG_ERROR("queueVideoFrame: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }
    if (g_end_of_stream_signalled[id.value])
    {
        LOG_ERROR("queueVideoFrame: end of stream already signalled for ID %d", id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }
    if (nsPresentationTime <= 0)
    {
        LOG_ERROR("queueVideoFrame: invalid nsPresentationTime %ld for ID %d", nsPresentationTime, id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    if (frameBufferHandle <= 0)
    {
        LOG_ERROR("queueVideoFrame: invalid frameBufferHandle %ld for ID %d", frameBufferHandle, id.value);
        *_aidl_return = false;
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    if (g_sink_queue_depth[id.value] >= MAX_QUEUE_DEPTH)
    {
        LOG_ERROR("queueVideoFrame: queue full for ID %d", id.value);
        g_queue_full_reported[id.value] = true;
        *_aidl_return = false;
        return android::binder::Status::ok();
    }
    g_frame_queue[id.value].push({nsPresentationTime, frameBufferHandle});
    g_sink_queue_depth[id.value]++;
    LOG_INFO("queueVideoFrame: queued frame with nsPresentationTime %ld for ID %d", nsPresentationTime, id.value);

    if (g_render_first_frame[id.value] && !g_first_frame_rendered[id.value] &&
        g_clock_ids[id.value].value == avclock::IAVClock::Id::UNDEFINED)
    {
        g_first_frame_rendered[id.value] = true;
        g_last_rendered_presentation_time[id.value] = nsPresentationTime;
        notifyFirstFrameRenderedLocked(id.value, nsPresentationTime);
    }

    consumeQueuedFramesIfPresentingLocked(id.value);
    *_aidl_return = true;
    return android::binder::Status::ok();
}

android::binder::Status VideoSinkController::signalEndOfStream()
{
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("signalEndOfStream: invalid ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }

    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::STARTED)
    {
        LOG_ERROR("signalEndOfStream: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }
    if (g_end_of_stream_signalled[id.value])
    {
        return android::binder::Status::ok();
    }

    g_end_of_stream_signalled[id.value] = true;
    g_end_of_stream_callback_sent[id.value] = false;
    consumeQueuedFramesIfPresentingLocked(id.value);
    completeEndOfStreamIfPossibleLocked(id.value);
    return android::binder::Status::ok();
}

android::binder::Status VideoSinkController::flush(bool holdLastFrame)
{
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("flush: invalid ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::STARTED) {
        LOG_ERROR("flush: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }
    g_videosink_state[id.value] = State::FLUSHING;
    LOG_INFO("flush: ID %d transitioning STARTED -> FLUSHING", id.value);
    notifyStateChangedLocked(id.value, State::STARTED, State::FLUSHING);
    state_cv.notify_all();

    clearQueuedFramesLocked(id.value);
    g_end_of_stream_signalled[id.value] = false;
    g_end_of_stream_callback_sent[id.value] = false;
    g_queue_full_reported[id.value] = false;
    g_first_frame_rendered[id.value] = false;
    if (holdLastFrame)
    {
        LOG_INFO("flush: holding last frame for ID %d", id.value);
    } else {
        g_last_rendered_presentation_time[id.value] = kUndefinedPresentationTime;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    g_videosink_state[id.value] = State::STARTED;
    LOG_INFO("flush: ID %d transitioning FLUSHING -> STARTED", id.value);
    notifyStateChangedLocked(id.value, State::FLUSHING, State::STARTED);
    notifyFlushCompleteLocked(id.value);
    state_cv.notify_all();
    return android::binder::Status::ok();
}

android::binder::Status VideoSinkController::discardFramesUntil(int64_t nsPresentationTime)
{
    if (!isValidSinkId(id.value))
    {
        LOG_ERROR("discardFramesUntil: invalid ID %d", id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    std::lock_guard<std::mutex> lock(state_mutex);
    if (g_videosink_state[id.value] != State::STARTED)
    {
        LOG_ERROR("discardFramesUntil: invalid state %d for ID %d", static_cast<int>(g_videosink_state[id.value]), id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_STATE);
    }
    if (nsPresentationTime < 0)
    {
        LOG_ERROR("discardFramesUntil: invalid nsPresentationTime %ld for ID %d", nsPresentationTime, id.value);
        return android::binder::Status::fromExceptionCode(android::binder::Status::EX_ILLEGAL_ARGUMENT);
    }
    // Discard frames before nsPresentationTime
    while (!g_frame_queue[id.value].empty() && g_frame_queue[id.value].front().first < nsPresentationTime)
    {
        g_frame_queue[id.value].pop();
        g_sink_queue_depth[id.value]--;
    }
    LOG_INFO("discardFramesUntil: discarded frames until %ld for ID %d", nsPresentationTime, id.value);
    notifyFrameBufferAvailableLocked(id.value);
    consumeQueuedFramesIfPresentingLocked(id.value);
    completeEndOfStreamIfPossibleLocked(id.value);
    return android::binder::Status::ok();
}

} // namespace videosink
} // namespace hal
} // namespace rdk
} // namespace com