/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
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

/**
 * @addtogroup HPK Hardware Porting Kit
 * @{
 *
 */

/**
 * @addtogroup VIDEO_SINK Video Sink Module
 * @{
 *
 */

/**
 * @defgroup VIDEO_SINK_HALTEST Video Sink HAL Tests
 * @{
 *
 */

/**
 * @defgroup VIDEO_SINK_HALTEST_L3 Video Sink HAL Tests L3 File
 * @{
 * @parblock
 *
 * ### L3 Test Cases for Video Sink HAL :
 *
 * This module includes Level 3 functional tests (success and failure scenarios).
 * This is to ensure that the API meets the operational requirements of the module across all vendors.
 *
 * **Pre-Conditions:** @n
 * **Dependencies:** None@n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink_l3_tests.c
 *
 */
#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink.h"
#include "vts_videodecoder.h"
#include "vts_videosink_test.h"

#include <condition_variable>
#include <mutex>

using namespace std::chrono;
using namespace std;

/* Test class registration with UT_Core */
UT_ADD_TEST_TO_GROUP(VTS_L3_VideoSink, UT_TESTS_L3)

const char *gTestGroup = "L3";
IAVBufferHelper* gAVbufferHelper;

static sp<IAVBuffer> vts_avbuffer_getService()
{
    return
        android::interface_cast<IAVBuffer>(
            android::defaultServiceManager()->getService(
                android::String16(IAVBuffer::serviceName().c_str())
            )
        );
}

class VTS_L3_VideoSink : public UTCore
{
public:
    VTS_L3_VideoSink() : UTCore(), mAVbufferHelper(nullptr) {}

    ~VTS_L3_VideoSink() override = default;

    vector<tuple<IVideoSink::Id, sp<IVideoSink>, sp<VideoSinkControllerListener>, sp<VideoSinkEventListener>>> mVideoSinkList;
    vector<tuple<IVideoDecoder::Id, sp<IVideoDecoder>, sp<VideoDecoderControllerListener>, sp<VideoDecoderEventListener>, videodecoder::Capabilities*>> mVideoDecoderList;
    sp<IAVBuffer> mAVBufferManager;
    IAVBufferHelper* mAVbufferHelper;
    bool mIsTunnelSupported = false;
    bool mIsNonTunnelSupported = false;

    const decoder_t* getDecoderHfpCapabilities(const IVideoDecoder::Id& decoderId) const
    {
        auto it = std::find_if(gVideodecoders_hfp.begin(),
                               gVideodecoders_hfp.end(),
                               [&decoderId](const decoder_t& item) {
                                   return item.id == decoderId.value;
                               });

        if (it == gVideodecoders_hfp.end())
        {
            return nullptr;
        }

        return &(*it);
    }

