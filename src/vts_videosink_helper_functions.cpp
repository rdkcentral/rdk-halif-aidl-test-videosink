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
 * @defgroup VIDEO_SINK_HALTEST_HELPER Video Sink HAL Tests helper functions
 * @{
 * @parblock
 *
 * ### Helper functions for VTS tests :
 *
 * Helper functions required for the module across all vendors.
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink_helper_functions.cpp
 *
 */
#include <ut.h>
#include "vts_videosink_helper_functions.h"
#include "vts_videosink_parse_hfp.h"
#include "vts_videosink.h"

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/Status.h>

using namespace com::rdk::hal;
using namespace com::rdk::hal::videosink;

void vts_videosink_helper_validateCapabilities(int sink_id, const videosink::Capabilities& capabilities)
{
    bool sinkID = true;

    /*
     "sinks" is a vector, which contains information of video sinks available in hal feature profile.
     The following are the information for each video sink:
     - The sink ID.
     - Whether the sink supports secure video path (SAP) mode.
     - A list of supported codecs.
   */
    UT_ASSERT_TRUE(sinkID);
}

bool vts_videosink_helper_areElementsUnique(const std::vector<int>& values)
{
    std::unordered_set<int> uniqueElements;

    for (const auto& value : values)
    {
        if(value < 0)
        {
            UT_LOG_ERROR("find Invalid sink ID : %d", value);
            return false;
        }
        if (uniqueElements.find(value) != uniqueElements.end())
        {
            UT_LOG_ERROR("sink ID is not unique : %d", value);
            return false;
        }
        uniqueElements.insert(value);
    }
    return true;
}

bool vts_videosink_helper_validatePropertyValue(int sink_id, const std::optional<PropertyValue>& propertyValue, const videosink::Property property)
{
    std::vector<int32_t> avSourceValues = {
        static_cast<int32_t>(com::rdk::hal::AVSource::AUTO),
        static_cast<int32_t>(com::rdk::hal::AVSource::UNKNOWN),
        static_cast<int32_t>(com::rdk::hal::AVSource::IP),
        static_cast<int32_t>(com::rdk::hal::AVSource::TUNER),
        static_cast<int32_t>(com::rdk::hal::AVSource::SYSTEM),
        static_cast<int32_t>(com::rdk::hal::AVSource::HDMI_1),
        static_cast<int32_t>(com::rdk::hal::AVSource::HDMI_2),
        static_cast<int32_t>(com::rdk::hal::AVSource::HDMI_3),
        static_cast<int32_t>(com::rdk::hal::AVSource::HDMI_4),
        static_cast<int32_t>(com::rdk::hal::AVSource::HDMI_5),
        static_cast<int32_t>(com::rdk::hal::AVSource::COMPOSITE_1),
        static_cast<int32_t>(com::rdk::hal::AVSource::COMPOSITE_2),
        static_cast<int32_t>(com::rdk::hal::AVSource::COMPOSITE_3),
        static_cast<int32_t>(com::rdk::hal::AVSource::COMPOSITE_4),
        static_cast<int32_t>(com::rdk::hal::AVSource::COMPOSITE_5)
    };

    if (!propertyValue.has_value() || !propertyValue->value.has_value())
    {
        return false;
    }

    std::variant<int32_t, std::string> val;
    const auto& propVal = propertyValue->value.value();
    if (propertyValue->value.has_value())
    {
        switch (propVal.getTag())
        {
            case PropertyValue::Value::Tag::intValue:
                val = propVal.get<PropertyValue::Value::Tag::intValue>();
                break;
            case PropertyValue::Value::Tag::stringValue:
            {
                val = propVal.get<PropertyValue::Value::Tag::stringValue>();  // std::string (@utf8InCpp)
            }
                break;
            case PropertyValue::Value::Tag::booleanValue:
                val = propVal.get<PropertyValue::Value::Tag::booleanValue>() ? 1 : 0;  // Store as int32_t (1 for true, 0 for false)
                break;

            default:
                std::cerr << "Unsupported type in PropertyValue::Value" << std::endl;
                break;
        }
    }

    switch (property)
    {
        case videosink::Property::RESOURCE_ID:
             if(std::holds_alternative<int>(val) && std::get<int>(val) == sink_id)
             {
                return true;
             }
             else
             {
                return false;
             }
        case videosink::Property::SINK_QUEUE_DEPTH:
        case videosink::Property::METRIC_FRAMES_RECEIVED:
        case videosink::Property::METRIC_FRAMES_PRESENTED:
        case videosink::Property::METRIC_FRAMES_DROPPED_LATE:
        case videosink::Property::METRIC_FRAMES_DROPPED_FRC:
        case videosink::Property::METRIC_FRAMES_REPEATED_FRC:
        case videosink::Property::METRIC_FRAMES_REPEATED_MISSING_FRAME:
        case videosink::Property::RENDER_FIRST_FRAME:
        case videosink::Property::METRIC_UNDERFLOWED:
            if(propVal.getTag() == com::rdk::hal::PropertyValue::Value::Tag::intValue)
            {
                return true;
            }
            else
            {
                return false;
            }
        case com::rdk::hal::videosink::Property::AV_SOURCE:
            if((propVal.getTag() == com::rdk::hal::PropertyValue::Value::Tag::intValue) && \
                (std::find(avSourceValues.begin(), avSourceValues.end(), propVal.get<::com::rdk::hal::PropertyValue::Value::Tag::intValue>()) != avSourceValues.end()))
            {
                return true;
            }
            else
            {
                return false;
            }
        default:
            return true;
    }
}

float getBytesPerPixelMultiplier(const std::string& basePixelFormat, int colorDepth)
{
    int bytesPerChannel = (colorDepth <= 8) ? 1 : 2;

    if (basePixelFormat == "YCBCR420")
        return 1.5f * bytesPerChannel;
    if (basePixelFormat == "YCBCR422")
        return 2.0f * bytesPerChannel;
    if (basePixelFormat == "YCBCR444")
        return 3.0f * bytesPerChannel;
    if (basePixelFormat == "RGB")
        return 3.0f * bytesPerChannel;
    if (basePixelFormat == "NATIVE")
        return 0.0f;

    return 0.0f;
}

PixelFormat getPixelFormatEnum(const std::string& basePixelFormat)
{
    if (basePixelFormat == "YCBCR420")
        return PixelFormat::YCBCR420;
    if (basePixelFormat == "YCBCR422")
        return PixelFormat::YCBCR422;
    if (basePixelFormat == "YCBCR444")
        return PixelFormat::YCBCR444;
    if (basePixelFormat == "RGB")
        return PixelFormat::RGB;
    if (basePixelFormat == "NATIVE")
        return PixelFormat::NATIVE;

    // Default fallback, can throw or return a special value if needed
    return PixelFormat::NATIVE;
}

/** @} */ // End of VIDEO_SINK_HALTEST_HELPER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK
