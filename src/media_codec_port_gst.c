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
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlog.h>
#include <media_codec_queue.h>
#include <media_codec_port_gst.h>
#include <media_codec_util.h>

#include <gst/gst.h>
#include <gst/gstelement.h>
#include <gst/app/gstappsrc.h>

#ifdef TIZEN_PROFILE_LITE
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/ion.h>
#endif

#define GST_MC_EVENT_CODEC_DATA "GstEventCodecData"
/*
* Internal Implementation
*/
static gpointer feed_task(gpointer data);
static media_packet_h _mc_get_input_buffer(mc_gst_core_t *core);

static gboolean __mc_gst_init_gstreamer();
static int _mc_output_media_packet_new(mc_gst_core_t *core, bool video, bool encoder, media_format_mimetype_e out_mime);
static mc_ret_e _mc_gst_create_pipeline(mc_gst_core_t* core, gchar *factory_name);
static mc_ret_e _mc_gst_destroy_pipeline(mc_gst_core_t *core);
static void __mc_gst_buffer_add (GstElement *element, GstBuffer *buffer, GstPad *pad, gpointer data);
static int __mc_output_buffer_finalize_cb(media_packet_h packet, int error_code, void *user_data);
static int __mc_output_packet_buffer_finalize_cb(media_packet_h packet, int error_code, void *user_data);

static void _mc_gst_update_caps(mc_gst_core_t *core, media_packet_h pkt, GstCaps **caps);
static mc_gst_buffer_t* _mc_gst_media_packet_to_gstbuffer(mc_gst_core_t* core, GstCaps **caps, media_packet_h pkt, bool codec_config);
static int _mc_gst_gstbuffer_to_appsrc(mc_gst_core_t *core, mc_gst_buffer_t *buff);
static guint32 __mc_get_gst_input_format(media_packet_h packet, bool is_hw);
static media_packet_h __mc_gst_gstbuffer_to_media_packet(mc_gst_core_t* core, GstBuffer* buffer);
static gboolean __mc_gst_bus_callback(GstBus *bus, GstMessage *msg, gpointer data);
static GstBusSyncReply __mc_gst_bus_sync_callback(GstBus * bus, GstMessage *msg, gpointer data);
static SCMN_IMGB* __mc_gst_make_tbm_buffer(mc_gst_core_t* core, media_packet_h pkt);
static gboolean event_probe_cb(GstPad *pad, GstEvent *event, gpointer user_data);
static GType __mc_gst_buffer_get_type(void);
static void __mc_gst_buffer_class_init(gpointer g_class, gpointer class_data);
static mc_gst_buffer_t* __mc_gst_buffer_new(mc_gst_core_t* core);
static void __mc_gst_buffer_finalize(mc_gst_buffer_t *buffer);

static gint __gst_handle_stream_error(mc_gst_core_t* core, GError* error, GstMessage * message);
static gint __gst_transform_gsterror( mc_gst_core_t *core, GstMessage * message, GError* error );
static gint __gst_handle_resource_error(mc_gst_core_t* core, int code );
static gint __gst_handle_library_error(mc_gst_core_t* core, int code);
static gint __gst_handle_core_error(mc_gst_core_t* core, int code );

static int _mc_link_vtable(mc_gst_core_t *core, mediacodec_codec_type_e id, gboolean is_encoder, gboolean is_hw);
#ifdef TIZEN_PROFILE_LITE
static int __tbm_get_physical_addr_bo(tbm_bo_handle tbm_bo_handle_fd_t, int* phy_addr, int* phy_size);
#endif
static void _mc_gst_set_flush_input(mc_gst_core_t *core);
static void _mc_gst_set_flush_output(mc_gst_core_t *core);

static GstBufferClass *mc_gst_buffer_parent_class = NULL;

#define GST_TYPE_MC_BUFFER (__mc_gst_buffer_get_type())

//static uint64_t wait_until = g_get_monotonic_time() + G_TIME_SPAN_SECOND / 2;

int(*vdec_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet,  &__mc_vdec_caps};
int(*venc_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet, &__mc_venc_caps};
int(*adec_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet, &__mc_adec_caps};
int(*aenc_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet, &__mc_aenc_caps};
int(*mp3dec_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_packet, &__mc_mp3dec_caps};
int(*h264dec_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_bo, &__mc_vdec_caps};
int(*h264enc_vtable[])() = {&__mc_fill_inbuf_with_bo, &__mc_fill_outbuf_with_packet, &__mc_venc_caps};
int(*h263dec_vtable[])() = {&__mc_fill_inbuf_with_packet, &__mc_fill_outbuf_with_bo, &__mc_vdec_caps};
int(*h263enc_vtable[])() = {&__mc_fill_inbuf_with_bo, &__mc_fill_outbuf_with_packet, &__mc_h263enc_caps};

/*
 * mc_gst_object functions
*/
int __mc_fill_input_buffer(mc_gst_core_t *core, mc_gst_buffer_t *buff)
{
    return core->vtable[fill_inbuf](core, buff);
}

int __mc_fill_output_buffer(mc_gst_core_t *core, GstBuffer* buff, media_packet_h *out_pkt)
{
    return core->vtable[fill_outbuf](core, buff, out_pkt);
}

int __mc_create_caps(mc_gst_core_t *core, GstCaps **caps)
{
    return core->vtable[create_caps](core, caps);
}

int __mc_fill_inbuf_with_bo(mc_gst_core_t *core, mc_gst_buffer_t *buff)
{
    int ret = MC_ERROR_NONE;
    void* buf_data = NULL;
    uint64_t buf_size = 0;

    MEDIACODEC_FENTER();

    if (!buff->pkt)
    {
        LOGE("output is null");
        return MC_INTERNAL_ERROR;
    }

    /* copy media_packet to new gstbuffer */
    ret = media_packet_get_buffer_size(buff->pkt, &buf_size);
    if (ret != MEDIA_PACKET_ERROR_NONE)
    {
        LOGW("buffer size get fail");
    }

    ret = media_packet_get_buffer_data_ptr(buff->pkt, &buf_data);
    if (ret != MEDIA_PACKET_ERROR_NONE)
    {
        LOGW("buffer size get fail");
        return MC_INTERNAL_ERROR;
    }

    SCMN_IMGB *imgb = NULL;
    GST_BUFFER_DATA(buff) = buf_data;
    GST_BUFFER_SIZE (buff) = buf_size;
    imgb = __mc_gst_make_tbm_buffer(core, buff->pkt);

    if (!imgb)
    {
        LOGE("get imgb failed");
        return MC_INTERNAL_ERROR;
    }
    GST_BUFFER_MALLOCDATA(buff) = (guint8 *)imgb;

    LOGD("__mc_fill_inbuf_with_bo :%llu", buf_size);

    MEDIACODEC_FLEAVE();

    return ret;
}

int __mc_fill_inbuf_with_packet(mc_gst_core_t *core, mc_gst_buffer_t *buff)
{
    int ret = MEDIA_PACKET_ERROR_NONE;
    void* buf_data = NULL;
    uint64_t buf_size = 0;

    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    MEDIACODEC_FENTER();

    /* copy media_packet to new gstbuffer */
    ret = media_packet_get_buffer_size(buff->pkt, &buf_size);
    if (ret != MEDIA_PACKET_ERROR_NONE)
    {
        LOGW("buffer size get fail");
        return ret;
    }

    ret = media_packet_get_buffer_data_ptr(buff->pkt, &buf_data);
    if (ret != MEDIA_PACKET_ERROR_NONE)
    {
        LOGW("buffer size get fail");
        return ret;
    }

    //mc_hex_dump("nal", buf_data, 8);
    GST_BUFFER_DATA(buff) = buf_data;
    GST_BUFFER_SIZE (buff) = buf_size;

    LOGD("filled with packet,%d, %p", (int)buf_size, buf_data);
    MEDIACODEC_FLEAVE();

    return ret;
}

int __mc_fill_outbuf_with_bo(mc_gst_core_t *core, GstBuffer *buff, media_packet_h* out_pkt)
{
    int i = 0;;
    int bo_num = 0;

	  g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);
	  g_return_val_if_fail (buff != NULL, MC_PARAM_ERROR);

    MEDIACODEC_FENTER();

    SCMN_IMGB *psimgb = NULL;
    tbm_surface_h tsurf = NULL;
    psimgb = (SCMN_IMGB*)GST_BUFFER_MALLOCDATA(buff);
    mc_decoder_info_t *codec_info = (mc_decoder_info_t *)core->codec_info;
#ifdef TBM_API_CHANGE
    tbm_surface_info_s tsurf_info;
    memset(&tsurf_info, 0x0, sizeof(tbm_surface_info_s));
