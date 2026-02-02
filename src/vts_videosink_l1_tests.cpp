
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
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/Status.h>
#include <utils/String16.h>
#include <stdio.h>
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

UT_ADD_TEST_TO_GROUP(VTS_L1_VideoSink, UT_TESTS_L1)

extern sp<IVideoSinkManager> gVideoSinkManager;
extern std::vector<int32_t> gVideosinks_hfp;

class VTS_L1_VideoSink : public UTCore
{
public:
    VTS_L1_VideoSink() : UTCore() {}

    ~VTS_L1_VideoSink() override = default;

    void SetUp() override
    {
        if (gVideoSinkManager == nullptr) {
                UT_FAIL_FATAL("VideoSink Service not available");
        }

        UTCore::SetUp();
    }

    void TearDown() override
    {
    }
};


UT_status_t vts_videosink_getVideoSinkService()
{
    UT_LOG_INFO("Getting Service\n");
    gVideoSinkManager =
        android::interface_cast<::com::rdk::hal::videosink::IVideoSinkManager>(
            android::defaultServiceManager()->getService(
                android::String16(::com::rdk::hal::videosink::IVideoSinkManager::serviceName().c_str())
            )
        );
    UT_LOG_INFO("Getting Service done\n");
    if (gVideoSinkManager == nullptr)
    {
        printf("Service not available\n");
        return UT_STATUS_FAILURE;
    }
    return UT_STATUS_OK;
}


/**
 * @brief Test to verify the retrieval of video sink IDs from the VideoSinkManager service.
 *
 * This test checks the functionality of the getVideoSinkIds method in the VideoSinkManager service.
 * It ensures that the method returns a valid list of video sink IDs and that the list is not empty.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 001@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * |01|Retrieve the initial video sink IDs|None|Should retrieve a non-empty list of IDs|Should Pass|
 * |02|Retrieve the second set of video sink IDs|None|Should retrieve a non-empty list of IDs|Should Pass|
 * |03|Ensure the sizes and values of both lists are equal|initialSinkIds.size(), secondSinkIDs.size(), initialSinkIds[i].value, secondSinkIds[i].value|Sizes and values should match|Should Pass|
 * |04|Get the supported sink IDs from the profile file|Profile file|Profile should contain supported decoder IDs|Should Pass|
 * |05|Ensure the sizes and values of profile file and inital set should match|initialSinkIds.size(), initialSinkIds[i].value|Sizes and values should match|Should Pass|
 */
UT_ADD_TEST(VTS_L1_VideoSink, getVideoSinkIds_pos)
{
    std::vector<IVideoSink::Id> initialSinkIds;
    std::vector<IVideoSink::Id> secondSinkIds;
    std::vector<int32_t> sinkId;
    std::unordered_set<int> sinkId_from_profile;

    // 01 - Retrieve the initial video sink IDs
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(initialSinkIds);

    for (const auto &obj : initialSinkIds)
    {
        sinkId.push_back(obj.value);
    }

    UT_ASSERT_TRUE(vts_videosink_helper_areElementsUnique(sinkId));

    //02 - Retrieve the second set of video sink IDs
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(secondSinkIds);

    //03- Ensure the sizes and values of both lists are equal
    UT_ASSERT_EQUAL(initialSinkIds.size(), secondSinkIds.size());

    for (size_t i = 0; i < initialSinkIds.size(); ++i)
    {
        UT_ASSERT_EQUAL(initialSinkIds[i].value, secondSinkIds[i].value);
    }

    //04 - Get the supported sink IDs from the profile file
    for (const auto &obj : gVideosinks_hfp)
    {
        sinkId_from_profile.insert(obj);
    }

    //05 - Ensure the sizes and values of profile file and inital set should match
    UT_ASSERT_EQUAL(initialSinkIds.size(), gVideosinks_hfp.size());
    for (int num : sinkId_from_profile)
    {
        auto it = std::find_if(sinkId.begin(), sinkId.end(), [num](const auto &id)
                               { return id == num; });
        UT_ASSERT_TRUE(it != sinkId.end());
    }
}

/**
 * @brief Test the positive scenario of retrieving video sink using valid video sink IDs.
 *
 * This test verifies that the VideoSinkManager service can successfully retrieve video sink objects
 * using valid video sink IDs. It ensures that the service returns a valid status and non-null video sink objects.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 002@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * |01| Retrieve the video sink IDs|None|Should retrieve a non-empty list of IDs|Should Pass|
 * |02| For each valid video sink ID, retrieve the corresponding video sink object. | videoSinkId = validId, videoSink_1 = nullptr | Status should be OK, videoSink should not be nullptr | Should Pass |
 * |03| Get Resource ID property for each sink object. | videosink_1 = valid sink object, property = Resource ID, resource_id_1 = Property value object | Status should be OK, videoSink should not be nullptr | Should Pass |
 * |04| For each valid video sink ID, retrieve the corresponding video sink object again. | videoSinkId = validId, videoSink = nullptr | Status should be OK, videoSink should not be nullptr | Should Pass |
 * |03| Get Resource ID property for each sink object again. | videosink_1 = valid sink object, property = Resource ID, resource_id_1 = Property value object | Status should be OK, videoSink should not be nullptr | Should Pass |
 * |06| compare resource ID for the sink Objects. | _resource_id_1, _resource_id_2 | Values should match | Should Pass |
 */
UT_ADD_TEST(VTS_L1_VideoSink, getVideoSink_pos) {


    sp<IVideoSink> videosink_1;
    sp<IVideoSink> videosink_2;
    IVideoSink::Id id ;
    std::optional<PropertyValue> resource_id_1;
    std::optional<PropertyValue> resource_id_2;
    std::vector<IVideoSink::Id> sinkIds;
    com::rdk::hal::videosink::Property property = com::rdk::hal::videosink::Property::RESOURCE_ID;

    //01 - Retrieve the video sink IDs
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        Status status;

        //02 - For each valid video sink ID, retrieve the corresponding video sink object.
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videosink_1);
        //03 - Get Resource ID property for each sink object.
        VIDEOSINK_GET_PROPERTY_SUCCESS(videosink_1, property, resource_id_1);

        //04 - For each valid video sink ID, retrieve the corresponding video sink object again.
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videosink_2);
        //05 - Get Resource ID property for each sink object again.
        VIDEOSINK_GET_PROPERTY_SUCCESS(videosink_2, property, resource_id_2);
        int32_t _resource_id_1 = resource_id_1->value->get<PropertyValue::Value::Tag::intValue>();
        int32_t _resource_id_2 = resource_id_2->value->get<PropertyValue::Value::Tag::intValue>();

        //06 - compare resource ID for the sink Objects
        UT_ASSERT_EQUAL(_resource_id_1, _resource_id_2);
    }
}

/**
 * @brief Test the negative scenarios for retrieving a video sink using invalid IDs.
 *
 * This test checks the behavior of the VideoSinkManager when attempting to retrieve a video sink using invalid IDs. It ensures that the system handles these cases gracefully without crashing and returns the expected results.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 003@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve Video Sink IDs using the getVideoSinkIds method | None | Status should be OK, IDs should not be empty | Should be successful |
 * | 02 | Sort the retrieved IDs to find the least and highest values | None | Sorted list of IDs | Should be successful |
 * | 03 | Test with invalid Video Sink ID (leastValue - 1) | invalidIdLow.value = leastValue - 1 | Status should be OK, videoSink should be nullptr | Should Pass |
 * | 04 | Test with invalid Video Sink ID (highestValue + 1) | invalidIdHigh.value = highestValue + 1 | Status should be OK, videoSink should be nullptr | Should Pass |
 */
