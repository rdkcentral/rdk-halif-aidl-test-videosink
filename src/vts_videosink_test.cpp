/**
 *  If not stated otherwise in this file or this component's LICENSE
 *  file the following copyright and licenses apply:
 *
 *  Copyright 2026 RDK Management
 *
 *  Licensed under the Apache License, Version 2.0 (the License);
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an AS IS BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * @addtogroup HPK Hardware Porting Kit
 * @{
 * @par The Hardware Porting Kit
 * HPK is the next evolution of the well-defined Hardware Abstraction Layer
 * (HAL), but augmented with more comprehensive documentation and test suites
 * that OEM or SOC vendors can use to self-certify their ports before taking
 * them to RDKM for validation or to an operator for final integration and
 * deployment. The Hardware Porting Kit effectively enables an OEM and/or SOC
 * vendor to self-certify their own Video Accelerator devices, with minimal RDKM
 * assistance.
 *
 */

/**
 * @addtogroup VIDEO_SINK Video Sink Module
 * @{
 */

/**
 * @addtogroup VIDEO_SINK_HALTEST Video Sink HAL Tests
 * @{
 */

/**
 * @defgroup VIDEO_SINK_HALTEST_CLASS Video Sink HAL Tests helper functions Header File
 * @{
 * @parblock
 *
 * ### Common functions/macros required for the video sink module :
 *
 * Common functions/macros required for the video sink module .
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink_test.cpp
 *
 */

#include "vts_videosink_test.h"

/* Max wait for onFrameBufferAvailable() after queueVideoFrame() returns false (sink queue full) */
static constexpr int VIDEO_SINK_FRAME_BUFFER_AVAILABLE_WAIT_MS = 2000;

/* NON_TUNNELLED: queue the decoded frame to the sink (with back-pressure handling if configured).
 * TUNNELLED: the vendor feeds the sink internally, so nothing is queued.
 * Returns false if the frame could not be queued (the buffer is freed and the test is failed). */
bool VideoSinkTest::queueDecodedFrameIfNonTunnel(const DecodedVideoFrame_t& decodedFrame, OperationalMode operationalMode,
                                                 videodecoder::FrameMetadata& metadata, uint32_t frameDurationMs)
{
    if (operationalMode == OperationalMode::TUNNELLED)
    {
        return true;
    }

    const int64_t pts = decodedFrame.framePTS;
    const int64_t bufferHandle = decodedFrame.frameAVBufferHandle;
    bool queueSucess = false;

    if(mTestConfiguration.sink_backpressure)
    {
        queueSucess = validateSinkBackPressure(bufferHandle, pts, metadata);
    }
    else
    {
        mVideoSinkControlListener->resetFrameBufferAvailable();
        VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_ATTEMPT(mVideoSinkController, pts, bufferHandle, metadata, queueSucess);
        if (!queueSucess)
        {
            // Queue full (timing at the VTS side): wait for onFrameBufferAvailable() and retry once
            UT_LOG_WARNING("[SinkID: %d] queueVideoFrame returned false at PTS %lld, waiting for onFrameBufferAvailable before retry",
                           mSinkId.value, static_cast<long long>(pts));
            UT_ASSERT_TRUE(mVideoSinkControlListener->waitForFrameBufferAvailableEvent(
                std::chrono::milliseconds(VIDEO_SINK_FRAME_BUFFER_AVAILABLE_WAIT_MS)))
                << "onFrameBufferAvailable not received after a refused frame (PTS " << pts << ")";
            VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_SUCCESS(mVideoSinkController, pts, bufferHandle, metadata, queueSucess);
        }
    }

    if(!queueSucess)
    {
        UT_LOG_ERROR("[SinkID: %d] Failed to queue frame with PTS: %lld", mSinkId.value, pts);
        UT_ASSERT_TRUE(false);
        AVBUFFER_FREE_SUCCESS(mAVBufferManager, bufferHandle);
        return false;
    }

    // Sleep for the frame duration to simulate real-time frame arrival
    std::this_thread::sleep_for(std::chrono::milliseconds(frameDurationMs));
    return true;
}