#endif

    /* create tbm surface */
    for (i = 0; i < SCMN_IMGB_MAX_PLANE; i++)
    {
        if (psimgb->bo[i])
        {
            bo_num++;
#ifdef TBM_API_CHANGE
            tsurf_info.planes[i].stride = psimgb->s[i];
#endif
        }
    }

    if (bo_num > 0)
    {
#ifdef TBM_API_CHANGE
        tsurf_info.width = codec_info->width;
        tsurf_info.height = codec_info->height;
        tsurf_info.format = TBM_FORMAT_NV12;          // bo_format
        tsurf_info.bpp = tbm_surface_internal_get_bpp(TBM_FORMAT_NV12);
        tsurf_info.num_planes = tbm_surface_internal_get_num_planes(TBM_FORMAT_NV12);
        tsurf_info.size = 0;

        for(i = 0; i < tsurf_info.num_planes; i++) {
            tsurf_info.planes[i].stride = psimgb->s[i];
            tsurf_info.planes[i].size = psimgb->s[i] * psimgb->e[i];

            if(i < bo_num)
                tsurf_info.planes[i].offset = 0;
            else
                tsurf_info.planes[i].offset = tsurf_info.planes[i - 1].size;
            tsurf_info.size += tsurf_info.planes[i].size;
        }

        tsurf = tbm_surface_internal_create_with_bos(&tsurf_info, (tbm_bo *)psimgb->bo, bo_num);
        LOGD("[NEW API] tbm surface %p", tsurf);
#else
        tsurf = tbm_surface_internal_create_with_bos(codec_info->width, codec_info->height, TBM_FORMAT_NV12, (tbm_bo *)psimgb->bo, bo_num);
        LOGD("[OLD  API] tbm surface %p", tsurf);
#endif
    }

    if (tsurf)
    {
        media_packet_create_from_tbm_surface(core->output_fmt, tsurf, (media_packet_finalize_cb)__mc_output_buffer_finalize_cb, core, out_pkt);
        LOGD("output pkt = %p", *out_pkt);
        media_packet_set_extra(*out_pkt, buff);
        media_packet_set_buffer_size(*out_pkt, GST_BUFFER_SIZE(buff));
        media_packet_set_pts(*out_pkt, GST_BUFFER_TIMESTAMP(buff));
    }

    MEDIACODEC_FLEAVE();

    return MC_ERROR_NONE;
}


int __mc_fill_outbuf_with_packet(mc_gst_core_t *core, GstBuffer *buff, media_packet_h *out_pkt)
{
    void* pkt_data = NULL;

    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);
    g_return_val_if_fail (buff != NULL, MC_PARAM_ERROR);

    MEDIACODEC_FENTER();

    media_packet_create_alloc(core->output_fmt, __mc_output_packet_buffer_finalize_cb, core, out_pkt);

    if (!out_pkt)
    {
        LOGE("out_pkt is null");
        return MC_MEMORY_ERROR;
    }

    media_packet_set_extra(*out_pkt, buff);
    LOGI("create_gst_buffer = %p", buff);
    LOGI("gstbuffer refcount %d", GST_MINI_OBJECT_REFCOUNT_VALUE(buff));

    // !!!! this time do memcpy because no way to set data ptr to media packet.
    media_packet_set_buffer_size(*out_pkt, GST_BUFFER_SIZE(buff));
    media_packet_get_buffer_data_ptr(*out_pkt, &pkt_data);

    if (!pkt_data)
    {
        media_packet_destroy(*out_pkt);
        return MC_OUTPUT_BUFFER_EMPTY;
    }

    memcpy(pkt_data, GST_BUFFER_DATA(buff), GST_BUFFER_SIZE(buff));
    media_packet_set_pts(*out_pkt, GST_BUFFER_TIMESTAMP(buff));

    MEDIACODEC_FLEAVE();

    return MC_ERROR_NONE;
}

int __mc_venc_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    MEDIACODEC_FENTER();

    mc_encoder_info_t *enc_info = (mc_encoder_info_t*)core->codec_info;

    *caps = gst_caps_new_simple(core->mime,
                               "format", GST_TYPE_FOURCC, core->format,
                               "width", G_TYPE_INT, enc_info->width,
                               "height", G_TYPE_INT, enc_info->height,
                               "framerate", GST_TYPE_FRACTION, enc_info->fps, 1, NULL);

    g_object_set (GST_OBJECT(core->codec), "byte-stream", TRUE, NULL);
    g_object_set (GST_OBJECT(core->codec), "bitrate", enc_info->bitrate*1000, NULL);

    LOGD("%d, %d, %d, %d", core->format, enc_info->width, enc_info->height, enc_info->fps);

    MEDIACODEC_FLEAVE();

    return MC_ERROR_NONE;
}

int __mc_h263enc_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_encoder_info_t *enc_info = (mc_encoder_info_t*)core->codec_info;

    MEDIACODEC_FENTER();

    *caps = gst_caps_new_simple(core->mime,
                               "format", GST_TYPE_FOURCC, core->format,
                               "width", G_TYPE_INT, enc_info->width,
                               "height", G_TYPE_INT, enc_info->height,
                               "framerate", GST_TYPE_FRACTION, enc_info->fps, 1, NULL);

    g_object_set (GST_OBJECT(core->codec), "bitrate", enc_info->bitrate*1000, NULL);

    LOGD("%d, %d, %d, %d", core->format, enc_info->width, enc_info->height, enc_info->fps);

    MEDIACODEC_FLEAVE();

    return MC_ERROR_NONE;
}

int __mc_mp3dec_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    MEDIACODEC_FENTER();

    mc_decoder_info_t *dec_info = (mc_decoder_info_t*)core->codec_info;

    *caps = gst_caps_new_simple(core->mime,
                                "mpegversion", G_TYPE_INT, 1,
                                "layer", G_TYPE_INT, 3,
                                "channels", G_TYPE_INT, dec_info->channel,
                                "rate", G_TYPE_INT, dec_info->samplerate,
                                NULL);

    MEDIACODEC_FLEAVE();

    return MC_ERROR_NONE;
}

int __mc_vdec_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    MEDIACODEC_FENTER();

    mc_decoder_info_t *dec_info = (mc_decoder_info_t*)core->codec_info;

    LOGD("%d, %d, ", dec_info->width, dec_info->height);
    *caps = gst_caps_new_simple(core->mime,
                               "width", G_TYPE_INT, dec_info->width,
                               "height", G_TYPE_INT, dec_info->height,
                               "framerate", GST_TYPE_FRACTION, 30, 1, NULL);

    LOGD("mime : %s, widht :%d, height : %d", core->mime, dec_info->width, dec_info->height);

    MEDIACODEC_FLEAVE();

    return MC_ERROR_NONE;
}

int __mc_aenc_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_encoder_info_t *enc_info = (mc_encoder_info_t*)core->codec_info;

    MEDIACODEC_FENTER();

    *caps = gst_caps_new_simple(core->mime,
                                "signed", G_TYPE_BOOLEAN, TRUE,
                                "width", G_TYPE_INT, enc_info->bit,
                                "depth", G_TYPE_INT, enc_info->bit,
                                "endianness", G_TYPE_INT, G_BYTE_ORDER,
                                "channels", G_TYPE_INT, enc_info->channel,
                                "rate", G_TYPE_INT, enc_info->samplerate, NULL);

    MEDIACODEC_FLEAVE();

    return MC_ERROR_NONE;
}

int __mc_adec_caps(mc_gst_core_t *core, GstCaps **caps)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    mc_decoder_info_t *dec_info = (mc_decoder_info_t*)core->codec_info;

    MEDIACODEC_FENTER();

    *caps = gst_caps_new_simple(core->mime,
                                "mpegversion", G_TYPE_INT, 4,
                                "channels", G_TYPE_INT, dec_info->channel,
                                "rate", G_TYPE_INT, dec_info->samplerate,
                                NULL);

    MEDIACODEC_FLEAVE();

    return MC_ERROR_NONE;
}

int _mc_output_media_packet_new(mc_gst_core_t *core, bool video, bool encoder, media_format_mimetype_e out_mime)
{
    MEDIACODEC_FENTER();

    if(media_format_create(&core->output_fmt) != MEDIA_FORMAT_ERROR_NONE)
    {
        LOGE("media format create failed");
        return MC_ERROR;
    }

    if(encoder)
    {
        mc_encoder_info_t *info;

        info = (mc_encoder_info_t*)core->codec_info;

        if (video)
        {
            media_format_set_video_mime(core->output_fmt, out_mime);
            media_format_set_video_width(core->output_fmt, info->width);
            media_format_set_video_height(core->output_fmt, info->height);
            media_format_set_video_avg_bps(core->output_fmt, info->bitrate);
        }
        else
        {
            media_format_set_audio_mime(core->output_fmt, out_mime);
            media_format_set_audio_channel(core->output_fmt, info->channel);
            media_format_set_audio_samplerate(core->output_fmt, info->samplerate);
            media_format_set_audio_bit(core->output_fmt, info->bit);
            media_format_set_audio_avg_bps(core->output_fmt, info->bitrate);
        }
    }
    else
    {
        mc_decoder_info_t *info;

        info = (mc_decoder_info_t*)core->codec_info;

        if (video)
        {
            media_format_set_video_mime(core->output_fmt, out_mime);
            media_format_set_video_width(core->output_fmt, info->width);
            media_format_set_video_height(core->output_fmt, info->height);
        }
        else
        {
            media_format_set_audio_mime(core->output_fmt, out_mime);
            media_format_set_audio_channel(core->output_fmt, info->channel);
            media_format_set_audio_samplerate(core->output_fmt, info->samplerate);
            media_format_set_audio_bit(core->output_fmt, info->bit);
        }
    }

    MEDIACODEC_FLEAVE();

    return MC_ERROR_NONE;
}

/*
 * mc_gst_core functions
*/
mc_gst_core_t *mc_gst_core_new()
{
    mc_gst_core_t *core;

    MEDIACODEC_FENTER();

    core = g_new0(mc_gst_core_t, 1);

    /* 0 : input, 1 : output */
    core->ports[0] = NULL;
    core->ports[1] = mc_gst_port_new(core);
    core->ports[1]->index = 1;

    core->available_queue = g_new0(mc_aqueue_t, 1);
    core->available_queue->input = mc_async_queue_new();

    //core->eos_mutex = g_mutex_new();
    g_mutex_init(&core->eos_mutex);
    //core->eos_wait_mutex = g_mutex_new();
    g_mutex_init(&core->eos_wait_mutex);
    //core->drain_mutex = g_mutex_new();
    g_mutex_init(&core->drain_mutex);
    //core->eos_cond = g_cond_new();
    g_cond_init(&core->eos_cond);
    //core->eos_waiting_cond = g_cond_new();
    g_cond_init(&core->eos_waiting_cond);
    //core->prepare_lock = g_mutex_new();
    g_mutex_init(&core->prepare_lock);
    //core->push_sem = mc_sem_new();
    //core->pop_sem = mc_sem_new();

    core->need_feed = false;
    core->eos = false;
    core->eos_waiting = false;
    core->prepare_count = 0;
    //g_atomic_int_set(&core->num_live_buffers, 0);

    g_atomic_int_set(&core->available_queue->running, 1);
    //core->available_queue->thread = g_thread_create(feed_task, core, TRUE, NULL);
    core->available_queue->thread = g_thread_new("feed thread", &feed_task, core);

    MEDIACODEC_FLEAVE();

    return core;
}

void mc_gst_core_free(mc_gst_core_t *core)
{
    mc_aqueue_t *async_queue;

    MEDIACODEC_FENTER();

    async_queue = core->available_queue;

    _mc_gst_set_flush_input(core);
    mc_async_queue_disable(async_queue->input);
    //mc_async_queue_flush(async_queue->input);

    g_atomic_int_set(&async_queue->running, 0);
    g_thread_join(async_queue->thread);
    LOGD("@%p g_thread_join", core);

    //mc_sem_free(core->push_sem);
    //mc_sem_free(core->pop_sem);
	  //g_mutex_free(core->drain_mutex);
	  g_mutex_clear(&core->drain_mutex);
    //g_mutex_free(core->eos_mutex);
    g_mutex_clear(&core->eos_mutex);
    //g_mutex_free(core->eos_wait_mutex);
    g_mutex_clear(&core->eos_wait_mutex);
    //g_mutex_free(core->prepare_lock);
    g_mutex_clear(&core->prepare_lock);
    //g_cond_free(core->eos_cond);
    g_cond_clear(&core->eos_cond);
    //g_cond_free(core->eos_waiting_cond);
    g_cond_clear(&core->eos_waiting_cond);


    mc_async_queue_free(async_queue->input);
    //mc_async_queue_free(async_queue->output);
    //g_queue_free(core->output_queue);

    if(core->ports[1] != NULL)
    {
        mc_gst_port_free(core->ports[1]);
        core->ports[1] = NULL;
    }

    g_free(core);

    MEDIACODEC_FLEAVE();
}

/*
 * mc_gst_port functions
 */
mc_gst_port_t *mc_gst_port_new(mc_gst_core_t *core)
{
    mc_gst_port_t *port;

    MEDIACODEC_FENTER();

    port = g_new0(mc_gst_port_t, 1);
    port->core = core;
    port->num_buffers = -1;
    port->buffer_size = 0;
    port->is_allocated = 0;
    port->buffers = NULL;

    //port->mutex = g_mutex_new();
    g_mutex_init(&port->mutex);
    //port->buffer_cond = g_cond_new();
    g_cond_init(&port->buffer_cond);
    port->queue = g_queue_new();

    MEDIACODEC_FLEAVE();

    return port;
}

void mc_gst_port_free(mc_gst_port_t *port)
{
    MEDIACODEC_FENTER();

    g_mutex_clear(&port->mutex);
    g_cond_clear(&port->buffer_cond);
    g_queue_free(port->queue);

    //destroy buffers
    g_free(port);

    MEDIACODEC_FLEAVE();

    return;
}

static void _mc_gst_update_caps(mc_gst_core_t *core, media_packet_h pkt, GstCaps **caps)
{
    //TODO remove is_hw param
    core->format = __mc_get_gst_input_format(pkt, core->is_hw);

    __mc_create_caps(core, caps);
}

static gboolean event_probe_cb(GstPad *pad, GstEvent *event, gpointer user_data)
{
    mc_gst_core_t *core = (mc_gst_core_t*)user_data;
    const GstStructure *s;
    gboolean codec_config = false;
    //uint64_t wait_until = g_get_monotonic_time() + G_TIME_SPAN_SECOND / 2;

    switch(GST_EVENT_TYPE (event))
    {
        case GST_EVENT_CUSTOM_DOWNSTREAM:
        {
            s = gst_event_get_structure (event);
            if(gst_structure_has_name (s, GST_MC_EVENT_CODEC_DATA))
            {
                gst_structure_get_boolean (s, "codec_config", &codec_config);
                core->codec_config = codec_config;
                LOGD("codec_config : %d", codec_config);
            }
            break;
        }
#if 0
        case GST_EVENT_EOS:
        {
            g_mutex_lock(core->eos_mutex);

            core->eos = true;
            g_cond_signal(core->eos_cond);
            LOGD("send eos signal");

            g_mutex_unlock(core->eos_mutex);
            LOGD ("End of stream\n");
            if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])
            {
                ((mc_eos_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])(core->user_data[_MEDIACODEC_EVENT_TYPE_EOS]);
            }
            g_mutex_lock(core->eos_mutex);
/*
            while(!core->eos)
            {
                LOGD("waiting for eos signal...");
                //g_cond_wait(core->eos_cond, core->eos_mutex);
                if(!g_cond_wait_until(core->eos_cond, core->eos_mutex, wait_until))
                {
                    core->eos = true;
                    LOGD("time out");

                    if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])
                    {
                        ((mc_eos_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])(core->user_data[_MEDIACODEC_EVENT_TYPE_EOS]);
                    }

                }
                else
                    LOGD("recevied signal");
            }

            //_mc_gst_set_flush_input(core);
            core->eos = false;
            LOGD("eos flag set to false");

            g_mutex_unlock(core->eos_mutex);
*/
            break;

        }
#endif
        default:
            break;
    }
    return true;
}

static gpointer feed_task(gpointer data)
{
    mc_gst_core_t *core = (mc_gst_core_t*)data;
    int ret = MC_ERROR_NONE;
    bool codec_config = FALSE;
    bool eos = FALSE;
    media_packet_h in_buf = NULL;
    mc_gst_buffer_t* buff = NULL;
    GstCaps *new_caps = NULL;
    bool initiative = true;
    //uint64_t wait_until = g_get_monotonic_time() + G_TIME_SPAN_SECOND / 2;
    MEDIACODEC_FENTER();

    while(g_atomic_int_get(&core->available_queue->running))
    {
        LOGD("waiting for next input....");
        in_buf = _mc_get_input_buffer(core);

        if(!in_buf)
            goto LEAVE;

        if(media_packet_is_codec_config(in_buf, &codec_config) != MEDIA_PACKET_ERROR_NONE)
        {
            LOGE("media_packet_is_codec_config failed");
            goto ERROR;
        }

        if(media_packet_is_end_of_stream(in_buf, &eos) != MEDIA_PACKET_ERROR_NONE)
        {
            LOGE("media_packet_is_end_of_stream failed");
            goto ERROR;
        }

        if(codec_config)
            initiative = true;

        if(eos)
        {
            g_mutex_lock(&core->eos_wait_mutex);
            core->eos_waiting = true;
            g_mutex_unlock(&core->eos_wait_mutex);
        }

        if(initiative)
        {
            GstStructure *s;
            GstEvent *event;
            GstPad *pad;

            _mc_gst_update_caps(core, in_buf, &new_caps);
            gst_app_src_set_caps(GST_APP_SRC(core->appsrc), new_caps);

            pad = gst_element_get_static_pad(core->appsrc, "src");
            s = gst_structure_new (GST_MC_EVENT_CODEC_DATA,
                            "codec_config", G_TYPE_BOOLEAN, true, NULL);
            event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, s);
            gst_pad_push_event (pad,event);
            gst_object_unref(pad);

            LOGD("caps updated");
        }

        buff = _mc_gst_media_packet_to_gstbuffer(core, &new_caps, in_buf, codec_config);
        if (!buff)
        {
            LOGW("gstbuffer can't make");
            goto ERROR;
        }
        LOGD("gstbuffer refcount %d", GST_MINI_OBJECT_REFCOUNT_VALUE(buff));

        g_mutex_lock(&core->drain_mutex);
        /* inject buffer */
        ret = _mc_gst_gstbuffer_to_appsrc(core, buff);
        if(ret != GST_FLOW_OK)
        {
            LOGE("Failed to push gst buffer");
            g_mutex_unlock(&core->drain_mutex);
            goto ERROR;
        }
        LOGD("after refcount %d", GST_MINI_OBJECT_REFCOUNT_VALUE(buff));

        initiative = false;
        g_mutex_unlock(&core->drain_mutex);

#if 1
        if (eos)
        {
            LOGD("end of stream");
            //goto EOS;

            uint64_t wait_until = g_get_monotonic_time() + G_TIME_SPAN_SECOND / 2;

            g_signal_emit_by_name(core->appsrc, "end-of-stream", &ret);

            while(!core->eos)
            {
                LOGD("waiting for eos signal...");
                if(!g_cond_wait_until(&core->eos_cond, &core->eos_mutex, wait_until))
                {
                    core->eos = true;
                    LOGD("time out");
                }
                else
                    LOGD("recevied signal");
            }

            if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])
            {
                LOGD("eos callback invoked");
                ((mc_eos_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])(core->user_data[_MEDIACODEC_EVENT_TYPE_EOS]);
            }
#if 1
            core->eos = false;
            core->eos_waiting = false;
            initiative = true;
            g_cond_signal(&core->eos_waiting_cond);
            LOGD("eos flag set to false");
#endif
/*

            core->eos = true;
            _mc_gst_set_flush_input(core);


            g_mutex_lock(core->eos_mutex);

            core->eos = false;
            initiative = true;

            g_mutex_unlock(core->eos_mutex);

            if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])
            {
                ((mc_eos_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])(core->user_data[_MEDIACODEC_EVENT_TYPE_EOS]);
                LOGD("send eos callback");
            }
            //g_signal_emit_by_name(core->appsrc, "end-of-stream", &ret);
*/

        }
#endif
        continue;
ERROR:

        g_mutex_lock(&core->drain_mutex);
        _mc_gst_set_flush_input(core);
        g_mutex_unlock(&core->drain_mutex);

        if (core->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR])
        {
            ((mc_error_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR])(MC_INTERNAL_ERROR, core->user_data[_MEDIACODEC_EVENT_TYPE_ERROR]);
        }

        continue;
/*
EOS:

        g_signal_emit_by_name(core->appsrc, "end-of-stream", &ret);

        while(!core->eos)
        {
            LOGD("waiting for eos signal...");
            //g_cond_wait(core->eos_cond, core->eos_mutex);
            if(!g_cond_wait_until(core->eos_cond, core->eos_mutex, wait_until))
            {
                core->eos = true;
                LOGD("time out");

            }
            else
                LOGD("recevied signal");

            if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])
            {
                LOGD("eos callback invoked");
                ((mc_eos_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])(core->user_data[_MEDIACODEC_EVENT_TYPE_EOS]);
            }
        }

        core->eos = false;
        core->eos_waiting = false;
        g_cond_signal(core->eos_waiting_cond);
        initiative = true;
        LOGD("eos flag set to false");
        continue;
*/
LEAVE:
        //LOGE("status : in_buf : %p, codec_config : %d, eos : %d, encoder : %d in feed_task", in_buf, codec_config, eos, core->encoder);
	continue;

    }

    if(new_caps)
    {
        gst_caps_unref(new_caps);
    }
    LOGI("status : in_buf : %p, codec_config : %d, eos : %d, video : %d, encoder : %d in feed_task", in_buf, codec_config, eos, core->video, core->encoder);
    LOGD("feed task finished %p v(%d)e(%d)", core, core->video, core->encoder);
    MEDIACODEC_FLEAVE();

    return NULL;
}

static int _mc_link_vtable(mc_gst_core_t *core, mediacodec_codec_type_e id, gboolean encoder, gboolean is_hw)
{
    g_return_val_if_fail (core != NULL, MC_PARAM_ERROR);

    switch (id)
    {
        case MEDIACODEC_AAC:
        {
            LOGD("aac vtable");
            core->vtable = encoder ? aenc_vtable : adec_vtable;
            break;
        }
        case MEDIACODEC_MP3:
        {
            LOGD("mp3 vtable");
            core->vtable = encoder ? aenc_vtable :mp3dec_vtable;
            break;
        }
        case MEDIACODEC_H263:
        {
            LOGD("h263 vtable");
            if(is_hw)
                core->vtable = encoder ? h263enc_vtable : h263dec_vtable;
            else
                core->vtable = encoder ? venc_vtable : vdec_vtable;
            break;
        }
        case MEDIACODEC_H264:
        {
            LOGD("h264 vtable");
            if(is_hw)
                core->vtable = encoder ? h264enc_vtable : h264dec_vtable;
            else
                core->vtable = encoder ? venc_vtable : vdec_vtable;
            break;
        }
        default:
            break;
    }

    return MC_ERROR_NONE;
}

static int _mc_gst_gstbuffer_to_appsrc(mc_gst_core_t *core, mc_gst_buffer_t *buff)
{
    int ret = MC_ERROR_NONE;
    MEDIACODEC_FENTER();

    ret = gst_app_src_push_buffer(GST_APP_SRC(core->appsrc), (GstBuffer*)buff);

    MEDIACODEC_FLEAVE();

    return ret;
}

media_packet_h _mc_get_input_buffer(mc_gst_core_t *core)
{
    LOGD("waiting for input...");
    return mc_async_queue_pop(core->available_queue->input);
}

mc_ret_e mc_gst_prepare(mc_handle_t *mc_handle)
{
    int ret = MC_ERROR_NONE;
    media_format_mimetype_e out_mime;
    int i = 0;

    if (!mc_handle)
        return MC_PARAM_ERROR;

    mediacodec_codec_type_e id;
    bool video;
    bool encoder;
    bool hardware;
    gchar *factory_name = NULL;
    static const mc_codec_map_t *codec_map;
    //media_packet_h out_pkt = NULL;

    MEDIACODEC_FENTER();

    id = mc_handle->codec_id;
    video = mc_handle->is_video;
    encoder = mc_handle->is_encoder;
    hardware = mc_handle->is_hw;

    const int codec_list = encoder ? (sizeof(encoder_map) / sizeof(encoder_map[0])) : (sizeof(decoder_map) / sizeof(decoder_map[0]));

    codec_map = encoder ? encoder_map : decoder_map;

    for(i = 0; i < codec_list; i++)
    {
        if((id == codec_map[i].id) && (hardware == codec_map[i].hardware))
            break;
    }

    if( i == codec_list )
        return MC_NOT_SUPPORTED;

    factory_name = codec_map[i].type.factory_name;
    out_mime = codec_map[i].type.out_format;

    /* gst_core create */
    mc_gst_core_t* new_core = mc_gst_core_new();

    new_core->mime = codec_map[i].type.mime;
    new_core->is_hw = hardware;
    new_core->eos = false;
    new_core->encoder = encoder;
    new_core->video = video;
    new_core->codec_info = encoder ? (void*)&mc_handle->info.encoder : (void*)&mc_handle->info.decoder;

    LOGD("@%p(%p) core is initializing...v(%d)e(%d)", mc_handle, new_core, new_core->video, new_core->encoder);
    LOGD("factory name : %s, output_fmt : %x, mime %s", factory_name, out_mime, new_core->mime);

    /* create media_packet for output fmt */
    if ( (ret = _mc_output_media_packet_new(new_core, video, encoder, out_mime)) != MC_ERROR_NONE)
    {
        LOGE("Failed to create output pakcet");
        return ret;
    }

    /* link vtable */
    if((ret = _mc_link_vtable(new_core, id, encoder, hardware)) != MC_ERROR_NONE)
    {
        LOGE("vtable link failed");
        return ret;
    }

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

    mc_handle->core = new_core;

    /* create basic core elements */
    ret = _mc_gst_create_pipeline(mc_handle->core, factory_name);

    MEDIACODEC_FLEAVE();

    return ret;
}

