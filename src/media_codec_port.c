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

//static gboolean _mc_check_is_supported(mc_handle_t* mc_handle, mediacodec_codec_type_e codec_id, mediacodec_support_type_e flags);

int mc_create(MMHandleType *mediacodec)
{
    mc_handle_t* new_mediacodec = NULL;
    int ret = MC_ERROR_NONE;

    /* alloc mediacodec structure */
    new_mediacodec = (mc_handle_t*)g_malloc(sizeof(mc_handle_t));
    if ( ! new_mediacodec )
    {
        LOGE("Cannot allocate memory for player\n");
        ret = MC_ERROR;
        goto ERROR;
    }
    memset(new_mediacodec, 0, sizeof(mc_handle_t));

    new_mediacodec->is_encoder = false;
    new_mediacodec->is_video = false;
    new_mediacodec->is_hw = true;
    new_mediacodec->is_prepared = false;
    new_mediacodec->codec_id = MEDIACODEC_NONE;

    new_mediacodec->ports[0] = NULL;
    new_mediacodec->ports[1] = NULL;

    new_mediacodec->core = NULL;

    g_mutex_init(&new_mediacodec->cmd_lock);
    /*
    if(!new_mediacodec->cmd_lock)
    {
        LOGE("failed to create cmd_lock");
        goto ERROR;
    }
    */
    *mediacodec = (MMHandleType)new_mediacodec;

    return ret;

    // TO DO
ERROR:
    // TO DO
    // If we need destroy and release for others (cmd, mutex..)
    g_mutex_clear(&new_mediacodec->cmd_lock);
    free(new_mediacodec);
    new_mediacodec = NULL;
    return MC_INVALID_ARG;

    return ret;
}

int mc_destroy(MMHandleType mediacodec)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    MEDIACODEC_CMD_LOCK( mediacodec );

    LOGD("mediacodec : %p", mediacodec);

    if(mc_handle->core != NULL)
    {
        if(mc_gst_unprepare(mc_handle) != MC_ERROR_NONE)
        {
            LOGE("mc_gst_unprepare() failed");
            return MC_ERROR;
        }
    }

    mc_handle->is_prepared = false;

    MEDIACODEC_CMD_UNLOCK( mediacodec );

    /* free mediacodec structure */
    if(mc_handle) {
        g_free( (void*)mc_handle );
        mc_handle = NULL;
    }
    return ret;
}

int mc_set_codec(MMHandleType mediacodec, mediacodec_codec_type_e codec_id, mediacodec_support_type_e flags)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;
    static const int support_list = sizeof(spec_emul) / sizeof(spec_emul[0]);
    int i;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    // Mandatory setting
    if ( !GET_IS_ENCODER(flags) && !GET_IS_DECODER(flags) )
    {
        LOGE("should be encoder or decoder\n");
        return MC_PARAM_ERROR;
    }
/*
    if(!_mc_check_is_supported(mc_handle, codec_id, flags))
        return MC_NOT_SUPPORTED;
*/
    for(i = 0; i < support_list; i++)
    {
        if((codec_id == spec_emul[i].codec_id) && (flags == spec_emul[i].codec_type))
        {
            break;
        }
    }

    LOGD("support_list : %d, i : %d", support_list, i);

    if(i == support_list)
        return MC_NOT_SUPPORTED;

    mc_handle->port_type = spec_emul[i].port_type;

    mc_handle->is_encoder = GET_IS_ENCODER(flags) ? 1 : 0;
    mc_handle->is_hw = GET_IS_HW(flags) ? 1 : 0;
    mc_handle->codec_id = codec_id;
    mc_handle->is_video = CHECK_BIT(codec_id, 13);

    mc_handle->is_prepared = false;

    LOGD("encoder : %d, hardware : %d, codec_id : %x, video : %d",
        mc_handle->is_encoder, mc_handle->is_hw, mc_handle->codec_id, mc_handle->is_video);