bool VideoSinkTest::getSinkPropertyInt(videosink::Property property, int32_t& outValue)
{

    std::optional<com::rdk::hal::PropertyValue> propertyValue;
    ::android::binder::Status status;

    outValue = -1;

    status = mSinkHandle->getProperty(property, &propertyValue);
    if (!status.isOk() ||
        status.exceptionCode() != ::android::binder::Status::EX_NONE ||
        !propertyValue.has_value() ||
        !propertyValue->value.has_value())
    {
        UT_LOG_ERROR("Failed to get property %d", static_cast<int>(property));
        UT_LOG_ERROR("Result getProperty(IN:property[%s], propertyValue.has_value[%s]), status:[isOK:%s, exception:%s]",
            videosink::toString(property).c_str(),
            UT_Control_GetMapString(bool_mapTable, propertyValue.has_value()),
            UT_Control_GetMapString(bool_mapTable, status.isOk()),
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));

        return false;
    }

    outValue = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();
    return true;
}

void VideoSinkTest::setupClock()
{
    // Open the clock
    AVCLOCK_MANAGER_GETAVCLOCK_SUCCESS(mClockId, mClock);

    // Open the clock controller
    AVCLOCK_OPEN_SUCCESS(mClock, mClockCtrlListener, mClockController,true);

    // Set the clock mode to Video Master
    AVCLOCK_SET_CLOCK_MODE_SUCCESS(mClock, mClockController, ClockMode::VIDEO_MASTER, true);

    if(!mTestConfiguration.sink_backpressure)
    {
        // Set the clock mode to Video Master
        AVCLOCK_START_SUCCESS(mClock, mClockController, true);
    }
}

void VideoSinkTest::tearDownClock()
{
    // Stop the clock
    AVCLOCK_STOP_SUCCESS(mClock, mClockController, true);

    // Close the clock controller
    AVCLOCK_CLOSE_SUCCESS(mClock, mClockController, true);
}

void VideoSinkTest::setupVideoSink()
{
    /*Open the Video Sink*/
    VIDEOSINK_OPEN_SUCCESS(mSinkHandle, mVideoSinkControlListener, mVideoSinkController, true);

    /*Associate sink with decoder before starting queue operations.*/
    VIDEOSINK_CONTROLLER_SETVIDEODECODER_SUCCESS(mVideoSinkController, mDecoderId);

    VIDEOSINK_ATTACH_CLOCK_SUCCESS(mVideoSinkController, mClockId);

    /*Start the Video Sink*/
    VIDEOSINK_START_SUCCESS(mSinkHandle, mVideoSinkController, true);

    /*Set the "discard until PTS" value for the relevant test cases*/
    if (mTestConfiguration.sink_discardFramesUntilPts >= 0)
    {
        mFirstFramePTS = static_cast<int64_t>(mTestConfiguration.sink_discardFramesUntilPts);
        VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_SUCCESS(mVideoSinkController, static_cast<uint64_t>(mTestConfiguration.sink_discardFramesUntilPts));
    }

    if(!mIsClockInUse)
    {
        setupClock();
    }
}

void VideoSinkTest::tearDownVideoSink()
{
    // Detach the clock
    VIDEOSINK_DETACH_CLOCK_SUCCESS(mVideoSinkController);

    /* Stop the Video sink */
    VIDEOSINK_STOP_SUCCESS(mSinkHandle, mVideoSinkController, true);

    /* Close the Video sink */
    VIDEOSINK_CLOSE_SUCCESS(mSinkHandle, mVideoSinkController, true);

    if(!mIsClockInUse)
    {
        tearDownClock();
    }
}

void VideoSinkTest::copyMetaData(const videodecoder::FrameMetadata& src, videodecoder::FrameMetadata& dst)
{
    dst.parX = src.parX;
    dst.parY = src.parY;
    dst.sarX = src.sarX;
    dst.sarY = src.sarY;
    dst.codedWidth = src.codedWidth;
    dst.codedHeight = src.codedHeight;
    dst.activeX = src.activeX;
    dst.activeY = src.activeY;
    dst.activeWidth = src.activeWidth;
    dst.activeHeight = src.activeHeight;
    dst.colorDepth = src.colorDepth;
    dst.pixelFormat = src.pixelFormat;
    dst.dynamicRange = src.dynamicRange;
    dst.scanType = src.scanType;
    dst.afd = src.afd;
    dst.frameRateNumerator = src.frameRateNumerator;
    dst.frameRateDenominator = src.frameRateDenominator;
    dst.bitstreamEOS = src.bitstreamEOS;
    dst.discontinuity = src.discontinuity;
    dst.lowLatency = src.lowLatency;
    dst.source = src.source;
}