mc_ret_e mc_gst_unprepare(mc_handle_t *mc_handle)
{
    int i;
    int ret = MC_ERROR_NONE;
    mc_gst_core_t *core = NULL;
    uint64_t wait_until = g_get_monotonic_time() + G_TIME_SPAN_SECOND / 2;

    MEDIACODEC_FENTER();

    if (!mc_handle)
        return MC_PARAM_ERROR;

    core = (mc_gst_core_t*)mc_handle->core;

    if(core)
    {
        LOGD("@%p(%p) core is uninitializing... v(%d)e(%d)",mc_handle, core, core->video, core->encoder);

        g_mutex_lock(&core->eos_wait_mutex);

        if(core->eos_waiting)
        {
            LOGD("waiting for eos is finished");
            if(!g_cond_wait_until(&core->eos_waiting_cond, &core->eos_wait_mutex, wait_until))
            {
                core->eos_waiting = false;
                LOGD("time out");
            }
            else
            {
                LOGD("recevied signal from feed_task");
            }

        }
        g_mutex_unlock(&core->eos_wait_mutex);


        LOGD("%p/%p(%d) input flush", mc_handle, core, core->encoder);
        g_mutex_lock(&core->drain_mutex);

        _mc_gst_set_flush_input(core);

        g_mutex_unlock(&core->drain_mutex);



        _mc_gst_set_flush_output(core);


        /* unset callback */
        for (i = 0; i < _MEDIACODEC_EVENT_TYPE_INTERNAL_FILLBUFFER ; i++)
        {
            LOGD("unset cb function [%d]", i);
            if (mc_handle->user_cb[i])
            {
                core->user_cb[i] = NULL;
                core->user_data[i] = NULL;
                LOGD("user_cb[%d] %p, %p", i, core->user_cb[i], mc_handle->user_cb[i]);
            }
        }

        ret = _mc_gst_destroy_pipeline(core);

        if(core != NULL)
        {
            mc_gst_core_free(core);
            mc_handle->core = NULL;
        }
    }

    MEDIACODEC_FLEAVE();

    return ret;
}

mc_ret_e mc_gst_process_input(mc_handle_t *mc_handle, media_packet_h inbuf, uint64_t timeOutUs)
{
    int ret = MC_ERROR_NONE;
    mc_gst_core_t *core = NULL;

    if (!mc_handle)
        return MC_PARAM_ERROR;

    core = (mc_gst_core_t*)mc_handle->core;
    LOGI("@%p v(%d)e(%d)process_input", core, core->video, core->encoder);

    MEDIACODEC_FENTER();

    g_mutex_lock(&core->eos_mutex);

    if(!core->eos)
        mc_async_queue_push(core->available_queue->input, inbuf);
    else
        ret = MC_INVALID_IN_BUF;

    g_mutex_unlock(&core->eos_mutex);

    MEDIACODEC_FLEAVE();

    return ret;
}

mc_ret_e mc_gst_get_output(mc_handle_t *mc_handle, media_packet_h *outbuf, uint64_t timeOutUs)
{
    int ret = MC_ERROR_NONE;
    mc_gst_core_t *core = NULL;
    media_packet_h out_pkt = NULL;

    if (!mc_handle)
        return MC_PARAM_ERROR;

    MEDIACODEC_FENTER();

    core = (mc_gst_core_t*)mc_handle->core;
    LOGI("@%p v(%d)e(%d) get_output", core, core->video, core->encoder);

    g_mutex_lock(&core->ports[1]->mutex);

    if(!g_queue_is_empty(core->ports[1]->queue))
    {
        out_pkt = g_queue_pop_head(core->ports[1]->queue);
        LOGD("pop from output_queue : %p", out_pkt);
    }
    else
    {
        ret = MC_OUTPUT_BUFFER_EMPTY;
        LOGD("output_queue is empty");
    }
    *outbuf = out_pkt;

    g_mutex_unlock(&core->ports[1]->mutex);

    MEDIACODEC_FLEAVE();

    return ret;
}

mc_ret_e mc_gst_flush_buffers(mc_handle_t *mc_handle)
{
    int ret = MC_ERROR_NONE;
    mc_gst_core_t *core = NULL;

    if (!mc_handle)
        return MC_PARAM_ERROR;

    MEDIACODEC_FENTER();

    core = (mc_gst_core_t*)mc_handle->core;
    LOGI("@%p v(%d)e(%d) get_output", core, core->video, core->encoder);

    _mc_gst_set_flush_input(core);
    _mc_gst_set_flush_output(core);

    MEDIACODEC_FLEAVE();

    return ret;
}

static gboolean __mc_gst_init_gstreamer()
{
    static gboolean initialized = FALSE;
    static const int max_argc = 50;
    gint* argc = NULL;
    gchar** argv = NULL;
    gchar** argv2 = NULL;
    GError *err = NULL;
    int i = 0;
    int arg_count = 0;

    MEDIACODEC_FENTER();

    if ( initialized )
    {
        LOGD("gstreamer already initialized.\n");
        return TRUE;
    }

    /* alloc */
    argc = malloc( sizeof(int) );
    argv = malloc( sizeof(gchar*) * max_argc );
    argv2 = malloc( sizeof(gchar*) * max_argc );

    if ( !argc || !argv || !argv2 )
        goto ERROR;

    memset( argv, 0, sizeof(gchar*) * max_argc );
    memset( argv2, 0, sizeof(gchar*) * max_argc );

    /* add initial */
    *argc = 1;
    argv[0] = g_strdup( "media codec" );

    /* we would not do fork for scanning plugins */
    argv[*argc] = g_strdup("--gst-disable-registry-fork");
    (*argc)++;

    /* check disable registry scan */
    argv[*argc] = g_strdup("--gst-disable-registry-update");
    (*argc)++;

    /* check disable segtrap */
    argv[*argc] = g_strdup("--gst-disable-segtrap");
    (*argc)++;

    LOGD("initializing gstreamer with following parameter\n");
    LOGD("argc : %d\n", *argc);
    arg_count = *argc;

    for ( i = 0; i < arg_count; i++ )
    {
        argv2[i] = argv[i];
        LOGD("argv[%d] : %s\n", i, argv2[i]);
    }

    /* initializing gstreamer */
    if ( ! gst_init_check (argc, &argv, &err))
    {
        LOGE("Could not initialize GStreamer: %s\n", err ? err->message : "unknown error occurred");
        if (err)
        {
            g_error_free (err);
        }

        goto ERROR;
    }

    /* release */
    for ( i = 0; i < arg_count; i++ )
    {
        //debug_log("release - argv[%d] : %s\n", i, argv2[i]);
        MC_FREEIF( argv2[i] );
    }

    MC_FREEIF( argv );
    MC_FREEIF( argv2 );
    MC_FREEIF( argc );

    /* done */
    initialized = TRUE;

    MEDIACODEC_FLEAVE();

    return TRUE;

ERROR:

    /* release */
    for ( i = 0; i < arg_count; i++ )
    {
        LOGD("free[%d] : %s\n", i, argv2[i]);
        MC_FREEIF( argv2[i] );
    }

    MC_FREEIF( argv );
    MC_FREEIF( argv2 );
    MC_FREEIF( argc );

    return FALSE;
}

