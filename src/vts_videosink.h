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
 * ### Common functions/macros required for the audio module :
 *
 * Common functions/macros required for the audio module .
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink.h
 *
 */

#ifndef __VTS_VIDEOSINK_H__
#define __VTS_VIDEOSINK_H__

#include <thread>
#include <chrono>
#include <ut.h>
#include <inttypes.h>
#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink_parse_testconfig.h"
#include "vts_videosink_init.h"
#include "vts_avbuffer.h"
#include "vts_videodecoder.h"

#include "com/rdk/hal/videosink/IVideoSinkManager.h"
#include "com/rdk/hal/videosink/BnVideoSinkControllerListener.h"
#include "com/rdk/hal/videosink/BnVideoSinkEventListener.h"

#include "com/rdk/hal/avbuffer/IAVBuffer.h"
#include "com/rdk/hal/avbuffer/BnAVBufferSpaceListener.h"

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

using namespace com::rdk::hal;
using namespace com::rdk::hal::videosink;
using namespace com::rdk::hal::videodecoder;
using namespace com::rdk::hal::avbuffer;

extern sp<IVideoSinkManager> gVideoSinkManager;
extern void *gVideoSinkTestConfiguration;


#define VIDEO_SINK_MAX_TRANSITORY_STATE_WAIT_MS  5
#define VIDEO_SINK_MAX_FINAL_STATE_WAIT_MS  500

#ifndef ENABLE_UT_INFO_LOGGING
#ifdef UT_LOG_DEBUG
#undef UT_LOG_DEBUG
#define UT_LOG_DEBUG(...)
#endif
#ifdef UT_LOG_INFO
#undef UT_LOG_INFO
#define UT_LOG_INFO(...)
#endif
#endif

#define VIDEOSINK_MANAGER_GETVIDEOSINKIDS(sinkIds) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getVideoSinkIds(OUT:sinkIds:[])"); \
        status = gVideoSinkManager->getVideoSinkIds(&sinkIds); \
        UT_LOG_DEBUG("Result getVideoSinkIds(OUT:sinkIds:[size: %d]), status:[isOK:%s, exception:%s]", \
                                sinkIds.size(), UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_FALSE_FATAL(sinkIds.empty()); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)