void VideoSinkTest::validateCallbacks()
{
    if (!mIsClockInUse)
    {
        UT_LOG_INFO("Waiting for AVClock onPrimed event");
        bool primedStatus = mClockCtrlListener->waitForPrimedEvent(
            std::chrono::seconds(5), threadtype::video);
        UT_ASSERT_TRUE(primedStatus);
    }

    /* Validate first frame rendered PTS */
    std::vector<int64_t> eventFirstFramePTS = mVideoSinkEventListener->getFirstPTS();
    std::vector<int64_t> controllerFirstFramePTS = mVideoSinkControlListener->getFirstPTS();
    if (mIsPlaneMapped)
    {
        /* A plane is mapped (planecontrol): exactly one FFR, for the frame shown once the plane was mapped,
         * so its PTS is at or after the first frame's PTS (the plane may be mapped after playback starts) */
        UT_ASSERT_EQUAL(eventFirstFramePTS.size(), 1);
        UT_ASSERT_TRUE(!eventFirstFramePTS.empty() && eventFirstFramePTS.front() >= mFirstFramePTS)
            << "First frame rendered PTS is before the first frame PTS " << mFirstFramePTS;
        UT_ASSERT_EQUAL(controllerFirstFramePTS.size(), 1);
        UT_ASSERT_TRUE(!controllerFirstFramePTS.empty() && controllerFirstFramePTS.front() >= mFirstFramePTS)
            << "First frame rendered PTS is before the first frame PTS " << mFirstFramePTS;
        /* Both listeners report the same first rendered frame */
        UT_ASSERT_EQUAL(eventFirstFramePTS.empty() ? -1 : eventFirstFramePTS.front(),
                        controllerFirstFramePTS.empty() ? -1 : controllerFirstFramePTS.front())
            << "Event and controller listeners reported different first frame rendered PTS";
    }
    else
    {
        /* No video plane is mapped: onFirstFrameRendered() must not be reported (rdk-halif-aidl 0.23.0) */
        UT_ASSERT_EQUAL(eventFirstFramePTS.size(), 0) << "First frame rendered received but no video plane is mapped";
        UT_ASSERT_EQUAL(controllerFirstFramePTS.size(), 0) << "First frame rendered received but no video plane is mapped";
    }

    /* Validate EOS PTS */
    std::vector<int64_t> controllerEOSPTS = mVideoSinkControlListener->getEosPTS();
    std::vector<int64_t> eventEOSPTS = mVideoSinkEventListener->getEosPTS();

    if(mTestConfiguration.eosPts >= 0)
    {
        UT_ASSERT_EQUAL(controllerEOSPTS.size(), 2);
        UT_ASSERT_EQUAL(controllerEOSPTS.empty() ? -1 : controllerEOSPTS[0], mTestConfiguration.eosPts);
        UT_ASSERT_EQUAL(controllerEOSPTS.size() < 2 ? -1 : controllerEOSPTS[1], mSignalEOSPTS);

        UT_ASSERT_EQUAL(eventEOSPTS.size(), 2);
        UT_ASSERT_EQUAL(eventEOSPTS.empty() ? -1 : eventEOSPTS[0], mTestConfiguration.eosPts);
        UT_ASSERT_EQUAL(eventEOSPTS.size() < 2 ? -1 : eventEOSPTS[1], mSignalEOSPTS);
    }
    else
    {
        UT_ASSERT_EQUAL(controllerEOSPTS.size(), 1);
        UT_ASSERT_EQUAL(controllerEOSPTS.empty() ? -1 : controllerEOSPTS[0], mSignalEOSPTS);
        UT_ASSERT_EQUAL(eventEOSPTS.size(), 1);
        UT_ASSERT_EQUAL(eventEOSPTS.empty() ? -1 : eventEOSPTS[0], mSignalEOSPTS);
    }

    //Sink test loop waits for decoder buffer to be filled. For the last frame,
    // the decoder will not have any more frames to decode
    // TODO: check if the time taken for last frame and EOS is within
    // acceptable limits and if yes check underflow count with zero.

    /* Validate underflow, resume */
    if(mTestConfiguration.sink_underflow_pts >= 0)
    {
        UT_ASSERT_TRUE(mVideoSinkControlListener->getUnderflowCount() > 0);
        UT_ASSERT_TRUE(mVideoSinkEventListener->getUnderflowCount() > 0);
        UT_ASSERT_TRUE(mVideoSinkControlListener->getResumedCount() > 0);
        UT_ASSERT_TRUE(mVideoSinkEventListener->getResumedCount() > 0);
    }
    else
    {
        UT_ASSERT_TRUE(mVideoSinkControlListener->getUnderflowCount() >= 0);
        UT_ASSERT_TRUE(mVideoSinkEventListener->getUnderflowCount() >= 0);
        UT_ASSERT_EQUAL(mVideoSinkControlListener->getResumedCount(), 0);
        UT_ASSERT_EQUAL(mVideoSinkEventListener->getResumedCount(), 0);
    }

    /* Validate Flush complete events*/
    int32_t expectedFlushCount = (mTestConfiguration.flushPts >= 0) ? 1 : 0;
    expectedFlushCount += (mTestConfiguration.eosPts >= 0) ? 1 : 0;

    UT_ASSERT_EQUAL(mVideoSinkControlListener->getFlushCompleteCount(), expectedFlushCount);
    UT_ASSERT_EQUAL(mVideoSinkEventListener->getFlushCompleteCount(), expectedFlushCount);
}

