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
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlog.h>

#include <media_codec.h>
#include <media_codec_private.h>
#include <media_codec_port.h>
#include <media_codec_port_gst.h>

#include <media_codec_spec_emul.h>

static mc_codec_spec_t spec_emul[MEDIA_CODEC_MAX_CODEC_TYPE];

int mc_create(MMHandleType *mediacodec)
{
    mc_handle_t *new_mediacodec = NULL;
    int ret = MC_ERROR_NONE;
    int i;
    int support_list = sizeof(spec_emul) / sizeof(spec_emul[0]);

    /* alloc mediacodec structure */
    new_mediacodec = (mc_handle_t *)g_malloc(sizeof(mc_handle_t));

    if (!new_mediacodec) {
        LOGE("Cannot allocate memory for mediacodec");
        ret = MC_ERROR;
        goto ERROR;
    }
    memset(new_mediacodec, 0, sizeof(mc_handle_t));
    memset(spec_emul, 0, sizeof(mc_codec_spec_t)*MEDIA_CODEC_MAX_CODEC_TYPE);

    new_mediacodec->is_encoder = false;
    new_mediacodec->is_video = false;
    new_mediacodec->is_hw = true;
    new_mediacodec->is_prepared = false;
    new_mediacodec->codec_id = MEDIACODEC_NONE;

    new_mediacodec->ports[0] = NULL;
    new_mediacodec->ports[1] = NULL;

    new_mediacodec->num_supported_codecs  = 0;
    new_mediacodec->num_supported_decoder = 0;
    new_mediacodec->num_supported_encoder = 0;

    new_mediacodec->core = NULL;

    /* load ini files */
    ret = mc_ini_load(&new_mediacodec->ini);
    if(ret != MC_ERROR_NONE)
        goto ERROR;
    _mc_create_codec_map_from_ini(new_mediacodec, spec_emul);

    for (i = 0; i < new_mediacodec->num_supported_codecs; i++) {
        new_mediacodec->supported_codecs =
            g_list_append(new_mediacodec->supported_codecs, GINT_TO_POINTER(spec_emul[i].codec_id));
    }

    /* create decoder map from ini */
    _mc_create_decoder_map_from_ini(new_mediacodec);

    /* create encoder map from ini */
    _mc_create_encoder_map_from_ini(new_mediacodec);

    *mediacodec = (MMHandleType)new_mediacodec;

    return ret;

ERROR:

    return MC_INVALID_ARG;
}

int mc_destroy(MMHandleType mediacodec)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *) mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    LOGD("mediacodec : %p", mediacodec);

    if (mc_handle->core != NULL) {
        if (mc_gst_unprepare(mc_handle) != MC_ERROR_NONE) {
            LOGE("mc_gst_unprepare() failed");
            return MC_ERROR;
        }
    }

    mc_handle->is_prepared = false;
    g_list_free(mc_handle->supported_codecs);

    /* free mediacodec structure */
    if (mc_handle) {
        g_free((void *)mc_handle);
        mc_handle = NULL;
    }

    return ret;
}

int mc_set_codec(MMHandleType mediacodec, mediacodec_codec_type_e codec_id, mediacodec_support_type_e flags)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *) mediacodec;
    int i;

    if (!mc_handle) {
        LOGE("fail invaild param");
        return MC_INVALID_ARG;
    }

    /* Mandatory setting */
    if (!GET_IS_ENCODER(flags) && !GET_IS_DECODER(flags)) {
        LOGE("should be encoder or decoder");
        return MC_PARAM_ERROR;
    }


    for (i = 0; i < mc_handle->num_supported_codecs; i++) {
        if ((codec_id == spec_emul[i].codec_id) && (flags == spec_emul[i].codec_type)) {
            break;
        }
    }
    LOGD("support_list : %d, i : %d", mc_handle->num_supported_codecs, i);

    if (i == mc_handle->num_supported_codecs)
        return MC_NOT_SUPPORTED;

    mc_handle->port_type = spec_emul[i].port_type;

    mc_handle->is_encoder = GET_IS_ENCODER(flags) ? 1 : 0;
    mc_handle->is_hw = GET_IS_HW(flags) ? 1 : 0;
    mc_handle->codec_id = codec_id;
    mc_handle->is_video = CHECK_BIT(codec_id, 13);

    mc_handle->is_prepared = false;

    LOGD("encoder : %d, hardware : %d, codec_id : %x, video : %d",
        mc_handle->is_encoder, mc_handle->is_hw, mc_handle->codec_id, mc_handle->is_video);

    return ret;
}

int mc_set_vdec_info(MMHandleType mediacodec, int width, int height)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if ((width <= 0) || (height <= 0))
        return MC_PARAM_ERROR;

    MEDIACODEC_CHECK_CONDITION(mc_handle->codec_id && mc_handle->is_video && !mc_handle->is_encoder,
            MEDIACODEC_ERROR_INVALID_PARAMETER, "MEDIACODEC_ERROR_INVALID_PARAMETER");

    mc_handle->info.decoder.width = width;
    mc_handle->info.decoder.height = height;

    mc_handle->is_prepared = true;

    switch (mc_handle->codec_id) {
        case MEDIACODEC_H264:
            mc_sniff_bitstream = mc_sniff_h264_bitstream;
            LOGD("mc_sniff_h264_bitstream");
            break;
        case MEDIACODEC_MPEG4:
            mc_sniff_bitstream = mc_sniff_mpeg4_bitstream;
            break;
        case MEDIACODEC_H263:
            mc_sniff_bitstream = mc_sniff_h263_bitstream;
            break;
        default:
            LOGE("NOT SUPPORTED!!!!");
            break;
    }

    return ret;
}

int mc_set_venc_info(MMHandleType mediacodec, int width, int height, int fps, int target_bits)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *) mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if ((width <= 0) || (height <= 0))
        return MC_PARAM_ERROR;

    MEDIACODEC_CHECK_CONDITION(mc_handle->codec_id && mc_handle->is_video && mc_handle->is_encoder,
        MEDIACODEC_ERROR_INVALID_PARAMETER, "MEDIACODEC_ERROR_INVALID_PARAMETER");

    mc_handle->info.encoder.width = width;
    mc_handle->info.encoder.height = height;
    mc_handle->info.encoder.fps = fps;
    mc_handle->info.encoder.bitrate = target_bits;
    mc_handle->is_prepared = true;
    mc_sniff_bitstream = mc_sniff_yuv;

    return ret;
}

int mc_set_adec_info(MMHandleType mediacodec, int samplerate, int channel, int bit)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *) mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if ((samplerate <= 0) || (channel <= 0) || (bit <= 0))
        return MC_PARAM_ERROR;

    MEDIACODEC_CHECK_CONDITION(mc_handle->codec_id && !mc_handle->is_video && !mc_handle->is_encoder,
        MEDIACODEC_ERROR_INVALID_PARAMETER, "MEDIACODEC_ERROR_INVALID_PARAMETER");

    mc_handle->info.decoder.samplerate = samplerate;
    mc_handle->info.decoder.channel = channel;
    mc_handle->info.decoder.bit = bit;
    mc_handle->is_prepared = true;

    return ret;
}

int mc_set_aenc_info(MMHandleType mediacodec, int samplerate, int channel, int bit, int bitrate)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t * mc_handle = (mc_handle_t *) mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if ((samplerate <= 0) || (channel <= 0) || (bit <= 0))
        return MC_PARAM_ERROR;

    MEDIACODEC_CHECK_CONDITION(mc_handle->codec_id && !mc_handle->is_video && mc_handle->is_encoder,
        MEDIACODEC_ERROR_INVALID_PARAMETER, "MEDIACODEC_ERROR_INVALID_PARAMETER");

    mc_handle->info.encoder.samplerate = samplerate;
    mc_handle->info.encoder.channel = channel;
    mc_handle->info.encoder.bit = bit;
    mc_handle->info.encoder.bitrate = bitrate;

    mc_handle->is_prepared = true;

    return ret;
}

int mc_prepare(MMHandleType mediacodec)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *) mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if (!mc_handle->is_prepared)
        return MC_NOT_INITIALIZED;

    /* setting core details */
    switch (mc_handle->port_type) {
        case MEDIACODEC_PORT_TYPE_GENERAL:
        {
        }
        break;

        case MEDIACODEC_PORT_TYPE_OMX:
        {
        }
        break;

        case MEDIACODEC_PORT_TYPE_GST:
        {
            mc_gst_prepare(mc_handle);
        }
        break;

        default:
        break;
    }

    return ret;
}

