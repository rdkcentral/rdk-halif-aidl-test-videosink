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
 * @addtogroup VIDEO_SINK Video sink Module
 * @{
 */

/**
 * @addtogroup VIDEO_SINK_HALTEST Video sink HAL Tests
 * @{
 */

/**
 * @defgroup VIDEO_SINK_HALTEST_PARSE_HFP_HEADER Video sink HAL Tests parse Hardware feature profile Header File
 * @{
 * @parblock
 *
 * ### Parse HFP functions for Video sink :
 *
 * Parse HFP functions required for the module across all vendors.
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 *
 */

/**
 * @file vts_videosink_parse_hfp.h
 *
 */

#ifndef __VTS_VIDEOSINK_PARSE_HFP_H__
#define __VTS_VIDEOSINK_PARSE_HFP_H__

#include <stdio.h>
#include <vector>

#include <ut.h>
#include <ut_kvp_profile.h>
#include <ut_log.h>

extern std::vector<int32_t> gVideosinks_hfp;
extern std::vector<int32_t> gAvailableVideosinks_hfp;
extern std::vector<int32_t> gAvailableVideodecoders_hfp;

#define VIDEOSINKMGR_IVIDEOSINK "videosink.IVideoSink"
#define VIDEOSINKMGR_SINK_AVAILABLE "videosink.availableSinks"
#define VIDEOSINKMGR_IVIDEODECODER "videosink.IVideoDecoder"

UT_status_t vts_videosink_parseVideoSinkHFP();

#endif //__VTS_VIDEOSINK_PARSE_HFP_H__

/** @} */ // End of VIDEO_SINK_HALTEST_PARSE_HFP_HEADER
/** @} */ // End of VIDEO_SINK_HALTEST
/** @} */ // End of VIDEO_SINK
/** @} */ // End of HPK