    bool checkDecoderSupportsConfig(const IVideoDecoder::Id& decoderId, const videodecoder::Capabilities* capabilities,
                                    const videoDecoderTestConfiguration_t& decoderConfig, bool secureDecode)
    {
        auto hfpIt = std::find_if(gVideodecoders_hfp.begin(), gVideodecoders_hfp.end(),
            [&decoderId](const decoder_t& item) { return item.id == decoderId.value; });
        if (hfpIt == gVideodecoders_hfp.end())
        {
            UT_LOG_WARNING("[DecoderID: %d] Not available in HFP", decoderId.value);
            return false;
        }
        const decoder_t& hfpDecoder = *hfpIt;

        auto supportsConfigFromHfp = [&](const char* fallbackReason) -> bool
        {
            if (secureDecode && !hfpDecoder.supportsSecure)
            {
                UT_LOG_WARNING("Decoder [%d] doesn't support secure decoding (HFP fallback)", decoderId.value);
                return false;
            }

            auto hfpCodecIt = std::find_if(hfpDecoder.supportedCodecs.begin(), hfpDecoder.supportedCodecs.end(),
                [&decoderConfig](const CodecCapability& cap) { return cap.codec == decoderConfig.codec; });
            if (hfpCodecIt == hfpDecoder.supportedCodecs.end())
            {
                UT_LOG_WARNING("Decoder [%d] doesn't support Codec [%s] in HFP",
                               decoderId.value,
                               videodecoder::toString(decoderConfig.codec).c_str());
                return false;
            }

            if (hfpCodecIt->supportedOutputPixelFormats.empty())
            {
                UT_LOG_WARNING("Decoder [%d] HFP missing supportedOutputPixelFormats for codec [%s]",
                               decoderId.value,
                               videodecoder::toString(decoderConfig.codec).c_str());
                return false;
            }

            if (std::find(hfpCodecIt->supportedOutputPixelFormats.begin(),
                          hfpCodecIt->supportedOutputPixelFormats.end(),
                          decoderConfig.pixelFormat) == hfpCodecIt->supportedOutputPixelFormats.end())
            {
                UT_LOG_INFO("Decoder [%d] skipping stream [%s]: pixel format [%s] unsupported in HFP",
                            decoderId.value,
                            decoderConfig.streamName.c_str(),
                            videodecoder::toString(decoderConfig.pixelFormat).c_str());
                return false;
            }

            if (decoderConfig.codedWidth > hfpCodecIt->maxFrameWidth ||
                decoderConfig.codedHeight > hfpCodecIt->maxFrameHeight)
            {
                UT_LOG_INFO("Decoder [%d] skipping stream [%s]: resolution %dx%d exceeds HFP capability %dx%d",
                            decoderId.value,
                            decoderConfig.streamName.c_str(),
                            decoderConfig.codedWidth,
                            decoderConfig.codedHeight,
                            hfpCodecIt->maxFrameWidth,
                            hfpCodecIt->maxFrameHeight);
                return false;
            }

            if (decoderConfig.frameRateDenominator > 0)
            {
                uint32_t streamFps = decoderConfig.frameRateNumerator / decoderConfig.frameRateDenominator;
                if (streamFps > static_cast<uint32_t>(hfpCodecIt->maxFrameRate))
                {
                    UT_LOG_INFO("Decoder [%d] skipping stream [%s]: frame rate %u fps exceeds HFP capability %d fps",
                                decoderId.value,
                                decoderConfig.streamName.c_str(),
                                streamFps,
                                hfpCodecIt->maxFrameRate);
                    return false;
                }
            }

            UT_LOG_WARNING("Decoder [%d] using HFP fallback for stream [%s] because %s",
                           decoderId.value,
                           decoderConfig.streamName.c_str(),
                           fallbackReason);
            return true;
        };

        if (capabilities == nullptr)
        {
            return supportsConfigFromHfp("decoder getCapabilities is unavailable");
        }

        if (secureDecode && !capabilities->supportsSecure)
        {
            UT_LOG_WARNING("Decoder doesn't support secure decoding");
            return false;
        }

        if (capabilities->supportedCodecs.empty())
        {
            return supportsConfigFromHfp("decoder getCapabilities missing supportedCodecs");
        }

        auto codecIt = std::find_if(capabilities->supportedCodecs.begin(), capabilities->supportedCodecs.end(),
                                    [&decoderConfig](const videodecoder::CodecCapabilities& cap) { return cap.codec == decoderConfig.codec; });
        if (codecIt == capabilities->supportedCodecs.end())
        {
            UT_LOG_WARNING("Decoder doesn't support Codec [%s]", videodecoder::toString(decoderConfig.codec).c_str());
            return false;
        }

        if (codecIt->supportedOutputPixelFormats.empty())
        {
            UT_LOG_INFO(
                "Decoder [%d] skipping stream [%s]: decoder getCapabilities missing supportedOutputPixelFormats",
                 decoderId.value,
                 decoderConfig.streamName.c_str());
            return supportsConfigFromHfp("decoder getCapabilities missing supportedOutputPixelFormats");
        }

        if (std::find(codecIt->supportedOutputPixelFormats.begin(),
                      codecIt->supportedOutputPixelFormats.end(),
                      decoderConfig.pixelFormat) == codecIt->supportedOutputPixelFormats.end())
        {
            UT_LOG_INFO(
                "Decoder [%d] skipping stream [%s]: pixel format [%s] unsupported",
                 decoderId.value,
                 decoderConfig.streamName.c_str(),
                 videodecoder::toString(decoderConfig.pixelFormat).c_str());
            return false;
        }

        if (decoderConfig.codedWidth > codecIt->maxFrameWidth ||
            decoderConfig.codedHeight > codecIt->maxFrameHeight)
        {
            UT_LOG_INFO(
                "Decoder [%d] skipping stream [%s]: resolution %dx%d exceeds capability %dx%d",
                 decoderId.value,
                 decoderConfig.streamName.c_str(),
                 decoderConfig.codedWidth,
                 decoderConfig.codedHeight,
                 codecIt->maxFrameWidth,
                 codecIt->maxFrameHeight);
            return false;
        }

        if (decoderConfig.frameRateDenominator > 0)
        {
            uint32_t streamFps =
                decoderConfig.frameRateNumerator / decoderConfig.frameRateDenominator;

            if (streamFps > static_cast<uint32_t>(codecIt->maxFrameRate))
            {
                UT_LOG_INFO(
                    "Decoder [%d] skipping stream [%s]: frame rate %u fps exceeds capability %d fps",
                     decoderId.value,
                     decoderConfig.streamName.c_str(),
                     streamFps,
                     codecIt->maxFrameRate);
                return false;
            }
        }
        return true;
    }