int mc_unprepare(MMHandleType mediacodec)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    /* deinit core details */
    switch (mc_handle->port_type) {
        case MEDIACODEC_PORT_TYPE_GENERAL:
        {
        }
        break;

        case MEDIACODEC_PORT_TYPE_OMX:
        {
        }
        break;

        case MEDIACODEC_PORT_TYPE_GST:
        {
            ret = mc_gst_unprepare(mc_handle);
        }
        break;

        default:
            break;
    }

    return ret;
}

int mc_process_input(MMHandleType mediacodec, media_packet_h inbuf, uint64_t timeOutUs)
{
    int ret = MC_ERROR_NONE;

    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param");
        return MC_INVALID_ARG;
    }

    if (mc_handle->is_video) {
        if ((ret = mc_sniff_bitstream(mc_handle, inbuf)) != MC_ERROR_NONE) {
            return MC_INVALID_IN_BUF;
        }
    }

    switch (mc_handle->port_type) {
        case MEDIACODEC_PORT_TYPE_GENERAL:
        break;

        case MEDIACODEC_PORT_TYPE_OMX:
        break;

        case MEDIACODEC_PORT_TYPE_GST:
        {
            ret = mc_gst_process_input(mc_handle, inbuf, timeOutUs);
        }
        break;

        default:
            break;
    }

    return ret;
}

int mc_get_output(MMHandleType mediacodec, media_packet_h *outbuf, uint64_t timeOutUs)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *) mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    /* setting core details */
    switch (mc_handle->port_type) {
        case MEDIACODEC_PORT_TYPE_GENERAL:
        break;

        case MEDIACODEC_PORT_TYPE_OMX:
        break;

        case MEDIACODEC_PORT_TYPE_GST:
        {
            ret = mc_gst_get_output(mc_handle, outbuf, timeOutUs);
        }
        break;

        default:
            break;
    }

    return ret;
}

int mc_flush_buffers(MMHandleType mediacodec)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    /* setting core details */
    switch (mc_handle->port_type) {
        case MEDIACODEC_PORT_TYPE_GENERAL:
        break;

        case MEDIACODEC_PORT_TYPE_OMX:
        break;

        case MEDIACODEC_PORT_TYPE_GST:
        {
            ret = mc_gst_flush_buffers(mc_handle);
        }
        break;

        default:
            break;
    }

    return ret;
}

int mc_get_supported_type(MMHandleType mediacodec, mediacodec_codec_type_e codec_type, bool encoder, int *support_type)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;
    mc_codec_map_t *codec_map;
    int num_supported_codec = 0;
    int i;

    *support_type = 0;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }
    codec_map = encoder ? mc_handle->encoder_map : mc_handle->decoder_map;
    num_supported_codec = encoder ? mc_handle->num_supported_encoder : mc_handle->num_supported_decoder;

    for (i = 0; i < num_supported_codec; i++)
    {
        if (codec_map[i].id == codec_type)
        {
            if (codec_map[i].hardware)
                *support_type |= MEDIACODEC_SUPPORT_TYPE_HW;
            else
                *support_type |= MEDIACODEC_SUPPORT_TYPE_SW;
            break;
        }

    }

    return ret;
}

int mc_set_empty_buffer_cb(MMHandleType mediacodec, mediacodec_input_buffer_used_cb callback, void *user_data)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if (mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER]) {
        LOGE("Already set mediacodec_empty_buffer_cb");
        return MC_PARAM_ERROR;
    } else {
        if (!callback) {
            return MC_INVALID_ARG;
        }

        LOGD("Set empty buffer callback(cb = %p, data = %p)", callback, user_data);

        mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER] = (mc_empty_buffer_cb) callback;
        mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER] = user_data;

        return MC_ERROR_NONE;
    }

    return ret;
}

int mc_unset_empty_buffer_cb(MMHandleType mediacodec)
{
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER] = NULL;
    mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER] = NULL;

    return MC_ERROR_NONE;
}

int mc_set_fill_buffer_cb(MMHandleType mediacodec, mediacodec_output_buffer_available_cb callback, void *user_data)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if (mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_FILLBUFFER]) {
        LOGE("Already set mediacodec_fill_buffer_cb");
        return MC_PARAM_ERROR;
    } else {
        if (!callback) {
            return MC_INVALID_ARG;
        }

        LOGD("Set fill buffer callback(cb = %p, data = %p)", callback, user_data);

        mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_FILLBUFFER] = (mc_fill_buffer_cb) callback;
        mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_FILLBUFFER] = user_data;
        return MC_ERROR_NONE;
    }

    return ret;
}

