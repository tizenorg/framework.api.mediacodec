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

static gboolean _mc_check_is_supported(mc_handle_t* mc_handle, mediacodec_codec_type_e codec_id, mediacodec_support_type_e flags);

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
    new_mediacodec->is_codec_config = false;
    new_mediacodec->output_allocated = false;
    new_mediacodec->is_prepared = false;
    new_mediacodec->codec_id = MEDIACODEC_NONE;

    new_mediacodec->gst_ports[0] = NULL;
    new_mediacodec->gst_ports[1] = NULL;

    new_mediacodec->gst_core = NULL;
    new_mediacodec->dec_info = NULL;
    new_mediacodec->enc_info = NULL;

    memcpy(new_mediacodec->g_media_codec_spec_emul, spec_emul, sizeof(mc_codec_spec_t)*MC_MAX_NUM_CODEC);

    *mediacodec = (MMHandleType)new_mediacodec;

    return ret;

    // TO DO
ERROR:
    if ( new_mediacodec )
    {
        // TO DO
        // If we need destroy and release for others (cmd, mutex..)
        free(new_mediacodec);
        new_mediacodec = NULL;
        return MC_INVALID_ARG;
    }

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

    if(mc_handle->gst_core != NULL)
    {
        if(mc_gst_unprepare(mc_handle->gst_core) != MC_ERROR_NONE)
        {
            LOGE("mc_gst_unprepare() failed");
            return MC_ERROR;
        }
        mc_gst_core_free(mc_handle->gst_core);
        mc_handle->gst_core = NULL;
    }

    mc_handle->is_prepared = false;

    g_free(mc_handle->dec_info);
    g_free(mc_handle->enc_info);


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

    if(!_mc_check_is_supported(mc_handle, codec_id, flags))
        return MC_NOT_SUPPORTED;

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
            MC_PARAM_ERROR,"MEDIACODEC_ERROR_INVALID_PARAMETER");

    if (mc_handle->dec_info == NULL)
    {
        mc_handle->dec_info = g_new0(mc_decoder_info_t, 1);
    }

    mc_handle->dec_info->width = width;
    mc_handle->dec_info->height = height;

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
                        MC_PARAM_ERROR, "MEDIACODEC_ERROR_INVALID_PARAMETER");

    if(mc_handle->enc_info == NULL)
    {
        mc_handle->enc_info = g_new0(mc_encoder_info_t, 1);
    }

    mc_handle->enc_info->frame_width = width;
    mc_handle->enc_info->frame_height = height;
    mc_handle->enc_info->fps = fps;
    mc_handle->enc_info->bitrate = target_bits;

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
            MC_PARAM_ERROR, "MEDIACODEC_ERROR_INVALID_PARAMETER");

    if (mc_handle->dec_info == NULL)
    {
        mc_handle->dec_info = g_new0(mc_decoder_info_t, 1);
    }

    mc_handle->dec_info->samplerate = samplerate;
    mc_handle->dec_info->channel = channel;
    mc_handle->dec_info->bit = bit;

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
                        MC_PARAM_ERROR, "MEDIACODEC_ERROR_INVALID_PARAMETER");

    if(mc_handle->enc_info == NULL)
    {
        mc_handle->enc_info = g_new0(mc_encoder_info_t, 1);
    }

    mc_handle->enc_info->samplerate = samplerate;
    mc_handle->enc_info->channel = channel;
    mc_handle->enc_info->bit = bit;
    mc_handle->enc_info->bitrate = bitrate;

    mc_handle->is_prepared = true;

    return ret;
}

int mc_prepare(MMHandleType mediacodec)
{
    int ret = MC_ERROR_NONE;
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;
    media_format_mimetype_e mimetype;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    if(!mc_handle->is_prepared)
        return MC_NOT_INITIALIZED;

    /* setting core details */
    switch ( mc_handle->port_type )
    {
        case MEDIACODEC_PORT_TYPE_GENERAL:
        {
            /*
            //!!!! need to set data to omx/gen core need to seperate
            mc_handle->gen_core->output_fmt = mc_handle->output_fmt;
            mc_handle->gen_core->encoder = mc_handle->is_encoder;

            if(mc_handle->is_encoder)
            {
            mc_encoder_info_t *info;

            info = mc_handle->enc_info;
            mc_handle->gen_core->enc_info = mc_handle->enc_info;

            media_format_set_video_info(mc_handle->output_fmt, mc_handle->mimetype, info->frame_width, info->frame_height, info->bitrate, 0);
            }
            else
            {
            mc_decoder_info_t *info;

            info = mc_handle->dec_info;
            mc_handle->gen_core->dec_info = mc_handle->dec_info;
            media_format_set_video_info(mc_handle->output_fmt, mc_handle->mimetype, info->width, info->height, 0, 0);
            }

            ret = mc_general_init(mc_handle->gen_core);
            */
        }
        break;

        case MEDIACODEC_PORT_TYPE_OMX:
        {
            //et = mc_omx_init(mc_handle);
            //ret = mc_omx_create_handle(mc_handle);
        }
        break;

        case MEDIACODEC_PORT_TYPE_GST:
        {
            int i;
            int codec_list;
            static const mc_codec_map_t *codec_map;

            mc_gst_core_t* new_core = mc_gst_core_new();
            // !!!! null check

            new_core->encoder = mc_handle->is_encoder;
            new_core->is_hw = mc_handle->is_hw;
            new_core->video = mc_handle->is_video;
            new_core->codec_id = mc_handle->codec_id;

            /* setting internal callbacks */
            for (i = 0; i < _MEDIACODEC_EVENT_TYPE_INTERNAL_FILLBUFFER ; i++)
            {
                LOGD("copy cb function [%d]", i);
                if (mc_handle->user_cb[i])
                {
                    new_core->user_cb[i] = mc_handle->user_cb[i];
                    new_core->user_data[i] = mc_handle->user_data[i];
                    LOGD("user_cb[%d] %p, %p", i, new_core->user_cb[i], mc_handle->user_cb[i]);
                }
            }

            if(new_core->output_fmt == NULL)
            {
                if(media_format_create(&new_core->output_fmt) != MEDIA_FORMAT_ERROR_NONE)
                {
                    LOGE("media format create failed");
                }
                LOGD("pkt_fmt is allocated");
            }

            if(new_core->encoder)
            {
                codec_list = sizeof(encoder_map) / sizeof(encoder_map[0]);
                codec_map = encoder_map;
            }
            else
            {
                codec_list = sizeof(decoder_map) / sizeof(decoder_map[0]);
                codec_map = decoder_map;
            }

            for(i = 0; i < codec_list; i++)
            {
                if((new_core->codec_id == codec_map[i].id) && (new_core->is_hw == codec_map[i].hardware))
                    break;
            }

            if(i == codec_list)
                return MC_NOT_SUPPORTED;

            new_core->factory_name = codec_map[i].type.factory_name;

            mimetype = codec_map[i].type.out_format;

            new_core->mime = codec_map[i].type.mime;

            LOGD("factory name : %s, output_fmt : %x mime : %s", new_core->factory_name, mimetype, new_core->mime);

            if(mc_handle->is_encoder)
            {
                mc_encoder_info_t *info;

                info = mc_handle->enc_info;
                new_core->enc_info = mc_handle->enc_info;

                if (new_core->video)
                {
                    media_format_set_video_mime(new_core->output_fmt, mimetype);
                    media_format_set_video_width(new_core->output_fmt, info->frame_width);
                    media_format_set_video_height(new_core->output_fmt, info->frame_height);
                    media_format_set_video_avg_bps(new_core->output_fmt, info->bitrate);
                }
                else
                {
                    media_format_set_audio_mime(new_core->output_fmt, mimetype);
                    media_format_set_audio_channel(new_core->output_fmt, info->channel);
                    media_format_set_audio_samplerate(new_core->output_fmt, info->samplerate);
                    media_format_set_audio_bit(new_core->output_fmt, info->bit);
                    media_format_set_audio_avg_bps(new_core->output_fmt, info->bitrate);
                }

            }
            else
            {
                mc_decoder_info_t *info;

                info = mc_handle->dec_info;
                new_core->dec_info = mc_handle->dec_info;
                if (new_core->video)
                {
                    media_format_set_video_mime(new_core->output_fmt, mimetype);
                    media_format_set_video_width(new_core->output_fmt, info->width);
                    media_format_set_video_height(new_core->output_fmt, info->height);
                }
                else
                {
                    media_format_set_audio_mime(new_core->output_fmt, mimetype);
                    media_format_set_audio_channel(new_core->output_fmt, info->channel);
                    media_format_set_audio_samplerate(new_core->output_fmt, info->samplerate);
                    media_format_set_audio_bit(new_core->output_fmt, info->bit);
                }

            }

            LOGD("is_encoder (%d)  is_video (%d)",  new_core->encoder, new_core->video);
            mc_handle->gst_core = new_core;
            mc_gst_prepare(mc_handle->gst_core);
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
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

    /* deinit core details */
    switch ( mc_handle->port_type )
    {
        case MEDIACODEC_PORT_TYPE_GENERAL:
        {
            //ret = mc_general_deinit(mc_handle->gen_core);
        }
        break;

        case MEDIACODEC_PORT_TYPE_OMX:
        {
            //ret = mc_omx_deinit(mc_handle);
        }
        break;

        case MEDIACODEC_PORT_TYPE_GST:
        {
            ret = mc_gst_unprepare(mc_handle->gst_core);

            if(mc_handle->gst_core != NULL)
            {
                mc_gst_core_free(mc_handle->gst_core);
                mc_handle->gst_core = NULL;
            }
        }
        break;

        default:
            break;
    }

    return ret;
}

int mc_process_input(MMHandleType mediacodec, media_packet_h inbuf, uint64_t timeOutUs )
{
    int ret = MC_ERROR_NONE;
    uint64_t buf_size = 0;
    void *buf_data = NULL;

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

    if((buf_data == NULL) || (buf_size == 0))
    {
        LOGE("invaild input buffer");
        return MC_INVALID_IN_BUF;
    }

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
            ret = mc_gst_process_input(mc_handle->gst_core, inbuf, timeOutUs);
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
    mc_handle_t* mc_handle = (mc_handle_t*) mediacodec;

    if (!mc_handle)
    {
        LOGE("fail invaild param\n");
        return MC_INVALID_ARG;
    }

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
            ret = mc_gst_get_output(mc_handle->gst_core, outbuf, timeOutUs);
        }
        break;

        default:
            break;
    }

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