#if 0
    //  mc_handle->is_omx = use_omx;
    // !!!! make it dynamic
    mc_handle->port_type = MEDIACODEC_PORT_TYPE_GST;

    // !!!! only gst case is here. expend it to all.
    if (encoder)
    {
        switch(codec_id)
        {
            case MEDIACODEC_H264:
                mc_handle->supported_codec = GST_ENCODE_H264;
                mc_handle->mimetype = MEDIA_FORMAT_H264_HP;
                mc_handle->is_video = 1;
            break;
            case MEDIACODEC_AAC:
                mc_handle->supported_codec = GST_ENCODE_AAC;
                mc_handle->mimetype = MEDIA_FORMAT_AAC;
                mc_handle->is_video = 0;
            break;
            default:
                LOGE("NOT SUPPORTED!!!!");
            break;
        }

        mc_handle->is_encoder = true;
    }
    else
    {
        switch(codec_id)
        {
            case MEDIACODEC_H264:
                mc_handle->supported_codec = GST_DECODE_H264;
                mc_handle->mimetype = MEDIA_FORMAT_NV12;
                mc_handle->is_video = 1;
                break;
            case MEDIACODEC_AAC:
                mc_handle->supported_codec = GST_DECODE_AAC;
                mc_handle->mimetype = MEDIA_FORMAT_PCM;
                mc_handle->is_video = 0;
                break;
            default:
                LOGE("NOT SUPPORTED!!!!");
            break;
        }

        // !!!! check if need to be dynamic
        mc_handle->is_encoder = false;
    }
#endif
    return ret;
}

int mc_set_vdec_info(MMHandleType mediacodec, int width, int height)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if ((width <= 0) || (height <= 0))
        return MC_PARAM_ERROR;

    MEDIACODEC_CHECK_CONDITION(mc_handle->codec_id && mc_handle->is_video && !mc_handle->is_encoder,
            MEDIACODEC_ERROR_INVALID_PARAMETER,"MEDIACODEC_ERROR_INVALID_PARAMETER");

    mc_handle->info.decoder.width = width;
    mc_handle->info.decoder.height = height;

    mc_handle->is_prepared = true;

    return ret;
}

int mc_set_venc_info(MMHandleType mediacodec, int width, int height, int fps, int target_bits)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
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

    return ret;
}

int mc_set_adec_info(MMHandleType mediacodec, int samplerate, int channel, int bit)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
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

int mc_set_aenc_info(MMHandleType mediacodec, int samplerate, int channel, int bit,  int bitrate)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
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
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if(!mc_handle->is_prepared)
        return MC_NOT_INITIALIZED;

    MEDIACODEC_CMD_LOCK( mediacodec );

    /* setting core details */
    switch ( mc_handle->port_type )
    {
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

    MEDIACODEC_CMD_UNLOCK( mediacodec );

    return ret;
}

int mc_unprepare(MMHandleType mediacodec)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    MEDIACODEC_CMD_LOCK( mediacodec );

    /* deinit core details */
    switch ( mc_handle->port_type )
    {
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

    MEDIACODEC_CMD_UNLOCK( mediacodec );

    return ret;
}

int mc_process_input(MMHandleType mediacodec, media_packet_h inbuf, uint64_t timeOutUs )
{
    int ret = MC_ERROR_NONE;
    uint64_t buf_size = 0;
    void *buf_data = NULL;
    bool eos = false;

    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;


    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    ret = media_packet_get_buffer_size(inbuf, &buf_size);
    if (ret != MEDIA_PACKET_ERROR_NONE)
    {
        LOGE("invaild input buffer");
        return MC_INVALID_IN_BUF;
    }

    ret = media_packet_get_buffer_data_ptr(inbuf, &buf_data);
    if (ret != MEDIA_PACKET_ERROR_NONE)
    {
        LOGE("invaild input buffer");
        return MC_INVALID_IN_BUF;
    }

    ret = media_packet_is_end_of_stream(inbuf, &eos);
    if (ret != MEDIA_PACKET_ERROR_NONE)
    {
        LOGE("invaild input buffer");
        return MC_INVALID_IN_BUF;
    }

    if(!eos)
    {
        if((buf_data == NULL) || (buf_size == 0))
        {
            LOGE("invaild input buffer");
            return MC_INVALID_IN_BUF;
        }
    }

    MEDIACODEC_CMD_LOCK( mediacodec );

    switch ( mc_handle->port_type )
    {
        case MEDIACODEC_PORT_TYPE_GENERAL:
        {
             //ret = mc_general_process_input(mc_handle->gen_core, inbuf, timeOutUs);
        }
        break;

        case MEDIACODEC_PORT_TYPE_OMX:
        {
            //ret = mc_omx_process_input(mc_handle, inbuf, timeOutUs);
        }
        break;

        case MEDIACODEC_PORT_TYPE_GST:
        {
            ret = mc_gst_process_input(mc_handle, inbuf, timeOutUs);
        }
        break;

        default:
            break;
    }

    MEDIACODEC_CMD_UNLOCK( mediacodec );

    return ret;
}