#define VIDEOSINK_MANAGER_GETVIDEOSINKIDS_FAILURE(sinkIds,exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getVideoSinkIds(OUT:sinkIds:[])"); \
        status = gVideoSinkManager->getVideoSinkIds(sinkIds); \
        UT_LOG_DEBUG("Result getVideoSinkIds(OUT: status:[isOK:%s, exception:%s]", \
                                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_MANAGER_GETVIDEOSINK(_sinkId, videoSink) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getVideoSink(IN:_sinkId:[%d], OUT:videoSink[])", _sinkId.value); \
        status = gVideoSinkManager->getVideoSink(_sinkId, &videoSink); \
        UT_LOG_DEBUG("Result getVideoSink(IN:_sinkId:[%d], OUT:videoSink[0x%X]), status:[isOK:%s, exception:%s]", \
                                _sinkId.value, videoSink.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_TRUE_FATAL(videoSink.get() != nullptr); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_MANAGER_GETVIDEOSINK_FAILURE(_sinkId, videoSink, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getVideoSink(IN:_sinkId:[%d], OUT:videoSink[])", _sinkId.value); \
        status = gVideoSinkManager->getVideoSink(_sinkId, &videoSink); \
        UT_LOG_DEBUG("Result getVideoSink(IN:_sinkId:[%d]), status:[isOK:%s, exception:%s]", \
                                _sinkId.value, UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_CONTROLLER_SETVIDEODECODER_SUCCESS(videoSinkController,videodecoderId) \
    do { \
        ::android::binder::Status status; \
        bool aidl_return = false;\
        UT_LOG_DEBUG("Calling setVideoDecoder(IN:videodecoderId:[%d], OUT:aidl_return)", videodecoderId.value);\
        status = videoSinkController->setVideoDecoder(videodecoderId, &aidl_return);\
        UT_LOG_DEBUG("Result setVideoDecoder(IN:videosinkId:[%d], OUT:aidl_return:[%s]), status:[isOK:%s, exception:%s]",\
                    videodecoderId.value, aidl_return ? "true" : "false",\
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),\
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));\
        UT_ASSERT_TRUE(status.isOk());\
        UT_ASSERT_TRUE(aidl_return);\
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_CONTROLLER_SETVIDEODECODER_FAILURE(videoSinkController,videodecoderID,exception) \
    do { \
        ::android::binder::Status status; \
        bool aidl_return = false;\
        UT_LOG_DEBUG("Calling setVideoDecoder(IN:videodecoderId:[%d], OUT:_aidl_return)", videodecoderID.value);\
        status = videoSinkController->setVideoDecoder(videodecoderID, &aidl_return);\
        UT_LOG_DEBUG("Result setVideoDecoder(IN:videodecoderId:[%d]), status:[isOK:%s, exception:%s]",\
                    videodecoderID.value, UT_Control_GetMapString(bool_mapTable, status.isOk()),\
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));\
        UT_ASSERT_FALSE(status.isOk());\
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_CONTROLLER_GETVIDEODECODER_SUCCESS(videoSinkController,videodecoderID) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getVideoDecoder(OUT:videoDecoderId:[%d], OUT:aidl_return)", videodecoderId.value);\
        status = videoSinkController->getVideoDecoder(&videodecoderId);\
        UT_LOG_DEBUG("Result getVideoDecoder(OUT:videodecoderId:[%d]), status:[isOK:%s, exception:%s]",\
                    videodecoderId.value,\
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),\
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));\
        UT_ASSERT_TRUE_FATAL(status.isOk());\
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_CONTROLLER_GETVIDEODECODER_FAILURE(videoSinkController,videodecoderID,exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getVideoDecoder(OUT:videodecoderId)");\
        status = videoSinkController->getVideoDecoder(videodecoderID);\
        UT_LOG_DEBUG("Result getVideoDecoder(out:videodecoderId:[]), status:[isOK:%s, exception:%s]",\
                    UT_Control_GetMapString(bool_mapTable, status.isOk()),\
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));\
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_GETCAPABILITIES(videoSink, capabilities) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getCapabilities(OUT:capabilities)"); \
            videoSink->getCapabilities(capabilities); \
        UT_LOG_DEBUG("Result getCapabilities(OUT:capabilities), status:[isOK:%s, exception:%s]", \
                                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_GETCAPABILITIES_FAILURE(videoSink, capabilities,exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getCapabilities(OUT:capabilities)"); \
            videoSink->getCapabilities(capabilities); \
        UT_LOG_DEBUG("Result getCapabilities(OUT:capabilities), status:[isOK:%s, exception:%s]", \
                                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)
