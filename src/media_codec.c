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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <media_codec.h>
#include <media_codec_private.h>
#include <media_codec_port.h>
#include <system_info.h>

#include <dlog.h>

static gboolean  __mediacodec_empty_buffer_cb(media_packet_h pkt, void *user_data);
static gboolean __mediacodec_fill_buffer_cb(media_packet_h pkt, void *user_data);
static gboolean __mediacodec_error_cb(mediacodec_error_e error, void *user_data);
static gboolean __mediacodec_eos_cb(void *user_data);
static gboolean __mediacodec_supported_codec_cb(mediacodec_codec_type_e codec_type, void *user_data);
static gboolean __mediacodec_buffer_status_cb(mediacodec_status_e status, void *user_data);


/*
* Internal Implementation
*/
int __convert_error_code(int code, char *func_name)
{
    int ret = MEDIACODEC_ERROR_INVALID_OPERATION;
    char *msg = "MEDIACOODEC_INVALID_OPERATION";

    switch (code) {
        case MC_ERROR_NONE:
            ret = MEDIACODEC_ERROR_NONE;
            msg = "MEDIACODEC_ERROR_NONE";
            break;
        case MC_PARAM_ERROR:
        case MC_INVALID_ARG:
            ret = MEDIACODEC_ERROR_INVALID_PARAMETER;
            msg = "MEDIACODEC_ERROR_INVALID_PARAMETER";
            break;
        case MC_PERMISSION_DENIED:
            ret = MEDIACODEC_ERROR_PERMISSION_DENIED;
            msg = "MEDIACODEC_ERROR_PERMISSION_DENIED";
            break;
        case MC_INVALID_STATUS:
            ret = MEDIACODEC_ERROR_INVALID_STATE;
            msg = "MEDIACODEC_ERROR_INVALID_STATE";
            break;
        case MC_NOT_SUPPORTED:
            ret = MEDIACODEC_ERROR_NOT_SUPPORTED_ON_DEVICE;
            msg = "MEDIACODEC_ERROR_NOT_SUPPORTED_ON_DEVICE";
            break;
        case MC_ERROR:
        case MC_INTERNAL_ERROR:
        case MC_HW_ERROR:
            ret = MEDIACODEC_ERROR_INVALID_OPERATION;
            msg = "MEDIACODEC_ERROR_INVALID_OPERATION";
            break;
        case MC_INVALID_STREAM:
            ret = MEDIACODEC_ERROR_INVALID_STREAM;
            msg = "MEDIACODEC_ERROR_INVALID_STREAM";
            break;
        case MC_CODEC_NOT_FOUND:
            ret = MEDIACODEC_ERROR_CODEC_NOT_FOUND;
            msg = "MEDIACODEC_ERROR_CODEC_NOT_FOUND";
            break;
        case MC_ERROR_DECODE:
            ret = MEDIACODEC_ERROR_DECODE;
            msg = "MEDIACODEC_ERROR_DECODE";
            break;
        case MC_INVALID_IN_BUF:
            ret = MEDIACODEC_ERROR_INVALID_INBUFFER;
            msg = "MEDIACODEC_ERROR_INVALID_INBUFFER";
            break;
        case MC_INVALID_OUT_BUF:
            ret = MEDIACODEC_ERROR_INVALID_OUTBUFFER;
            msg = "MEDIACODEC_ERROR_INVALID_OUTBUFFER";
            break;
        case MC_NOT_INITIALIZED:
            ret = MEDIACODEC_ERROR_NOT_INITIALIZED;
            msg = "MEDIACODEC_ERROR_NOT_INITIALIZED";
            break;
        case MC_OUTPUT_BUFFER_EMPTY:
        case MC_OUTPUT_BUFFER_OVERFLOW:
            ret = MEDIACODEC_ERROR_BUFFER_NOT_AVAILABLE;
            msg = "MEDIACODEC_ERROR_BUFFER_NOT_AVAILABLE";
            break;
        default:
            ret = MEDIACODEC_ERROR_INTERNAL;
            msg = "MEDIACODEC_ERROR_INTERNAL";
            break;
    }
    LOGD("[%s] %s(0x%08x) : core fw error(0x%x)", func_name, msg, ret, code);
    return ret;
}

/*
 *  Public Implementation
 */

