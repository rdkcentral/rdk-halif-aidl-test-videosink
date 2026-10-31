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
#include <future>
#include <map>

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
    std::map<int32_t, videosink::Capabilities> mSinkCapabilities;

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

            videosink::Capabilities capabilities;
            VIDEOSINK_GETCAPABILITIES(videoSink, &capabilities);
            mSinkCapabilities.emplace(_sinkId.value, capabilities);

            mVideoSinkList.push_back({_sinkId, std::move(videoSink), std::move(videoSinkControlListener), std::move(videoSinkEventListener)});
        }

    }

    bool runSequentialSink(const videoDecoderTestConfiguration_t& decoderConfig)
    {
        bool executedAtLeastOne = false;
        std::unique_ptr<videoDecoderTest> decodeTest;
        std::unique_ptr<VideoSinkTest> sinkTest;
        if (gVideosinks_hfp.availableClocks_hfp.empty())
        {
            UT_LOG_ERROR("No available AVClock for sequential sink playback");
            UT_ASSERT_TRUE(false);
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

                for (const auto& [decoderId, decoderHandle, decoderControlListener, decoderEventListener, capabilities] : mVideoDecoderList)
                {
                    const bool secureDecode = capabilities->supportsSecure && mSinkCapabilities.at(_sinkId.value).supportsSecure;
                    if (!checkDecoderSupportsConfig(decoderId, capabilities, decoderConfig, secureDecode))
                        continue;
                    IAVClock::Id clockId;
                    clockId.value = gVideosinks_hfp.availableClocks_hfp[0];
                    bool isSinkAttached = (sinkHandle != nullptr);
                    bool isClockInUse = false;

                    HeapMetrics beforeHeapMetrics, afterHeapMetrics;
                    AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, beforeHeapMetrics);

                    decodeTest = std::make_unique<videoDecoderTest>(decoderId, decoderHandle, decoderControlListener,
                                               decoderEventListener, mAVBufferManager, secureDecode,
                                               decoderConfig, mAVbufferHelper, isSinkAttached);

                    sinkTest = std::make_unique<VideoSinkTest>(_sinkId, sinkHandle, controlListener, eventListener,
                                           decoderControlListener, mAVBufferManager, decoderId, decoderHandle, clockId, decoderConfig,
                                           isClockInUse);

                    decoderControlListener->resetQueues();
                    decoderEventListener->resetQueues();

                    sinkTest->prepare();
                    auto decodeFuture = std::async(std::launch::async, &videoDecoderTest::run, decodeTest.get(), true);
                    sinkTest->run();

                    executedAtLeastOne = true;

                    decodeFuture.get();
                    sinkTest->closeSession();
                    decodeTest->closeDecoderSession();
                    sinkTest.reset();
                    decodeTest.reset();

                    AVBUFFER_GETMETRICS(mAVBufferManager, secureDecode, afterHeapMetrics);
                    UT_ASSERT_TRUE(beforeHeapMetrics.secure == afterHeapMetrics.secure);
                    UT_ASSERT_TRUE(beforeHeapMetrics.bytesUsed == afterHeapMetrics.bytesUsed);
                    UT_ASSERT_TRUE(beforeHeapMetrics.bytesTotal == afterHeapMetrics.bytesTotal);
                }
            }
        } catch (const std::exception& ex) {
            UT_LOG_ERROR("Exception in runSequentialSink: %s", ex.what());
            UT_ASSERT_TRUE(false);
            executedAtLeastOne = false;
        } catch (...) {
            UT_LOG_ERROR("Unknown exception in runSequentialSink");
            UT_ASSERT_TRUE(false);
            executedAtLeastOne = false;
        }

        if (sinkTest)
            sinkTest->closeSession();
        if (decodeTest)
            decodeTest->closeDecoderSession();
        return executedAtLeastOne;
    }

    bool runConcurrentSink(const videoDecoderTestConfiguration_t& decoderConfig)
    {
        bool executedAtLeastOne = false;
        std::vector<std::unique_ptr<VideoSinkTest>> sinkTests;
        std::vector<std::unique_ptr<videoDecoderTest>> decoderTests;
        try
        {
            std::vector<std::future<void>> decoderFutures;
            std::vector<std::future<void>> sinkFutures;
            HeapMetrics beforeHeapMetrics[2], afterHeapMetrics[2];
            bool heapUsed[2] = {false, false};
            std::vector<bool> secureModes;
            vector<std::tuple<IVideoDecoder::Id, sp<IVideoDecoder>, sp<VideoDecoderControllerListener>, sp<VideoDecoderEventListener>, videodecoder::Capabilities*>> availableDecoderList;
            vector<std::tuple<IVideoSink::Id, sp<IVideoSink>, sp<VideoSinkControllerListener>, sp<VideoSinkEventListener>>> availableSinkList;
            bool isSinkAttached = true;
            bool isClockInUse = false;

            // Collect usable decoders
            for (const auto& entry : mVideoDecoderList)
            {
                const auto& [decoderId, decoderHandle, controlListener, eventListener, capabilities] = entry;
                if (checkDecoderSupportsConfig(decoderId, capabilities, decoderConfig, false))
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

            for (int32_t i = 0; i < numInstances; ++i)
            {
                const auto& sinkId = std::get<0>(availableSinkList[i]);
                const auto* capabilities = std::get<4>(availableDecoderList[i]);
                const bool secure = capabilities->supportsSecure && mSinkCapabilities.at(sinkId.value).supportsSecure;
                secureModes.push_back(secure);
                heapUsed[secure ? 1 : 0] = true;
            }
            for (int heap = 0; heap < 2; ++heap)
            {
                if (heapUsed[heap])
                    AVBUFFER_GETMETRICS(mAVBufferManager, heap != 0, beforeHeapMetrics[heap]);
            }

            for(int32_t i = 0; i < numInstances; ++i)
            {
                auto& [_sinkId, videoSink, videoSinkControlListener, videoSinkEventListener] = availableSinkList[i];
                auto& [decoderId, decoderHandle, decoderControlListener, decoderEventListener, capabilities] = availableDecoderList[i];

                IAVClock::Id clockId;
                clockId.value = gVideosinks_hfp.availableClocks_hfp[i];

                auto sinkTest = std::make_unique<VideoSinkTest>(_sinkId, videoSink,
                                    videoSinkControlListener, videoSinkEventListener,
                                    decoderControlListener, mAVBufferManager, decoderId, decoderHandle, clockId,
                                    decoderConfig, isClockInUse);

                decoderControlListener->resetQueues();
                decoderEventListener->resetQueues();

                sinkTest->prepare();
                sinkTests.push_back(std::move(sinkTest));
            }

            for (int32_t i = 0; i < numInstances; ++i)
            {
                auto& [decoderId, decoderHandle, decoderControlListener, decoderEventListener, capabilities] = availableDecoderList[i];

                auto decoderTest = make_unique<videoDecoderTest>(decoderId, decoderHandle,
                        sp<VideoDecoderControllerListener>::cast(decoderControlListener),
                        sp<VideoDecoderEventListener>::cast(decoderEventListener),
                        sp<IAVBuffer>::cast(mAVBufferManager),
                        secureModes[i], decoderConfig, mAVbufferHelper, isSinkAttached);

                decoderTests.push_back(std::move(decoderTest));
                decoderFutures.push_back(std::async(std::launch::async, &videoDecoderTest::run, decoderTests.back().get(), true));
                sinkFutures.push_back(std::async(std::launch::async, &VideoSinkTest::run, sinkTests[i].get()));
            }

            for (auto& future : decoderFutures)
                future.get();
            for (auto& future : sinkFutures)
                future.get();

            executedAtLeastOne = true;

            for (auto& sinkTest : sinkTests)
                sinkTest->closeSession();
            for (auto& decoderTest : decoderTests)
            {
                decoderTest->closeDecoderSession();
            }

            decoderTests.clear();
            sinkTests.clear();

            for (int heap = 0; heap < 2; ++heap)
            {
                if (heapUsed[heap])
                {
                    AVBUFFER_GETMETRICS(mAVBufferManager, heap != 0, afterHeapMetrics[heap]);
                    UT_ASSERT_TRUE(beforeHeapMetrics[heap].secure == afterHeapMetrics[heap].secure);
                    UT_ASSERT_TRUE(beforeHeapMetrics[heap].bytesUsed == afterHeapMetrics[heap].bytesUsed);
                    UT_ASSERT_TRUE(beforeHeapMetrics[heap].bytesTotal == afterHeapMetrics[heap].bytesTotal);
                }
            }
        } catch (const std::exception& ex) {
            UT_LOG_ERROR("Exception in runConcurrentSink: %s", ex.what());
            UT_ASSERT_TRUE(false);
            executedAtLeastOne = false;
        } catch (...) {
            UT_LOG_ERROR("Unknown exception in runConcurrentSink");
            UT_ASSERT_TRUE(false);
            executedAtLeastOne = false;
        }
        for (auto& sinkTest : sinkTests)
            sinkTest->closeSession();
        for (auto& decoderTest : decoderTests)
            decoderTest->closeDecoderSession();
        return executedAtLeastOne;
    }

    void runSingleProcessTest(const char *testName, bool concurrentMode)
    {
        vector<videoDecoderTestConfiguration_t> sinkConfigs;

        UT_status_t status = vts_videodecoder_parse_testconfig(gVideoSinkTestConfiguration, VIDEOSINKMGR,
                                          gTestGroup, testName, sinkConfigs);

        if (status != UT_STATUS_OK || sinkConfigs.empty())
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
                executedAnyConfig = runConcurrentSink(sinkConfig) || executedAnyConfig;
            }
            else
            {
                executedAnyConfig = runSequentialSink(sinkConfig) || executedAnyConfig;
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
        mSinkCapabilities.clear();
        for (const auto& [id, decoderHandle, controlListener, eventListener, capabilities] : mVideoDecoderList)
        {
            VIDEODECODER_UNREGISTER_EVENT_SUCCESS(decoderHandle, eventListener);
            delete capabilities;
        }
        mVideoDecoderList.clear();

        //TODO: check right place to call joinThreadPool
        //android::IPCThreadState::self()->joinThreadPool();

    }
};