UT_ADD_TEST(VTS_L1_VideoSink, getVideoSink_neg) {
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    std::vector<int> sinkId_from_profile;
    IVideoSink::Id invalidID;

    //01 - Retrieve the video sink IDs
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    if(sinkIds.size() > 1)
        std::sort(sinkIds.begin(), sinkIds.end());

    invalidID = sinkIds.front();
    invalidID.value -= 1;
    //02 - Retrieve the video sink object for invalid ID less than minimum value
    VIDEOSINK_MANAGER_GETVIDEOSINK_FAILURE(invalidID, videoSink, Status::EX_ILLEGAL_ARGUMENT);

    invalidID = sinkIds.back();
    invalidID.value += 1;
    //03 - Retrieve the video sink object for invalid ID greater than maximum value
    VIDEOSINK_MANAGER_GETVIDEOSINK_FAILURE(invalidID, videoSink, Status::EX_ILLEGAL_ARGUMENT);

    //04 - Retrieve the video sink object for undefined value
    invalidID.value = IVideoSink::Id::UNDEFINED;
    VIDEOSINK_MANAGER_GETVIDEOSINK_FAILURE(invalidID, videoSink, Status::EX_ILLEGAL_ARGUMENT);

}


/**
 * @brief Test the positive scenario for getting capabilities from VideoSink
 *
 * This test verifies the functionality of the getCapabilities method in the VideoSink class. It ensures that the method returns the expected capabilities without any errors.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 004@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * |01| Retrieve Video Sink IDs using the getVideoSinkIds method | None | Status should be OK, IDs should not be empty | Should be successful |
 * |02| For each valid video sink ID, retrieve the corresponding video sink object. | videoSinkId = validId, videoSink_1 = nullptr | Status should be OK, videoSink should not be nullptr | Should Pass |
 * |03| Iterate over each VideoSink object and call the getCapabilities method | videoSink = current VideoSink object, capabilities = uninitialized | status.isOk() = true, capabilities != nullptr | Should Pass |
 * |04| Log the result of the getCapabilities call for each VideoSink | capabilities = current capabilities, status = current status | Log should show capabilities and status | Should be successful |
 * |05| Assert that the status is OK and capabilities is not null | status.isOk() = true, capabilities != nullptr | Assertions should pass | Should Pass |
 */
UT_ADD_TEST(VTS_L1_VideoSink, getCapabilities_pos)
{
    com::rdk::hal::videosink::Capabilities capabilities;
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        /* Get Capabilities of Video sink */
        VIDEOSINK_GETCAPABILITIES(videoSink, &capabilities);
    }
}


/**
 * @brief Test the positive scenario of getting properties from VideoSink
 *
 * This test verifies the retrieval of various properties from VideoSink instances to ensure that the properties are correctly fetched and validated.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 005@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve Video Sink IDs using the getVideoSinkIds method | None | Status should be OK, IDs should not be empty | Should be successful |
 * | 02 | For each valid video sink ID, retrieve the corresponding video sink object. | videoSinkId = validId, videoSink_1 = nullptr | Status should be OK, videoSink should not be nullptr | Should Pass |
 * | 03 | Iterate over each property in the properties list | property = RESOURCE_ID | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 04 | Iterate over each property in the properties list | property = SINK_QUEUE_DEPTH | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 05 | Iterate over each property in the properties list | property = AV_SOURCE | Status is OK, aidlReturn has value, Tag is intValue, value is in AVSource | Should Pass |
 * | 06 | Iterate over each property in the properties list | property = RENDER_FIRST_FRAME | Status is OK, aidlReturn has value, Tag is booleanValue | Should Pass |
 * | 07 | Iterate over each property in the properties list | property = METRIC_FRAMES_RECEIVED | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 08 | Iterate over each property in the properties list | property = METRIC_FRAMES_PRESENTED | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 09 | Iterate over each property in the properties list | property = METRIC_FRAMES_DROPPED_LATE | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 10 | Iterate over each property in the properties list | property = METRIC_FRAMES_DROPPED_FRC | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 11 | Iterate over each property in the properties list | property = METRIC_FRAMES_REPEATED_FRC | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 12 | Iterate over each property in the properties list | property = METRIC_FRAMES_REPEATED_MISSING_FRAME | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 * | 13 | Iterate over each property in the properties list | property = METRIC_UNDERFLOWED | Status is OK, aidlReturn has value, Tag is intValue | Should Pass |
 */
 UT_ADD_TEST(VTS_L1_VideoSink, getProperty_pos) {
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    std::vector<com::rdk::hal::videosink::Property> testProperties = {
        com::rdk::hal::videosink::Property::RESOURCE_ID,
        com::rdk::hal::videosink::Property::SINK_QUEUE_DEPTH,
        com::rdk::hal::videosink::Property::AV_SOURCE,
        com::rdk::hal::videosink::Property::RENDER_FIRST_FRAME,
        com::rdk::hal::videosink::Property::METRIC_FRAMES_RECEIVED,
        com::rdk::hal::videosink::Property::METRIC_FRAMES_PRESENTED,
        com::rdk::hal::videosink::Property::METRIC_FRAMES_DROPPED_LATE,
        com::rdk::hal::videosink::Property::METRIC_FRAMES_DROPPED_FRC,
        com::rdk::hal::videosink::Property::METRIC_FRAMES_REPEATED_FRC,
        com::rdk::hal::videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME,
        com::rdk::hal::videosink::Property::METRIC_UNDERFLOWED
    };
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        /* Get RESOURCE_ID */
        for (auto property : testProperties)
        {
            std::optional<PropertyValue> propertyValue;
            VIDEOSINK_GET_PROPERTY_SUCCESS(videoSink, property, propertyValue);
            UT_ASSERT_TRUE(vts_videosink_helper_validatePropertyValue(_sinkId.value, propertyValue, property));
        }
    }
}


/**
 * @brief Test the negative scenarios for the getProperty function in VideoSink
 *
 * This test checks the behavior of the getProperty function when invalid or null properties are passed. It ensures that the function handles these cases gracefully by returning appropriate error statuses and values.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 006@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve Video Sink IDs using the getVideoSinkIds method | None | Status should be OK, IDs should not be empty | Should be successful |
 * | 02 | Sort the retrieved IDs to find the least and highest values | None | Sorted list of IDs | Should be successful |
 * | 03 | Call getProperty with an invalid property value | property = -1, aidl_return = nullptr | status.isOk() = false, aidl_return.has_value() = false | Should Fail |
 * | 04 | Call getProperty with a null property value | property = 0, aidl_return = nullptr | status.isOk() = false | Should Fail |
 */
UT_ADD_TEST(VTS_L1_VideoSink, getProperty_neg) {

    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    std::optional<PropertyValue> propertyValue;
    int32_t firstValue = static_cast<int32_t>(com::rdk::hal::videosink::Property::RESOURCE_ID);
    com::rdk::hal::videosink::Property Invalidproperty = static_cast<com::rdk::hal::videosink::Property>(firstValue - 1);

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        VIDEOSINK_GET_PROPERTY_FAILURE(videoSink, Invalidproperty, propertyValue, Status::EX_ILLEGAL_ARGUMENT);
    }
}


/**
 * @brief Test the positive scenario of getting the state from VideoSink
 *
 * This test verifies that the getState function correctly retrieves the state of each VideoSink object in mVideoSinks. It checks that the state is valid and the function call is successful.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 007@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve Video Sink IDs using the getVideoSinkIds method | None | Status should be OK, IDs should not be empty | Should be successful |
 * | 02 | Sort the retrieved IDs to find the least and highest values | None | Sorted list of IDs | Should be successful |
 * | 02 | Iterate over each VideoSink and call OPEN | sink = VideoSink, state = State, | status.isOk() = true, state in validStates | Should Pass |
 * | 03 | Check the status of getState call | status = android::binder::Status | status.isOk() = true | Should Pass |
 * | 04 | Validate the state is within expected valid states | state = State | state in validStates | Should Pass |
 */