int mediacodec_create(mediacodec_h *mediacodec)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle;
    int ret;

    LOGD("mediacodec_create..");

    handle = (mediacodec_s *)malloc(sizeof(mediacodec_s));
    if (handle != NULL) {
        memset(handle, 0 , sizeof(mediacodec_s));
    } else {
        LOGE("MEDIACODEC_ERROR_OUT_OF_MEMORY(0x%08x)", MEDIACODEC_ERROR_OUT_OF_MEMORY);
        return MEDIACODEC_ERROR_OUT_OF_MEMORY;
    }

    ret = mc_create(&handle->mc_handle);
    if (ret != MEDIACODEC_ERROR_NONE) {
        LOGE("MEDIACODEC_ERROR_INVALID_OPERATION(0x%08x)", MEDIACODEC_ERROR_INVALID_OPERATION);
        handle->state = MEDIACODEC_STATE_NONE;
        free(handle);
        handle = NULL;
        return MEDIACODEC_ERROR_INVALID_OPERATION;
    } else {
        *mediacodec = (mediacodec_h)handle;
        handle->state = MEDIACODEC_STATE_IDLE;
        LOGD("new handle : %p", *mediacodec);
    }

    /* set callback */
    mc_set_empty_buffer_cb(handle->mc_handle, (mediacodec_input_buffer_used_cb)__mediacodec_empty_buffer_cb, handle);
    mc_set_fill_buffer_cb(handle->mc_handle, (mediacodec_output_buffer_available_cb)__mediacodec_fill_buffer_cb, handle);
    mc_set_error_cb(handle->mc_handle, (mediacodec_error_cb)__mediacodec_error_cb, handle);
    mc_set_eos_cb(handle->mc_handle, (mediacodec_eos_cb)__mediacodec_eos_cb, handle);
    mc_set_buffer_status_cb(handle->mc_handle, (mediacodec_buffer_status_cb)__mediacodec_buffer_status_cb, handle);
    mc_set_supported_codec_cb(handle->mc_handle, (mediacodec_supported_codec_cb)__mediacodec_supported_codec_cb, handle);

    return MEDIACODEC_ERROR_NONE;

}

int mediacodec_destroy(mediacodec_h mediacodec)
{
    LOGD("[%s] Start, handle to destroy : %p", __FUNCTION__, mediacodec);
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;

    int ret = mc_destroy(handle->mc_handle);
    if (ret != MEDIACODEC_ERROR_NONE) {
        LOGD("MEDIACODEC_ERROR_INVALID_OPERATION(0x%08x)", MEDIACODEC_ERROR_INVALID_OPERATION);
        return MEDIACODEC_ERROR_INVALID_OPERATION;
    } else {
        handle->state = MEDIACODEC_STATE_NONE;
        free(handle);
        handle = NULL;
        return MEDIACODEC_ERROR_NONE;
    }
}

int mediacodec_set_codec(mediacodec_h mediacodec, mediacodec_codec_type_e codec_id, mediacodec_support_type_e flags)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_IDLE);

    int ret = mc_set_codec(handle->mc_handle, codec_id, flags);

    if (ret != MEDIACODEC_ERROR_NONE) {
        return __convert_error_code(ret, (char *)__FUNCTION__);
    } else {
        handle->state = MEDIACODEC_STATE_IDLE;
        return MEDIACODEC_ERROR_NONE;
    }
}

int mediacodec_set_vdec_info(mediacodec_h mediacodec, int width, int height)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_IDLE);

    int ret = mc_set_vdec_info(handle->mc_handle, width, height);

    if (ret != MEDIACODEC_ERROR_NONE) {
        return __convert_error_code(ret, (char *)__FUNCTION__);
    } else {
        handle->state = MEDIACODEC_STATE_IDLE;
        return MEDIACODEC_ERROR_NONE;
    }
}

int mediacodec_set_venc_info(mediacodec_h mediacodec, int width, int height, int fps, int target_bits)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_IDLE);

    int ret = mc_set_venc_info(handle->mc_handle, width, height, fps, target_bits);

    if (ret != MEDIACODEC_ERROR_NONE) {
        return __convert_error_code(ret, (char *)__FUNCTION__);
    } else {
        handle->state = MEDIACODEC_STATE_IDLE;
        return MEDIACODEC_ERROR_NONE;
    }
}

int mediacodec_set_adec_info(mediacodec_h mediacodec, int samplerate, int channel, int bit)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_IDLE);

    int ret = mc_set_adec_info(handle->mc_handle, samplerate, channel, bit);

    if (ret != MEDIACODEC_ERROR_NONE) {
        return __convert_error_code(ret, (char *)__FUNCTION__);
    } else {
        handle->state = MEDIACODEC_STATE_IDLE;
        return MEDIACODEC_ERROR_NONE;
    }
}

