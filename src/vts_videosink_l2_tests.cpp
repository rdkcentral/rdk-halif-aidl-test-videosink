
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

#include <ut_kvp_profile.h>
#include <ut.h>
#include <ut_log.h>
#include <ut_control_plane.h>
#include <deque>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/Status.h>
#include <utils/String16.h>
#include <vector>
#include <optional>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <unordered_set>

#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink.h"

#include "com/rdk/hal/videosink/IVideoSink.h"
#include "com/rdk/hal/videosink/IVideoSinkManager.h"
#include "com/rdk/hal/videosink/BnVideoSinkControllerListener.h"
#include "com/rdk/hal/videosink/BnVideoSinkEventListener.h"

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

using namespace com::rdk::hal;
using namespace com::rdk::hal::videosink;
using namespace com::rdk::hal::videodecoder;

extern android::sp<IVideoSinkManager> gVideoSinkManager;

class VTS_L2_VideoSink : public UTCore
{
public:
    std::vector<std::tuple<sp<IVideoSink>, sp<VideoSinkControllerListener>, sp<VideoSinkEventListener>>> mVideoSinkList;
    VTS_L2_VideoSink() : UTCore() {}

    ~VTS_L2_VideoSink() override = default;

    void SetUp() override
    {
        UTCore::SetUp();

        if (gVideoSinkManager == nullptr)
        {
                UT_FAIL_FATAL("VideoSink Service not available");
        }

        std::vector<IVideoSink::Id> sinkIds;
        /* Get the Video Sink IDs */
        VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

        android::ProcessState::self()->startThreadPool();

        for(const auto& _sinkId: sinkIds)
        {
            sp<IVideoSink> videoSink;

            sp<VideoSinkEventListener> eventListener = new VideoSinkEventListener();

            sp<VideoSinkControllerListener> controlListener = new VideoSinkControllerListener();

            /* Get Video Sink handle */
            VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

            /* Register events */
            VIDEOSINK_REGISTER_EVENT_SUCCESS(videoSink, eventListener);

            mVideoSinkList.push_back({std::move(videoSink), std::move(controlListener), std::move(eventListener)});
        }
    }
    void checkCompleteStateTransition(sp<IVideoSink> sink, sp<VideoSinkEventListener> eventListener,
                                      videosink::State oldState, videosink::State intermediateState, videosink::State newState)
    {
        videosink::State currentState;
        eventListener->waitForState(intermediateState, std::chrono::milliseconds(VIDEO_MAX_TRANSITORY_STATE_WAIT_MS));

        UT_ASSERT_TRUE(eventListener->checkStateTransistion(oldState, intermediateState));

        eventListener->waitForState(newState, std::chrono::milliseconds(VIDEO_SINK_MAX_FINAL_STATE_WAIT_MS));

        VIDEOSINK_GET_STATE(sink, currentState);

        UT_ASSERT_EQUAL(currentState, newState);

        UT_ASSERT_TRUE(eventListener->checkStateTransistion(intermediateState, newState));

        eventListener->resetStateChanges();
    }

    void checkControllerCompleteStateTransition(sp<VideoSinkControllerListener> controlListener,
                                                videosink::State oldState, videosink::State intermediateState, videosink::State newState)
    {
        controlListener->waitForState(intermediateState, std::chrono::milliseconds(VIDEO_MAX_TRANSITORY_STATE_WAIT_MS));

        UT_ASSERT_TRUE(controlListener->checkStateTransistion(oldState, intermediateState));

        controlListener->waitForState(newState, std::chrono::milliseconds(VIDEO_SINK_MAX_FINAL_STATE_WAIT_MS));

        UT_ASSERT_TRUE(controlListener->checkStateTransistion(intermediateState, newState));

        controlListener->resetStateChanges();
    }

    void TearDown() override
    {
        for (const auto& [sink, controlListener, eventListener] : mVideoSinkList)
        {
            /* Un-register the events */
            VIDEOSINK_UNREGISTER_EVENT_SUCCESS(sink, eventListener);
        }
        mVideoSinkList.clear();
    }
};

struct PropertyTestCase
{
    ::com::rdk::hal::videosink::Property property;
    int32_t testValue;
    const char* propertyName;
};
std::vector<PropertyTestCase> testCases = {
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::AUTO), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::IP), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::TUNER), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::SYSTEM), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::HDMI_1), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::HDMI_2), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::HDMI_3), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::HDMI_4), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::HDMI_5), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::COMPOSITE_1), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::COMPOSITE_2), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::COMPOSITE_3), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::COMPOSITE_4), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::AV_SOURCE, static_cast<int32_t>(AVSource::COMPOSITE_5), "AV_SOURCE"},
    {::com::rdk::hal::videosink::Property::RENDER_FIRST_FRAME, 1, "RENDER_FIRST_FRAME"},
    {::com::rdk::hal::videosink::Property::RENDER_FIRST_FRAME, 0, "RENDER_FIRST_FRAME"},
};