UT_ADD_TEST(VTS_L1_VideoSink, getState_pos)
{
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<VideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink,controllerListener, videoSinkController, true);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}

/**
 * @brief Test the positive scenario of opening a video sink.
 *
 * This test verifies that the `open` function of each video sink in `mVideoSinks` can be successfully called with a valid listener, and that it returns a non-null `IVideoSinkController` object. The test ensures that the video sink is in the CLOSED state before calling `open`.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 008@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if `mVideoSinks` is not empty | mVideoSinks | Should not be empty | Should be successful |
 * | 02 | Iterate over each video sink in `mVideoSinks` | mVideoSinks | Each sink should be processed | Should be successful |
 * | 03 | Ensure the resource is in the CLOSED state before calling `open` | Initial state: CLOSED | Resource should be in CLOSED state | Should be successful |
 * | 04 | Call `open` on the video sink with a valid listener and capture the controller | listener = VideoSinkControllerListenerImpl, controller = nullptr | status.isOk() should be true, controller should not be null | Should Pass |
 * | 05 | Verify that the returned `IVideoSinkController` is non-null | controller | controller should not be null | Should Pass |
 * | 06 | Log the result of the `open` call | listener, controller, status | Log should contain correct status and controller information | Should be successful |
 */
UT_ADD_TEST(VTS_L1_VideoSink, open_pos)
{
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}

/**
 * @brief Test the behavior of VideoSink open function under negative scenarios.
 *
 * This test evaluates the VideoSink open function when provided with invalid inputs or called in an incorrect state. It checks the robustness of the function in handling such scenarios.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 009@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize the VideoSinkEventListenerImpl object | mSinkEventListener = new VideoSinkEventListenerImpl | Should be successful | |
 * | 02 | Open Video Sink with Null Listener | videoSinkControllerListener = null, controller = nullptr | controller = nullptr, status.isOk() = false | Should Fail |
 * | 03 | Open Video Sink with valid Listener | videoSinkControllerListener = mControllerListener, controller = valid | controller != nullptr, status.isOk() = true | Should Pass |
 * | 04 | Start the VideoSinkController to change state to READY | controller->start() | status.isOk() = true | Should be successful |
 * | 05 | Attempt to open Video Sink again in READY state | videoSinkControllerListener = mControllerListener, controller = nullptr | controller = nullptr, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 */
UT_ADD_TEST(VTS_L1_VideoSink, open_neg) {
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;
    sp<IVideoSinkController> videoSinkController = nullptr;
    sp<IVideoSinkController> videoSinkController_nullptr = nullptr;
    sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        /* Send invalid listener */
        VIDEOSINK_OPEN_FAILURE(videoSink, nullptr, videoSinkController_nullptr, Status::EX_NULL_POINTER);

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        /* Open the sink again */
        VIDEOSINK_OPEN_FAILURE(videoSink, controllerListener, videoSinkController_nullptr, Status::EX_ILLEGAL_STATE);

        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}

/**
 * @brief Test the positive scenario of closing video sink controllers.
 *
 * This test verifies that the close function for each video sink controller in the list is executed successfully and returns the expected results.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 010@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Verify that the list of controllers is not empty. | mControllers.empty() = false | Assertion should pass | Should be successful |
 * | 02 | Iterate over each controller in the list and call the close function. | controller = videoSinkController, closed = false | status.isOk() = true, closed = true | Should Pass |
 * | 03 | Log the result of the close function call and check the status and closed flag. | status.isOk() = true, closed = true | Assertions should pass | Should be successful |
 */
UT_ADD_TEST(VTS_L1_VideoSink, close_pos)
{
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for (const auto &_sinkId : sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}

/**
 * @brief Test the behavior of closing a Video Sink in invalid and valid states.
 *
 * This test checks the functionality of closing a Video Sink controller in both invalid and valid states. It ensures that the close operation fails when the Video Sink is in an invalid state and succeeds when it is in a valid state, followed by a failure on subsequent close attempts.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 011@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Attempt to close Video Sink in invalid state | controller = mControllers[i], aidl_return = false | status.isOk() = false, aidl_return = false | Should Fail |
 * | 02 | Open Video Sink | sinkId = mSinkIds[0], videoSink = nullptr | status.isOk() = true, videoSink != nullptr | Should be successful |
 * | 03 | Open Video Sink Controller | controllerListener = mControllerListener, newController = nullptr | status.isOk() = true, newController != nullptr | Should be successful |
 * | 04 | First close of Video Sink | newController = newController, aidl_return = false | status.isOk() = true, aidl_return = true | Should Pass |
 * | 05 | Second close of Video Sink | newController = newController, aidl_return = false | status.isOk() = false, aidl_return = false | Should Fail |
 */
UT_ADD_TEST(VTS_L1_VideoSink, close_neg)
{
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for (const auto &_sinkId : sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController, videoSinkController_nullptr = nullptr;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        /* Close with invalid controller */
        VIDEOSINK_CLOSE_FAILURE(videoSink, videoSinkController_nullptr, Status::EX_NULL_POINTER);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);

        /* Close the sink again*/
        VIDEOSINK_CLOSE_FAILURE(videoSink, videoSinkController, Status::EX_ILLEGAL_STATE);
    }
}


/**
 * @brief Test the positive registration of a VideoSink event listener.
 *
 * This test verifies the successful registration of a VideoSinkEventListener with all available video sinks. It ensures that the listener is correctly initialized and registered, and checks the status and return values to confirm successful operation.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 012@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize VideoSinkEventListener if not already initialized | SinkEventListener = nullptr | SinkEventListener should be initialized | Should be successful |
 * | 02 | Iterate over all available video sinks | videoSink in mVideoSinks | videoSink should be available for iteration | Should be successful |
 * | 03 | Log the API call for registerEventListener | SinkEventListener, aidl_return = false | Log should be generated | Should be successful |
 * | 04 | Call registerEventListener API | SinkEventListener, aidl_return | status.isOk() = true, aidl_return = true | Should Pass |
 * | 05 | Log the result of registerEventListener | SinkEventListener, aidl_return, status | Log should be generated | Should be successful |
 * | 06 | Verify the status is OK | status.isOk() | status.isOk() = true | Should Pass |
 * | 07 | Verify the aidl_return is true | aidl_return | aidl_return = true | Should Pass |
 */
UT_ADD_TEST(VTS_L1_VideoSink, registerEventListener_pos)
{
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        sp<IVideoSinkEventListener> eventListener = new VideoSinkEventListener();

        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        /* Register events */
        VIDEOSINK_REGISTER_EVENT_SUCCESS(videoSink, eventListener);

        /* Un-register the events */
        VIDEOSINK_UNREGISTER_EVENT_SUCCESS(videoSink, eventListener);
    }
}


/**
 * @brief Test the behavior of the registerEventListener function with invalid and duplicate inputs.
 *
 * This test evaluates the registerEventListener function by passing a null VideoSinkEventListener and
 * attempting to register a listener that is already registered. It checks the function's response to
 * invalid and duplicate listener registrations.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 013@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Attempt to register a null VideoSinkEventListener. | videoSinkEventListener = null, _aidl_return = false | status.isOk() = false, aidlReturn = false | Should Fail |
 * | 02 | Create a new VideoSinkEventListenerImpl instance. | mSinkEventListener = new VideoSinkEventListenerImpl | Instance created successfully | Should be successful |
 * | 03 | Register the newly created VideoSinkEventListenerImpl. | videoSinkEventListener = mSinkEventListener, _aidl_return = true | status.isOk() = true, aidlReturn = true | Should Pass |
 * | 04 | Attempt to register the same VideoSinkEventListenerImpl again. | videoSinkEventListener = mSinkEventListener, _aidl_return = false | status.isOk() = true, aidlReturn = false | Should Pass |
 */