    bool isVideoSinkUsable(const IVideoSink::Id& sinkId) const
    {
        return std::find(gVideosinks_hfp.availableVideoSinks_hfp.begin(), gVideosinks_hfp.availableVideoSinks_hfp.end(), sinkId.value) != gVideosinks_hfp.availableVideoSinks_hfp.end();
    }

    void SetUp() override
    {
        mAVBufferManager = vts_avbuffer_getService();

        if (mAVBufferManager == nullptr) {
                UT_FAIL_FATAL("AVBuffer Service not available");
        }

        if (gVideoDecoderManager == nullptr) {
                UT_FAIL_FATAL("VideoDecoder Service not available");
        }

        if (gVideoSinkManager == nullptr) {
                UT_FAIL_FATAL("VideoSink Service not available");
        }

        vector<IVideoSink::Id> sinkIds;
        vector<IVideoDecoder::Id> videoDecoderIds;

        mAVbufferHelper = getAVBufferHelperInstance();

        if (mAVbufferHelper == nullptr)
        {
            UT_FAIL_FATAL("AVBuffer Helper not available");
        }

        /*Get the Video Sink IDs*/
        VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

        if(getVideoDecoderService() != UT_STATUS_OK)
        {
            UT_LOG_ERROR("getVideoDecoderService failed %d",__LINE__);
        }
        else
        {
            if (gVideodecoders_hfp.empty())
            {
                if (vts_videodecoder_parseVideoDecoderHFP() != UT_STATUS_OK)
                {
                    UT_LOG_WARNING("Failed to parse VideoDecoder HFP capabilities for fallback path.");
                }
            }

            /*Get the Video Decoder IDs*/
            VIDEODECODER_MANAGER_GETVIDEODECODERIDS(videoDecoderIds);

            vector<videodecoder::OperationalMode> supportedModes;
            Status opModeStatus = gVideoDecoderManager->getSupportedOperationalModes(&supportedModes);
            if (opModeStatus.isOk() && opModeStatus.exceptionCode() == Status::EX_NONE)
            {
                mIsTunnelSupported = std::find(supportedModes.begin(), supportedModes.end(), videodecoder::OperationalMode::TUNNELLED) != supportedModes.end();
                mIsNonTunnelSupported = std::find(supportedModes.begin(), supportedModes.end(), videodecoder::OperationalMode::NON_TUNNELLED) != supportedModes.end();
            }
            else
            {
                mIsTunnelSupported = false;
                mIsNonTunnelSupported = false;
                UT_LOG_WARNING("Failed to query decoder operational modes. Tunnel and non-tunnel tests will be skipped.");
            }

            android::ProcessState::self()->startThreadPool();

            for (const auto& _videoDecoderId : videoDecoderIds)
            {
                sp<IVideoDecoder> videoDecoder;

                std::unique_ptr<videodecoder::Capabilities> capabilitiesOwner(new videodecoder::Capabilities());
                videodecoder::Capabilities* videoDecoderCapabilities = capabilitiesOwner.get();

                sp<VideoDecoderEventListener> videoDecoderEventListener = new VideoDecoderEventListener();

                sp<VideoDecoderControllerListener> videoDecoderControlListener = new VideoDecoderControllerListener();

                /*Get Video Decoder details*/
                VIDEODECODER_MANAGER_GETVIDEODECODER(_videoDecoderId, videoDecoder);

                /*Register the events */
                VIDEODECODER_REGISTER_EVENT_SUCCESS(videoDecoder, videoDecoderEventListener);

                /*Get Capabilities of Video Decoder */
                VIDEODECODER_GETCAPABILITIES(videoDecoder, videoDecoderCapabilities);

                mVideoDecoderList.push_back({_videoDecoderId, std::move(videoDecoder), std::move(videoDecoderControlListener), std::move(videoDecoderEventListener), capabilitiesOwner.release()});
            }
        }

        for(const auto& _sinkId: sinkIds)
        {

            sp<IVideoSink> videoSink;

            sp<VideoSinkEventListener> videoSinkEventListener = new VideoSinkEventListener();

            sp<VideoSinkControllerListener> videoSinkControlListener = new VideoSinkControllerListener();

            videoSinkControlListener->setSinkID(_sinkId.value);
            videoSinkEventListener->setSinkID(_sinkId.value);

            /* Get Video Sink handle */
            VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

            /* Register events */
            VIDEOSINK_REGISTER_EVENT_SUCCESS(videoSink, videoSinkEventListener);

            //TODO: VideoSink Get capabilities are yet to implement in the video sink

            mVideoSinkList.push_back({_sinkId, std::move(videoSink), std::move(videoSinkControlListener), std::move(videoSinkEventListener)});
        }

    }

