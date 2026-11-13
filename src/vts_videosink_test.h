/**
 *  If not stated otherwise in this file or this component's LICENSE
 *  file the following copyright and licenses apply:
 *
 *  Copyright 2025 RDK Management
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
 * @defgroup VIDEO_SINK_HALTEST_HEADER Video Sink HAL Tests helper functions Header File
 * @{
 * @parblock
 *
 * ### Common functions/macros required for the video sink module :

 * Common functions/macros required for the video sink module .
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink_test.h
 *
 */

#ifndef __VTS_VIDEOSINK_TEST_H__
#define __VTS_VIDEOSINK_TEST_H__

#include <thread>
#include <chrono>
#include <ut.h>
#include <set>
#include <inttypes.h>
#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink_init.h"
#include "vts_avclock.h"

#include "com/rdk/hal/videosink/IVideoSinkManager.h"
#include "com/rdk/hal/videosink/BnVideoSinkControllerListener.h"
#include "com/rdk/hal/videosink/BnVideoSinkEventListener.h"

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

using namespace com::rdk::hal;
using namespace com::rdk::hal::videosink;

//sink test implementation.
class VideoSinkTest
{
public:
    VideoSinkTest(IVideoSink::Id sinkId,
        sp<IVideoSink> sinkHandle,
        sp<IVideoDecoder> decoderHandle,
        sp<VideoSinkControllerListener> VideoSinkControllerListener,
        sp<VideoSinkEventListener> VideoSinkEventListner,
        sp<VideoDecoderControllerListener> decoderControllerListener,
        sp<IAVBuffer> AVBufferManager,
        IVideoDecoder::Id decoderId,
        IAVClock::Id clockId,
        const videoDecoderTestConfiguration_t& config, bool isClockInUse,
        bool isPlaneMapped)
    {
        mSinkId = sinkId;
        mSinkHandle = sinkHandle;
        mDecoderHandle = decoderHandle;
        mVideoSinkControlListener = VideoSinkControllerListener;
        mVideoSinkEventListener = VideoSinkEventListner;
        mDecoderControllerListener = decoderControllerListener;
        mDecoderId = decoderId;
        mTestConfiguration = config;
        mIsClockInUse = isClockInUse;
        mClockId = clockId;
        mClockCtrlListener = new AVClockControllerListener();
        mAVBufferManager = AVBufferManager;
        mFirstFramePTS = -1;
        mSignalEOSPTS = -1;
        mFrameCount = 0;
        mIsPlaneMapped = isPlaneMapped;
    }

    /* rdk-halif-aidl 0.23.0: tunnel / non-tunnel is selected by the vendor; the sink reads the
     * decoder's operational mode on the first decoded frame and follows it. */
    void run ();
    bool getSinkPropertyInt(videosink::Property property, int32_t& outValue);

private:
    IVideoSink::Id mSinkId;
    sp<IVideoSink> mSinkHandle;
    sp<IVideoDecoder> mDecoderHandle;
    sp<VideoSinkControllerListener> mVideoSinkControlListener;
    sp<VideoSinkEventListener> mVideoSinkEventListener;
    sp<VideoDecoderControllerListener> mDecoderControllerListener;
    IVideoDecoder::Id mDecoderId;
    IAVClock::Id mClockId;
    sp<IAVClock> mClock;
    videoDecoderTestConfiguration_t mTestConfiguration;
    sp<IVideoSinkController> mVideoSinkController;
    std::set<int64_t> mReceivedPtsSet;
    bool mIsClockInUse = false;
    /* rdk-halif-aidl 0.23.0: a video plane is mapped to this sink (planecontrol). Without a plane (videosink, avclock)
     * no first frame rendered is reported and nothing reaches the display (display metrics are 0). */
    bool mIsPlaneMapped = false;
    int mActualFramesReceived = 0;
    sp<AVClockControllerListener> mClockCtrlListener;
    sp<IAVClockController> mClockController;
    sp<IAVBuffer> mAVBufferManager;
    int64_t mFirstFramePTS;
    int64_t mSignalEOSPTS;
    int32_t mFrameCount;

    float getBytesPerPixelMultiplier(const std::string& basePixelFormat, int colorDepth) const;
    PixelFormat getPixelFormatEnum(const std::string& basePixelFormat);
    void setupClock();
    void tearDownClock();
    void validateCallbacks();
    void validateMetrics();
    bool waitForDecoderEndOfStream();
    void cleanupVideoDecoder();
    void setupVideoSink();
    void tearDownVideoSink();
    void copyMetaData(const videodecoder::FrameMetadata& src, videodecoder::FrameMetadata& dst);
    bool validateSinkBackPressure(int64_t bufferHandle, int64_t pts, videodecoder::FrameMetadata& metadata);
    bool queueDecodedFrameIfNonTunnel(const DecodedVideoFrame_t& decodedFrame, OperationalMode operationalMode,
                                      videodecoder::FrameMetadata& metadata, uint32_t frameDurationMs);
};

#endif //__VTS_VIDEOSINK_TEST_H__

/** @} */ // End of VIDEO_SINK_HALTEST_HEADER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