UT_ADD_TEST(VTS_L1_VideoSink, registerEventListener_neg) {
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        sp<IVideoSinkEventListener> eventListener = new VideoSinkEventListener();
        sp<IVideoSinkEventListener> eventListener_nullptr = nullptr;

        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        /* Register events with invalid listener */
        VIDEOSINK_REGISTER_EVENT_FAILURE(videoSink, eventListener_nullptr, Status::EX_NULL_POINTER);

        /* Register events */
        VIDEOSINK_REGISTER_EVENT_SUCCESS(videoSink, eventListener);

        /* Register events again */
        VIDEOSINK_REGISTER_EVENT_FAILURE(videoSink, eventListener, Status::EX_NONE);

        /* Un-register the events */
        VIDEOSINK_UNREGISTER_EVENT_SUCCESS(videoSink, eventListener);
    }
}

/**
 * @brief Test the positive scenario of unregistering an event listener from VideoSink.
 *
 * This test verifies the successful unregistration of a previously registered event listener from the VideoSink. It ensures that the listener is correctly removed and that the operation returns a successful status.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 014@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if gVideoSinkManager is not null | gVideoSinkManager = nullptr | Should not be null | Should be successful |
 * | 02 | Initialize mSinkEventListener if null | mSinkEventListener = nullptr | Should be initialized | Should be successful |
 * | 03 | Register mSinkEventListener to each sink in mVideoSinks | mSinkEventListener = VideoSinkEventListenerImpl | Status should be OK | Should be successful |
 * | 04 | Unregister mSinkEventListener from gVideoSinkManager | mSinkEventListener = VideoSinkEventListenerImpl, aidl_return = false | Status should be OK, aidl_return should be true | Should Pass |
 */
UT_ADD_TEST(VTS_L1_VideoSink, unregisterEventListener_pos)
{
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        sp<IVideoSinkEventListener> eventListener = new VideoSinkEventListener();

        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        /* Register events */
        VIDEOSINK_REGISTER_EVENT_SUCCESS(videoSink, eventListener);

        /* Un-register the events */
        VIDEOSINK_UNREGISTER_EVENT_SUCCESS(videoSink, eventListener);

    }
}



/**
 * @brief Test the behavior of unregistering a VideoSinkEventListener in negative scenarios.
 *
 * This test evaluates the behavior of the VideoSinkManagerService when attempting to unregister a VideoSinkEventListener that is either not registered or has already been unregistered. It ensures that the service handles these cases gracefully without errors.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 015@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Create a VideoSinkEventListenerImpl instance | listener = new VideoSinkEventListenerImpl() | Instance should be created successfully | Should be successful |
 * | 02 | Attempt to unregister a non-registered listener | listener = 0x[listener address], aidlReturn = false | aidlReturn = false, status.isOk() = true | Should Fail |
 * | 03 | Register the listener | listener = 0x[listener address], registerReturn = false | registerReturn = true, status.isOk() = true | Should be successful |
 * | 04 | Unregister the listener for the first time | listener = 0x[listener address], aidlReturn = false | aidlReturn = true, status.isOk() = true | Should Pass |
 * | 05 | Attempt to unregister the listener again | listener = 0x[listener address], aidlReturn = false | aidlReturn = false, status.isOk() = true | Should Fail |
 */
UT_ADD_TEST(VTS_L1_VideoSink, unregisterEventListener_neg)
{
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        sp<IVideoSinkEventListener> eventListener = new VideoSinkEventListener();
        sp<IVideoSinkEventListener> eventListener_nullptr = nullptr;

        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        /* Un-register the events */
        VIDEOSINK_UNREGISTER_EVENT_FAILURE(videoSink, eventListener, Status::EX_NONE);

        /* Register events */
        VIDEOSINK_REGISTER_EVENT_SUCCESS(videoSink, eventListener);

        /* Un-register the events with nullptr*/
        VIDEOSINK_UNREGISTER_EVENT_FAILURE(videoSink, eventListener_nullptr, Status::EX_NULL_POINTER);

        /* Un-register the events */
        VIDEOSINK_UNREGISTER_EVENT_SUCCESS(videoSink, eventListener);

        /* Un-register the events again */
        VIDEOSINK_UNREGISTER_EVENT_FAILURE(videoSink, eventListener, Status::EX_NONE);
    }
}



/**
 * @brief Test the positive scenarios for setting properties in VideoSinkController
 *
 * This test verifies the functionality of setting various properties in the VideoSinkController.
 * It ensures that valid properties with boolean and integer values are correctly set and return expected results.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 016@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mControllers is not empty | mControllers | Should not be empty | Should be successful |
 * | 02 | Set a valid property with a boolean value (true) | property = RENDER_FIRST_FRAME, propertyValue = booleanValue:true, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 03 | Set a valid property with a boolean value (false) | property = RENDER_FIRST_FRAME, propertyValue = booleanValue:false, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 04 | Set a valid property with an integer value (AVSource::AUTO) | property = AV_SOURCE, propertyValue = intValue:AVSource::AUTO, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 05 | Set a valid property with an integer value (AVSource::UNKNOWN) | property = AV_SOURCE, propertyValue = intValue:AVSource::UNKNOWN, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 06 | Set a valid property with an integer value (AVSource::IP) | property = AV_SOURCE, propertyValue = intValue:AVSource::IP, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 07 | Set a valid property with an integer value (AVSource::TUNER) | property = AV_SOURCE, propertyValue = intValue:AVSource::TUNER, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 08 | Set a valid property with an integer value (AVSource::SYSTEM) | property = AV_SOURCE, propertyValue = intValue:AVSource::SYSTEM, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 09 | Set a valid property with an integer value (AVSource::HDMI_1) | property = AV_SOURCE, propertyValue = intValue:AVSource::HDMI_1, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 10 | Set a valid property with an integer value (AVSource::HDMI_2) | property = AV_SOURCE, propertyValue = intValue:AVSource::HDMI_2, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 11 | Set a valid property with an integer value (AVSource::HDMI_3) | property = AV_SOURCE, propertyValue = intValue:AVSource::HDMI_3, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 12 | Set a valid property with an integer value (AVSource::HDMI_4) | property = AV_SOURCE, propertyValue = intValue:AVSource::HDMI_4, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 13 | Set a valid property with an integer value (AVSource::HDMI_5) | property = AV_SOURCE, propertyValue = intValue:AVSource::HDMI_5, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 14 | Set a valid property with an integer value (AVSource::COMPOSITE_1) | property = AV_SOURCE, propertyValue = intValue:AVSource::COMPOSITE_1, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 15 | Set a valid property with an integer value (AVSource::COMPOSITE_2) | property = AV_SOURCE, propertyValue = intValue:AVSource::COMPOSITE_2, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 16 | Set a valid property with an integer value (AVSource::COMPOSITE_3) | property = AV_SOURCE, propertyValue = intValue:AVSource::COMPOSITE_3, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 17 | Set a valid property with an integer value (AVSource::COMPOSITE_4) | property = AV_SOURCE, propertyValue = intValue:AVSource::COMPOSITE_4, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 * | 18 | Set a valid property with an integer value (AVSource::COMPOSITE_5) | property = AV_SOURCE, propertyValue = intValue:AVSource::COMPOSITE_5, _aidl_return = false | status.isOk() = true, _aidl_return = true | Should Pass |
 */