    bool runSequentialSink(const videoDecoderTestConfiguration_t& decoderConfig,
                           bool secureDecode, bool isTunnel)
    {
        bool executedAtLeastOne = false;
        // Find a usable decoder
        IVideoDecoder::Id decoderId;
        sp<IVideoDecoder> decoderHandle;
        sp<VideoDecoderControllerListener> decoderControlListener;
        sp<VideoDecoderEventListener> decoderEventListener;
        bool foundDecoder = false;

        for (const auto& [_decoderId, _decoderHandle, _controlListener, _eventListener, _capabilities] : mVideoDecoderList)
        {
            std::string unsupportedReason;
            if (!checkDecoderSupportsConfig(_decoderId, _capabilities, decoderConfig, secureDecode))
            {
                UT_LOG_INFO("[DecoderID: %d] Doesn't support the requested configuration, skipping.", _decoderId.value);
                continue;
            }

            decoderId = _decoderId;
            decoderHandle = _decoderHandle;
            decoderControlListener = _controlListener;
            decoderEventListener = _eventListener;
            foundDecoder = true;
            break;
        }

        if (!foundDecoder)
        {
            UT_LOG_WARNING("No usable decoder found for test configuration '%s', skipping.", decoderConfig.input.c_str());
            return false;
        }

        try
        {
            for (const auto& [_sinkId, sinkHandle, controlListener, eventListener] : mVideoSinkList)
            {
                if (!isVideoSinkUsable(_sinkId))
                {
                    UT_LOG_INFO("[SinkID: %d] is not usable, skipping.", _sinkId.value);
                    continue;
                }

                IAVClock::Id clockId;
                clockId.value = gVideosinks_hfp.availableClocks_hfp[0];
                bool isSinkAttached = (sinkHandle != nullptr);
                bool isClockInUse = false;

                HeapMetrics beforeHeapMetrics, afterHeapMetrics;
                AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, beforeHeapMetrics);

                videoDecoderTest decodeTest(decoderId, decoderHandle, decoderControlListener,
                                           decoderEventListener, mAVBufferManager, secureDecode,
                                           isTunnel, decoderConfig, mAVbufferHelper, isSinkAttached);

                VideoSinkTest sinkTest(_sinkId, sinkHandle, controlListener, eventListener,
                                       decoderControlListener, mAVBufferManager, decoderId, clockId, decoderConfig,
                                       isClockInUse);

                decoderControlListener->resetQueues();
                decoderEventListener->resetQueues();

                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                std::thread decodeThread(&videoDecoderTest::run, &decodeTest, true);

                sinkTest.run(isTunnel);

                executedAtLeastOne = true;

                if (decodeThread.joinable())
                {
                    decodeThread.join();
                }

                decodeTest.closeDecoderSession();

                AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, afterHeapMetrics);
                UT_ASSERT_TRUE(beforeHeapMetrics.secure == afterHeapMetrics.secure);
                UT_ASSERT_TRUE(beforeHeapMetrics.bytesUsed == afterHeapMetrics.bytesUsed);
                UT_ASSERT_TRUE(beforeHeapMetrics.bytesTotal == afterHeapMetrics.bytesTotal);
            }
        } catch (const std::exception& ex) {
            UT_LOG_ERROR("Exception in runSequentialSink: %s", ex.what());
            UT_ASSERT_TRUE(false);
            return false;
        } catch (...) {
            UT_LOG_ERROR("Unknown exception in runSequentialSink");
            UT_ASSERT_TRUE(false);
            return false;
        }

        return executedAtLeastOne;
    }

    bool runConcurrentSink(const videoDecoderTestConfiguration_t& decoderConfig,
                           bool secure, bool tunnel)
    {
        bool executedAtLeastOne = false;
        try
        {
            std::vector<std::thread> threads;
            std::vector<std::thread> decoderThreads;
            std::vector<std::unique_ptr<VideoSinkTest>> sinkTests;
            std::vector<std::thread> sinkThreads;
            std::vector<std::unique_ptr<videoDecoderTest>> decoderTests;
            HeapMetrics beforeHeapMetrics, afterHeapMetrics;
            vector<std::tuple<IVideoDecoder::Id, sp<IVideoDecoder>, sp<VideoDecoderControllerListener>, sp<VideoDecoderEventListener>, videodecoder::Capabilities*>> availableDecoderList;
            vector<std::tuple<IVideoSink::Id, sp<IVideoSink>, sp<VideoSinkControllerListener>, sp<VideoSinkEventListener>>> availableSinkList;
            bool isSinkAttached = true;
            bool isClockInUse = false;

            /* Get heap metrics */
            AVBUFFER_GETMETRICS(mAVBufferManager, secure, beforeHeapMetrics);

            // Collect usable decoders
            for (const auto& entry : mVideoDecoderList)
            {
                const auto& [decoderId, decoderHandle, controlListener, eventListener, capabilities] = entry;
                if (checkDecoderSupportsConfig(decoderId, capabilities, decoderConfig, secure))
                    availableDecoderList.push_back(entry);
            }

            // Collect usable sinks
            for (const auto& entry : mVideoSinkList)
            {
                const auto& [sinkId, sinkHandle, controlListener, eventListener] = entry;
                if (isVideoSinkUsable(sinkId))
                    availableSinkList.push_back(entry);
            }

            int32_t availableDecoderCount = availableDecoderList.size();
            int32_t availableSinkCount = availableSinkList.size();
            int32_t availableClockCount = gVideosinks_hfp.availableClocks_hfp.size();
            int32_t numInstances = std::min({availableDecoderCount, availableSinkCount, availableClockCount});
            if (availableDecoderCount == 0)
            {
                UT_ASSERT_TRUE(false);
                UT_LOG_WARNING("No usable decoder found for the given test configuration, skipping concurrent test.");
                return false;
            }
            if (availableSinkCount == 0)
            {
                UT_ASSERT_TRUE(false);
                UT_LOG_WARNING("No usable sink found for the given test configuration, skipping concurrent test.");
                return false;
            }
            if (availableClockCount == 0)
            {
                UT_ASSERT_TRUE(false);
                UT_LOG_WARNING("No available clock found for the given test configuration, skipping concurrent test.");
                return false;
            }

            for(int32_t i = 0; i < numInstances; ++i)
            {
                auto& [_sinkId, videoSink, videoSinkControlListener, videoSinkEventListener] = availableSinkList[i];
                auto& [decoderId, decoderHandle, decoderControlListener, decoderEventListener, capabilities] = availableDecoderList[i];

                IAVClock::Id clockId;
                clockId.value = gVideosinks_hfp.availableClocks_hfp[i];

                auto sinkTest = std::make_unique<VideoSinkTest>(_sinkId, videoSink,
                                    videoSinkControlListener, videoSinkEventListener,
                                    decoderControlListener, mAVBufferManager, decoderId, clockId,
                                    decoderConfig, isClockInUse);

                decoderControlListener->resetQueues();
                decoderEventListener->resetQueues();

                sinkThreads.emplace_back(&VideoSinkTest::run, sinkTest.get(), tunnel);
                sinkTests.push_back(std::move(sinkTest));
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            for (int32_t i = 0; i < numInstances; ++i)
            {
                auto& [decoderId, decoderHandle, decoderControlListener, decoderEventListener, capabilities] = availableDecoderList[i];

                auto decoderTest = make_unique<videoDecoderTest>(decoderId, decoderHandle,
                        sp<VideoDecoderControllerListener>::cast(decoderControlListener),
                        sp<VideoDecoderEventListener>::cast(decoderEventListener),
                        sp<IAVBuffer>::cast(mAVBufferManager),
                        secure, tunnel, decoderConfig, mAVbufferHelper, isSinkAttached);

                decoderThreads.emplace_back(&videoDecoderTest::run, decoderTest.get(), true);

                decoderTests.push_back(move(decoderTest));
            }

            for (auto& t : decoderThreads)
            {
                if (t.joinable())
                    t.join();
            }
            for (auto& t : sinkThreads)
            {
                if (t.joinable())
                    t.join();
            }

            executedAtLeastOne = true;

            for (auto& decoderTest : decoderTests)
            {
                decoderTest->closeDecoderSession();
            }

            decoderTests.clear();
            sinkTests.clear();

            /* Get heap metrics */
            AVBUFFER_GETMETRICS(mAVBufferManager, secure, afterHeapMetrics);

            /* check if the decoder freed all the buffer pools */
            UT_ASSERT_TRUE(beforeHeapMetrics.secure == afterHeapMetrics.secure);
            UT_ASSERT_TRUE(beforeHeapMetrics.bytesUsed == afterHeapMetrics.bytesUsed);
            UT_ASSERT_TRUE(beforeHeapMetrics.bytesTotal == afterHeapMetrics.bytesTotal);
        } catch (const std::exception& ex) {
            UT_LOG_ERROR("Exception in runConcurrentSink: %s", ex.what());
            UT_ASSERT_TRUE(false);
            return false;
        } catch (...) {
            UT_LOG_ERROR("Unknown exception in runConcurrentSink");
            UT_ASSERT_TRUE(false);
            return false;
        }
        return executedAtLeastOne;
    }

    void runSingleProcessTest(const char *testName, bool secureMode, bool isTunnel, bool concurrentMode)
    {
        vector<videoDecoderTestConfiguration_t> sinkConfigs;

        if (isTunnel && !mIsTunnelSupported)
        {
            GTEST_SKIP() << "Tunnel mode is not supported by decoder capabilities";
            return;
        }

        if (!isTunnel && !mIsNonTunnelSupported)
        {
            GTEST_SKIP() << "Non-tunnel mode is not supported by decoder capabilities";
            return;
        }

        vts_videodecoder_parse_testconfig(gVideoSinkTestConfiguration, VIDEOSINKMGR,
                                          gTestGroup, testName, sinkConfigs);

        if (sinkConfigs.empty())
        {
            std::string errorMsg = std::string("No valid test configurations found - missing test streams or invalid configuration for test: ") + testName;
            UT_FAIL_FATAL(errorMsg.c_str());
            return;
        }

        bool executedAnyConfig = false;

        for (size_t i = 0; i < sinkConfigs.size(); ++i)
        {
            auto& sinkConfig = sinkConfigs[i];

            UT_LOG_INFO("=====================================================================================");
            UT_LOG_INFO("Running test configuration %zu/%zu: %s", i + 1, sinkConfigs.size(), sinkConfig.streamName.c_str());
            UT_LOG_INFO("=====================================================================================");
            if (concurrentMode)
            {
                executedAnyConfig = runConcurrentSink(sinkConfig, secureMode, isTunnel) || executedAnyConfig;
            }
            else
            {
                executedAnyConfig = runSequentialSink(sinkConfig, secureMode, isTunnel) || executedAnyConfig;
            }
        }

        if (!executedAnyConfig)
        {
            GTEST_SKIP() << "No usable decoder/sink combination for this test configuration (capability filtered)";
        }
    }

    void TearDown() override
    {
        for (const auto& [_sinkId, sink, videoSinkControlListener, videoSinkEventListener] : mVideoSinkList)
        {
            /* Un-register the events */
            VIDEOSINK_UNREGISTER_EVENT_SUCCESS(sink, videoSinkEventListener);
        }
        mVideoSinkList.clear();

        //TODO: check right place to call joinThreadPool
        //android::IPCThreadState::self()->joinThreadPool();

    }
};

