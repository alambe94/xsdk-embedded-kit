// Copyright 2022 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef XUSB_UVC_DEFS_H
#define XUSB_UVC_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/* USB UVC specific descriptor length */
#define USB_UVC_VC_IF_HEADER_DESC_LEN            0x0DU
#define USB_UVC_INPUT_TERMINAL_DESC_SIZE         0x12U
#define USB_UVC_VC_PROCESSING_UNIT_DESC_SIZE     0x0DU /* UVC 1.1: +1 for bmVideoStandards */
#define USB_UVC_VC_EXTENSION_UNIT_DESC_SIZE      0x1BU
#define USB_UVC_OUT_TERMINAL_DESC_SIZE           0x09U
#define USB_UVC_CS_ENDPOINT_DESC_SIZE            0x05U
#define USB_UVC_VS_IF_IN_HEADER_DESC_SIZE        0x0EU
#define USB_UVC_VS_FORMAT_UNCOMPRESSED_DESC_SIZE 0x1BU
#define USB_UVC_VS_FORMAT_MJPEG_DESC_SIZE        0x0BU
#define USB_UVC_VS_FRAME_DESC_SIZE               0x1EU
#define USB_UVC_VS_COLOR_MATCHING_DESC_SIZE      0x06U

/* UVC Subclass Code  */
/* Table A- 2 Video Interface Subclass Codes */
#define USB_UVC_SC_UNDEFINED                  0x00U
#define USB_UVC_SC_VC                         0x01U
#define USB_UVC_SC_VS                         0x02U
#define USB_UVC_SC_VIDEO_INTERFACE_COLLECTION 0x03U

/* Table A- 3 Video Interface Protocol Codes */
#define USB_UVC_PC_PROTOCOL_UNDEFINED 0x00U
#define USB_UVC_PC_PROTOCOL_15        0x01U

/* Table A- 4 Video Class-Specific Descriptor Types */
#define USB_UVC_CS_UNDEFINED     0x20U
#define USB_UVC_CS_DEVICE        0x21U
#define USB_UVC_CS_CONFIGURATION 0x22U
#define USB_UVC_CS_STRING        0x23U
#define USB_UVC_CS_INTERFACE     0x24U
#define USB_UVC_CS_ENDPOINT      0x25U

/* Table A- 5 Video Class-Specific VC Interface Descriptor Subtypes */
#define USB_UVC_VC_DESCRIPTOR_UNDEFINED 0x00U
#define USB_UVC_VC_HEADER               0x01U
#define USB_UVC_VC_INPUT_TERMINAL       0x02U
#define USB_UVC_VC_OUTPUT_TERMINAL      0x03U
#define USB_UVC_VC_SELECTOR_UNIT        0x04U
#define USB_UVC_VC_PROCESSING_UNIT      0x05U
#define USB_UVC_VC_EXTENSION_UNIT       0x06U
#define USB_UVC_VC_ENCODING_UNIT        0x07U

/* Table A- 6 Video Class-Specific VS Interface Descriptor Subtypes */
#define USB_UVC_VS_UNDEFINED             0x00U
#define USB_UVC_VS_INPUT_HEADER          0x01U
#define USB_UVC_VS_OUTPUT_HEADER         0x02U
#define USB_UVC_VS_STILL_IMAGE_FRAME     0x03U
#define USB_UVC_VS_FORMAT_UNCOMPRESSED   0x04U
#define USB_UVC_VS_FRAME_UNCOMPRESSED    0x05U
#define USB_UVC_VS_FORMAT_MJPEG          0x06U
#define USB_UVC_VS_FRAME_MJPEG           0x07U
#define USB_UVC_VS_FORMAT_MPEG2TS        0x0AU
#define USB_UVC_VS_FORMAT_DV             0x0CU
#define USB_UVC_VS_COLORFORMAT           0x0DU
#define USB_UVC_VS_FORMAT_FRAME_BASED    0x10U
#define USB_UVC_VS_FRAME_FRAME_BASED     0x11U
#define USB_UVC_VS_FORMAT_STREAM_BASED   0x12U
#define USB_UVC_VS_FORMAT_H264           0x13U
#define USB_UVC_VS_FRAME_H264            0x14U
#define USB_UVC_VS_FORMAT_H264_SIMULCAST 0x15U
#define USB_UVC_VS_FORMAT_VP8            0x16U
#define USB_UVC_VS_FRAME_VP8             0x17U
#define USB_UVC_VS_FORMAT_VP8_SIMULCAST  0x18U