int mc_get_output(MMHandleType mediacodec, media_packet_h *outbuf, uint64_t timeOutUs)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    MEDIACODEC_CMD_LOCK( mediacodec );

    /* setting core details */
    switch ( mc_handle->port_type )
    {
        case MEDIACODEC_PORT_TYPE_GENERAL:
        {
            //ret= mc_general_get_output(mc_handle->gen_core, outbuf, timeOutUs);
        }
        break;

        case MEDIACODEC_PORT_TYPE_OMX:
        {
            //ret = mc_omx_get_output(mc_handle, outbuf, timeOutUs);
        }
        break;

        case MEDIACODEC_PORT_TYPE_GST:
        {
            ret = mc_gst_get_output(mc_handle, outbuf, timeOutUs);
        }
        break;

        default:
            break;
    }

    MEDIACODEC_CMD_UNLOCK( mediacodec );

    return ret;
}

int mc_set_empty_buffer_cb(MMHandleType mediacodec, mediacodec_input_buffer_used_cb callback, void* user_data)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if(mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER])
    {
        LOGE("Already set mediacodec_empty_buffer_cb\n");
        return MC_PARAM_ERROR;
    }
    else
    {
        if (!callback)
        {
            return MC_INVALID_ARG;
        }

        LOGD("Set empty buffer callback(cb = %p, data = %p)\n", callback, user_data);

        mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER] = (mc_empty_buffer_cb) callback;
        mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER] = user_data;
        return MC_ERROR_NONE;
    }

    return ret;
}
int mc_unset_empty_buffer_cb(MMHandleType mediacodec)
{
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER] = NULL;
    mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER] = NULL;

    return MC_ERROR_NONE;
}

int mc_set_fill_buffer_cb(MMHandleType mediacodec, mediacodec_output_buffer_available_cb callback, void* user_data)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if(mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_FILLBUFFER])
    {
        LOGE("Already set mediacodec_fill_buffer_cb\n");
        return MC_PARAM_ERROR;
    }
    else
    {
        if (!callback) {
            return MC_INVALID_ARG;
        }

        LOGD("Set fill buffer callback(cb = %p, data = %p)\n", callback, user_data);

        mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_FILLBUFFER] = (mc_fill_buffer_cb) callback;
        mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_FILLBUFFER] = user_data;
        return MC_ERROR_NONE;
    }

    return ret;
}
int mc_unset_fill_buffer_cb(MMHandleType mediacodec)
{
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_FILLBUFFER] = NULL;
    mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_FILLBUFFER] = NULL;

    return MC_ERROR_NONE;
}

int mc_set_error_cb(MMHandleType mediacodec, mediacodec_error_cb callback, void* user_data)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if(mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR])
    {
        LOGE("Already set mediacodec_fill_buffer_cb\n");
        return MC_PARAM_ERROR;
    }
    else
    {
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
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR] = NULL;
    mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_ERROR] = NULL;

    return MC_ERROR_NONE;
}

int mc_set_eos_cb(MMHandleType mediacodec, mediacodec_eos_cb callback, void* user_data)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if(mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])
    {
        LOGE("Already set mediacodec_fill_buffer_cb\n");
        return MC_PARAM_ERROR;
    }
    else
    {
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
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    mc_handle->user_cb[_MEDIACODEC_EVENT_TYPE_EOS] = NULL;
    mc_handle->user_data[_MEDIACODEC_EVENT_TYPE_EOS] = NULL;

    return MC_ERROR_NONE;
}
/*
gboolean _mc_check_is_supported(mc_handle_t* mc_handle, mediacodec_codec_type_e codec_id, mediacodec_support_type_e flags)
{
    int i=0;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return FALSE;
    }

    for (i = 0; i < MC_MAX_NUM_CODEC; i++)
    {
        if (mc_handle->g_media_codec_spec_emul[i].mime == codec_id)
        {
            if (mc_handle->g_media_codec_spec_emul[i].codec_type & (flags & 0x3))
            {
                if (mc_handle->g_media_codec_spec_emul[i].support_type & (flags & 0xC))
                {
                    mc_handle->port_type = mc_handle->g_media_codec_spec_emul[i].port_type;
                    LOGD("port type : %d", mc_handle->port_type);
                    return TRUE;
                }
            }

        }
    }

	return FALSE;
}
*/