UT_ADD_TEST(VTS_L1_VideoSink, setProperty_pos) {
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);
        ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::RENDER_FIRST_FRAME;
        PropertyValue propertyValue;
        propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(true);
        VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController,property,propertyValue);

        propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(false);
        VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController,property,propertyValue);

        // Test case 2: Set a valid property with a string value
        {
            ::com::rdk::hal::videosink::Property property = ::com::rdk::hal::videosink::Property::AV_SOURCE;
            for (int32_t avSourceValue : {static_cast<int32_t>(AVSource::AUTO), static_cast<int32_t>(AVSource::UNKNOWN),
                                          static_cast<int32_t>(AVSource::IP), static_cast<int32_t>(AVSource::TUNER),
                                          static_cast<int32_t>(AVSource::SYSTEM), static_cast<int32_t>(AVSource::HDMI_1),
                                          static_cast<int32_t>(AVSource::HDMI_2), static_cast<int32_t>(AVSource::HDMI_3),
                                          static_cast<int32_t>(AVSource::HDMI_4), static_cast<int32_t>(AVSource::HDMI_5),
                                          static_cast<int32_t>(AVSource::COMPOSITE_1), static_cast<int32_t>(AVSource::COMPOSITE_2),
                                          static_cast<int32_t>(AVSource::COMPOSITE_3), static_cast<int32_t>(AVSource::COMPOSITE_4),
                                          static_cast<int32_t>(AVSource::COMPOSITE_5)}) {
                PropertyValue propertyValue;
                propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(avSourceValue);
                VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController,property,propertyValue);

            }
        }
        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}



/**
 * @brief Test the negative scenarios for setting properties in VideoSinkController
 *
 * This test verifies the behavior of the VideoSinkController when attempting to set properties that are either read-only, invalid, or have invalid values. It ensures that the controller correctly handles these scenarios by not allowing the property to be set and returning the appropriate status.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 017@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Attempt to set a read-only property | property = RESOURCE_ID, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 02 | Attempt to set a read-only property | property = SINK_QUEUE_DEPTH, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 03 | Attempt to set a read-only property | property = METRIC_FRAMES_RECEIVED, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 04 | Attempt to set a read-only property | property = METRIC_FRAMES_PRESENTED, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 05 | Attempt to set a read-only property | property = METRIC_FRAMES_DROPPED_LATE, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 06 | Attempt to set a read-only property | property = METRIC_FRAMES_DROPPED_FRC, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 07 | Attempt to set a read-only property | property = METRIC_FRAMES_REPEATED_FRC, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 08 | Attempt to set a read-only property | property = METRIC_FRAMES_REPEATED_MISSING_FRAME, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 09 | Attempt to set a read-only property | property = METRIC_UNDERFLOWED, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 10 | Attempt to set an invalid property | property = -999, propertyValue = intValue:1 | aidl_return = false | Should Fail |
 * | 11 | Attempt to set a valid property with an invalid value | property = AV_SOURCE, propertyValue = intValue:10 | aidl_return = false | Should Fail |
 * | 12 | Transition controller to STARTED state |  | status.isOk() = true, started = true | Should be successful |
 * | 13 | Attempt to set AV_SOURCE from STARTED state | property = AV_SOURCE, propertyValue = intValue:AUTO | aidl_return = false | Should Fail |
 * | 14 | Attempt to set AV_SOURCE from STARTED state | property = AV_SOURCE, propertyValue = intValue:COMPOSITE_5 | aidl_return = false | Should Fail |
 * | 15 | Transition controller back to READY state |  | status.isOk() = true, stopped = true | Should be successful |
 */
UT_ADD_TEST(VTS_L1_VideoSink, setProperty_neg) {
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    std::vector<::com::rdk::hal::videosink::Property> readOnlyProperties = {
        ::com::rdk::hal::videosink::Property::RESOURCE_ID,
        ::com::rdk::hal::videosink::Property::SINK_QUEUE_DEPTH,
        ::com::rdk::hal::videosink::Property::METRIC_FRAMES_RECEIVED,
        ::com::rdk::hal::videosink::Property::METRIC_FRAMES_PRESENTED,
        ::com::rdk::hal::videosink::Property::METRIC_FRAMES_DROPPED_LATE,
        ::com::rdk::hal::videosink::Property::METRIC_FRAMES_DROPPED_FRC,
        ::com::rdk::hal::videosink::Property::METRIC_FRAMES_REPEATED_FRC,
        ::com::rdk::hal::videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME,
        ::com::rdk::hal::videosink::Property::METRIC_UNDERFLOWED};

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for (const auto &_sinkId : sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);
        for (const auto &property : readOnlyProperties)
        {
            PropertyValue propertyValue;
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(1);
            // Test Case 1: Set a READ-ONLY property
            VIDEOSINK_SETPROPERTY_FAILURE(videoSinkController, property, propertyValue, Status::EX_UNSUPPORTED_OPERATION);
        }
        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }

    // Test Case 2: Set an invalid property
    ::com::rdk::hal::videosink::Property invalidProperty = static_cast<::com::rdk::hal::videosink::Property>(-1); // Invalid property
    PropertyValue invalidPropertyValue;
    //invalidPropertyValue.value = PropertyValue::Value{PropertyValue::Value::Tag::intValue, 1};
    invalidPropertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(1);

    for (const auto &_sinkId : sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);
        VIDEOSINK_SETPROPERTY_FAILURE(videoSinkController,invalidProperty, invalidPropertyValue,Status::EX_ILLEGAL_ARGUMENT);
        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }

    // Test Case 3: Set a valid property with an invalid value
    ::com::rdk::hal::videosink::Property validProperty = ::com::rdk::hal::videosink::Property::AV_SOURCE;
    PropertyValue invalidValue;
    //invalidValue.value = PropertyValue::Value{PropertyValue::Value::Tag::intValue, 10}; // Invalid AVSource value
    invalidValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(256);


    for (const auto &_sinkId : sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);
        VIDEOSINK_SETPROPERTY_FAILURE(videoSinkController,validProperty, invalidValue,Status::EX_ILLEGAL_ARGUMENT);
        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }

    for (const auto &_sinkId : sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);
    // Test Case 4: Set AV_SOURCE from STARTED state

        for (int32_t avSource : {
            static_cast<int32_t>(AVSource::AUTO),
            static_cast<int32_t>(AVSource::UNKNOWN),
            static_cast<int32_t>(AVSource::IP),
            static_cast<int32_t>(AVSource::TUNER),
            static_cast<int32_t>(AVSource::SYSTEM),
            static_cast<int32_t>(AVSource::HDMI_1),
            static_cast<int32_t>(AVSource::HDMI_2),
            static_cast<int32_t>(AVSource::HDMI_3),
            static_cast<int32_t>(AVSource::HDMI_4),
            static_cast<int32_t>(AVSource::HDMI_5),
            static_cast<int32_t>(AVSource::COMPOSITE_1),
            static_cast<int32_t>(AVSource::COMPOSITE_2),
            static_cast<int32_t>(AVSource::COMPOSITE_3),
            static_cast<int32_t>(AVSource::COMPOSITE_4),
            static_cast<int32_t>(AVSource::COMPOSITE_5)})
        {

            PropertyValue avSourceValue;
            // avSourceValue.value = PropertyValue::Value{PropertyValue::Value::Tag::intValue, avSource};
            avSourceValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::intValue>(avSource);
            VIDEOSINK_SETPROPERTY_FAILURE(videoSinkController,validProperty, avSourceValue,Status::EX_ILLEGAL_STATE);
        }

        /* Stop the sink */
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);

    }
}