/* Table A- 7 Video Class-Specific Endpoint Descriptor Subtypes */
#define USB_UVC_EP_UNDEFINED 0x00U
#define USB_UVC_EP_GENERAL   0x01U
#define USB_UVC_EP_ENDPOINT  0x02U
#define USB_UVC_EP_INTERRUPT 0x03U

/* Table A- 8 Video Class-Specific Request Codes */
#define USB_UVC_RC_UNDEFINED 0x00U
#define USB_UVC_SET_CUR      0x01U
#define USB_UVC_GET_CUR      0x81U
#define USB_UVC_GET_MIN      0x82U
#define USB_UVC_GET_MAX      0x83U
#define USB_UVC_GET_RES      0x84U
#define USB_UVC_GET_LEN      0x85U
#define USB_UVC_GET_INFO     0x86U
#define USB_UVC_GET_DEF      0x87U
#define USB_UVC_GET_CUR_ALL  0x91U
#define USB_UVC_GET_MIN_ALL  0x92U
#define USB_UVC_GET_MAX_ALL  0x93U
#define USB_UVC_GET_RES_ALL  0x94U
#define USB_UVC_GET_DEF_ALL  0x97U

/* Table A- 9 VideoControl Interface Control Selectors */
#define USB_UVC_VC_CONTROL_UNDEFINED          0x00U
#define USB_UVC_VC_VIDEO_POWER_MODE_CONTROL   0x01U
#define USB_UVC_VC_REQUEST_ERROR_CODE_CONTROL 0x02U

/* Table A- 10 Terminal Control Selectors */
#define USB_UVC_TE_CONTROL_UNDEFINED 0x00U

/* Table A- 11 Selector Unit Control Selectors */
#define USB_UVC_SU_CONTROL_UNDEFINED    0x00U
#define USB_UVC_SU_INPUT_SELECT_CONTROL 0x01U

/* Table A- 12 Camera Terminal Control Selectors */
#define USB_UVC_CT_CONTROL_UNDEFINED              0x00U
#define USB_UVC_CT_SCANNING_MODE_CONTROL          0x01U
#define USB_UVC_CT_AE_MODE_CONTROL                0x02U
#define USB_UVC_CT_AE_PRIORITY_CONTROL            0x03U
#define USB_UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL 0x04U
#define USB_UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL 0x05U
#define USB_UVC_CT_FOCUS_ABSOLUTE_CONTROL         0x06U
#define USB_UVC_CT_FOCUS_RELATIVE_CONTROL         0x07U
#define USB_UVC_CT_FOCUS_AUTO_CONTROL             0x08U
#define USB_UVC_CT_IRIS_ABSOLUTE_CONTROL          0x09U
#define USB_UVC_CT_IRIS_RELATIVE_CONTROL          0x0AU
#define USB_UVC_CT_ZOOM_ABSOLUTE_CONTROL          0x0BU
#define USB_UVC_CT_ZOOM_RELATIVE_CONTROL          0x0CU
#define USB_UVC_CT_PANTILT_ABSOLUTE_CONTROL       0x0DU
#define USB_UVC_CT_PANTILT_RELATIVE_CONTROL       0x0EU
#define USB_UVC_CT_ROLL_ABSOLUTE_CONTROL          0x10U
#define USB_UVC_CT_ROLL_RELATIVE_CONTROL          0x11U
#define USB_UVC_CT_PRIVACY_CONTROL                0x12U
#define USB_UVC_CT_FOCUS_SIMPLE_CONTROL           0x13U
#define USB_UVC_CT_WINDOW_CONTROL                 0x14U
#define USB_UVC_CT_REGION_OF_INTEREST_CONTROL     0x15U