int mediacodec_set_aenc_info(mediacodec_h mediacodec, int samplerate, int channel, int bit, int bitrate)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_IDLE);

    int ret = mc_set_aenc_info(handle->mc_handle, samplerate, channel, bit, bitrate);

    if (ret != MEDIACODEC_ERROR_NONE) {
        return __convert_error_code(ret, (char *)__FUNCTION__);
    } else {
        handle->state = MEDIACODEC_STATE_IDLE;
        return MEDIACODEC_ERROR_NONE;
    }
}

int mediacodec_prepare(mediacodec_h mediacodec)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_IDLE);

    int ret = mc_prepare(handle->mc_handle);

    if (ret != MEDIACODEC_ERROR_NONE) {
        return __convert_error_code(ret, (char *)__FUNCTION__);
    } else {
        handle->state = MEDIACODEC_STATE_READY;
        return MEDIACODEC_ERROR_NONE;
    }
}

int mediacodec_unprepare(mediacodec_h mediacodec)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;

    int ret = mc_unprepare(handle->mc_handle);

    if (ret != MEDIACODEC_ERROR_NONE) {
        return __convert_error_code(ret, (char *)__FUNCTION__);
    } else {
        handle->state = MEDIACODEC_STATE_IDLE;
        return MEDIACODEC_ERROR_NONE;
    }
}

int mediacodec_process_input(mediacodec_h mediacodec, media_packet_h inbuf, uint64_t timeOutUs)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_READY);

    int ret = mc_process_input(handle->mc_handle, inbuf, timeOutUs);

    if (ret != MEDIACODEC_ERROR_NONE) {
        return __convert_error_code(ret, (char *)__FUNCTION__);
    } else {
        return MEDIACODEC_ERROR_NONE;
    }
}

int mediacodec_get_output(mediacodec_h mediacodec, media_packet_h *outbuf, uint64_t timeOutUs)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_READY);

    int ret = mc_get_output(handle->mc_handle, outbuf, timeOutUs);

    if (ret != MEDIACODEC_ERROR_NONE) {
        return __convert_error_code(ret, (char *)__FUNCTION__);
    } else {
        return MEDIACODEC_ERROR_NONE;
    }
}

int mediacodec_flush_buffers(mediacodec_h mediacodec)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;

    int ret = mc_flush_buffers(handle->mc_handle);

    if (ret != MEDIACODEC_ERROR_NONE) {
        return __convert_error_code(ret, (char *)__FUNCTION__);
    } else {
        return MEDIACODEC_ERROR_NONE;
    }
}

int mediacodec_get_supported_type(mediacodec_h mediacodec, mediacodec_codec_type_e codec_type, bool encoder, int *support_type)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_IDLE);

    int ret = mc_get_supported_type(handle->mc_handle, codec_type, encoder, support_type);

    if (ret != MEDIACODEC_ERROR_NONE) {
        return __convert_error_code(ret, (char *)__FUNCTION__);
    } else {
        handle->state = MEDIACODEC_STATE_IDLE;
        return MEDIACODEC_ERROR_NONE;
    }
}

int mediacodec_set_input_buffer_used_cb(mediacodec_h mediacodec, mediacodec_input_buffer_used_cb callback, void *user_data)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_IDLE);

    handle->empty_buffer_cb = callback;
    handle->empty_buffer_cb_userdata = user_data;

    LOGD("set empty_buffer_cb(%p)", callback);

    return MEDIACODEC_ERROR_NONE;
}

int mediacodec_unset_input_buffer_used_cb(mediacodec_h mediacodec)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;

    handle->empty_buffer_cb = NULL;
    handle->empty_buffer_cb_userdata = NULL;

    return MEDIACODEC_ERROR_NONE;
}


int mediacodec_set_output_buffer_available_cb(mediacodec_h mediacodec, mediacodec_output_buffer_available_cb callback, void *user_data)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_IDLE);

    handle->fill_buffer_cb = callback;
    handle->fill_buffer_cb_userdata = user_data;

    LOGD("set fill_buffer_cb(%p)", callback);

    return MEDIACODEC_ERROR_NONE;

}

int mediacodec_unset_output_buffer_available_cb(mediacodec_h mediacodec)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;

    handle->fill_buffer_cb = NULL;
    handle->fill_buffer_cb_userdata = NULL;

    return MEDIACODEC_ERROR_NONE;
}

int mediacodec_set_error_cb(mediacodec_h mediacodec, mediacodec_error_cb callback, void *user_data)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_IDLE);

    handle->error_cb = callback;
    handle->error_cb_userdata = user_data;

    LOGD("set error_cb(%p)", callback);

    return MEDIACODEC_ERROR_NONE;

}

int mediacodec_unset_error_cb(mediacodec_h mediacodec)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;

    handle->error_cb = NULL;
    handle->error_cb_userdata = NULL;

    return MEDIACODEC_ERROR_NONE;
}