mc_ret_e _mc_gst_create_pipeline(mc_gst_core_t* core, gchar *factory_name)
{
    GstBus *bus = NULL;
    GstPad *pad = NULL;

    MEDIACODEC_FENTER();

    g_mutex_lock(&core->prepare_lock);
    if(core->prepare_count == 0)
    {

        if (!__mc_gst_init_gstreamer())
        {
            LOGE ("gstreamer initialize fail");
            g_mutex_unlock(&core->prepare_lock);
            return MC_NOT_INITIALIZED;
        }
        core->codec = gst_element_factory_make(factory_name, NULL);

        if(!core->codec)
        {
            LOGE ("codec element create fail");
            goto ERROR;
        }

        LOGD("@%p v(%d)e(%d) create_pipeline", core, core->video, core->encoder);
        MEDIACODEC_ELEMENT_SET_STATE(core->codec, GST_STATE_READY);

        /* create common elements */
        core->pipeline = gst_pipeline_new(NULL);

        if (!core->pipeline)
        {
            LOGE ("pipeline create fail");
            goto ERROR;
        }

        core->appsrc = gst_element_factory_make("appsrc", NULL);

        if (!core->appsrc)
        {
            LOGE ("appsrc can't create");
            goto ERROR;
        }

        core->fakesink = gst_element_factory_make("fakesink", NULL);

        if (!core->fakesink)
        {
            LOGE ("fakesink create fail");
            goto ERROR;
        }
        g_object_set(core->fakesink, "enable-last-buffer", FALSE, NULL);

        //__mc_link_elements(core);
        gst_bin_add_many(GST_BIN(core->pipeline), core->appsrc, core->codec, core->fakesink, NULL);

        /* link elements */
        gst_element_link_many(core->appsrc, core->codec, core->fakesink, NULL);

        /* connect signals, bus watcher */

        bus = gst_pipeline_get_bus (GST_PIPELINE (core->pipeline));
        core->bus_whatch_id = gst_bus_add_watch (bus, __mc_gst_bus_callback, core);
        core->thread_default = g_main_context_get_thread_default();

        /* set sync handler to get tag synchronously */
        gst_bus_set_sync_handler(bus, __mc_gst_bus_sync_callback, core);

        gst_object_unref (GST_OBJECT(bus));

        /* add pad probe */
        pad = gst_element_get_static_pad(core->fakesink, "sink");
        core->probe_id = gst_pad_add_event_probe(pad, G_CALLBACK(event_probe_cb), core);
        gst_object_unref (pad);

        /* connect handoff */
        g_object_set (GST_OBJECT(core->fakesink), "signal-handoffs", TRUE, NULL);
        core->signal_handoff = g_signal_connect(core->fakesink, "handoff", G_CALLBACK(__mc_gst_buffer_add), core);
        /*
           __mc_create_caps(core, &caps);

           gst_app_src_set_caps(GST_APP_SRC(core->appsrc), caps);
           gst_caps_unref(caps);
           */
        /* set state PLAYING */
        MEDIACODEC_ELEMENT_SET_STATE(GST_ELEMENT_CAST(core->pipeline), GST_STATE_PLAYING);

        //g_mutex_unlock(core->prepare_lock);

    }
    core->prepare_count++;
    g_mutex_unlock(&core->prepare_lock);

    MEDIACODEC_FLEAVE();

    return MC_ERROR_NONE;

STATE_CHANGE_FAILED:
ERROR:

    if(core->codec)
        gst_object_unref(GST_OBJECT(core->codec));

    if(core->pipeline)
        gst_object_unref(GST_OBJECT(core->pipeline));

    if(core->appsrc)
        gst_object_unref(GST_OBJECT(core->appsrc));

    if(core->fakesink)
        gst_object_unref(GST_OBJECT(core->fakesink));

    g_mutex_unlock(&core->prepare_lock);

    return MC_ERROR;
}

mc_ret_e _mc_gst_destroy_pipeline(mc_gst_core_t *core)
{
    int ret = MC_ERROR_NONE;
    GstPad *pad = NULL;

    MEDIACODEC_FENTER();

    g_mutex_lock(&core->prepare_lock);
    core->prepare_count--;
    if(core->prepare_count == 0)
    {

        if(core->pipeline)
        {
            /* disconnect signal */
            if(core->fakesink && GST_IS_ELEMENT(core->fakesink))
            {
                if(g_signal_handler_is_connected(core->fakesink, core->signal_handoff))
                {
                    g_signal_handler_disconnect(core->fakesink, core->signal_handoff);
                    LOGD("handoff signal destroy");
                }
            }

            if(core->bus_whatch_id)
            {
                GSource *source = NULL;
                source = g_main_context_find_source_by_id (core->thread_default, core->bus_whatch_id);
                g_source_destroy(source);
                //g_source_remove(core->bus_whatch_id);
                LOGD("bus_whatch_id destroy");
            }

            pad = gst_element_get_static_pad(core->fakesink, "sink");
            gst_pad_remove_event_probe(pad, core->probe_id);
            g_object_unref(pad);

            MEDIACODEC_ELEMENT_SET_STATE(core->pipeline, GST_STATE_NULL);

            gst_object_unref(GST_OBJECT(core->pipeline));
        }
    }

    LOGD("@%p v(%d)e(%d) destroy_pipeline : %d ", core, core->video, core->encoder, core->prepare_count);
    g_mutex_unlock(&core->prepare_lock);

    MEDIACODEC_FLEAVE();

    return ret;

STATE_CHANGE_FAILED:
    if(core->pipeline)
        gst_object_unref(GST_OBJECT(core->pipeline));

    LOGD("@%p v(%d)e(%d) destroy_pipeline failed", core, core->video, core->encoder);
    g_mutex_unlock(&core->prepare_lock);

    return MC_ERROR;
}

void __mc_gst_buffer_add (GstElement *element, GstBuffer *buffer, GstPad *pad, gpointer data)
{
    mc_gst_core_t* core = (mc_gst_core_t*) data;

    media_packet_h out_pkt = NULL;

    MEDIACODEC_FENTER();

    //g_atomic_int_int(&core->num_live_buffers);
    LOGI("@%p(%d)", core, core->encoder);

    out_pkt = __mc_gst_gstbuffer_to_media_packet(core, buffer);
    if (out_pkt == NULL)
    {
        LOGE("out_pkt create failed.");
        return;
    }

    if(core->encoder && core->codec_config)
    {
        media_packet_set_flags(out_pkt, MEDIA_PACKET_CODEC_CONFIG);
        LOGD("set the codec data %p", buffer);
    }
    core->codec_config = false;

    g_mutex_lock(&core->ports[1]->mutex);
    /* push it to output buffer queue */
    //g_queue_push_tail(core->output_queue, out_pkt);
    g_queue_push_tail(core->ports[1]->queue, out_pkt);

    g_mutex_unlock(&core->ports[1]->mutex);

    if (core->user_cb[_MEDIACODEC_EVENT_TYPE_FILLBUFFER])
    {
        ((mc_fill_buffer_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_FILLBUFFER])(out_pkt, core->user_data[_MEDIACODEC_EVENT_TYPE_FILLBUFFER]);
    }

    MEDIACODEC_FLEAVE();
}

static int __mc_output_packet_buffer_finalize_cb(media_packet_h packet, int error_code, void *user_data)
{
    void* buffer = NULL;

    MEDIACODEC_FENTER();

    LOGD("packet finalized: %p", packet);
    media_packet_get_extra(packet, &buffer);
    gst_buffer_unref((GstBuffer*)buffer);

    MEDIACODEC_FLEAVE();

    return MEDIA_PACKET_FINALIZE;
}

int __mc_output_buffer_finalize_cb(media_packet_h packet, int error_code, void *user_data)
{
    void* buffer = NULL;
    SCMN_IMGB *imgb = NULL;
    tbm_surface_h surface = NULL;
    bool has_tbm_surface = false;

    MEDIACODEC_FENTER();

    media_packet_has_tbm_surface_buffer(packet, &has_tbm_surface);
    LOGI("has_tbm_surface : %d", has_tbm_surface);

    if(has_tbm_surface)
    {
        LOGI("destroy tbm surface");
        media_packet_get_tbm_surface(packet, &surface);
        tbm_surface_destroy(surface);
    }
    media_packet_get_extra(packet, &buffer);
    LOGD("finalized_gst_buffer = %p", buffer);
    LOGI("gstbuffer refcount %d", GST_MINI_OBJECT_REFCOUNT_VALUE(buffer));
    if (GST_BUFFER_MALLOCDATA(buffer))
    {
        imgb = (SCMN_IMGB *)GST_BUFFER_MALLOCDATA(buffer);
        if (imgb)
        {
            LOGI("imgb is destroyed");
            free(imgb);
            imgb = NULL;
            GST_BUFFER_MALLOCDATA(buffer) = NULL;
        }
    }

    //GST_BUFFER_DATA(buffer) = NULL;
    gst_buffer_unref((GstBuffer*)buffer);

    MEDIACODEC_FLEAVE();
    return MEDIA_PACKET_FINALIZE;
}

guint32 __mc_get_gst_input_format(media_packet_h packet, bool is_hw)
{
    guint32 format = 0;
    media_format_h fmt = NULL;
    media_format_mimetype_e mimetype = 0;

    media_packet_get_format(packet, &fmt);
    media_format_get_video_info(fmt, &mimetype, NULL, NULL, NULL, NULL);
    LOGD("input packet mimetype : %x", mimetype);

    switch(mimetype)
    {
        case MEDIA_FORMAT_I420:
            format = GST_MAKE_FOURCC ('I', '4', '2', '0');
            break;
        case MEDIA_FORMAT_NV12:
            if (is_hw)
                format = GST_MAKE_FOURCC ('S', 'N', '1', '2');
            else
                format = GST_MAKE_FOURCC ('N', 'V', '1', '2');
            break;
        default:
            break;
    }
    return format;
}