void VideoSinkTest::validateMetrics()
{
    std::optional<PropertyValue> propertyValue;
    videosink::Property property = videosink::Property::METRIC_FRAMES_RECEIVED;

    /* METRIC_FRAMES_PRESENTED counts frames presented to the display, so it is 0 when no video plane is mapped */
    int32_t actualFramesPresented = mIsPlaneMapped ? (mFrameCount - mTestConfiguration.frames_to_discard) : 0;

    /* Validate frames received */
    VIDEOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, property, propertyValue);
    if (propertyValue.has_value() && propertyValue->value.has_value())
    {
        int32_t framesReceived = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

        if (framesReceived != -1)
        {
            if(mTestConfiguration.flushPts >= 0)
            {
                UT_ASSERT_TRUE(framesReceived <= mFrameCount);
            }
            else
            {
                UT_ASSERT_EQUAL(framesReceived, mFrameCount);
            }
        }
    }

    /* validate frames presented */
    property = videosink::Property::METRIC_FRAMES_PRESENTED;
    VIDEOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, property, propertyValue);
    if (propertyValue.has_value() && propertyValue->value.has_value())
    {
        int32_t framesPresented = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

        if (framesPresented != -1)
        {
            if(mTestConfiguration.flushPts >= 0)
            {
                UT_ASSERT_TRUE(framesPresented <= actualFramesPresented);
            }
            else
            {
                UT_ASSERT_EQUAL(framesPresented, actualFramesPresented);
            }
        }
    }

    /* validate frames dropped */
    property = videosink::Property::METRIC_FRAMES_DROPPED_LATE;
    VIDEOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, property, propertyValue);
    if (propertyValue.has_value() && propertyValue->value.has_value())
    {
        int32_t framesDropped = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();
        if (framesDropped != -1)
        {
            if(mTestConfiguration.sink_underflow_pts >= 0)
            {
                UT_ASSERT_TRUE(framesDropped > 0);
            }
            else
            {
                UT_ASSERT_EQUAL(framesDropped, 0);
            }
        }
    }

    /* validate missing repeated frames due to underflow */
    property = videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME;
    VIDEOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, property, propertyValue);
    if (propertyValue.has_value() && propertyValue->value.has_value())
    {
        int32_t framesRepeatedMissing = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

        if (framesRepeatedMissing != -1)
        {
            // Repeating a frame is a display action, so it cannot happen when no video plane is mapped
            if(mIsPlaneMapped && mTestConfiguration.sink_underflow_pts >= 0)
            {
                UT_ASSERT_TRUE(framesRepeatedMissing > 0);
            }
            else
            {
                UT_ASSERT_EQUAL(framesRepeatedMissing, 0);
            }
        }
    }

    /* validate underflow count */

    //Sink test loop waits for decoder buffer to be filled. For the last frame,
    // the decoder will not have any more frames to decode
    // TODO: check if the time taken for last frame and EOS is within
    // acceptable limits and if yes check underflow count with zero.
    property = videosink::Property::METRIC_UNDERFLOWED;
    VIDEOSINK_GET_PROPERTY_SUCCESS(mSinkHandle, property, propertyValue);
    if (propertyValue.has_value() && propertyValue->value.has_value())
    {
        int32_t underflowCount = propertyValue->value->get<PropertyValue::Value::Tag::intValue>();

        if (underflowCount != -1)
        {
            if(mTestConfiguration.sink_underflow_pts >= 0)
            {
                UT_ASSERT_TRUE(underflowCount > 0);
            }
            else
            {
                UT_ASSERT_TRUE(underflowCount >= 0);
            }
        }
    }
}

