/**
 *  If not stated otherwise in this file or this component's LICENSE
 *  file the following copyright and licenses apply:
 *
 *  Copyright 2024 RDK Management
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
 * @file vts_videosink.cpp
 *
 */

#include <fstream>
#include <ut.h>
#include <ut_log.h>
#include <ut_kvp_profile.h>
#include <ut_control_plane.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/Status.h>
#include <sys/syscall.h>
#include <utils/String16.h>
#include <stdio.h>
#include <vector>
#include <optional>
#include <algorithm>
#include <unordered_set>
#include <tuple>
#include <map>
#include <chrono>


#include "com/rdk/hal/avbuffer/IAVBuffer.h"
#include "com/rdk/hal/avbuffer/BnAVBufferSpaceListener.h"

#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink.h"


#include "avbufferhelper.h"

using namespace std::chrono;
using namespace std;

/*************************************
* Controller Listener Implementation
*************************************/
VideoSinkControllerListener::VideoSinkControllerListener()
    :mFirstFrameRendered(false),
     mFlushComplete(false),
     mEosReceived(false),
     mFrameBufferAvailable(false),
     mFrameBufferAvailableCount(0),
     mUnderflowCount(0),
     mResumeCount(0)
{
    mFirstPTS.clear();
    mEosPTS.clear();
}

VideoSinkControllerListener::~VideoSinkControllerListener() = default;

/**
 * @brief Sets the Sink ID for this listener instance.
 *
 * @param id The sink identifier to set.
 */
void VideoSinkControllerListener::setSinkID(int32_t id)
{
    mSinkID = id;
}

// onFirstFrameRendered callback implementation
/**
 * @brief Callback invoked when the first video frame has been rendered.
 *
 * This function is triggered once the video sink renders its first frame. It records
 * the presentation timestamp (PTS) and signals any waiting test logic.
 *
 * @param nsPresentationTime Presentation timestamp (in nanoseconds) of the first rendered frame.
 * @return android::binder::Status Always returns Status::ok().
 */
android::binder::Status VideoSinkControllerListener::onFirstFrameRendered(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[Video SinkID: %d] [PTS %lld]  VideoSinkControllerListener : onFirstFrameRendered Callback Received", mSinkID, nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mMutex_ffr);
        if (!mFirstFrameRendered)
        {
            mFirstPTS.push_back(nsPresentationTime);
            mFirstFrameRendered = true;
        }
    }
    mCV_ffr.notify_all();
    return android::binder::Status::ok();
}

void VideoSinkControllerListener::resetFirstFrameRendered()
{
    std::lock_guard<std::mutex> lock(mMutex_ffr);
    mFirstFrameRendered = false;
    mFirstPTS.clear();
}

bool VideoSinkControllerListener::waitForFFR(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mMutex_ffr);
    return mCV_ffr.wait_for(lock, timeout, [this] { return mFirstFrameRendered; });
}

std::vector<int64_t> VideoSinkControllerListener::getFirstPTS()
{
    std::lock_guard<std::mutex> lock(mMutex_ffr);
    return mFirstPTS;
}

// onEndOfStream callback implementation
/**
 * @brief Callback invoked when End of Stream (EOS) is received.
 *
 * Records the EOS flag and presentation timestamp (PTS), then notifies waiting threads.
 *
 * @param nsPresentationTime Presentation timestamp of the EOS event in nanoseconds.
 * @return android::binder::Status::ok() on success.
 */
android::binder::Status VideoSinkControllerListener::onEndOfStream(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkControllerListener: onEndOfStream Received PTS[%ld]",mSinkID,nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mMutex_eos);
        mEosReceived = true;
        mEosPTS.push_back(nsPresentationTime);
    }
    mCV_eos.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Resets the End of Stream (EOS) received status and associated PTS.
 *
 * After calling this, EOS is considered not received and PTS is reset.
 */
void VideoSinkControllerListener::resetEos()
{
    std::lock_guard<std::mutex> lock(mMutex_eos);
    mEosReceived = false;
    mEosPTS.clear();
}