mc_gst_buffer_t* _mc_gst_media_packet_to_gstbuffer(mc_gst_core_t* core, GstCaps **caps, media_packet_h pkt, bool codec_config)
{
    mc_gst_buffer_t* buff = NULL;
    uint64_t pts = 0, dur = 0;

    buff = __mc_gst_buffer_new(core);
    buff->pkt = pkt;

    //mc_hex_dump("nal", pkt, 8);

    __mc_fill_input_buffer(core, buff);

    /* pts */
    media_packet_get_pts(pkt, &pts);
    GST_BUFFER_TIMESTAMP(buff) = (GstClockTime) pts;
    LOGD("GST_BUFFER_TIMESTAMP = %"GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buff)));
    LOGD("PTS = %llu", pts);

    /* duration */
    media_packet_get_duration(pkt, &dur);
    GST_BUFFER_DURATION(buff) = dur;

    GST_BUFFER_CAPS(buff) = gst_caps_copy(*caps);

    return buff;
}

media_packet_h __mc_gst_gstbuffer_to_media_packet(mc_gst_core_t* core, GstBuffer* buff)
{
    media_packet_h out_pkt = NULL;
    mc_ret_e ret = MC_ERROR_NONE;
    if (!buff)
        return NULL;

    ret = __mc_fill_output_buffer(core, buff, &out_pkt);
	if (ret != MC_ERROR_NONE)
	{
	    gst_buffer_ref(buff);
		return NULL;
	}

    gst_buffer_ref(buff);

    return out_pkt;
}

gboolean __mc_gst_bus_callback (GstBus *bus, GstMessage *msg, gpointer data)
{
    int ret  = MC_ERROR_NONE;
    mc_gst_core_t *core = (mc_gst_core_t*)data;
    LOGD("@%p v(%d)e(%d)", core, core->video, core->encoder);

    switch (GST_MESSAGE_TYPE (msg)) {

        case GST_MESSAGE_EOS:
        {
            core->eos = true;
            g_cond_signal(&core->eos_cond);
            LOGD("send eos signal");

            LOGD ("End of stream\n");
/*
            if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])
            {
                ((mc_eos_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])(core->user_data[_MEDIACODEC_EVENT_TYPE_EOS]);
            }
*/
        }
        break;

        case GST_MESSAGE_ERROR:
        {
            GError* error = NULL;

            gst_message_parse_error (msg, &error, NULL);

            if (!error)
            {
                LOGW("GST error message parsing failed");
                break;
            }

            LOGW ("Error: %s\n", error->message);

            if(error)
            {
                if(error->domain == GST_STREAM_ERROR)
                {
                    ret = __gst_handle_stream_error(core, error, msg);
                }
                else if (error->domain == GST_RESOURCE_ERROR)
                {
                    ret = __gst_handle_resource_error(core, error->code);
                }
                else if (error->domain == GST_LIBRARY_ERROR)
                {
                    ret = __gst_handle_library_error(core, error->code);
                }
                else if (error->domain == GST_CORE_ERROR)
                {
                    ret = __gst_handle_core_error(core, error->code);
                }
                else
                {
                    LOGW("Unexpected error has occured");
                }
            }
            g_error_free (error);
        }
        break;

        default:
        break;
    }

    if (core->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR])
    {
        ((mc_error_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR])(ret, core->user_data[_MEDIACODEC_EVENT_TYPE_ERROR]);
    }

    return TRUE;
}

static gboolean
__mc_gst_check_useful_message(mc_gst_core_t *core, GstMessage *msg)
{
    gboolean retval = false;

    if(!core->pipeline)
    {
        LOGE("mediacodec pipeline handle is null");
        return true;
    }

    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_TAG:
        case GST_MESSAGE_EOS:
        case GST_MESSAGE_ERROR:
        case GST_MESSAGE_WARNING:
            retval = true;
            break;
        default:
            retval = false;
            break;
    }

    return retval;
}

static GstBusSyncReply
__mc_gst_bus_sync_callback(GstBus *bus, GstMessage *msg, gpointer data)
{
    mc_gst_core_t *core = (mc_gst_core_t*)data;
    GstBusSyncReply reply = GST_BUS_DROP;

    if(!core->pipeline)
    {
        LOGE("mediacodec pipeline handle is null");
        return GST_BUS_PASS;
    }

    if(!__mc_gst_check_useful_message(core, msg))
    {
        gst_message_unref(msg);
        return GST_BUS_DROP;
    }

    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_STATE_CHANGED:
          __mc_gst_bus_callback(NULL, msg, core);
          //__mediacodec_gst_callback(NULL, msg, core);
          reply = GST_BUS_DROP;
        break;

        default:
          reply = GST_BUS_PASS;
          break;
    }

    if( reply == GST_BUS_DROP )
        gst_message_unref(msg);

    return reply;
}

static SCMN_IMGB * __mc_gst_make_tbm_buffer(mc_gst_core_t* core, media_packet_h pkt)
{
    tbm_surface_h surface = NULL;
    tbm_bo bo = NULL;
    mc_encoder_info_t *enc_info = (mc_encoder_info_t*)core->codec_info;

    if (!pkt) {
        LOGE("output is null");
        return NULL;
    }

    SCMN_IMGB *psimgb = NULL;
    psimgb = (SCMN_IMGB *)malloc(sizeof(SCMN_IMGB));
    if (!psimgb) {
        LOGE("Failed to alloc SCMN_IMGB");
        return NULL;
    }
    memset(psimgb, 0x00, sizeof(SCMN_IMGB));

    media_packet_get_tbm_surface(pkt, &surface);
    bo = tbm_surface_internal_get_bo(surface, 0);

    tbm_bo_handle handle = tbm_bo_get_handle(bo, TBM_DEVICE_CPU);
#ifdef TIZEN_PROFILE_LITE
    int phy_addr = 0;
    int phy_size = 0;
    tbm_bo_handle handle_fd = tbm_bo_get_handle(bo, TBM_DEVICE_MM);

    if (__tbm_get_physical_addr_bo(handle_fd, &phy_addr, &phy_size) == 0)
    {
        psimgb->p[0] = (void*)phy_addr;
    }
#endif

    psimgb->buf_share_method = BUF_SHARE_METHOD_TIZEN_BUFFER;
    psimgb->bo[0] = bo;
    psimgb->a[0] =  handle.ptr;
    psimgb->w[0] = enc_info->width;
    psimgb->h[0] = enc_info->height;
    psimgb->s[0] = psimgb->w[0];
    psimgb->e[0] = psimgb->h[0];

    return psimgb;
}

static GType __mc_gst_buffer_get_type(void)
{
    static GType _mc_gst_buffer_type;

    if (G_UNLIKELY(_mc_gst_buffer_type == 0)) {
        static const GTypeInfo mc_gst_buffer_info = {
            sizeof (GstBufferClass),
            NULL,
            NULL,
            __mc_gst_buffer_class_init,
            NULL,
            NULL,
            sizeof (mc_gst_buffer_t),
            0,
            NULL,
            NULL
        };

        _mc_gst_buffer_type = g_type_register_static(GST_TYPE_BUFFER,
                "McGstBuffer",
                &mc_gst_buffer_info,
                0);
    }

    return _mc_gst_buffer_type;
}

static void __mc_gst_buffer_class_init(gpointer g_class, gpointer class_data)
{
    GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS(g_class);
    mc_gst_buffer_parent_class = g_type_class_peek_parent(g_class);
    mini_object_class->finalize = (GstMiniObjectFinalizeFunction)__mc_gst_buffer_finalize;
}

static mc_gst_buffer_t* __mc_gst_buffer_new(mc_gst_core_t* core)
{
    mc_gst_buffer_t *ret = NULL;
    ret = (mc_gst_buffer_t *)gst_mini_object_new(GST_TYPE_MC_BUFFER);
    LOGD("creating buffer : %p", ret);
    ret->core = core;
    return ret;
}

static void __mc_gst_buffer_finalize(mc_gst_buffer_t *buffer)
{
    SCMN_IMGB *imgb = NULL;
    mc_gst_core_t *core = buffer->core;

    LOGD("__mc_gst_buffer_finalize() is called");
    if (GST_BUFFER_MALLOCDATA(buffer))
    {
        imgb = (SCMN_IMGB *)GST_BUFFER_MALLOCDATA(buffer);
        if (imgb)
        {
            LOGI("imgb is destroyed");
            free(imgb);
            imgb = NULL;
            GST_BUFFER_MALLOCDATA(buffer) = NULL;
        }
    }

    //GST_BUFFER_DATA(buffer) = NULL;

    if (GST_MINI_OBJECT_CLASS (mc_gst_buffer_parent_class)->finalize) {
        GST_MINI_OBJECT_CLASS (mc_gst_buffer_parent_class)->finalize (GST_MINI_OBJECT(buffer));
    }

    if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER])
    {
        ((mc_empty_buffer_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER])(buffer->pkt, core->user_data[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER]);
        LOGD("finalize, pkt : %p, buffer : %p", buffer->pkt, buffer);
    }

    return;
}

