/*
* Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
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

#ifndef __TIZEN_MEDIA_CODEC_PRIVATE_H__
#define __TIZEN_MEDIA_CODEC_PRIVATE_H__

#include <tizen.h>
#include <stdint.h>

#include <media_codec.h>

#include <mm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "TIZEN_N_MEDIACODEC"


#define MEDIACODEC_CHECK_CONDITION(condition,error,msg)     \
                if(condition) {} else \
                { LOGE("[%s] %s(0x%08x)",__FUNCTION__, msg,error); return error;}; \

#define MEDIACODEC_INSTANCE_CHECK(mediacodec)   \
        MEDIACODEC_CHECK_CONDITION(mediacodec != NULL, MEDIACODEC_ERROR_INVALID_PARAMETER,"MEDIACODEC_ERROR_INVALID_PARAMETER")

#define MEDIACODEC_STATE_CHECK(mediacodec,expected_state)       \
        MEDIACODEC_CHECK_CONDITION(mediacodec->state == expected_state,MEDIACODEC_ERROR_INVALID_STATE,"MEDIACODEC_ERROR_INVALID_STATE")

#define MEDIACODEC_NULL_ARG_CHECK(arg)      \
        MEDIACODEC_CHECK_CONDITION(arg != NULL,MEDIACODEC_ERROR_INVALID_PARAMETER,"MEDIACODEC_ERROR_INVALID_PARAMETER")

#define MEDIACODEC_SUPPORT_CHECK(arg)       \
        MEDIACODEC_CHECK_CONDITION(arg != false, MEDIACODEC_ERROR_NOT_SUPPORTED_ON_DEVICE, "MEDIACODEC_ERROR_NOT_SUPPORTED_ON_DEVICE")
/**
 * @brief Enumeration of media codec state
 * @since_tizen 2.3
 */
typedef enum
{
    MEDIACODEC_STATE_NONE = 0,      /**< Media codec is not created */
    MEDIACODEC_STATE_IDLE,          /**< Media codec is created, but not prepared */
    MEDIACODEC_STATE_READY,         /**< Media codec is ready to process */
    MEDIACODEC_STATE_EXCUTE,        /**< Media codec is executing media */
} mediacodec_state_e;

typedef enum
{
    MEDIACODEC_SUPPORT_TYPE_OMX = 0x10,     /**< This is optional flag for using openmax il */
    MEDIACODEC_SUPPORT_TYPE_GEN = 0x20      /**< This is optional flag for using general codec */
} mediacodec_internal_support_type_e;

typedef enum
{
    MEDIACODEC_PORT_TYPE_GENERAL,
    MEDIACODEC_PORT_TYPE_OMX,
    MEDIACODEC_PORT_TYPE_GST,
    MEDIACODEC_PORT_TYPE_MAX,
} mediacodec_port_type_e;

/**
 * @brief Media Codec's format for configuring codec.
 */
typedef struct _mediacodecformat_s {
    // Video
    int fps;
    int height;
    int width;
    float aspect;
    bool vfr;
    int level;
    int profile;

    // Audio
    int channels;
    int samplerate;
    int bitrate;

    // Codec Extra Data
    void * extradata;
    unsigned int extrasize;
} mediacodec_format_s;

typedef struct _mediacodec_s {
    MMHandleType mc_handle;
    int state;
    bool is_omx;
    char *m_mime;

    mediacodec_input_buffer_used_cb empty_buffer_cb;
    void* empty_buffer_cb_userdata;
    mediacodec_output_buffer_available_cb fill_buffer_cb;
    void* fill_buffer_cb_userdata;
    mediacodec_error_cb error_cb;
    void* error_cb_userdata;
    mediacodec_eos_cb eos_cb;
    void* eos_cb_userdata;
    mediacodec_buffer_status_cb buffer_status_cb;
    void* buffer_status_cb_userdata;
    mediacodec_supported_codec_cb supported_codec_cb;
    void* supported_codec_cb_userdata;

} mediacodec_s;

bool __mediacodec_state_validate(mediacodec_h mediacodec, mediacodec_state_e threshold);

#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_PRIVATE_H__ */