int mc_unset_fill_buffer_cb(MMHandleType mediacodec)
{
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_FILLBUFFER] = NULL;
    mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_FILLBUFFER] = NULL;

    return MC_ERROR_NONE;
}

int mc_set_error_cb(MMHandleType mediacodec, mediacodec_error_cb callback, void *user_data)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *) mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if (mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR]) {
        LOGE("Already set mediacodec_fill_buffer_cb\n");
        return MC_PARAM_ERROR;
    } else {
        if (!callback) {
            return MC_INVALID_ARG;
        }

        LOGD("Set event handler callback(cb = %p, data = %p)\n", callback, user_data);

        mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR] = (mc_error_cb) callback;
        mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_ERROR] = user_data;
        return MC_ERROR_NONE;
    }

    return ret;
}

int mc_unset_error_cb(MMHandleType mediacodec)
{
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param");
        return MC_INVALID_ARG;
    }

    mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR] = NULL;
    mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_ERROR] = NULL;

    return MC_ERROR_NONE;
}

int mc_set_eos_cb(MMHandleType mediacodec, mediacodec_eos_cb callback, void *user_data)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *) mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if (mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_EOS]) {
        LOGE("Already set mediacodec_fill_buffer_cb");
        return MC_PARAM_ERROR;
    } else {
        if (!callback) {
            return MC_INVALID_ARG;
        }

        LOGD("Set event handler callback(cb = %p, data = %p)\n", callback, user_data);

        mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_EOS] = (mc_eos_cb) callback;
        mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_EOS] = user_data;
        return MC_ERROR_NONE;
    }

    return ret;
}

int mc_unset_eos_cb(MMHandleType mediacodec)
{
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_EOS] = NULL;
    mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_EOS] = NULL;

    return MC_ERROR_NONE;
}

int mc_set_buffer_status_cb(MMHandleType mediacodec, mediacodec_buffer_status_cb callback, void *user_data)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if (mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_BUFFER_STATUS]) {
        LOGE("Already set mediacodec_need_data_cb\n");
        return MC_PARAM_ERROR;
    } else {
        if (!callback) {
            return MC_INVALID_ARG;
        }

        LOGD("Set start feed callback(cb = %p, data = %p)\n", callback, user_data);

        mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_BUFFER_STATUS] = (mc_buffer_status_cb) callback;
        mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_BUFFER_STATUS] = user_data;
        return MC_ERROR_NONE;
    }

    return ret;
}

int mc_unset_buffer_status_cb(MMHandleType mediacodec)
{
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_BUFFER_STATUS] = NULL;
    mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_BUFFER_STATUS] = NULL;

    return MC_ERROR_NONE;
}

int mc_set_supported_codec_cb(MMHandleType mediacodec, mediacodec_supported_codec_cb callback, void *user_data)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if (mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_SUPPORTED_CODEC]) {
        LOGE("Already set mediacodec_supported_codec_cb\n");
        return MC_PARAM_ERROR;
    } else {
        if (!callback) {
            return MC_INVALID_ARG;
        }

        LOGD("Set event handler callback(cb = %p, data = %p)", callback, user_data);

        mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_SUPPORTED_CODEC] = (mc_supported_codec_cb) callback;
        mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_SUPPORTED_CODEC] = user_data;
        return MC_ERROR_NONE;
    }

    return ret;
}

int _mediacodec_foreach_supported_codec(MMHandleType mediacodec, mediacodec_supported_codec_cb callback, void *user_data)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t *mc_handle = (mc_handle_t *)mediacodec;
    int codecs_num;
    gpointer tmp;

    if (!mc_handle) {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if (mc_handle->supported_codecs) {
        codecs_num = g_list_length(mc_handle->supported_codecs);
        LOGD("supported_codecs : %d", codecs_num);

        while (codecs_num) {
            tmp = g_list_nth_data(mc_handle->supported_codecs, codecs_num - 1);
            if (tmp) {
                if (!callback(GPOINTER_TO_INT(tmp), user_data)) {
                    ret = MEDIACODEC_ERROR_INTERNAL;
                    goto CALLBACK_ERROR;
                }
            }
            codecs_num--;
        }

        if (!callback(-1, user_data)) {
            ret = MEDIACODEC_ERROR_INTERNAL;
            goto CALLBACK_ERROR;
        }
    }

CALLBACK_ERROR:
    LOGD("foreach callback returned error");
    return ret;
}