/* Table A- 13 Processing Unit Control Selectors */
#define USB_UVC_PU_CONTROL_UNDEFINED                      0x00U
#define USB_UVC_PU_BACKLIGHT_COMPENSATION_CONTROL         0x01U
#define USB_UVC_PU_BRIGHTNESS_CONTROL                     0x02U
#define USB_UVC_PU_CONTRAST_CONTROL                       0x03U
#define USB_UVC_PU_GAIN_CONTROL                           0x04U
#define USB_UVC_PU_POWER_LINE_FREQUENCY_CONTROL           0x05U
#define USB_UVC_PU_HUE_CONTROL                            0x06U
#define USB_UVC_PU_SATURATION_CONTROL                     0x07U
#define USB_UVC_PU_SHARPNESS_CONTROL                      0x08U
#define USB_UVC_PU_GAMMA_CONTROL                          0x09U
#define USB_UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL      0x0AU
#define USB_UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL 0x0BU
#define USB_UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL        0x0CU
#define USB_UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL   0x0DU
#define USB_UVC_PU_DIGITAL_MULTIPLIER_CONTROL             0x0EU
#define USB_UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL       0x0FU
#define USB_UVC_PU_HUE_AUTO_CONTROL                       0x10U
#define USB_UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL          0x11U
#define USB_UVC_PU_ANALOG_LOCK_STATUS_CONTROL             0x12U
#define USB_UVC_PU_CONTRAST_AUTO_CONTROL                  0x13U

/* Table A- 14 Coding Unit Control Selectors */
#define USB_UVC_EU_CONTROL_UNDEFINED           0x00U
#define USB_UVC_EU_SELECT_LAYER_CONTROL        0x01U
#define USB_UVC_EU_PROFILE_TOOLSET_CONTROL     0x02U
#define USB_UVC_EU_VIDEO_RESOLUTION_CONTROL    0x03U
#define USB_UVC_EU_MIN_FRAME_INTERVAL_CONTROL  0x04U
#define USB_UVC_EU_SLICE_MODE_CONTROL          0x05U
#define USB_UVC_EU_RATE_CONTROL_MODE_CONTROL   0x06U
#define USB_UVC_EU_AVERAGE_BITRATE_CONTROL     0x07U
#define USB_UVC_EU_CPB_SIZE_CONTROL            0x08U
#define USB_UVC_EU_PEAK_BIT_RATE_CONTROL       0x09U
#define USB_UVC_EU_QUANTIZATION_PARAMS_CONTROL 0x0AU
#define USB_UVC_EU_SYNC_REF_FRAME_CONTROL      0x0BU
#define USB_UVC_EU_LTR_BUFFER_CONTROL          0x0CU
#define USB_UVC_EU_LTR_PICTURE_CONTROL         0x0DU
#define USB_UVC_EU_LTR_VALIDATION_CONTROL      0x0EU
#define USB_UVC_EU_LEVEL_IDC_LIMIT_CONTROL     0x0FU
#define USB_UVC_EU_SEI_PAYLOADTYPE_CONTROL     0x10U
#define USB_UVC_EU_QP_RANGE_CONTROL            0x11U
#define USB_UVC_EU_PRIORITY_CONTROL            0x12U
#define USB_UVC_EU_START_OR_STOP_LAYER_CONTROL 0x13U
#define USB_UVC_EU_ERROR_RESILIENCY_CONTROL    0x14U

/* Table A- 15 Extension Unit Control Selectors */
#define USB_UVC_XU_CONTROL_UNDEFINED 0x00U

/* Table A- 16 VideoStreaming Interface Control Selectors */
#define USB_UVC_VS_CONTROL_UNDEFINED            0x00U
#define USB_UVC_VS_PROBE_CONTROL                0x01U
#define USB_UVC_VS_COMMIT_CONTROL               0x02U
#define USB_UVC_VS_STILL_PROBE_CONTROL          0x03U
#define USB_UVC_VS_STILL_COMMIT_CONTROL         0x04U
#define USB_UVC_VS_STILL_IMAGE_TRIGGER_CONTROL  0x05U
#define USB_UVC_VS_STREAM_ERROR_CODE_CONTROL    0x06U
#define USB_UVC_VS_GENERATE_KEY_FRAME_CONTROL   0x07U
#define USB_UVC_VS_UPDATE_FRAME_SEGMENT_CONTROL 0x08U
#define USB_UVC_VS_SYNC_DELAY_CONTROL           0x09U

/* Table B- 1 USB Terminal Types */
#define USB_UVC_TT_VENDOR_SPECIFIC 0x0100U
#define USB_UVC_TT_STREAMING       0x0101U