/****************************************************************
* Sequential platform-selected playback tests
****************************************************************/
/**
 * @brief Test the sink's ability to render decoded frames
 *        Type: Sequential
 *        Output mode: platform-selected after decoder start()
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 001@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential)
{
    const char *testName = "Sink_Decode";
    bool isConcurrent = false;

    runSingleProcessTest(testName, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Sequential
 *        Output mode: platform-selected after decoder start()
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 002@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_EOS)
{
    const char *testName = "Sink_EOS";
    bool isConcurrent = false;

    runSingleProcessTest(testName, isConcurrent);
}

/**
 * @brief Video sink playback with underflow detection
 *        Type: Sequential
 *        Output mode: platform-selected after decoder start()
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 003@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Underflow)
{
    const char *testName = "Sink_Underflow";
    bool isConcurrent = false;
    runSingleProcessTest(testName, isConcurrent);
}

/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Sequential
 *        Output mode: platform-selected after decoder start()
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 004@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_Flush)
{
    const char *testName = "Sink_Flush";
    bool isConcurrent = false;

    runSingleProcessTest(testName, isConcurrent);
}

/**
 * @brief Test the sink's discard-until-PTS behavior
 *        Type: Sequential
 *        Output mode: platform-selected after decoder start()
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 005@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool isConcurrent = false;
    runSingleProcessTest(testName, isConcurrent);
}

/**
 * @brief Test client queue back-pressure when output is non-tunnelled
 *        Type: Sequential
 *        Output mode: platform-selected after decoder start()
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 006@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Sequential_BackPressure)
{
    const char *testName = "Sink_BackPressure";
    bool isConcurrent = false;
    runSingleProcessTest(testName, isConcurrent);
}

/****************************************************************
* Concurrent platform-selected playback tests
****************************************************************/
/**
 * @brief Test the sink's ability to render decoded frames
 *        Type: Concurrent
 *        Output mode: platform-selected after decoder start()
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 007@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent)
{
    const char *testName = "Sink_Decode";
    bool isConcurrent = true;

    runSingleProcessTest(testName, isConcurrent);
}

/**
 * @brief Test to verify EOS is received after eos pts to the sink.
 *        Type: Concurrent
 *        Output mode: platform-selected after decoder start()
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 008@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_EOS)
{
    const char *testName = "Sink_EOS";
    bool isConcurrent = true;

    runSingleProcessTest(testName, isConcurrent);
}

/**
 * @brief Video sink playback with underflow detection
 *        Type: Concurrent
 *        Output mode: platform-selected after decoder start()
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 009@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Underflow)
{
    const char *testName = "Sink_Underflow";
    bool isConcurrent = true;
    runSingleProcessTest(testName, isConcurrent);
}

/**
 * @brief Test to verify onFlushComplete is received after flushing sink.
 *        Type: Concurrent
 *        Output mode: platform-selected after decoder start()
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 010@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_Flush)
{
    const char *testName = "Sink_Flush";
    bool isConcurrent = true;

    runSingleProcessTest(testName, isConcurrent);
}

/**
 * @brief Test the sink's discard-until-PTS behavior
 *        Type: Concurrent
 *        Output mode: platform-selected after decoder start()
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 011@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_DiscardFramesUntil)
{
    const char *testName = "Sink_DiscardFramesUntil";
    bool isConcurrent = true;
    runSingleProcessTest(testName, isConcurrent);
}

/**
 * @brief Test client queue back-pressure when output is non-tunnelled
 *        Type: Concurrent
 *        Output mode: platform-selected after decoder start()
 *
 * **Test Group ID:** 003@n
 * **Test Case ID:** 012@n
 */
UT_ADD_TEST(VTS_L3_VideoSink, VSink_Concurrent_BackPressure)
{
    const char *testName = "Sink_BackPressure";
    bool isConcurrent = true;
    runSingleProcessTest(testName, isConcurrent);
}

/** @} */ // End of VIDEO_SINK_HALTEST L3 File
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK Module
/** @} */ // End of HPK