int mc_sniff_h264_bitstream(mc_handle_t *handle, media_packet_h pkt)
{
    int ret = MC_ERROR_NONE;
    void *buf_data = NULL;
    void *codec_data = NULL;
    unsigned int codec_data_size = 0;
    uint64_t buf_size = 0;
    bool eos = false;
    bool codec_config = false;

    media_packet_get_buffer_size(pkt, &buf_size);
    media_packet_get_buffer_data_ptr(pkt, &buf_data);
    media_packet_get_codec_data(pkt, &codec_data, &codec_data_size);
    media_packet_is_end_of_stream(pkt, &eos);
    media_packet_is_codec_config(pkt, &codec_config);

    LOGD("codec_data_size : %d, buf_size : %d, codec_config : %d, eos : %d",
        codec_data_size, (int)buf_size, codec_config, eos);

    if (codec_config) {
        if (codec_data_size == 0)
          ret = _mc_check_h264_bytestream(buf_data, (int)buf_size, 0, NULL, NULL, NULL);
    }

    return ret;
}

int mc_sniff_mpeg4_bitstream(mc_handle_t *handle, media_packet_h pkt)
{
    uint64_t buf_size = 0;
    unsigned char *buf_data = NULL;
    void *codec_data = NULL;
    int codec_data_size = 0, voss_size = 0;
    bool eos = false;
    bool codec_config = false;
    bool sync_frame = false;

    media_packet_get_buffer_size(pkt, &buf_size);
    media_packet_get_buffer_data_ptr(pkt, (void *)&buf_data);
    media_packet_get_codec_data(pkt, &codec_data, &codec_data_size);
    media_packet_is_end_of_stream(pkt, &eos);
    media_packet_is_codec_config(pkt, &codec_config);
    media_packet_is_sync_frame(pkt, &sync_frame);

    LOGD("codec_data_size : %d, buff_size : %d, codec_config : %d, sync_frame : %d, eos : %d",
        codec_data_size, (int)buf_size, codec_config, sync_frame, eos);

    if (eos)
        return MC_ERROR_NONE;

    if (codec_config) {
        if (codec_data) {
            if (!_mc_is_voss(codec_data, codec_data_size, NULL))/*voss not in codec data */
                return MC_INVALID_IN_BUF;
        }
        /* Codec data + I-frame in buffer */
        if (_mc_is_voss(buf_data, buf_size, &voss_size)) {
            if (_mc_is_ivop(buf_data, buf_size, voss_size))   /* IVOP after codec_data */
                return MC_ERROR_NONE;
        } else {
            if (_mc_is_ivop(buf_data, buf_size, 0))            /* IVOP at the start */
                return MC_ERROR_NONE;
        }
    } else if (_mc_is_vop(buf_data, buf_size, 0))
        return MC_ERROR_NONE;

    return MC_INVALID_IN_BUF;
}

int mc_sniff_h263_bitstream(mc_handle_t *handle, media_packet_h pkt)
{
    void *buf_data = NULL;
    uint64_t buf_size = 0;
    bool eos = false;

    media_packet_get_buffer_size(pkt, &buf_size);
    media_packet_get_buffer_data_ptr(pkt, &buf_data);
    media_packet_is_end_of_stream(pkt, &eos);

    if (eos)
        return MC_ERROR_NONE;

    return _mc_check_valid_h263_frame((unsigned char *)buf_data, (int)buf_size);
}

int mc_sniff_yuv(mc_handle_t *handle, media_packet_h pkt)
{
    int ret = MC_ERROR_NONE;

#if 0 /* libtbm, media-tool should be updated */
    uint64_t buf_size = 0;
    int plane_num = 0;
    int padded_width = 0;
    int padded_height = 0;
    int allocated_buffer = 0;
    int index;

    media_packet_get_buffer_size(pkt, &buf_size);
    media_packet_get_video_number_of_planes(pkt, &plane_num);

    for (index = 0; index < plane_num; index++) {
        media_packet_get_video_stride_width(pkt, index, &padded_width);
        media_packet_get_video_stride_height(pkt, index, &padded_height);
        allocated_buffer += padded_width * padded_height;

        LOGD("%d plane size : %d", padded_width * padded_height);
    }

    if (buf_size > allocated_buffer) {
        LOGE("Buffer exceeds maximum size [buf_size: %d, allocated_size :%d", (int)buf_size, allocated_buffer);
        ret = MC_INVALID_IN_BUF;
    }
#endif
    return ret;
}