/**
 * @brief Test the positive scenario of setting a video sink in VideoSinkController.
 *
 * This test verifies the functionality of setting a video sink in the VideoSinkController. It ensures that the controller is in the correct state before setting the sink and checks the expected outcomes after the operation.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 018@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve the count of video sinks from the profile. | videosinkCount = UT_KVP_PROFILE_GET_LIST_COUNT(VIDEOSINKMGR_IVIDEODECODER) | videosinkCount > 0 | Should be successful |
 * | 02 | Iterate over each video sink ID retrieved from the profile. | videosinkId.value = UT_KVP_PROFILE_GET_UINT32(VIDEOSINKMGR_IVIDEODECODER.<index>.<index>, i) | N/A | Should be successful |
 * | 03 | Ensure each controller is in the READY state before setting the sink. | controller->getState() | State::READY | Should be successful |
 * | 04 | Call the setVideosink API with the videosinkId and check the return status. | videosinkId = value, aidl_return = false | status.isOk() = true, aidl_return = true | Should Pass |
 * | 05 | Log the result of the setVideosink API call. | N/A | N/A | Should be successful |
 * | 06 | Verify the status and aidl_return are as expected. | status.isOk(), aidl_return | status.isOk() = true, aidl_return = true | Should Pass |
 */
 UT_ADD_TEST(VTS_L1_VideoSink, setVideodecoder_pos) {
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;

    // Retrieve videosinkId from YAML using macros
    std::string videodecoderCount_path = VIDEOSINKMGR_IVIDEODECODER;
    int videodecoderCount = UT_KVP_PROFILE_GET_LIST_COUNT(videodecoderCount_path.c_str());
    UT_ASSERT_GREATER(videodecoderCount, 0);
    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();
        IVideoDecoder::Id videodecoderId;

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);
        for (int i = 0; i < videodecoderCount; ++i)
        {
            std::string videodecoderid_path = videodecoderCount_path+"."+std::to_string(i);
            videodecoderId.value = UT_KVP_PROFILE_GET_UINT32(videodecoderid_path.c_str());
            VIDEOSINK_CONTROLLER_SETVIDEODECODER_SUCCESS(videoSinkController,videodecoderId);
        }
        videodecoderId.value = IVideoDecoder::Id::UNDEFINED;
        VIDEOSINK_CONTROLLER_SETVIDEODECODER_SUCCESS(videoSinkController,videodecoderId);
        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}

/**
 * @brief Test the behavior of setVideosink with invalid and valid IDs in different states
 *
 * This test evaluates the setVideosink function by providing invalid video sink IDs and checking the response. It also tests the function when the resource is not in the READY state to ensure proper exception handling.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 019@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Retrieve videosinkIds from YAML using macros | count = UT_KVP_PROFILE_GET_LIST_COUNT(VIDEOSINKMGR_IVIDEODECODER) | Should be successful | Should be successful |
 * | 02 | Sort the videosinkIds to find the least and highest values | videosinkIds | Should be successful | Should be successful |
 * | 03 | Test with invalid video sink ID (less than least value) | invalidId.value = leastValue - 1 | aidlReturn = false, status.isOk() = true | Should Pass |
 * | 04 | Test with invalid video sink ID (greater than highest value) | invalidId.value = highestValue + 1 | aidlReturn = false, status.isOk() = true | Should Pass |
 * | 05 | Start the controller to change state | controller->start() | status.isOk() = true | Should be successful |
 * | 06 | Test with valid video sink IDs when resource is not in READY state | validId.value = id | aidlReturn = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Pass |
 * | 07 | Stop the controller to reset state | controller->stop() | status.isOk() = true | Should be successful |
 */
UT_ADD_TEST(VTS_L1_VideoSink, setVideodecoder_neg) {
    // Retrieve videosinkIds from YAML using macros
    std::vector<int32_t> videodecoderIds;

    std::string videodecoderCount_path = VIDEOSINKMGR_IVIDEODECODER;

    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    videodecoderIds.push_back(IVideoDecoder::Id::UNDEFINED);
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);
    int count = UT_KVP_PROFILE_GET_LIST_COUNT(videodecoderCount_path.c_str());

    for (int32_t i = 0; i < count; ++i) {
        std::string videodecoderid_path = videodecoderCount_path+"."+std::to_string(i);
        int32_t id = UT_KVP_PROFILE_GET_UINT32(videodecoderid_path.c_str());
        videodecoderIds.push_back(id);
    }

    // Sort the IDs to find the least and highest values
    std::sort(videodecoderIds.begin(), videodecoderIds.end());
    int32_t leastValue = videodecoderIds.front();
    //int32_t highestValue = videodecoderIds.back();

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        IVideoDecoder::Id invalidID;
        invalidID.value = leastValue - 1;
        VIDEOSINK_CONTROLLER_SETVIDEODECODER_FAILURE(videoSinkController,invalidID,android::binder::Status::EX_ILLEGAL_ARGUMENT);
#if 0
         invalidID.value = highestValue + 1;
        VIDEOSINK_CONTROLLER_SETVIDEODECODER_FAILURE(videoSinkController,invalidID,android::binder::Status::EX_ILLEGAL_ARGUMENT);
#endif
        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        /* Start the sink */
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);

        for (int32_t id : videodecoderIds) {
            IVideoDecoder::Id validId;
            validId.value = id;
            VIDEOSINK_CONTROLLER_SETVIDEODECODER_FAILURE(videoSinkController,validId,android::binder::Status::EX_ILLEGAL_STATE);
        }

        /* Stop the sink */
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}

/**
 * @brief Test the positive scenario of getting a video sink from the VideoSinkController.
 *
 * This test verifies the functionality of the getVideosink method in different valid states of the VideoSinkController.
 * It ensures that the method returns a valid video sink ID when the controller is in READY or STARTED state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 020@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if the controller is in READY state | controller = mControllers[i], isReady = false | status.isOk() = true, isReady = true/false | Should be successful |
 * | 02 | If READY, call getVideosink and verify | controller = mControllers[i], videosinkId = UNDEFINED | status.isOk() = true, videosinkId = UNDEFINED or valid | Should Pass |
 * | 03 | Check if the controller is in STARTED state | controller = mControllers[i], isStarted = false | status.isOk() = true, isStarted = true/false | Should be successful |
 * | 04 | If STARTED, call getVideosink and verify | controller = mControllers[i], videosinkId = UNDEFINED | status.isOk() = true, videosinkId = UNDEFINED or valid | Should Pass |
 */
UT_ADD_TEST(VTS_L1_VideoSink, getVideodecoder_pos)
{
    android::binder::Status status;
    IVideoDecoder::Id videodecoderId;
    // Retrieve videosinkIds from YAML using macros
    std::vector<int32_t> videodecoderIds;
    videodecoderIds.push_back(IVideoDecoder::Id::UNDEFINED);
    std::string videodecoderCount_path = VIDEOSINKMGR_IVIDEODECODER;
    int count = UT_KVP_PROFILE_GET_LIST_COUNT(videodecoderCount_path.c_str());

    for (int32_t i = 0; i < count; ++i)
    {
        std::string videodecoderid_path = videodecoderCount_path+"."+std::to_string(i);
        int32_t id = UT_KVP_PROFILE_GET_UINT32(videodecoderid_path.c_str());
        videodecoderIds.push_back(id);
    }


    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        // Test Case 1: Valid State - READY
        VIDEOSINK_CONTROLLER_GETVIDEODECODER_SUCCESS(videoSinkController,videodecoderId);
        UT_ASSERT_TRUE(std::find(videodecoderIds.begin(), videodecoderIds.end(), videodecoderId.value) != videodecoderIds.end());
        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }

    for(const auto& _sinkId: sinkIds)
    {
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);
        // Test Case 2: Valid State - STARTED
        /* Start the sink */
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);

        VIDEOSINK_CONTROLLER_GETVIDEODECODER_SUCCESS(videoSinkController,videodecoderId);
        UT_ASSERT_TRUE(std::find(videodecoderIds.begin(), videodecoderIds.end(), videodecoderId.value) != videodecoderIds.end());

        /* Stop the sink */
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }

}