/**
 * @brief Waits for the End-Of-Stream (EOS) event to be received within the specified timeout.
 *
 * Blocks the calling thread until the EOS event is received or the timeout expires.
 *
 * @param timeout Duration to wait for the EOS event.
 * @param expectedCount Number of EOS events required to complete the wait.
 * @return true if EOS event received within timeout, false otherwise.
 */
bool VideoSinkControllerListener::waitForEosEvent(std::chrono::milliseconds timeout, size_t expectedCount)
{
    std::unique_lock<std::mutex> lock(mMutex_eos);
    return mCV_eos.wait_for(lock, timeout, [this, expectedCount] { return mEosPTS.size() >= expectedCount; });
}

/**
 * @brief Retrieves the presentation timestamp (PTS) associated with the received End of Stream (EOS).
 *
 * @return The EOS PTS value.
 */
std::vector<int64_t> VideoSinkControllerListener::getEosPTS()
{
    std::lock_guard<std::mutex> lock(mMutex_eos);
    return mEosPTS;
}

// onVideoUnderflow callback implementation
/**
 * @brief Callback invoked when a video underflow event occurs.
 *
 * Increments the underflow count and notifies any waiting threads.
 *
 * @return android::binder::Status indicating success.
 */
android::binder::Status VideoSinkControllerListener::onVideoUnderflow()
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkControllerListener: onVideoUnderflow Callback Received",mSinkID);
    {
        std::lock_guard<std::mutex> lock(mMutex_underflow);
        mUnderflowCount++;
    }
    mCV_underflow.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Resets the video underflow count to zero.
 *
 * This method clears the recorded underflow events.
 */
void VideoSinkControllerListener::resetUnderflow()
{
    std::lock_guard<std::mutex> lock(mMutex_underflow);
    mUnderflowCount = 0;
}

/**
 * @brief Retrieves the current count of video underflow events.
 *
 * @return int The number of underflow events recorded.
 */
int VideoSinkControllerListener::getUnderflowCount()
{
    std::lock_guard<std::mutex> lock(mMutex_underflow);
    return mUnderflowCount;
}

// onVideoResumed callback implementation
/**
 * @brief Callback invoked when video resumes after underflow.
 *
 * Increments the resume event count and notifies waiting threads.
 *
 * @return android::binder::Status Status indicating success.
 */
android::binder::Status VideoSinkControllerListener::onVideoResumed()
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkControllerListener: onVideoResumed Callback Received",mSinkID);
    {
        std::lock_guard<std::mutex> lock(mMutex_resumed);
        mResumeCount++;
    }
    mCV_resumed.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Resets the count of video resumed events to zero.
 *
 * Thread-safe reset of the internal resume event counter.
 */
void VideoSinkControllerListener::resetResumed()
{
    std::lock_guard<std::mutex> lock(mMutex_resumed);
    mResumeCount = 0;
}

/**
 * @brief Returns the count of video resumed events received.
 *
 * Thread-safe retrieval of the resume event counter.
 *
 * @return Number of video resumed events received.
 */
int VideoSinkControllerListener::getResumedCount()
{
    std::lock_guard<std::mutex> lock(mMutex_resumed);
    return mResumeCount;
}

// onStateChanged callback implementation
/**
 * @brief Callback invoked when a flush operation is complete.
 *
 * Sets the flush complete flag and notifies waiting threads.
 *
 * @return android::binder::Status::ok() on success.
 */
android::binder::Status VideoSinkControllerListener::onFlushComplete()
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkEventListener: onFlushComplete callback received ", mSinkID);
    {
        std::lock_guard<std::mutex> lock(mMutex_flush);
        mFlushCompleteCount++;   // increment the count
        mFlushComplete = true;   // optionally keep this flag if needed
    }
    mCV_flush.notify_all();
    return ::android::binder::Status::ok();
}

/**
 * @brief Retrieves the number of flush complete events received.
 *
 * This method returns the internal counter indicating how many times the
 * flush complete callback (`onFlushComplete()`) was invoked on the video sink
 * control listener. It uses a mutex to ensure thread-safe access.
 *
 * @return The number of flush complete events observed so far.
 */