/* Frees decoded frames still queued after the sink stopped (same as audiosink cleanupAudioDecoder()).
 * The sink loop ends once the decoder EOS was received, so no frame is expected here. */
void VideoSinkTest::cleanupVideoDecoder()
{
    DecodedVideoFrame_t decodedFrame;
    while (mDecoderControllerListener->getNextFrame(decodedFrame, std::chrono::milliseconds(0)))
    {
        if (decodedFrame.frameAVBufferHandle != -1)
        {
            UT_LOG_ERROR("[SinkID: %d] Frame (PTS %lld) received after the video sink has been stopped",
                         mSinkId.value, static_cast<long long>(decodedFrame.framePTS));
            UT_ASSERT_TRUE(false);

            bool isBufferValid = false;
            AVBUFFER_ISVALID_SUCCESS(mAVBufferManager, decodedFrame.frameAVBufferHandle, isBufferValid);
            if (isBufferValid)
            {
                AVBUFFER_FREE_SUCCESS(mAVBufferManager, decodedFrame.frameAVBufferHandle);
            }
        }
    }
}

bool VideoSinkTest::validateSinkBackPressure(int64_t bufferHandle, int64_t pts, videodecoder::FrameMetadata& metadata)
{
    bool status = false;
    bool queueAgainSucess = false;
    bool frameAccepted = false;
    
    VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_ATTEMPT(mVideoSinkController, pts, bufferHandle, metadata, frameAccepted);

    if (frameAccepted)
    {
        // Queue had room - accepted normally. sink_backpressure stays true,
        // so the caller will invoke us again on the next frame.
        return true;
    }

    // Genuinely rejected - the queue is full. This is the real backpressure condition.
    mTestConfiguration.sink_backpressure = false;

    // If queue is full due to backpressure, subsequent queue attempts should fail until the clock starts.
    VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_FAILURE(mVideoSinkController, pts, bufferHandle, metadata, Status::EX_NONE);
    VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_FAILURE(mVideoSinkController, pts, bufferHandle, metadata, Status::EX_NONE);
    VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_FAILURE(mVideoSinkController, pts, bufferHandle, metadata, Status::EX_NONE);

    // Start the clock to allow queued frames to be processed and free up space in the queue.
    AVCLOCK_START_SUCCESS(mClock, mClockController, true);

    status = mVideoSinkControlListener->waitForFrameBufferAvailableEvent(std::chrono::milliseconds(60));
    UT_ASSERT_TRUE(status);

    // Attempt to queue the frame again after starting the clock.
    VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_SUCCESS(mVideoSinkController, pts, bufferHandle, metadata, queueAgainSucess);

    return queueAgainSucess;
}

bool VideoSinkTest::waitForDecoderEndOfStream()
{
    if (mDecoderControllerListener->waitForEndOfStream(1, std::chrono::milliseconds(0)))
    {
        return true;
    }

    UT_LOG_ERROR("[SinkID: %d] Neither a decoded frame nor decoder EOS was received within %d ms",
                 mSinkId.value, 5000);
    UT_ASSERT_TRUE(false);
    return false;
}