#define VIDEOSINK_GET_PROPERTY_SUCCESS(videoSink, property, propertyValue) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getProperty(IN:property[%s], OUT:propertyValue[]", videosink::toString(property).c_str()); \
        status = (videoSink)->getProperty(property, &propertyValue); \
        switch (propertyValue->value.value().getTag()) \
        { \
            case PropertyValue::Value::Tag::intValue: \
                UT_LOG_DEBUG("Result getProperty(IN:property[%s], OUT:propertyValue[%d]), status:[isOK:%s, exception:%s]", videosink::toString(property).c_str(), \
                propertyValue->value->get<PropertyValue::Value::Tag::intValue>(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
                break; \
            case PropertyValue::Value::Tag::stringValue: \
                UT_LOG_DEBUG("Result getProperty(IN:property[%s], OUT:propertyValue[%s]), status:[isOK:%s, exception:%s]", videosink::toString(property).c_str(), \
                propertyValue->value->get<PropertyValue::Value::Tag::stringValue>().string(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
                break; \
            default: \
                break; \
        } \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_GET_PROPERTY_FAILURE(videoSink, property, propertyValue, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getProperty(IN:property[%s], OUT:propertyValue[]", videosink::toString(property).c_str()); \
        status = (videoSink)->getProperty(property, &propertyValue); \
        UT_LOG_DEBUG("Result getProperty(IN:property[%s], OUT:propertyValue[]), status:[isOK:%s, exception:%s]", videosink::toString(property).c_str(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_GET_STATE(videoSink, state) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getState(OUT:state[]"); \
        status = (videoSink)->getState(&state); \
        UT_LOG_DEBUG("Result getState(OUT:state[%s]), status:[isOK:%s, exception:%s]", com::rdk::hal::toString(state).c_str(), \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_GET_STATE_FAILURE(videoSink, state, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getState(OUT:state[]"); \
        status = (videoSink)->getState(state); \
        UT_LOG_DEBUG("Result getState(OUT:state[]), status:[isOK:%s, exception:%s]", \
                UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_WAIT_FOR_STATE(videoSink, desiredState, lastState) \
    do { \
        int32_t sleep_time = 0; \
        do \
        { \
            std::this_thread::sleep_for(std::chrono::milliseconds(VIDEO_SINK_MAX_TRANSITORY_STATE_WAIT_MS)); \
            sleep_time += VIDEO_SINK_MAX_TRANSITORY_STATE_WAIT_MS; \
            /* Check the state of the sink */ \
            VIDEOSINK_GET_STATE((videoSink), lastState); \
        } while (sleep_time < VIDEO_SINK_MAX_FINAL_STATE_WAIT_MS && lastState != desiredState); \
    } while (0)

#define VIDEOSINK_OPEN_SUCCESS(videoSink, controllerListener, videoSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling open(IN:controllerListener[0x%p], OUT:videoSinkController[])", \
            controllerListener); \
        status = (videoSink)->open(controllerListener,&videoSinkController); \
        UT_LOG_DEBUG("Result open(IN:controllerListener[0x%p], OUT:videoSinkController[0x%X]), status:[exception:%s]", \
            controllerListener, videoSinkController.get(), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_NOT_EQUAL_FATAL(videoSinkController, nullptr); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
        \
        if (stateCheck) \
        { \
            ::com::rdk::hal::State lastState; \
            VIDEOSINK_WAIT_FOR_STATE(videoSink, ::com::rdk::hal::State::READY, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::State::READY); \
        } \
    } while (0)

#define VIDEOSINK_OPEN_FAILURE(videoSink, controllerListener, videoSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling open(IN:controllerListener[0x%p])", \
            controllerListener); \
        status = videoSink->open(controllerListener, &videoSinkController); \
        UT_LOG_DEBUG("Result open(IN:controllerListener[0x%p]), status:[exception:%s]", \
            controllerListener, \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0);

#define VIDEOSINK_CLOSE_SUCCESS(videoSink, videoSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_DEBUG("Calling close(IN:videoSinkController[0x%p])", videoSinkController.get()); \
        status = videoSink->close(videoSinkController,&_aidl_return); \
        UT_LOG_DEBUG("Result close(IN:videoSinkController[0x%p]), status:[isOK:%s, exception:%s]", \
            videoSinkController.get(), UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE_FATAL(status.isOk()); \
        UT_ASSERT_TRUE_FATAL(_aidl_return); \
        UT_ASSERT_EQUAL_FATAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            ::com::rdk::hal::State lastState; \
            VIDEOSINK_WAIT_FOR_STATE(videoSink, ::com::rdk::hal::State::CLOSED, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::State::CLOSED); \
        } \
    } while (0)

#define VIDEOSINK_CLOSE_FAILURE(videoSink, videoSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling close(IN:videoSinkController[0x%p], OUT:_aidl_return[])", videoSinkController.get()); \
        status = videoSink->close(videoSinkController, &_aidl_return); \
        UT_LOG_DEBUG("Result close(IN:videoSinkController[0x%p]), status:[isOK:%s, exception:%s]", \
            videoSinkController.get(), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_REGISTER_EVENT_SUCCESS(videoSink, eventListener) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = videoSink->registerEventListener(eventListener, &_aidl_return); \
        UT_LOG_DEBUG("Result registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(_aidl_return); \
    } while (0)

#define VIDEOSINK_REGISTER_EVENT_FAILURE(videoSink, eventListener, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return=false; \
        UT_LOG_DEBUG("Calling registerEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = videoSink->registerEventListener(eventListener, &_aidl_return); \
        UT_LOG_DEBUG("Result registerEventListener(IN:eventListener[0x%p]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        if (exception == Status::EX_NONE) \
            UT_ASSERT_TRUE(status.isOk()); \
        else \
            UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_UNREGISTER_EVENT_SUCCESS(videoSink, eventListener) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return; \
        UT_LOG_DEBUG("Calling unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = videoSink->unregisterEventListener(eventListener, &_aidl_return); \
        UT_LOG_DEBUG("Result unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_TRUE(_aidl_return); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_UNREGISTER_EVENT_FAILURE(videoSink, eventListener, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_DEBUG("Calling unregisterEventListener(IN:eventListener[0x%p], OUT:_aidl_return[])", eventListener.get()); \
        status = videoSink->unregisterEventListener(eventListener, &_aidl_return); \
        UT_LOG_DEBUG("Result unregisterEventListener(IN:eventListener[0x%p]), status:[isOK:%s, exception:%s]", \
            eventListener.get(), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_START_SUCCESS(videoSink, videoSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling start()"); \
        status = videoSinkController->start(); \
        UT_LOG_DEBUG("Result start(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            ::com::rdk::hal::State lastState; \
            VIDEOSINK_WAIT_FOR_STATE(videoSink, ::com::rdk::hal::State::STARTED, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::State::STARTED); \
        } \
    } while (0)

#define VIDEOSINK_START_FAILURE(videoSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling start()"); \
        status = videoSinkController->start(); \
        UT_LOG_DEBUG("Result start(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define VIDEOSINK_STOP_SUCCESS(videoSink, videoSinkController, stateCheck) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling stop()"); \
        status = videoSinkController->stop(); \
        UT_LOG_DEBUG("Result stop(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        if (stateCheck) \
        { \
            ::com::rdk::hal::State lastState; \
            VIDEOSINK_WAIT_FOR_STATE(videoSink, ::com::rdk::hal::State::READY, lastState); \
            UT_ASSERT_EQUAL(lastState, ::com::rdk::hal::State::READY); \
        } \
    } while (0)

#define VIDEOSINK_STOP_FAILURE(videoSinkController, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling stop()"); \
        status = videoSinkController->stop(); \
        UT_LOG_DEBUG("Result stop(), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define VIDEOSINK_FLUSH_SUCCESS(videoSink, videoSinkController, holdLastFrame) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling flush(IN: holdLastFrame[%s])", UT_Control_GetMapString(bool_mapTable, holdLastFrame)); \
        status = videoSinkController->flush(holdLastFrame); \
        UT_LOG_DEBUG("Result flush(IN: holdLastFrame[%s]), status:[isOK:%s, exception:%s]", UT_Control_GetMapString(bool_mapTable, holdLastFrame), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_FLUSH_FAILURE(videoSinkController, holdLastFrame, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling flush(IN: holdLastFrame[%s])", UT_Control_GetMapString(bool_mapTable, holdLastFrame)); \
        status = videoSinkController->flush(holdLastFrame); \
        UT_LOG_DEBUG("Result flush(IN: holdLastFrame[%s]), status:[isOK:%s, exception:%s]", UT_Control_GetMapString(bool_mapTable, holdLastFrame), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_SUCCESS(videoSinkController, nsPresentationTime) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling discardFramesUntil(IN:nsPresentationTime:[%lld])", nsPresentationTime);\
        status = videoSinkController->discardFramesUntil(nsPresentationTime);\
        UT_LOG_DEBUG("Result discardFramesUntil(IN:nsPresentationTime:[%lld]), status:[isOK:%s, exception:%s]",\
                    nsPresentationTime, UT_Control_GetMapString(bool_mapTable, status.isOk()),\
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode()));\
        UT_ASSERT_TRUE(status.isOk());\
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_SIGNAL_DISCARDFRAMESTILL_FAILURE(videoSinkController, nsPresentationTime, exception) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling discardFramesUntil(IN:nsPresentationTime:[%lld])", nsPresentationTime); \
        status = videoSinkController->discardFramesUntil(nsPresentationTime); \
        UT_LOG_DEBUG("Result discardFramesUntil(IN:nsPresentationTime:[%lld]), status:[isOK:%s, exception:%s]", \
                    nsPresentationTime, UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_FAILURE(videoSinkController, nsPresentationTime, frameBufferHandle, metadata, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return=false; \
        UT_LOG_DEBUG("Calling queueVideoFrame(IN:nsPresentationTime:[%lld], IN:frameBufferHandle:[0x%016" PRIx64 "], IN:metadata:[...])", nsPresentationTime, frameBufferHandle); \
        status = videoSinkController->queueVideoFrame(nsPresentationTime, frameBufferHandle, metadata, &_aidl_return); \
        UT_LOG_DEBUG("Result queueVideoFrame(IN:nsPresentationTime:[%lld], IN:frameBufferHandle:[0x%016" PRIx64 "], IN:metadata:[...]), status:[isOK:%s, exception:%s]", \
                    nsPresentationTime, frameBufferHandle,\
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
    } while (0)

#define VIDEOSINK_SIGNAL_QUEUEVIDEOFRAME_SUCCESS(videoSinkController, nsPresentationTime, frameBufferHandle, metadata) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        auto now = std::chrono::system_clock::now(); \
        auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now); \
        auto epoch = now_ms.time_since_epoch(); \
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count(); \
        \
        UT_LOG_DEBUG("Calling queueVideoFrame(IN:nsPresentationTime:[%lld ns], RealTime:[%lld ms], IN:frameBufferHandle:[0x%016" PRIx64 "], IN:metadata:[...])", \
                    nsPresentationTime, ms, frameBufferHandle); \
        \
        status = videoSinkController->queueVideoFrame(nsPresentationTime, frameBufferHandle, metadata, &_aidl_return); \
        \
        UT_LOG_DEBUG("Result queueVideoFrame(IN:nsPresentationTime:[%lld ns], RealTime:[%lld ms], IN:frameBufferHandle:[0x%016" PRIx64 "], IN:metadata:[...]), status:[isOK:%s, exception:%s]", \
                    nsPresentationTime, ms, frameBufferHandle, \
                    UT_Control_GetMapString(bool_mapTable, status.isOk()), \
                    UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(_aidl_return); \
        UT_ASSERT_TRUE(status.isOk()); \
    } while (0)


#define VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController, property, propertyValue) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return=false; \
        UT_LOG_DEBUG("Calling setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[])", com::rdk::hal::videosink::toString(property).c_str(), propertyValue.value); \
        status = videoSinkController->setProperty(property, propertyValue, &_aidl_return); \
        UT_LOG_DEBUG("Result setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", com::rdk::hal::videosink::toString(property).c_str(), propertyValue.value, \
            UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_TRUE(_aidl_return); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define VIDEOSINK_SETPROPERTY_FAILURE(videoSinkController, property, propertyValue, exception) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_DEBUG("Calling setProperty(IN:property[%s], IN:propertyValue[0x%X], OUT:_aidl_return[])", com::rdk::hal::videosink::toString(property).c_str(), propertyValue.value); \
        status = videoSinkController->setProperty(property, propertyValue, &_aidl_return); \
        UT_LOG_DEBUG("Result setProperty(IN:property[%s], IN:propertyValue[0x%X]), status:[isOK:%s, exception:%s]", com::rdk::hal::videosink::toString(property).c_str(), propertyValue.value, \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_EQUAL(status.exceptionCode(), exception); \
        UT_ASSERT_FALSE(status.isOk()); \
    } while (0)

#define VIDEOSINK_SETPROPERTY_LIST_SUCCESS(videoSinkController, properyList, type) \
    do { \
        for (const auto& [property, value] : properyList) \
        { \
            PropertyValue propertyValue; \
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::type>(value); \
            VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController, property, propertyValue); \
        } \
    } while (0)

#define VIDEOSINK_SETPROPERTY_STRING_LIST_SUCCESS(videoSinkController, properyList) \
    do { \
        for (const auto& [property, value] : properyList) \
        { \
            PropertyValue propertyValue; \
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::stringValue>(value); \
            VIDEOSINK_SETPROPERTY_SUCCESS(videoSinkController, property, propertyValue); \
        } \
    } while (0)

#define VIDEOSINK_SETPROPERTY_LIST_FAILURE(videoSinkController, properyList, type, exception) \
    do { \
        for (const auto& [property, value] : properyList) \
        { \
            PropertyValue propertyValue; \
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::type>(value); \
            VIDEOSINK_SETPROPERTY_FAILURE(videoSinkController, property, propertyValue, exception); \
        } \
    } while (0)

#define VIDEOSINK_SETPROPERTY_STRING_LIST_FAILURE(videoSinkController, properyList, exception) \
    do { \
        for (const auto& [property, value] : properyList) \
        { \
            PropertyValue propertyValue; \
            propertyValue.value = PropertyValue::Value::make<PropertyValue::Value::Tag::stringValue>(value); \
            VIDEOSINK_SETPROPERTY_FAILURE(videoSinkController, property, propertyValue, exception); \
        } \
    } while (0)

#define VIDEOSINK_QUEUEVIDEOFRAME_SUCCESS(videoSinkController, nsPresentationTime, frameBufferHandle, metadata, expectedReturn) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_DEBUG("Calling queueVideoFrame(IN:nsPresentationTime[%ld], IN:frameBufferHandle[0x%016" PRIx64 "], IN:metadata{width:%d, height:%d, format:%d}, OUT:_aidl_return[])", \
            nsPresentationTime, frameBufferHandle, metadata.width, metadata.height, metadata.format); \
        status = videoSinkController->queueVideoFrame(nsPresentationTime, frameBufferHandle, metadata, &_aidl_return); \
        UT_LOG_DEBUG("Result queueVideoFrame(..., OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(bool_mapTable, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable, status.isOk()), \
            UT_Control_GetMapString(statusException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_EQUAL(_aidl_return, expectedReturn); \
    } while (0)

struct VideoSinkData
{

};

struct DecodedVideoFrame
{

};

class VideoSinkControllerListener : public BnVideoSinkControllerListener
{
public:
    VideoSinkControllerListener();
    virtual ~VideoSinkControllerListener();

    // Override methods from BnVideoSinkControllerListener
    android::binder::Status onFirstFrameRendered(int64_t nsPresentationTime) override;
    android::binder::Status onFlushComplete() override;
    android::binder::Status onEndOfStream(int64_t nsPresentationTime) override;
    android::binder::Status onVideoUnderflow() override;
    android::binder::Status onVideoResumed() override;
    android::binder::Status onStateChanged(::com::rdk::hal::State oldState, ::com::rdk::hal::State newState) override;

    // Utility functions: Flush complete
    void resetFlushComplete();
    int getFlushCompleteCount();

    // Utility functions: End of stream (EOS)
    void resetEos();
    int64_t getEosPTS();
    bool waitForEosEvent(int timeoutSeconds);

    // Utility functions: Underflow
    void resetUnderflow();
    int getUnderflowCount();

    // Utility functions: Resume
    void resetResumed();
    int getResumedCount();

    void setSinkID(int32_t id);

private:
    mutable std::mutex mMutex;
    std::condition_variable mCV;

    // Current state and history
    ::com::rdk::hal::State mCurrentState;
    std::vector<std::pair<::com::rdk::hal::State, ::com::rdk::hal::State>> mStateChanges;

    // First frame rendered info
    bool mFirstFrameRendered;
    int64_t mFirstPTS;

    // Flush complete flag
    bool mFlushComplete;
    int mFlushCompleteCount = 0;

    // EOS info
    bool mEosReceived;
    int64_t mEosPTS;

    // Counters for events
    int mUnderflowCount = 0;
    int mResumeCount = 0;

    // Sink ID
    int32_t mSinkID;
};


class VideoSinkEventListener : public BnVideoSinkEventListener
{
public:
    VideoSinkEventListener();
    virtual ~VideoSinkEventListener() = default;
    static int32_t mUsedDecoderID;

    // Override methods from BnVideoSinkEventListener
    ::android::binder::Status onFirstFrameRendered(int64_t nsPresentationTime) override;
    ::android::binder::Status onEndOfStream(int64_t nsPresentationTime) override;
    ::android::binder::Status onVideoUnderflow() override;
    ::android::binder::Status onVideoResumed() override;
    ::android::binder::Status onFlushComplete() override;
    ::android::binder::Status onStateChanged(::com::rdk::hal::State oldState, ::com::rdk::hal::State newState) override;

    // Flush helpers
    void resetFlushComplete();
    int getFlushCompleteCount();

    // End of stream (EOS) helpers
    void resetEos();
    int64_t getEosPTS();
    bool waitForEosEvent(int timeoutSeconds);

    // Underflow helpers
    void resetUnderflow();
    int getUnderflowCount();

    // Resume helpers
    void resetResumed();
    int getResumedCount();

    // State transition helpers
    std::vector<std::pair<::com::rdk::hal::State, ::com::rdk::hal::State>> getStateChanges() const;
    void resetStateChanges();
    bool checkStateTransistion(::com::rdk::hal::State expOldState, ::com::rdk::hal::State expNewState);

    // Wait functions (blocking with timeout)
    bool waitForState(::com::rdk::hal::State desiredState, std::chrono::milliseconds timeout);

    // Error helpers
    std::vector<int32_t> getErrors() const;

    void setSinkID(int32_t id);

    static int32_t getUsedDecoder()
    {
        return VideoSinkEventListener::mUsedDecoderID;
    }
private:
    mutable std::mutex mMutex;
    std::condition_variable mCV;

    // Frame info
    bool mFirstFrameRendered;
    int64_t mFirstPTS;

    // State info
    ::com::rdk::hal::State mCurrentState;
    std::vector<std::pair<::com::rdk::hal::State, ::com::rdk::hal::State>> mStateChanges;

    // Errors
    std::vector<int32_t> mErrors;

    // EOS info
    bool mEosReceived;
    int64_t mEosPTS;

    // Event counters
    int mUnderflowCount = 0;
    int mResumeCount = 0;

    // Flush flag
    bool mFlushComplete;
    int mFlushCompleteCount = 0;

    // Sink ID
    int32_t mSinkID;
};
//sink test implementation.
class SinkTest
{
public:
    SinkTest(IVideoSink::Id sinkId,
        sp<IVideoSink> sinkHandle,
        sp<VideoSinkControllerListener> VideoSinkControllerListener,
        sp<VideoSinkEventListener> VideoSinkEventListner,
        sp<VideoDecoderControllerListener> decoderControllerListener,
        videoDecoderTestConfiguration_t& config,
        videoTestConfiguration_t& sinkConfig);

    void verifyNoUnexpectedUnderflow();
    void verifyUnderflow();
    void verifyResume();
    void verifyQueueDepth();
    void verifyFramesReceived();
    void verifyFramesDroppedLate();
    void verifyFramesDroppedFRC();
    void verifyFramesRepeatedFRC();
    void verifyFramesMissed();
    void verifyFlushComplete();
    void handleEosFrame(const videodecoder::FrameMetadata& metadata, int64_t pts);
    void runPostVerification();
    void run ( bool isTunnel );
    bool getSinkPropertyInt(videosink::Property property, int32_t& outValue);

private:
    IVideoSink::Id mSinkId;
    sp<IVideoSink> mSinkHandle;
    sp<VideoSinkControllerListener> mVideoSinkControlListener;
    sp<VideoSinkEventListener> mVideoSinkEventListner;
    sp<VideoDecoderControllerListener> mDecoderControllerListener;
    videoDecoderTestConfiguration_t mTestConfiguration;
    videoTestConfiguration_t mSinkTestConfiguration;
    std::set<int64_t> receivedPtsSet;
    int actualFramesReceived = 0;

    float getBytesPerPixelMultiplier(const std::string& basePixelFormat, int colorDepth) const;
    PixelFormat getPixelFormatEnum(const std::string& basePixelFormat);
    void videoSinkNonTunnelMode();
    void videoSinkTunnelMode();

};
#endif //__VTS_VIDEOSINK_H__

/** @} */ // End of VIDEO_SINK_HALTEST_HEADER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