int VideoSinkControllerListener::getFlushCompleteCount()
{
    std::lock_guard<std::mutex> lock(mMutex_flush);
    return mFlushCompleteCount;
}

/**
 * @brief Resets the flush complete flag to false.
 */
void VideoSinkControllerListener::resetFlushComplete()
{
    std::lock_guard<std::mutex> lock(mMutex_flush);
    mFlushComplete = false;
    mFlushCompleteCount = 0;
}

// onStateChanged callback implementation
/**
 * @brief Callback for state changes in the video sink controller.
 *
 * This implementation currently does nothing and always returns success.
 *
 * @param oldState Previous state
 * @param newState New state
 * @return android::binder::Status Always returns Status::ok()
 */
android::binder::Status VideoSinkControllerListener::onStateChanged(videosink::State oldState, videosink::State newState)
{
    UT_LOG_INFO("[SinkId %d] VideoSinkControllerListener: State Change received: %s -> %s ", mSinkID, videosink::toString(oldState).c_str(), videosink::toString(newState).c_str());
    {
        std::lock_guard<std::mutex> lock(mMutex_stateChanges);
        mStateChanges.emplace_back(oldState, newState);
        mCurrentState = newState;
    }
    mCV_stateChanges.notify_all();
    return ::android::binder::Status::ok();
}

/**
 * @brief Waits for the listener to reach the specified state within the given timeout.
 *        Blocks using condition variable until the desired state is reached or timeout expires.
 *
 * @param desiredState The state to wait for.
 * @param timeout Duration to wait for the desired state.
 * @return true if the desired state was reached before timeout, false otherwise.
 */
bool VideoSinkControllerListener::waitForState(videosink::State desiredState, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mMutex_stateChanges);
    return mCV_stateChanges.wait_for(lock, timeout, [this, desiredState] { return mCurrentState == desiredState; });
}

/**
 * @brief Retrieves a copy of all recorded state changes.
 *
 * @return A vector of pairs representing (oldState, newState) transitions.
 */
std::vector<std::pair<videosink::State, videosink::State>> VideoSinkControllerListener::getStateChanges() const
{
    std::lock_guard<std::mutex> lock(mMutex_stateChanges);
    return mStateChanges;
}

/**
 * @brief Clears all recorded state change entries.
 */
void VideoSinkControllerListener::resetStateChanges()
{
    std::lock_guard<std::mutex> lock(mMutex_stateChanges);
    mStateChanges.clear();
}

/**
 * @brief Checks if a specific state transition occurred.
 *
 * @param expOldState Expected old state.
 * @param expNewState Expected new state.
 * @return true if the transition from expOldState to expNewState was recorded, false otherwise.
 */
bool VideoSinkControllerListener::checkStateTransistion(videosink::State expOldState, videosink::State expNewState)
{
    std::vector<std::pair<videosink::State, videosink::State>> stateChange;
    {
        std::lock_guard<std::mutex> lock(mMutex_stateChanges);
        stateChange = mStateChanges;
    }
    for (const auto& [oldState, newState] : stateChange)
    {
        if (oldState == expOldState && newState == expNewState)
        {
            return true;
        }
    }
    return false;
}

// onFrameBufferAvailable callback implementation
/**
 * @brief Callback invoked when the sink queue has space after back-pressure.
 *
 * Increments frame-buffer-available count and notifies waiting threads.
 *
 * @return android::binder::Status indicating success.
 */
android::binder::Status VideoSinkControllerListener::onFrameBufferAvailable()
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkControllerListener: onFrameBufferAvailable Callback Received", mSinkID);
    {
        std::lock_guard<std::mutex> lock(mMutex_fba);
        mFrameBufferAvailable = true;
        mFrameBufferAvailableCount++;
    }
    mCV_fba.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Resets frame-buffer-available callback state.
 */
void VideoSinkControllerListener::resetFrameBufferAvailable()
{
    std::lock_guard<std::mutex> lock(mMutex_fba);
    mFrameBufferAvailable = false;
    mFrameBufferAvailableCount = 0;
}