/****************************************************************
* Sequentaial Non-Tunnel Non-Secure Tests
****************************************************************/
/**
 * @brief Test the sink's ability to render decoded frames
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 001@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_NonTunnel_NonSecure)
{
    const char *testName = "Sink_Decode";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 002@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_NonTunnel_NonSecure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Video sink playback with underflow detection
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 003@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_NonTunnel_NonSecure_Underflow)
{
    const char *testName = "Sink_Underflow";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 004@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_NonTunnel_NonSecure_Flush)
{
    const char *testName = "Sink_Flush";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 005@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_NonTunnel_NonSecure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Sequential
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 006@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_NonTunnel_NonSecure_BackPressure)
{
    const char *testName = "Sink_BackPressure";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/****************************************************************
* Sequentaial Tunnel Non-Secure Tests
****************************************************************/
/**
 * @brief Test the sink's ability to render decoded frames
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 007@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Tunnel_NonSecure)
{
    const char *testName = "Sink_Decode";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 008@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Tunnel_NonSecure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Video sink playback with underflow detection
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 009@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Tunnel_NonSecure_Underflow)
{
    const char *testName = "Sink_Underflow";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 010@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Tunnel_NonSecure_Flush)
{
    const char *testName = "Sink_Flush";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 011@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Tunnel_NonSecure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Sequential
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 012@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Tunnel_NonSecure_BackPressure)
{
    const char *testName = "Sink_BackPressure";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/****************************************************************
* Sequentaial Non-Tunnel Secure Tests
****************************************************************/
/**
 * @brief Test the sink's ability to render decoded frames
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 013@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_NonTunnel_Secure)
{
    const char *testName = "Sink_Decode";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 014@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_NonTunnel_Secure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Video sink playback with underflow detection
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 015@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_NonTunnel_Secure_Underflow)
{
    const char *testName = "Sink_Underflow";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 016@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_NonTunnel_Secure_Flush)
{
    const char *testName = "Sink_Flush";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 017@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_NonTunnel_Secure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Sequential
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 018@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_NonTunnel_Secure_BackPressure)
{
    const char *testName = "Sink_BackPressure";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/****************************************************************
* Sequentaial Tunnel Secure Tests
****************************************************************/
/**
 * @brief Test the sink's ability to render decoded frames
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 019@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Tunnel_Secure)
{
    const char *testName = "Sink_Decode";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 020@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Tunnel_Secure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Video sink playback with underflow detection
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 021@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Tunnel_Secure_Underflow)
{
    const char *testName = "Sink_Underflow";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 022@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Tunnel_Secure_Flush)
{
    const char *testName = "Sink_Flush";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 023@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Tunnel_Secure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Sequential
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 024@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Tunnel_Secure_BackPressure)
{
    const char *testName = "Sink_BackPressure";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = false;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/****************************************************************
* Concurrent Non-Tunnel Non-Secure Tests
****************************************************************/
/**
 * @brief Test the sink's ability to render decoded frames
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 001@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_NonTunnel_NonSecure)
{
    const char *testName = "Sink_Decode";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 002@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_NonTunnel_NonSecure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Video sink playback with underflow detection
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 003@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_NonTunnel_NonSecure_Underflow)
{
    const char *testName = "Sink_Underflow";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 004@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_NonTunnel_NonSecure_Flush)
{
    const char *testName = "Sink_Flush";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 005@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_NonTunnel_NonSecure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 006@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_NonTunnel_NonSecure_BackPressure)
{
    const char *testName = "Sink_BackPressure";
    bool secureMode = false;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/****************************************************************
* Concurrent Tunnel Non-Secure Tests
****************************************************************/
/**
 * @brief Test the sink's ability to render decoded frames
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 007@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Tunnel_NonSecure)
{
    const char *testName = "Sink_Decode";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 008@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Tunnel_NonSecure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Video sink playback with underflow detection
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 009@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Tunnel_NonSecure_Underflow)
{
    const char *testName = "Sink_Underflow";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 010@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Tunnel_NonSecure_Flush)
{
    const char *testName = "Sink_Flush";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 011@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Tunnel_NonSecure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Concurrent
 *        Modes: Tunnel, Non-Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 012@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Tunnel_NonSecure_BackPressure)
{
    const char *testName = "Sink_BackPressure";
    bool secureMode = false;
    bool isTunnel = true;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/****************************************************************
* Concurrent Non-Tunnel Secure Tests
****************************************************************/
/**
 * @brief Test the sink's ability to render decoded frames
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 013@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_NonTunnel_Secure)
{
    const char *testName = "Sink_Decode";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 014@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_NonTunnel_Secure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Video sink playback with underflow detection
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 015@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_NonTunnel_Secure_Underflow)
{
    const char *testName = "Sink_Underflow";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 016@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_NonTunnel_Secure_Flush)
{
    const char *testName = "Sink_Flush";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 017@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_NonTunnel_Secure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Concurrent
 *        Modes: Non-Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 018@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_NonTunnel_Secure_BackPressure)
{
    const char *testName = "Sink_BackPressure";
    bool secureMode = true;
    bool isTunnel = false;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/****************************************************************
* Concurrent Tunnel Secure Tests
****************************************************************/
/**
 * @brief Test the sink's ability to render decoded frames
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 019@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Tunnel_Secure)
{
    const char *testName = "Sink_Decode";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 020@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Tunnel_Secure_EOS)
{
    const char *testName = "Sink_EOS";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Video sink playback with underflow detection
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 021@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Tunnel_Secure_Underflow)
{
    const char *testName = "Sink_Underflow";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 022@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Tunnel_Secure_Flush)
{
    const char *testName = "Sink_Flush";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;

    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 023@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Tunnel_Secure_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/**
 * @brief Test to verify sink behavior when no clock is attched
 *        Type: Concurrent
 *        Modes: Tunnel, Secure
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 024@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Tunnel_Secure_BackPressure)
{
    const char *testName = "Sink_BackPressure";
    bool secureMode = true;
    bool isTunnel = true;
    bool isConcurrent = true;
    runSingleProcessTest(testName, secureMode, isTunnel, isConcurrent);
}

/** @} */ // End of VIDEO_SINK_HALTEST L3 File
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK Module
/** @} */ // End of HPK