/**
 * @brief Test the positive start functionality of VideoSinkController.
 *
 * This test verifies that the VideoSinkController can successfully start and stop a video sink when it is in the READY state. It ensures that the start and stop operations are executed without errors.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 021@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mControllers is not empty | mControllers.size() | Should not be empty | Should be successful |
 * | 02 | Check if mControllers size equals mVideoSinks size | mControllers.size(), mVideoSinks.size() | Sizes should be equal | Should be successful |
 * | 03 | Loop through each controller and video sink pair | i = 0 to mControllers.size() | Loop should execute for each pair | Should be successful |
 * | 04 | Ensure the Video Sink is in READY state | videoSink->isReady(&isReady) | status.isOk() = true, isReady = true | Should be successful |
 * | 05 | Log before calling start | "Calling start()" | Log should be successful | Should be successful |
 * | 06 | Call start API | videoSink->start() | status.isOk() = true | Should Pass |
 * | 07 | Log the result of start | "Result start(), status:[isOK:%s, exception:%s]" | Log should be successful | Should be successful |
 * | 08 | Verify the start was successful | status.isOk() | status.isOk() = true | Should Pass |
 * | 09 | Teardown: Call stop | videoSink->stop() | status.isOk() = true | Should Pass |
 * | 10 | Log the result of stop | "Result stop(), status:[isOK:%s, exception:%s]" | Log should be successful | Should be successful |
 * | 11 | Verify the stop was successful | status.isOk() | status.isOk() = true | Should Pass |
 */
UT_ADD_TEST(VTS_L1_VideoSink, start_pos) {
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        /* Start the sink */
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);

        /* Stop the sink */
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}

/**
 * @brief Test the negative start scenario for VideoSinkController
 *
 * This test verifies the behavior of the VideoSinkController when the start method is called while the Video Sink is not in the READY state. It ensures that the start method fails as expected in this scenario.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 022@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mControllers is not empty | mControllers | Should be successful | |
 * | 02 | Iterate over each controller in mControllers | controller | Should be successful | |
 * | 03 | Ensure the Video Sink is in a state other than READY by calling start | controller, isStarted = false | status.isOk() = true, isStarted = true | Should be successful |
 * | 04 | Log the call to start API | | | Should be successful |
 * | 05 | Call start again when the Video Sink is not in READY state | controller, isStarted | status.isOk() = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 * | 06 | Log the result of the start API | | | Should be successful |
 * | 07 | Verify the expected output | status | status.isOk() = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 */
UT_ADD_TEST(VTS_L1_VideoSink, start_neg) {
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        /* Start the sink */
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);

        /* Start the sink again */
        VIDEOSINK_START_FAILURE(videoSinkController, Status::EX_ILLEGAL_STATE);

        /* Stop the sink */
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);

        /* Start the sink after close */
        // VIDEOSINK_START_FAILURE(videoSinkController, Status::EX_ILLEGAL_STATE);
    }
}

/**
 * @brief Test the positive scenario of stopping the Video Sink Controller
 *
 * This test verifies that the Video Sink Controller can be successfully stopped after being started. It ensures that the stop function works correctly when the controller is in the STARTED state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 023@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Initialize the test environment and prepare the Video Sink Controller for testing. | N/A | Environment should be set up successfully. | Should be successful |
 * | 02 | Ensure the Video Sink is in the STARTED state by calling the start() method on the controller. | controller = mControllers[i], method = start() | status.isOk() should return true. | Should Pass |
 * | 03 | Log the result of the start() method call. | status.isOk() = true, status.exceptionCode() = 0 | Log should indicate success. | Should be successful |
 * | 04 | Call the stop() method on the controller to stop the Video Sink. | controller = mControllers[i], method = stop() | status.isOk() should return true. | Should Pass |
 * | 05 | Log the result of the stop() method call. | status.isOk() = true, status.exceptionCode() = 0 | Log should indicate success. | Should be successful |
 */
UT_ADD_TEST(VTS_L1_VideoSink, stop_pos)
{
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        /* Start the sink */
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);

        /* Stop the sink */
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}


/**
 * @brief Test the behavior of VideoSinkController when stop is called in invalid states.
 *
 * This test checks the VideoSinkController's response to stop() calls in two negative scenarios:
 * when the controller is in a non-started state and when stop() is called consecutively after a successful stop.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 024@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Attempt to stop VideoSinkController in non-started state | controller = mControllers[i] | exceptionCode = EX_ILLEGAL_STATE | Should Fail |
 * | 02 | Start the VideoSinkController | controller = mControllers[i] | status.isOk() = true | Should be successful |
 * | 03 | Stop the VideoSinkController | controller = mControllers[i] | status.isOk() = true | Should be successful |
 * | 04 | Attempt to stop VideoSinkController again after a successful stop | controller = mControllers[i] | exceptionCode = EX_ILLEGAL_STATE | Should Fail |
 */
UT_ADD_TEST(VTS_L1_VideoSink, stop_neg)
{
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        /* Start the sink */
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);

        /* Stop the sink */
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);

        /* Stop the sink again */
        VIDEOSINK_STOP_FAILURE(videoSinkController, Status::EX_ILLEGAL_STATE);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);

        /* stop the sink again */
        VIDEOSINK_STOP_FAILURE(videoSinkController, Status::EX_ILLEGAL_STATE);
    }
}

/**
 * @brief Test the negative scenario of queueing a video frame with invalid parameters.
 *
 * This test verifies the behavior of the VideoSinkController when attempting to queue a video frame with an invalid frame buffer handle. It ensures that the function correctly handles invalid input and returns the expected failure result.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 025@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if the controller list is not empty | mControllers | Should be successful | |
 * | 02 | Retrieve supported dynamic ranges from VideoSinkManagerService | supportedDynamicRanges | Should be successful | |
 * | 03 | Set metadata parameters for the video frame | metadata parameters | Should be successful | |
 * | 04 | Call queueVideoFrame with invalid frameBufferHandle | nsPresentationTime = 1000000000, frameBufferHandle = -1, metadata = [...] | _aidl_return = false, status.isOk() = true | Should Fail |
 */
UT_ADD_TEST(VTS_L1_VideoSink, queueVideoFrame_neg) {
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    int64_t *tempFrameBuffer=0x0;
    ::android::binder::Status status;
    videodecoder::FrameMetadata metadata;
    metadata.parX = 1;
    metadata.parY = 1;
    metadata.sarX = 16;
    metadata.sarY = 9;
    metadata.codedWidth = 1920;
    metadata.codedHeight = 1080;
    metadata.activeX = 0;
    metadata.activeY = 0;
    metadata.activeWidth = 1920;
    metadata.activeHeight = 1080;
    metadata.colorDepth = 8;
    metadata.pixelFormat = PixelFormat::RGB;
    metadata.dynamicRange = DynamicRange::SDR;
    metadata.scanType = ScanType::PROGRESSIVE_FRAME;
    metadata.afd = 0;
    metadata.frameRateNumerator = 24;
    metadata.frameRateDenominator = 1;
    metadata.endOfStream = false;
    metadata.discontinuity = false;
    metadata.lowLatency = false;
    metadata.source = AVSource::HDMI_1;
    metadata.sha1 = {0x12, 0x34, 0x56};

    //tempFrameBuffer = (int64_t *)malloc(512*sizeof(int64_t));
    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for (const auto &_sinkId : sinkIds)
    {
        int64_t nsPresentationTime = 1000000000;
        int64_t illegalFrameBufferHandle = -1;

        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_FAILURE(videoSinkController, nsPresentationTime, (int64_t)tempFrameBuffer, metadata, Status::EX_ILLEGAL_STATE);
        /* Start the sink */
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);

        VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_FAILURE(videoSinkController, -1, (int64_t)tempFrameBuffer, metadata, Status::EX_ILLEGAL_ARGUMENT);

        VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_FAILURE(videoSinkController, nsPresentationTime, illegalFrameBufferHandle, metadata, Status::EX_ILLEGAL_ARGUMENT);

        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);

        VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_FAILURE(videoSinkController, nsPresentationTime, (int64_t)tempFrameBuffer, metadata, Status::EX_ILLEGAL_STATE);

        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);

    }
    free (tempFrameBuffer);
}