static gint __gst_handle_core_error(mc_gst_core_t* core, int code )
{
    gint trans_err = MEDIACODEC_ERROR_NONE;

    g_return_val_if_fail(core, MC_PARAM_ERROR);

    switch ( code )
    {
        case GST_CORE_ERROR_MISSING_PLUGIN:
            return MEDIACODEC_ERROR_NOT_SUPPORTED_FORMAT;
        case GST_CORE_ERROR_STATE_CHANGE:
        case GST_CORE_ERROR_SEEK:
        case GST_CORE_ERROR_NOT_IMPLEMENTED:
        case GST_CORE_ERROR_FAILED:
        case GST_CORE_ERROR_TOO_LAZY:
        case GST_CORE_ERROR_PAD:
        case GST_CORE_ERROR_THREAD:
        case GST_CORE_ERROR_NEGOTIATION:
        case GST_CORE_ERROR_EVENT:
        case GST_CORE_ERROR_CAPS:
        case GST_CORE_ERROR_TAG:
        case GST_CORE_ERROR_CLOCK:
        case GST_CORE_ERROR_DISABLED:
        default:
            trans_err =  MEDIACODEC_ERROR_INVALID_STREAM;
            break;
    }

    return trans_err;
}

static gint __gst_handle_library_error(mc_gst_core_t* core, int code)
{
    gint trans_err = MEDIACODEC_ERROR_NONE;

    g_return_val_if_fail(core, MC_PARAM_ERROR);

    switch ( code )
    {
        case GST_LIBRARY_ERROR_FAILED:
        case GST_LIBRARY_ERROR_TOO_LAZY:
        case GST_LIBRARY_ERROR_INIT:
        case GST_LIBRARY_ERROR_SHUTDOWN:
        case GST_LIBRARY_ERROR_SETTINGS:
        case GST_LIBRARY_ERROR_ENCODE:
        default:
            trans_err =  MEDIACODEC_ERROR_INVALID_STREAM;
            break;
    }

    return trans_err;
}


static gint __gst_handle_resource_error(mc_gst_core_t* core, int code )
{
    gint trans_err = MEDIACODEC_ERROR_NONE;

    g_return_val_if_fail(core, MC_PARAM_ERROR);

    switch ( code )
    {
        case GST_RESOURCE_ERROR_NO_SPACE_LEFT:
            trans_err = MEDIACODEC_ERROR_NO_FREE_SPACE;
            break;
        case GST_RESOURCE_ERROR_WRITE:
        case GST_RESOURCE_ERROR_FAILED:
        case GST_RESOURCE_ERROR_SEEK:
        case GST_RESOURCE_ERROR_TOO_LAZY:
        case GST_RESOURCE_ERROR_BUSY:
        case GST_RESOURCE_ERROR_OPEN_WRITE:
        case GST_RESOURCE_ERROR_OPEN_READ_WRITE:
        case GST_RESOURCE_ERROR_CLOSE:
        case GST_RESOURCE_ERROR_SYNC:
        case GST_RESOURCE_ERROR_SETTINGS:
        default:
            trans_err = MEDIACODEC_ERROR_INTERNAL;
            break;
    }

    return trans_err;
}

static gint __gst_handle_stream_error(mc_gst_core_t* core, GError* error, GstMessage * message)
{
    gint trans_err = MEDIACODEC_ERROR_NONE;

    g_return_val_if_fail(core, MC_PARAM_ERROR);
    g_return_val_if_fail(error, MC_PARAM_ERROR);
    g_return_val_if_fail (message, MC_PARAM_ERROR);

    switch ( error->code )
    {
        case GST_STREAM_ERROR_FAILED:
        case GST_STREAM_ERROR_TYPE_NOT_FOUND:
        case GST_STREAM_ERROR_DECODE:
        case GST_STREAM_ERROR_WRONG_TYPE:
        case GST_STREAM_ERROR_DECRYPT:
        case GST_STREAM_ERROR_DECRYPT_NOKEY:
        case GST_STREAM_ERROR_CODEC_NOT_FOUND:
            trans_err = __gst_transform_gsterror( core, message, error );
            break;

        case GST_STREAM_ERROR_NOT_IMPLEMENTED:
        case GST_STREAM_ERROR_TOO_LAZY:
        case GST_STREAM_ERROR_ENCODE:
        case GST_STREAM_ERROR_DEMUX:
        case GST_STREAM_ERROR_MUX:
        case GST_STREAM_ERROR_FORMAT:
        default:
            trans_err = MEDIACODEC_ERROR_INVALID_STREAM;
            break;
    }

    return trans_err;
}

static gint __gst_transform_gsterror( mc_gst_core_t *core, GstMessage * message, GError* error )
{
    gchar *src_element_name = NULL;
    GstElement *src_element = NULL;
    GstElementFactory *factory = NULL;
    const gchar* klass = NULL;


    src_element = GST_ELEMENT_CAST(message->src);
    if ( !src_element )
        goto INTERNAL_ERROR;

    src_element_name = GST_ELEMENT_NAME(src_element);
    if ( !src_element_name )
        goto INTERNAL_ERROR;

    factory = gst_element_get_factory(src_element);
    if ( !factory )
        goto INTERNAL_ERROR;

    klass = gst_element_factory_get_klass(factory);
    if ( !klass )
        goto INTERNAL_ERROR;

    LOGD("error code=%d, msg=%s, src element=%s, class=%s\n",
            error->code, error->message, src_element_name, klass);

    switch ( error->code )
    {
        case GST_STREAM_ERROR_DECODE:
            return MEDIACODEC_ERROR_INVALID_STREAM;
            break;

        case GST_STREAM_ERROR_CODEC_NOT_FOUND:
        case GST_STREAM_ERROR_TYPE_NOT_FOUND:
        case GST_STREAM_ERROR_WRONG_TYPE:
            return MEDIACODEC_ERROR_NOT_SUPPORTED_FORMAT;
            break;

        case GST_STREAM_ERROR_FAILED:
            return MEDIACODEC_ERROR_NOT_SUPPORTED_FORMAT;
            break;

        default:
            break;
    }

    return MEDIACODEC_ERROR_INVALID_STREAM;

INTERNAL_ERROR:
    return MEDIACODEC_ERROR_INTERNAL;
}

static void _mc_gst_set_flush_input(mc_gst_core_t *core)
{
    media_packet_h pkt = NULL;

    LOGI("_mc_gst_set_flush_input is called");
    while( pkt != mc_async_queue_pop_forced(core->available_queue->input) )
    {
        LOGD("%p pkt is poped");
        if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER])
        {
            ((mc_empty_buffer_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER])
                (pkt, core->user_data[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER]);
        }
    }

    mc_async_queue_flush(core->available_queue->input);
}

static void _mc_gst_set_flush_output(mc_gst_core_t *core)
{
    media_packet_h pkt = NULL;

    LOGI("_mc_gst_set_flush_output is called");
    g_mutex_lock(&core->ports[1]->mutex);

    if(!g_queue_is_empty(core->ports[1]->queue))
    {
        while(pkt != g_queue_pop_head(core->ports[1]->queue))
        {
            LOGD("outpkt in output_queue : %p", pkt);
            media_packet_destroy(pkt);
        }
    }

    g_mutex_unlock(&core->ports[1]->mutex);
}

#ifdef TIZEN_PROFILE_LITE
int __tbm_get_physical_addr_bo(tbm_bo_handle tbm_bo_handle_fd_t, int* phy_addr, int* phy_size)
{
    int tbm_bo_handle_fd;

    int ret=0;

    tbm_bo_handle_fd = tbm_bo_handle_fd_t.u32;

    int open_flags = O_RDWR;
    int ion_fd = -1;

    struct ion_mmu_data mmu_data;
    struct ion_custom_data  custom_data;

    mmu_data.fd_buffer = tbm_bo_handle_fd;
    custom_data.cmd = 4;
    custom_data.arg = (unsigned long)&mmu_data;

    ion_fd = open ("/dev/ion", open_flags);
    if (ion_fd < 0)
    {
      LOGE ("[tbm_get_physical_addr_bo] ion_fd open device failed");
    }

    if (ioctl(ion_fd, ION_IOC_CUSTOM, &custom_data)<0)
    {
        LOGE ("[tbm_get_physical_addr_bo] ION_IOC_CUSTOM fails %d %s",errno,strerror(errno));
        ret=-1;
    }

    if (!ret)
    {
        *phy_addr = mmu_data.iova_addr;
        *phy_size = mmu_data.iova_size;
    }
    else
    {
        *phy_addr = 0;
        *phy_size = 0;
        LOGW ("[tbm_get_physical_addr_bo] getting physical address is failed. phy_addr = 0");
    }

    if (ion_fd != -1)
    {
        close (ion_fd);
        ion_fd = -1;
    }

    return 0;
}
#endif