/* Table B- 2 Input Terminal Types */
#define USB_UVC_ITT_VENDOR_SPECIFIC       0x0200U
#define USB_UVC_ITT_CAMERA                0x0201U
#define USB_UVC_ITT_MEDIA_TRANSPORT_INPUT 0x0202U

/* Table B- 3 Output Terminal Types */
#define USB_UVC_OTT_VENDOR_SPECIFIC        0x0300U
#define USB_UVC_OTT_DISPLAY                0x0301U
#define USB_UVC_OTT_MEDIA_TRANSPORT_OUTPUT 0x0302U

/* Table B- 4 External Terminal Types */
#define USB_UVC_EXTERNAL_VENDOR_SPECIFIC 0x0400U
#define USB_UVC_COMPOSITE_CONNECTOR      0x0401U
#define USB_UVC_SVIDEO_CONNECTOR         0x0402U
#define USB_UVC_COMPONENT_CONNECTOR      0x0403U

/* Request Error Code Control */
#define USB_UVC_NO_ERROR_ERR        0x00U
#define USB_UVC_NOT_READY_ERR       0x01U
#define USB_UVC_WRONG_STATE_ERR     0x02U
#define USB_UVC_POWER_ERR           0x03U
#define USB_UVC_OUT_OF_RANGE_ERR    0x04U
#define USB_UVC_INVALID_UNIT_ERR    0x05U
#define USB_UVC_INVALID_CONTROL_ERR 0x06U
#define USB_UVC_INVALID_REQUEST_ERR 0x07U
#define USB_UVC_UNKNOWN_ERR         0xFFU

/* Payload Header Information */
#define USB_UVC_Payload_Header_EOH (1U << 7)
#define USB_UVC_Payload_Header_ERR (1U << 6)
#define USB_UVC_Payload_Header_STI (1U << 5)
#define USB_UVC_Payload_Header_RES (1U << 4)
#define USB_UVC_Payload_Header_SCR (1U << 3)
#define USB_UVC_Payload_Header_PTS (1U << 2)
#define USB_UVC_Payload_Header_EOF (1U << 1)
#define USB_UVC_Payload_Header_FID (1U << 0)

/* Control Capabilities */
#define USB_UVC_SUPPORTS_GET         0x01U
#define USB_UVC_SUPPORTS_SET         0x02U
#define USB_UVC_STATE_DISABLED       0x04U
#define USB_UVC_AUTOUPDATE_CONTROL   0x08U
#define USB_UVC_ASYNCHRONOUS_CONTROL 0x10U

/* Probe control bmHint Bitmap */
#define USB_UVC_DW_FRAME_INTERVAL  (1U << 0)
#define USB_UVC_W_KEY_FRAME_RATE   (1U << 1)
#define USB_UVC_W_PFRAME_RATE      (1U << 2)
#define USB_UVC_W_COMP_QUALITY     (1U << 3)
#define USB_UVC_W_COMP_WINDOW_SIZE (1U << 4)

/* Input terminal bmcontrols[3] Bitmap */
#define USB_SCANNING_MODE          (1U << 0)
#define USB_AUTO_EXPOSURE_MODE     (1U << 1)
#define USB_AUTO_EXPOSURE_PRIORITY (1U << 2)
#define USB_EXPOSURE_TIME_ABSOLUTE (1U << 3)
#define USB_EXPOSURE_TIME_RELATIVE (1U << 4)
#define USB_FOCUS_ABSOLUTE         (1U << 5)
#define USB_FOCUS_RELATIVE         (1U << 6)
#define USB_IRIS_ABSOLUTE          (1U << 7)
#define USB_IRIS_RELATIVE          (1U << 8)
#define USB_ZOOM_ABSOLUTE          (1U << 9)
#define USB_ZOOM_RELATIVE          (1U << 10)
#define USB_PANTILT_ABSOLUTE       (1U << 11)
#define USB_PANTILT_RELATIVE       (1U << 12)
#define USB_ROLL_ABSOLUTE          (1U << 13)
#define USB_ROLL_RELATIVE          (1U << 14)
#define USB_FOCUS_AUTO             (1U << 17)
#define USB_PRIVACY                (1U << 18)

#ifdef __cplusplus
}
#endif

#endif // XUSB_UVC_DEFS_H