/**
 * @brief Test the setting and getting of properties in VideoSink
 *
 * This test verifies that properties can be set and retrieved correctly
 * in the VideoSink module. It ensures that the set and get operations are
 * successful and that the retrieved property values match the set values.
 *
 * **Test Group ID:** 02@n
 * **Test Case ID:** 001@n
 *
 * **Test Procedure:**
 * Refer to UT specification documentation [VideoSink_L2_Low-Level_TestSpecification.md](../docs/pages/VideoSink_L2_Low-Level_TestSpecification.md)
 */
UT_ADD_TEST(VTS_L2_VideoSink, TestSetAndGetProperty)
{

    for(const auto& [videoSink, controlListener, eventListener] : mVideoSinkList)
    {
        sp<IVideoSinkController> videoSinkController;

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controlListener, videoSinkController, true);

        for (const auto& testCase : testCases)
        {
            PropertyValue setValue;
            setValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(testCase.testValue);

            VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController,testCase.property,setValue);

            std::optional<PropertyValue> getPropertyValue;
            VIDEOSINK_GET_PROPERTY_SUCCESS(videoSink, testCase.property, getPropertyValue);
            UT_ASSERT_EQUAL(getPropertyValue->value->get<com::rdk::hal::PropertyValue::Value::Tag::intValue>(), testCase.testValue);
        }

        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}

/**
 * @brief Test the setting and getting of properties in  multiple instance of VideoSink
 *
 * This test verifies that properties can be set and retrieved correctly in the VideoSink module. It ensures that the set and get operations are successful and that the retrieved property values match the set values.
 *
 * **Test Group ID:** 02@n
 * **Test Case ID:** 002@n
 *
 * **Test Procedure:**
 * Refer to UT specification documentation [VideoSink_L2_Low-Level_TestSpecification.md](../docs/pages/VideoSink_L2_Low-Level_TestSpecification.md)
 */
UT_ADD_TEST(VTS_L2_VideoSink, TestSetAndGetProperty_multipleInstance)
{
    std::vector<sp<IVideoSinkController>> videoSinkControllersList;
    for(const auto& [videoSink, controlListener, eventListener] : mVideoSinkList)
    {

        sp<IVideoSinkController> videoSinkController;

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controlListener, videoSinkController, true);
        videoSinkControllersList.push_back(videoSinkController);
    }
    for(size_t i = 0; i< mVideoSinkList.size();++i)
    {
        const auto& [videoSink, controlListener, eventListener] = mVideoSinkList[i];
        const auto& videoSinkController = videoSinkControllersList[i];

        for (const auto& testCase : testCases)
        {
            PropertyValue setValue;
            setValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(testCase.testValue);

            VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController,testCase.property,setValue);

            std::optional<PropertyValue> getPropertyValue;
            VIDEOSINK_GET_PROPERTY_SUCCESS(videoSink, testCase.property, getPropertyValue);
            UT_ASSERT_EQUAL(getPropertyValue->value->get<com::rdk::hal::PropertyValue::Value::Tag::intValue>(), testCase.testValue);
        }

    }

    for(size_t i = 0; i< mVideoSinkList.size();++i)
    {
        const auto& [videoSink, controlListener, eventListener] = mVideoSinkList[i];
        const auto& videoSinkController = videoSinkControllersList[i];

        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }

}

/**
* @brief Test the state transitions of the VideoSink component
*
* This test verifies the correct state transitions of the VideoSink component,
* ensuring that it moves through the expected states: READY, STARTED, FLUSHING, and CLOSED.
* It checks the integrity and correctness of each transition and the final state.
*
* **Test Group ID:** 02@n
* **Test Case ID:** 003@n
*
* **Test Procedure:**
* Refer to UT specification documentation [VideoSink_L2_Low-Level_TestSpecification.md](../docs/pages/VideoSink_L2_Low-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L2_VideoSink, TestStateTransitions)
{

    for(const auto& [videoSink, controlListener, eventListener] : mVideoSinkList)
    {
        /* Get Video sink handle */

        sp<IVideoSinkController> videoSinkController;

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controlListener, videoSinkController, false);
        checkCompleteStateTransition(videoSink,eventListener, videosink::State::CLOSED, videosink::State::OPENING, videosink::State::READY);
        checkControllerCompleteStateTransition(controlListener, videosink::State::CLOSED, videosink::State::OPENING, videosink::State::READY);

        // Transition to STARTED state
        // rdk-halif-aidl 0.23.0: a video decoder must be associated before start()
        VIDEOSINK_CONTROLLER_SETDEFAULTVIDEODECODER(videoSinkController);
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);
        checkCompleteStateTransition(videoSink,eventListener, videosink::State::READY, videosink::State::STARTING, videosink::State::STARTED);
        checkControllerCompleteStateTransition(controlListener, videosink::State::READY, videosink::State::STARTING, videosink::State::STARTED);

        // Transition to FLUSHING state
        VIDEOSINK_FLUSH_SUCCESS(videoSinkController, false);
        checkCompleteStateTransition(videoSink,eventListener, videosink::State::STARTED, videosink::State::FLUSHING, videosink::State::STARTED);
        checkControllerCompleteStateTransition(controlListener, videosink::State::STARTED, videosink::State::FLUSHING, videosink::State::STARTED);

        // Transition to READY state
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);
        checkCompleteStateTransition(videoSink,eventListener, videosink::State::STARTED, videosink::State::STOPPING, videosink::State::READY);
        checkControllerCompleteStateTransition(controlListener, videosink::State::STARTED, videosink::State::STOPPING, videosink::State::READY);

        // Transition to CLOSED state
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
        checkCompleteStateTransition(videoSink,eventListener, videosink::State::READY, videosink::State::CLOSING, videosink::State::CLOSED);
        checkControllerCompleteStateTransition(controlListener, videosink::State::READY, videosink::State::CLOSING, videosink::State::CLOSED);
    }
}

/**
* @brief Test the state transitions of the VideoSink component
*
* This test verifies the correct state transitions of the VideoSink component,
* ensuring that it moves through the expected states: READY, STARTED, FLUSHING, and CLOSED.
* It checks the integrity and correctness of each transition and the final state.
*
* **Test Group ID:** 02@n
* **Test Case ID:** 004@n
*
* **Test Procedure:**
* Refer to UT specification documentation [VideoSink_L2_Low-Level_TestSpecification.md](../docs/pages/VideoSink_L2_Low-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L2_VideoSink, TestStateTransitions_multipleInstance)
{
    std::vector<sp<IVideoSinkController>> videoSinkControllersList;
    for(const auto& [videoSink, controlListener, eventListener] : mVideoSinkList)
    {

        sp<IVideoSinkController> videoSinkController;

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controlListener, videoSinkController, false);
        checkCompleteStateTransition(videoSink,eventListener, videosink::State::CLOSED, videosink::State::OPENING, videosink::State::READY);
        checkControllerCompleteStateTransition(controlListener, videosink::State::CLOSED, videosink::State::OPENING, videosink::State::READY);
        videoSinkControllersList.push_back(videoSinkController);
    }

    for(size_t i = 0; i< mVideoSinkList.size();++i)
    {
        const auto& [videoSink, controlListener, eventListener] = mVideoSinkList[i];
        const auto& videoSinkController = videoSinkControllersList[i];
        /* Get Video sink handle */

        // Transition to STARTED state
        // rdk-halif-aidl 0.23.0: a video decoder must be associated before start()
        VIDEOSINK_CONTROLLER_SETDEFAULTVIDEODECODER(videoSinkController);
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);
        checkCompleteStateTransition(videoSink,eventListener, videosink::State::READY, videosink::State::STARTING, videosink::State::STARTED);
        checkControllerCompleteStateTransition(controlListener, videosink::State::READY, videosink::State::STARTING, videosink::State::STARTED);

        // Transition to FLUSHING state
        VIDEOSINK_FLUSH_SUCCESS(videoSinkController, false);
        checkCompleteStateTransition(videoSink,eventListener, videosink::State::STARTED, videosink::State::FLUSHING, videosink::State::STARTED);
        checkControllerCompleteStateTransition(controlListener, videosink::State::STARTED, videosink::State::FLUSHING, videosink::State::STARTED);

        // Transition to READY state
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);
        checkCompleteStateTransition(videoSink,eventListener, videosink::State::STARTED, videosink::State::STOPPING, videosink::State::READY);
        checkControllerCompleteStateTransition(controlListener, videosink::State::STARTED, videosink::State::STOPPING, videosink::State::READY);
    }

    for(size_t i = 0; i< mVideoSinkList.size();++i)
    {
        const auto& [videoSink, controlListener, eventListener] = mVideoSinkList[i];
        const auto& videoSinkController = videoSinkControllersList[i];
        // Transition to CLOSED state
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
        checkCompleteStateTransition(videoSink,eventListener, videosink::State::READY, videosink::State::CLOSING, videosink::State::CLOSED);
        checkControllerCompleteStateTransition(controlListener, videosink::State::READY, videosink::State::CLOSING, videosink::State::CLOSED);
    }
}

/**
* @brief Test the attach and detach clock functionality of the VideoSink component
*
* This test verifies the attach and detach clock functionality of the VideoSink component.
*
* **Test Group ID:** 02@n
* **Test Case ID:** 005@n
*
* **Test Procedure:**
* Refer to UT specification documentation [VideoSink_L2_Low-Level_TestSpecification.md](../docs/pages/VideoSink_L2_Low-Level_TestSpecification.md)
*/
UT_ADD_TEST(VTS_L2_VideoSink, TestAttachDetachClock)
{
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    sp<IVideoSinkController> videoSinkController;
    sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

    // Use AVClockManager-provided IDs for valid attach/replace flows
    avclock::IAVClock::Id validClockId1{};
    avclock::IAVClock::Id undefinedClockId;
    undefinedClockId.value = avclock::IAVClock::Id::UNDEFINED;
    avclock::IAVClock::Id retrievedClockId{};

    validClockId1.value = gVideosinks_hfp.availableClocks_hfp.front();

    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    // Attach and detach clock for each sink in READY state
    for(const auto& _sinkId: sinkIds)
    {
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        // Step 01: Open sink in READY state
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        // Step 02: Attach valid clock in READY and verify
        VIDEOSINK_ATTACH_CLOCK_SUCCESS(videoSinkController, validClockId1);
        VIDEOSINK_GET_CLOCK_SUCCESS(videoSinkController, retrievedClockId);
        UT_ASSERT_EQUAL(retrievedClockId.value, validClockId1.value);

        // Step 03: Attach UNDEFINED (detach effect)
        VIDEOSINK_ATTACH_CLOCK_SUCCESS(videoSinkController, undefinedClockId);
        VIDEOSINK_GET_CLOCK_SUCCESS(videoSinkController, retrievedClockId);
        UT_ASSERT_EQUAL(retrievedClockId.value, avclock::IAVClock::Id::UNDEFINED);

        // Step 04: Attach valid clock in READY and verify
        VIDEOSINK_ATTACH_CLOCK_SUCCESS(videoSinkController, validClockId1);
        VIDEOSINK_GET_CLOCK_SUCCESS(videoSinkController, retrievedClockId);
        UT_ASSERT_EQUAL(retrievedClockId.value, validClockId1.value);

        // Step 05: Detach clock and verify
        VIDEOSINK_DETACH_CLOCK_SUCCESS(videoSinkController);
        VIDEOSINK_GET_CLOCK_SUCCESS(videoSinkController, retrievedClockId);
        UT_ASSERT_EQUAL(retrievedClockId.value, avclock::IAVClock::Id::UNDEFINED);

        // Step 06: Close the sink
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }

    // Attach and detach clock for each sink in STARTED state
    for(const auto& _sinkId: sinkIds)
    {
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        // Step 01: Open sink in READY state
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        // Transition to STARTED state
        // rdk-halif-aidl 0.23.0: a video decoder must be associated before start()
        VIDEOSINK_CONTROLLER_SETDEFAULTVIDEODECODER(videoSinkController);
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);

        // Step 02: Attach valid clock in STARTED and verify
        VIDEOSINK_ATTACH_CLOCK_SUCCESS(videoSinkController, validClockId1);
        VIDEOSINK_GET_CLOCK_SUCCESS(videoSinkController, retrievedClockId);
        UT_ASSERT_EQUAL(retrievedClockId.value, validClockId1.value);

        // Step 03: Attach UNDEFINED (detach effect)
        VIDEOSINK_ATTACH_CLOCK_SUCCESS(videoSinkController, undefinedClockId);
        VIDEOSINK_GET_CLOCK_SUCCESS(videoSinkController, retrievedClockId);
        UT_ASSERT_EQUAL(retrievedClockId.value, avclock::IAVClock::Id::UNDEFINED);

        // Step 04: Attach valid clock in STARTED and verify
        VIDEOSINK_ATTACH_CLOCK_SUCCESS(videoSinkController, validClockId1);
        VIDEOSINK_GET_CLOCK_SUCCESS(videoSinkController, retrievedClockId);
        UT_ASSERT_EQUAL(retrievedClockId.value, validClockId1.value);

        // Step 05: Detach clock and verify
        VIDEOSINK_DETACH_CLOCK_SUCCESS(videoSinkController);
        VIDEOSINK_GET_CLOCK_SUCCESS(videoSinkController, retrievedClockId);
        UT_ASSERT_EQUAL(retrievedClockId.value, avclock::IAVClock::Id::UNDEFINED);

        // Step 06: Stop and Close the sink
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}