/**
 * @brief Returns the number of onFrameBufferAvailable callbacks received.
 *
 * @return int frame-buffer-available callback count.
 */
int VideoSinkControllerListener::getFrameBufferAvailableCount()
{
    std::lock_guard<std::mutex> lock(mMutex_fba);
    return mFrameBufferAvailableCount;
}

/**
 * @brief Waits for onFrameBufferAvailable callback for up to timeoutSeconds.
 *
 * @param timeoutSeconds Maximum wait in seconds.
 * @return true if callback was observed, false otherwise.
 */
bool VideoSinkControllerListener::waitForFrameBufferAvailableEvent(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mMutex_fba);
    const bool callbackReceived = mCV_fba.wait_for(
        lock, timeout, [this] { return mFrameBufferAvailable; });
    if (callbackReceived)
    {
        mFrameBufferAvailable = false;
    }
    return callbackReceived;
}


/*************************************
* Event Listener Implementation
*************************************/
VideoSinkEventListener::VideoSinkEventListener()
        :mFirstFrameRendered(false),
         mFlushComplete(false),
         mEosReceived(false),
         mFrameBufferAvailable(false),
         mFrameBufferAvailableCount(0),
         mUnderflowCount(0),
         mResumeCount(0)
{
    mEosPTS.clear();
    mFirstPTS.clear();
}

/**
 * @brief Sets the Sink ID.
 *
 * @param id The Sink ID to set.
 */
void VideoSinkEventListener::setSinkID(int32_t id)
{
    mSinkID = id;
}

VideoSinkEventListener::~VideoSinkEventListener() = default;

/**
 * @brief Callback invoked when the first video frame is rendered.
 *
 * @param nsPresentationTime Presentation timestamp (in nanoseconds) of the first rendered frame.
 * @return android::binder::Status Indicates success of the callback handling.
 */
::android::binder::Status VideoSinkEventListener::onFirstFrameRendered(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[Video SinkID: %d] [PTS %lld] VideoSinkEventListener: onFirstFrameRendered Callback Received", mSinkID, nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mMutex_ffr);
        if (!mFirstFrameRendered)
        {
            mFirstPTS.push_back(nsPresentationTime);
            mFirstFrameRendered = true;
        }
    }
    mCV_ffr.notify_all();
    return android::binder::Status::ok();
}

void VideoSinkEventListener::resetFirstFrameRendered()
{
    std::lock_guard<std::mutex> lock(mMutex_ffr);
    mFirstFrameRendered = false;
    mFirstPTS.clear();
}

bool VideoSinkEventListener::waitForFFR(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mMutex_ffr);
    return mCV_ffr.wait_for(lock, timeout, [this] { return mFirstFrameRendered; });
}

std::vector<int64_t> VideoSinkEventListener::getFirstPTS()
{
    std::lock_guard<std::mutex> lock(mMutex_ffr);
    return mFirstPTS;
}

/**
 * @brief Callback invoked when End-Of-Stream (EOS) is received.
 *
 * @param nsPresentationTime Presentation timestamp (in nanoseconds) of the EOS event.
 * @return android::binder::Status Status of the callback execution.
 */
::android::binder::Status VideoSinkEventListener::onEndOfStream(int64_t nsPresentationTime)
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkEventListener: onEndOfStream Received PTS[%ld]",mSinkID,nsPresentationTime);
    {
        std::lock_guard<std::mutex> lock(mMutex_eos);
        mEosReceived = true;
        mEosPTS.push_back(nsPresentationTime);
    }
    mCV_eos.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Retrieves the Presentation Timestamp (PTS) of the End-Of-Stream (EOS) event.
 *
 * @return std::vector<int64_t> The EOS PTS values.
 */
std::vector<int64_t> VideoSinkEventListener::getEosPTS()
{
    std::lock_guard<std::mutex> lock(mMutex_eos);
    return mEosPTS;
}

/**
 * @brief Resets the End-Of-Stream (EOS) status and PTS.
 */