int mediacodec_set_eos_cb(mediacodec_h mediacodec, mediacodec_eos_cb callback, void *user_data)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_IDLE);

    handle->eos_cb = callback;
    handle->eos_cb_userdata = user_data;

    LOGD("set eos_cb(%p)", callback);

    return MEDIACODEC_ERROR_NONE;

}

int mediacodec_unset_eos_cb(mediacodec_h mediacodec)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;

    handle->eos_cb = NULL;
    handle->eos_cb_userdata = NULL;

    return MEDIACODEC_ERROR_NONE;
}

int mediacodec_set_buffer_status_cb(mediacodec_h mediacodec, mediacodec_buffer_status_cb callback, void *user_data)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;
    MEDIACODEC_STATE_CHECK(handle, MEDIACODEC_STATE_IDLE);

    handle->buffer_status_cb = callback;
    handle->buffer_status_cb_userdata = user_data;

    LOGD("set buffer_status_cb(%p)", callback);

    return MEDIACODEC_ERROR_NONE;

}

int mediacodec_unset_buffer_status_cb(mediacodec_h mediacodec)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;

    handle->buffer_status_cb = NULL;
    handle->buffer_status_cb_userdata = NULL;

    return MEDIACODEC_ERROR_NONE;
}

int mediacodec_foreach_supported_codec(mediacodec_h mediacodec, mediacodec_supported_codec_cb callback, void *user_data)
{
    MEDIACODEC_INSTANCE_CHECK(mediacodec);
    mediacodec_s *handle = (mediacodec_s *)mediacodec;

    handle->supported_codec_cb = callback;
    handle->supported_codec_cb_userdata = user_data;

    LOGD("set supported_codec_cb(%p)", callback);
    int ret = _mediacodec_foreach_supported_codec(handle->mc_handle, callback, handle);

    if (ret != MEDIACODEC_ERROR_NONE) {
        return __convert_error_code(ret, (char *)__FUNCTION__);
    } else {
        return MEDIACODEC_ERROR_NONE;
    }

    return MEDIACODEC_ERROR_NONE;

}

static gboolean __mediacodec_empty_buffer_cb(media_packet_h pkt, void *user_data)
{
    if (user_data == NULL || pkt == NULL)
        return 0;

    mediacodec_s *handle = (mediacodec_s *)user_data;

    if (handle->empty_buffer_cb) {
        ((mediacodec_input_buffer_used_cb)handle->empty_buffer_cb)(pkt, handle->empty_buffer_cb_userdata);
    }

    return 1;
}

static gboolean  __mediacodec_fill_buffer_cb(media_packet_h pkt, void *user_data)
{
    if (user_data == NULL || pkt == NULL)
        return 0;

    mediacodec_s *handle = (mediacodec_s *)user_data;

    if (handle->fill_buffer_cb) {
        ((mediacodec_output_buffer_available_cb)handle->fill_buffer_cb)(pkt, handle->fill_buffer_cb_userdata);
    }

    return 1;
}

static gboolean __mediacodec_error_cb(mediacodec_error_e error, void *user_data)
{
    if (user_data == NULL)
        return 0;

    mediacodec_s *handle = (mediacodec_s *)user_data;

    if (handle->error_cb) {
        ((mediacodec_error_cb)handle->error_cb)(error, handle->error_cb_userdata);
    }

    return 1;
}

static gboolean __mediacodec_eos_cb(void *user_data)
{
    if (user_data == NULL)
        return 0;

    mediacodec_s *handle = (mediacodec_s *)user_data;

    if (handle->eos_cb) {
        ((mediacodec_eos_cb)handle->eos_cb)(handle->eos_cb_userdata);
    }

    return 1;
}

static gboolean __mediacodec_supported_codec_cb(mediacodec_codec_type_e codec_type, void *user_data)
{
    if (user_data == NULL)
        return 0;

    mediacodec_s *handle = (mediacodec_s *)user_data;

    if (handle->supported_codec_cb) {
        return ((mediacodec_supported_codec_cb)handle->supported_codec_cb)(codec_type, handle->supported_codec_cb_userdata);
    }
    return false;
}

static gboolean __mediacodec_buffer_status_cb(mediacodec_status_e status, void *user_data)
{
    if (user_data == NULL)
        return 0;

    mediacodec_s *handle = (mediacodec_s *)user_data;

    if (handle->buffer_status_cb) {
        ((mediacodec_buffer_status_cb)handle->buffer_status_cb)(status, handle->buffer_status_cb_userdata);
    }

    return 1;
}