/**
 * @brief Test the positive scenarios of the flush function in VideoSinkController
 *
 * This test verifies the behavior of the flush function in the VideoSinkController when the holdLastFrame parameter is set to true and false. It ensures that the function operates correctly without errors in both cases.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 026@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Start each controller to ensure it is in the State::STARTED state | None | Status should be OK | Should be successful |
 * | 02 | Call flush with holdLastFrame set to true | holdLastFrame = true | Status should be OK | Should Pass |
 * | 03 | Call flush with holdLastFrame set to false | holdLastFrame = false | Status should be OK | Should Pass |
 * | 04 | Stop each controller after testing | None | Status should be OK | Should be successful |
 */
UT_ADD_TEST(VTS_L1_VideoSink, flush_pos)
{
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for (const auto &_sinkId : sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        /* Start the sink */
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);

        /* Flush the sink with reset flag false*/
        VIDEOSINK_FLUSH_SUCCESS(videoSink, videoSinkController, false);

        /* Flush the sink with reset flag false*/
        VIDEOSINK_FLUSH_SUCCESS(videoSink, videoSinkController, true);

        /* Stop the sink */
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);
    }
}

/**
 * @brief Test the negative scenarios for the flush operation in VideoSinkController
 *
 * This test verifies the behavior of the flush operation when the resource is not in the STARTED state and when it is STARTED then STOPPED. The test ensures that the flush operation fails as expected in these scenarios.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 027@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mControllers is not empty | mControllers | Should be successful |  |
 * | 02 | Attempt to flush when resource is not in STARTED state | holdLastFrame = true | status.isOk() = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 * | 03 | Start the controller |  | status.isOk() = true | Should be successful |
 * | 04 | Stop the controller |  | status.isOk() = true | Should be successful |
 * | 05 | Attempt to flush when resource is STARTED then STOPPED | holdLastFrame = true | status.isOk() = false, status.exceptionCode() = EX_ILLEGAL_STATE | Should Fail |
 */
UT_ADD_TEST(VTS_L1_VideoSink, flush_neg) {
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for (const auto &_sinkId : sinkIds)
    {
        /* Get Video sink handle */
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        /* Flush the sink before start with reset flag false */
        VIDEOSINK_FLUSH_FAILURE(videoSinkController, false, Status::EX_ILLEGAL_STATE);

        /* Flush the sink before start with reset flag true */
        VIDEOSINK_FLUSH_FAILURE(videoSinkController, true, Status::EX_ILLEGAL_STATE);

        /* Start the sink */
        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);

        /* Stop the sink */
        VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, true);

        /* Flush the sink with reset flag false*/
        VIDEOSINK_FLUSH_FAILURE(videoSinkController, false, Status::EX_ILLEGAL_STATE);

        /* Flush the sink with reset flag false*/
        VIDEOSINK_FLUSH_FAILURE(videoSinkController, true, Status::EX_ILLEGAL_STATE);

        /* Close the sink */
        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);

        /* Flush the sink with reset flag false*/
        VIDEOSINK_FLUSH_FAILURE(videoSinkController, false, Status::EX_ILLEGAL_STATE);

        /* Flush the sink with reset flag false*/
        VIDEOSINK_FLUSH_FAILURE(videoSinkController, true, Status::EX_ILLEGAL_STATE);
    }
}

/**
 * @brief Test the positive scenario of the discardFramesUntil function in VideoSinkController.
 *
 * This test verifies that the discardFramesUntil function can successfully discard frames up to a specified presentation time when the controller is in the STARTED state. It ensures that the function behaves correctly with valid input and transitions the controller through the expected states.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 028@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Check if mControllers is not empty | mControllers | Should not be empty | Should be successful |
 * | 02 | Start each controller to ensure it is in the STARTED state | started = false | status.isOk() = true, started = true | Should be successful |
 * | 03 | Call discardFramesUntil with a valid nsPresentationTime | nsPresentationTime = 10000 | status.isOk() = true | Should Pass |
 * | 04 | Stop each controller to ensure it is in the STOPPED state | stopped = false | status.isOk() = true, stopped = true | Should be successful |
 */
UT_ADD_TEST(VTS_L1_VideoSink, discardFramesUntil_pos) {
    int64_t nsPresentationTime = 10000; // Using a valid future timestamp
    sp<IVideoSink> videoSink;
    std::vector<IVideoSink::Id> sinkIds;

    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);

        VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_SUCCESS(videoSinkController,nsPresentationTime);

        VIDEOSINK_STOP_SUCCESS(videoSink,videoSinkController,true);

        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);

    }
}

/**
 * @brief Test the negative scenario for discardFramesUntil API in VideoSinkController
 *
 * This test verifies that the discardFramesUntil API correctly handles the scenario where the resource is not in the STARTED state. It ensures that the API returns an ILLEGAL_STATE exception when called in an inappropriate state.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 029@n
 * **Priority:** High@n
 * @n
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * @n
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |
 * | :----: | --------- | ---------- |-------------- | ----- |
 * | 01 | Ensure the resource is not in the STARTED state by checking each controller | None | Controllers should not be in STARTED state | Should be successful |
 * | 02 | Log the call to discardFramesUntil with the given nsPresentationTime | nsPresentationTime = 10000 | Log should be successful | Should be successful |
 * | 03 | Call the discardFramesUntil API with nsPresentationTime | nsPresentationTime = 10000 | Status should return ILLEGAL_STATE exception | Should Fail |
 * | 04 | Log the result of the discardFramesUntil call | nsPresentationTime = 10000, status = ILLEGAL_STATE | Log should be successful | Should be successful |
 * | 05 | Verify the expected exception code from the API call | status.exceptionCode() = EX_ILLEGAL_STATE | Assertion should pass | Should Pass |
 */
UT_ADD_TEST(VTS_L1_VideoSink, discardFramesUntil_neg)
{
    std::vector<IVideoSink::Id> sinkIds;
    sp<IVideoSink> videoSink;
    ::android::binder::Status status;
    int64_t nsPresentationTime = 10000;
    int64_t illegalPresentationTime = -1;

    /* Get the Video sink IDs */
    VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds);

    for(const auto& _sinkId: sinkIds)
    {
        VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink);

        sp<IVideoSinkController> videoSinkController;
        sp<IVideoSinkControllerListener> controllerListener = new VideoSinkControllerListener();

        /* Open the sink */
        VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, true);

        VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_FAILURE(videoSinkController, nsPresentationTime, android::binder::Status::EX_ILLEGAL_STATE);

        VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, true);

        VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_FAILURE(videoSinkController, illegalPresentationTime, android::binder::Status::EX_ILLEGAL_ARGUMENT);

        VIDEOSINK_STOP_SUCCESS(videoSink,videoSinkController,true);

        VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_FAILURE(videoSinkController, nsPresentationTime, android::binder::Status::EX_ILLEGAL_STATE);

        VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, true);

        VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_FAILURE(videoSinkController, nsPresentationTime, android::binder::Status::EX_ILLEGAL_STATE);

    }
}