void VideoSinkEventListener::resetEos()
{
    std::lock_guard<std::mutex> lock(mMutex_eos);
    mEosReceived = false;
    mEosPTS.clear();
}

bool VideoSinkEventListener::waitForEosEvent(std::chrono::milliseconds timeout, size_t expectedCount)
{
    std::unique_lock<std::mutex> lock(mMutex_eos);
    return mCV_eos.wait_for(lock, timeout, [this, expectedCount] { return mEosPTS.size() >= expectedCount; });
}

/**
 * @brief Retrieves the count of video underflow events received.
 *
 * @return int Number of underflow events.
 */
::android::binder::Status VideoSinkEventListener::onVideoUnderflow()
{
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = now_ms.time_since_epoch();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

    UT_LOG_INFO("[Sink ID : %d] VideoSinkEventListener: onVideoUnderflow Callback Received. RealTime:[%lld ms]",mSinkID, ms);
    {
        std::lock_guard<std::mutex> lock(mMutex_underflow);
        mUnderflowCount++;
    }
    mCV_underflow.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Waits for the End-Of-Stream (EOS) event to be received or until timeout.
 *
 * @param timeoutSeconds Timeout duration in seconds to wait for the EOS event.
 * @return bool True if EOS event was received within the timeout, false otherwise.
 */
void VideoSinkEventListener::resetUnderflow()
{
    std::lock_guard<std::mutex> lock(mMutex_underflow);
    mUnderflowCount = 0;
}

int VideoSinkEventListener::getUnderflowCount()
{
    std::lock_guard<std::mutex> lock(mMutex_underflow);
    return mUnderflowCount;
}

/**
 * @brief Callback invoked when video resumes after underflow.
 *        Increments the resume event count and notifies waiting threads.
 *
 * @return android::binder::Status indicating success.
 */
::android::binder::Status VideoSinkEventListener::onVideoResumed()
{
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = now_ms.time_since_epoch();
    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

    UT_LOG_INFO("[Sink ID : %d] VideoSinkEventListener: onVideoResumed Callback Received. RealTime:[%lld ms]",mSinkID, ms);
    {
        std::lock_guard<std::mutex> lock(mMutex_resumed);
        mResumeCount++;
    }
    mCV_resumed.notify_all();
    return android::binder::Status::ok();
}

/**
 * @brief Resets the count of video resumed events.
 */
void VideoSinkEventListener::resetResumed()
{
    std::lock_guard<std::mutex> lock(mMutex_resumed);
    mResumeCount = 0;
}

/**
 * @brief Retrieves the count of video resumed events received.
 * @return Number of times the video resumed event was received.
 */
int VideoSinkEventListener::getResumedCount()
{
    std::lock_guard<std::mutex> lock(mMutex_resumed);
    return mResumeCount;
}

/**
 * @brief Callback invoked when a flush operation is complete.
 *        Sets the flush complete flag and notifies waiting threads.
 *
 * @return ::android::binder::Status indicating success.
 */
::android::binder::Status VideoSinkEventListener::onFlushComplete()
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkEventListener: onFlushComplete callback received ", mSinkID);
    {
        std::lock_guard<std::mutex> lock(mMutex_flush);
        mFlushCompleteCount++;   // increment the count
        mFlushComplete = true;   // optionally keep this flag if needed
    }
    mCV_flush.notify_all();
    return ::android::binder::Status::ok();
}

/**
 * @brief Retrieves the number of flush complete events received.
 *
 * This method returns the internal counter indicating how many times the
 * flush complete callback (`onFlushComplete()`) was invoked on the video sink
 * control listener. It uses a mutex to ensure thread-safe access.
 *
 * @return The number of flush complete events observed so far.
 */
int VideoSinkEventListener::getFlushCompleteCount()
{
    std::lock_guard<std::mutex> lock(mMutex_flush);
    return mFlushCompleteCount;
}

/**
 * @brief Resets the flush complete status flag to false.
 *        Thread-safe operation with mutex locking.
 */
void VideoSinkEventListener::resetFlushComplete()
{
    std::lock_guard<std::mutex> lock(mMutex_flush);
    mFlushComplete = false;
    mFlushCompleteCount = 0;
}