/* rdk-halif-aidl 0.23.0: the mode is selected by the vendor and read from the decoder on the first frame */
void VideoSinkTest::run()
{
    try
    {
        /*Reset the Flush, EOS, Underflow and Resume Counts*/
        mVideoSinkControlListener->resetFirstFrameRendered();
        mVideoSinkEventListener->resetFirstFrameRendered();
        mVideoSinkControlListener->resetUnderflow();
        mVideoSinkEventListener->resetUnderflow();
        mVideoSinkControlListener->resetResumed();
        mVideoSinkEventListener->resetResumed();
        mVideoSinkControlListener->resetEos();
        mVideoSinkEventListener->resetEos();
        mVideoSinkControlListener->resetFlushComplete();
        mVideoSinkEventListener->resetFlushComplete();

        setupVideoSink();

        UT_LOG_INFO("[SinkID: %d] Start of Testing Stream: %s", mSinkId.value, mTestConfiguration.input.c_str());
        DecodedVideoFrame_t decodedFrame;
        uint32_t frameDurationMs = 50; // Default to 50 ms if frame rate is not available

        /*Wait up to 500 ms for a frame, and tolerate a few consecutive misses*/
        const int64_t frameTimeoutMs = 5000; // Wait up to 500 ms for a frame

        mFirstFramePTS = mTestConfiguration.sink_discardFramesUntilPts;
        mFrameCount = 0;
        OperationalMode operationalMode = OperationalMode::NON_TUNNELLED;
        bool flushDone = false;

        while (true)
        {
            if (!mDecoderControllerListener->getNextFrame(decodedFrame, std::chrono::milliseconds(frameTimeoutMs)))
            {
                if (!waitForDecoderEndOfStream())
                {
                    tearDownVideoSink();
                    return;
                }

                // EOS can arrive immediately before the final decoded frame is queued.
                if (!mDecoderControllerListener->getNextFrame(decodedFrame, std::chrono::milliseconds(0)))
                {
                    break;
                }
            }

            int64_t pts = decodedFrame.framePTS;
            int64_t bufferHandle = decodedFrame.frameAVBufferHandle;

            // Use fully qualified name to avoid ambiguity
            videodecoder::FrameMetadata metadata{};

            mFrameCount++;

            if (mFrameCount == 1)
            {
                /* Get the operational mode of the decoder, which can be queried only after the decoder start */
                VIDEODECODER_GET_OPERATIONALMODE_SUCCESS(mDecoderHandle, operationalMode);
                UT_LOG_INFO("[SinkID: %d] Running %s sink path", mSinkId.value,
                            (operationalMode == OperationalMode::TUNNELLED) ? "tunnel" : "non-tunnel");

                /* Client-queue back-pressure needs NON_TUNNELLED. In TUNNELLED mode the clock (left stopped by
                 * setupClock() for back-pressure) is started here so the sink can consume and reach EOS. */
                if (operationalMode == OperationalMode::TUNNELLED && mTestConfiguration.sink_backpressure && !mIsClockInUse)
                {
                    UT_LOG_WARNING("[SinkID: %d] Sink back-pressure is not applicable in TUNNELLED mode; running as normal playback",
                                   mSinkId.value);
                    AVCLOCK_START_SUCCESS(mClock, mClockController, true);
                }

                // discardFramesUntil() applies in both modes, so the configured PTS stays the lower bound
                if(mTestConfiguration.sink_discardFramesUntilPts == -1)
                    mFirstFramePTS = decodedFrame.framePTS;

                if(!decodedFrame.metadata)
                {
                    UT_LOG_ERROR("[SinkID: %d] First frame received without metadata, skipping the frame", mSinkId.value);
                    UT_ASSERT_TRUE(false);
                    break;
                }
            }

            if (mTestConfiguration.flushPts >= 0 && !flushDone &&
                pts >= static_cast<int64_t>(mTestConfiguration.flushPts))
            {
                VIDEOSINK_FLUSH_SUCCESS(mVideoSinkController, true);
                flushDone = true;
            }

            /* NON_TUNNELLED: decoded frames are returned with a valid buffer handle and queued to the sink.
             * TUNNELLED: the vendor feeds the sink internally and the frame handle is -1. */
            const bool isTunnel = (operationalMode == OperationalMode::TUNNELLED);
            if ((!isTunnel && bufferHandle == -1) || (isTunnel && bufferHandle != -1))
            {
                UT_LOG_ERROR("[SinkID: %d] %s frame received with %s buffer handle, skipping the frame", mSinkId.value,
                             isTunnel ? "Tunnelled" : "Non-tunnelled", isTunnel ? "a valid" : "an invalid");
                UT_ASSERT_TRUE(false);
                if (isTunnel)
                {
                    // Not queued to the sink, so free it here (same as audiosink)
                    bool isBufferValid = false;
                    AVBUFFER_ISVALID_SUCCESS(mAVBufferManager, bufferHandle, isBufferValid);
                    if (isBufferValid)
                    {
                        AVBUFFER_FREE_SUCCESS(mAVBufferManager, bufferHandle);
                    }
                }
                continue;
            }

            if (!isTunnel && decodedFrame.metadata)
            {
                const auto& src = *(decodedFrame.metadata);
                copyMetaData(src, metadata);
                if (metadata.frameRateNumerator > 0 && metadata.frameRateDenominator > 0)
                {
                    frameDurationMs = static_cast<uint32_t>(
                        1000.0 * metadata.frameRateDenominator / metadata.frameRateNumerator);
                }
            }

            // Simulate underflow
            if (mTestConfiguration.sink_underflow_pts >= 0 &&
                pts == static_cast<int64_t>(mTestConfiguration.sink_underflow_pts))
            {
                UT_LOG_DEBUG("[SinkID: %d] Injecting delay at PTS %lld to simulate underflow",
                             mSinkId.value, static_cast<long long>(pts));
                std::this_thread::sleep_for(std::chrono::milliseconds(2 * frameDurationMs));
            }

            mSignalEOSPTS = pts;

            if (!queueDecodedFrameIfNonTunnel(decodedFrame, operationalMode, metadata, frameDurationMs))
            {
                break;
            }

            /* Check for end-of-stream (both modes: the client signals EOS to the sink, then flushes) */
            if(mTestConfiguration.eosPts >= 0 && pts == static_cast<int64_t>(mTestConfiguration.eosPts))
            {
                if (!mDecoderControllerListener->waitForEndOfStream(1, std::chrono::milliseconds(5000)))
                {
                    UT_LOG_ERROR("[SinkID: %d] Decoder EOS was not received before sink EOS", mSinkId.value);
                    UT_ASSERT_TRUE(false);
                    tearDownVideoSink();
                    return;
                }

                VIDEOSINK_SIGNAL_EOS_SUCCESS(mVideoSinkController);

                /* After EOS signal, any further attempts to queue frames should fail (client queue: non-tunnel only) */
                if (!isTunnel)
                {
                    VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_FAILURE(mVideoSinkController, pts, bufferHandle, metadata, Status::EX_ILLEGAL_STATE);
                }

                /* onEndOfStream() fires after the final frame's presentation time; flushing earlier could discard
                 * the pending frames before it arrives, so wait for it on both listeners first */
                if (!mVideoSinkControlListener->waitForEosEvent(std::chrono::milliseconds(5000)) ||
                    !mVideoSinkEventListener->waitForEosEvent(std::chrono::milliseconds(5000)))
                {
                    UT_LOG_ERROR("[SinkID: %d] Sink EOS was not received for EOS at PTS %lld", mSinkId.value, pts);
                    UT_ASSERT_TRUE(false);
                    tearDownVideoSink();
                    return;
                }

                /* To queue the frames after EOS, flush the video sink */
                VIDEOSINK_FLUSH_SUCCESS(mVideoSinkController, true);
            }
        }

        /* Signal EOS, exptected EOS PTS callback is at mSignalEOSPTS */
        VIDEOSINK_SIGNAL_EOS_SUCCESS(mVideoSinkController);

        //TODO: Check if this sleep required.
        // It is added to ensure that the all event is received before validating callbacks and metrics.
        if (operationalMode == OperationalMode::NON_TUNNELLED)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        validateCallbacks();
        validateMetrics();

        tearDownVideoSink();

        /* cleanup if any buffers received from the decoder after closing the sink */
        cleanupVideoDecoder();
    }
    catch (const std::exception& ex) {
        UT_LOG_ERROR("[SinkID: %d] Exception in run: %s", mSinkId.value, ex.what());
        UT_ASSERT_TRUE(false);
    } catch (...) {
        UT_LOG_ERROR("[SinkID: %d] Unknown exception in run", mSinkId.value);
        UT_ASSERT_TRUE(false);
    }
}


/** @} */ // End of VIDEO_SINK_HALTEST_CLASS
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