/**
 * @brief Callback invoked when the sink state changes.
 *        Logs the state change, records the transition, updates current state,
 *        and notifies any waiting threads.
 *
 * @param oldState Previous state of the sink.
 * @param newState New state of the sink.
 * @return ::android::binder::Status indicating success.
 */
::android::binder::Status VideoSinkEventListener::onStateChanged(videosink::State oldState, videosink::State newState)
{
    UT_LOG_INFO("[SinkId %d] VideoSinkEventListener: State Change received: %s -> %s ", mSinkID, videosink::toString(oldState).c_str(), videosink::toString(newState).c_str());
    {
        std::lock_guard<std::mutex> lock(mMutex_stateChanges);
        mStateChanges.emplace_back(oldState, newState);
        mCurrentState = newState;
    }
    mCV_stateChanges.notify_all();
    return ::android::binder::Status::ok();
}

/**
 * @brief Waits for the listener to reach the specified state within the given timeout.
 *        Blocks using condition variable until the desired state is reached or timeout expires.
 *
 * @param desiredState The state to wait for.
 * @param timeout Duration to wait for the desired state.
 * @return true if the desired state was reached before timeout, false otherwise.
 */
bool VideoSinkEventListener::waitForState(videosink::State desiredState, std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mMutex_stateChanges);
    return mCV_stateChanges.wait_for(lock, timeout, [this, desiredState] { return mCurrentState == desiredState; });
}

/**
 * @brief Retrieves a copy of all recorded state changes.
 *
 * @return A vector of pairs representing (oldState, newState) transitions.
 */
std::vector<std::pair<videosink::State, videosink::State>> VideoSinkEventListener::getStateChanges() const
{
    std::lock_guard<std::mutex> lock(mMutex_stateChanges);
    return mStateChanges;
}

/**
 * @brief Clears all recorded state change entries.
 */
void VideoSinkEventListener::resetStateChanges()
{
    std::lock_guard<std::mutex> lock(mMutex_stateChanges);
    mStateChanges.clear();
}

/**
 * @brief Checks if a specific state transition occurred.
 *
 * @param expOldState Expected old state.
 * @param expNewState Expected new state.
 * @return true if the transition from expOldState to expNewState was recorded, false otherwise.
 */
bool VideoSinkEventListener::checkStateTransistion(videosink::State expOldState, videosink::State expNewState)
{
    std::vector<std::pair<videosink::State, videosink::State>> stateChange;
    {
        std::lock_guard<std::mutex> lock(mMutex_stateChanges);
        stateChange = mStateChanges;
    }
    for (const auto& [oldState, newState] : stateChange)
    {
        if (oldState == expOldState && newState == expNewState)
        {
            return true;
        }
    }
    return false;
}

android::binder::Status VideoSinkEventListener::onFrameBufferAvailable()
{
    UT_LOG_INFO("[Sink ID : %d] VideoSinkEventListener: onFrameBufferAvailable Callback Received", mSinkID);
    {
        std::lock_guard<std::mutex> lock(mMutex_fba);
        mFrameBufferAvailable = true;
        mFrameBufferAvailableCount++;
    }
    mCV_fba.notify_all();
    return android::binder::Status::ok();
}

void VideoSinkEventListener::resetFrameBufferAvailable()
{
    std::lock_guard<std::mutex> lock(mMutex_fba);
    mFrameBufferAvailable = false;
    mFrameBufferAvailableCount = 0;
}

int VideoSinkEventListener::getFrameBufferAvailableCount()
{
    std::lock_guard<std::mutex> lock(mMutex_fba);
    return mFrameBufferAvailableCount;
}

bool VideoSinkEventListener::waitForFrameBufferAvailableEvent(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(mMutex_fba);
    const bool callbackReceived = mCV_fba.wait_for(
        lock, timeout, [this] { return mFrameBufferAvailable; });
    if (callbackReceived)
    {
        mFrameBufferAvailable = false;
    }
    return callbackReceived;
}

/** @} */ // End of VIDEO_SINK_HALTEST_HEADER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK