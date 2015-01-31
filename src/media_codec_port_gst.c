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

/*
* Internal Implementation
*/
static gpointer feed_task(gpointer data);
static media_packet_h get_input_buffer(mc_gst_core_t *core);

static gboolean __mc_gst_init_gstreamer(mc_gst_core_t* core);
static mc_ret_e _mc_gst_create_pipeline(mc_gst_core_t* core);
static void __mc_gst_buffer_add (GstElement *element, GstBuffer *buffer, GstPad *pad, gpointer data);
static int __mc_output_buffer_finalize_cb(media_packet_h packet, int error_code, void *user_data);
static void _mc_gst_update_caps(mc_gst_core_t *core, media_packet_h pkt, GstCaps **caps);
static mc_gst_buffer* _mc_gst_media_packet_to_gstbuffer(mc_gst_core_t* core, GstCaps **caps, media_packet_h pkt, bool codec_config);
static guint32 __mc_get_gst_input_format(media_packet_h packet, bool is_hw);
static media_packet_h __mc_gst_gstbuffer_to_media_packet(mc_gst_core_t* core, GstBuffer* buffer);
static gboolean __mc_gst_bus_call(GstBus *bus, GstMessage *msg, gpointer data);
static SCMN_IMGB* __mc_gst_make_tbm_buffer(mc_gst_core_t* core, media_packet_h pkt);

static GType __mc_gst_buffer_get_type(void);
static void __mc_gst_buffer_class_init(gpointer g_class, gpointer class_data);
static mc_gst_buffer* __mc_gst_buffer_new(mc_gst_core_t* core);
static void __mc_gst_buffer_finalize(mc_gst_buffer *buffer);

static gint __gst_handle_stream_error(mc_gst_core_t* core, GError* error, GstMessage * message);
static gint __gst_transform_gsterror( mc_gst_core_t *core, GstMessage * message, GError* error );
static gint __gst_handle_resource_error(mc_gst_core_t* core, int code );
static gint __gst_handle_library_error(mc_gst_core_t* core, int code);
static gint __gst_handle_core_error(mc_gst_core_t* core, int code );

static GstBufferClass *mc_gst_buffer_parent_class = NULL;

#define GST_TYPE_MC_BUFFER (__mc_gst_buffer_get_type())

/*
 * mc_gst_core functions
*/
mc_gst_core_t *mc_gst_core_new() // @
{
    mc_gst_core_t *core;

    core = g_new0(mc_gst_core_t, 1);

    /* 0 : input, 1 : output */
    core->ports[0] = NULL;
    core->ports[1] = mc_gst_port_new(core);
    core->ports[1]->index = 1;

    core->available_queue = g_new0(mc_aqueue_t, 1);
    core->available_queue->input = mc_async_queue_new();
    core->output_queue = g_queue_new();

    g_atomic_int_set(&core->available_queue->running, 1);
    core->available_queue->thread = g_thread_create(feed_task, core, TRUE, NULL); // !!!! move to mc_gst_prepare()
    core->dec_info = g_new0(mc_decoder_info_t, 1);
    core->enc_info = g_new0(mc_encoder_info_t, 1);

    LOGD("mc_gst_core_new() is created");

    return core;
}

void mc_gst_core_free(mc_gst_core_t *core) // @
{
    mc_aqueue_t *async_queue;

    async_queue = core->available_queue;
    mc_async_queue_disable(async_queue->input);
    mc_async_queue_flush(async_queue->input);

    g_atomic_int_set(&async_queue->running, 0);
    g_thread_join(async_queue->thread);
    LOGD("g_thread_join");

    mc_async_queue_free(async_queue->input);
    g_queue_free(core->output_queue);

    if(core->ports[1] != NULL)
    {
        mc_gst_port_free(core->ports[1]);
        core->ports[1] = NULL;
    }

    g_free(core);
    core = NULL;

    LOGD("gst_core is freed");

    return;
}

/*
 * mc_gst_port functions
 */
mc_gst_port_t *mc_gst_port_new(mc_gst_core_t *core)
{
    mc_gst_port_t *port;

    port = g_new0(mc_gst_port_t, 1);
    port->core = core;
    port->num_buffers = -1;
    port->buffer_size = 0;
    port->is_allocated = 0;
    port->buffers = NULL;

    port->mutex = g_mutex_new();
    port->queue = g_queue_new();
    port->buffer_cond = g_cond_new();

    LOGD("gst_port is created");
    return port;
}

void mc_gst_port_free(mc_gst_port_t *port)
{
    g_mutex_free(port->mutex);
    g_queue_free(port->queue);
    g_cond_free(port->buffer_cond);

    //destroy buffers
    g_free(port);
    LOGD("gst_port is freed");

    return;
}

static void _mc_gst_update_caps(mc_gst_core_t *core, media_packet_h pkt, GstCaps **caps)
{
    guint format;

    format = __mc_get_gst_input_format(pkt, core->is_hw);

    if(!core->encoder && core->video)
    {
        *caps = gst_caps_new_simple ("video/x-h264",
                "width", G_TYPE_INT, core->dec_info->width,
                "height", G_TYPE_INT, core->dec_info->height,
                "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
    }
    else if (core->encoder && core->video && core->is_hw)
    {
        *caps = gst_caps_new_simple ("video/x-raw-yuv",
                "format", GST_TYPE_FOURCC, format,
                "width", G_TYPE_INT, core->enc_info->frame_width,
                "height", G_TYPE_INT, core->enc_info->frame_height,
                "framerate", GST_TYPE_FRACTION, core->enc_info->fps, 1, NULL);

        g_object_set (GST_OBJECT(core->codec), "bitrate", core->enc_info->bitrate*1000, NULL);
    }
    else if (core->encoder && core->video && !core->is_hw)
    {
        *caps = gst_caps_new_simple ("video/x-raw-yuv",
                "format", GST_TYPE_FOURCC, format,
                "width", G_TYPE_INT, core->enc_info->frame_width,
                "height", G_TYPE_INT, core->enc_info->frame_height,
                "framerate", GST_TYPE_FRACTION, core->enc_info->fps, 1, NULL);

        g_object_set (GST_OBJECT(core->codec), "bitrate", core->enc_info->bitrate*1000, NULL);
    }
    else if(!core->encoder && !core->video)
    {
        *caps = gst_caps_new_simple("audio/mpeg",
                "mpegversion", G_TYPE_INT, 4,
                "channels", G_TYPE_INT, 2,
                "rate", G_TYPE_INT, 48000,
                NULL);
    }
    else if(core->encoder && !core->video)
    {
        *caps = gst_caps_new_simple("audio/x-raw-int",
                "signed", G_TYPE_BOOLEAN, TRUE,
                "width", G_TYPE_INT, 16,
                "depth", G_TYPE_INT, 16,
                "endianness", G_TYPE_INT, G_BYTE_ORDER,
                "channels", G_TYPE_INT, 2,
                "rate", G_TYPE_INT, 48000, NULL);
    }
}

static gpointer feed_task(gpointer data)
{
    mc_gst_core_t *core = (mc_gst_core_t*)data;
    int ret = MC_ERROR_NONE;
    bool codec_config = FALSE;
    bool eos = FALSE;
    media_packet_h in_buf = NULL;
    mc_gst_buffer* buff = NULL;
    GstCaps *new_caps = NULL;
    bool initiative = true;

    while(g_atomic_int_get(&core->available_queue->running))
    {
        in_buf = get_input_buffer(core);

        if(!in_buf)
            goto EXIT;

        if(media_packet_is_codec_config(in_buf, &codec_config) != MEDIA_PACKET_ERROR_NONE)
        {
            LOGE("media_packet_is_codec_config failed");
            goto EXIT;
        }

        if(media_packet_is_end_of_stream(in_buf, &eos) != MEDIA_PACKET_ERROR_NONE)
        {
            LOGE("media_packet_is_end_of_stream failed");
            goto EXIT;
        }

        if(codec_config)
            initiative = true;

        if(initiative)
        {
            _mc_gst_update_caps(core, in_buf, &new_caps);
            LOGD("caps updated");
        }

        buff = _mc_gst_media_packet_to_gstbuffer(core, &new_caps, in_buf, codec_config);
        if (!buff)
        {
            LOGW("gstbuffer can't make");
            if (core->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR])
            {
                ((mc_error_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_ERROR])(MC_INVALID_IN_BUF, core->user_data[_MEDIACODEC_EVENT_TYPE_ERROR]);
            }
            goto EXIT;
        }

        /* inject buffer */
        ret = gst_app_src_push_buffer(GST_APP_SRC(core->appsrc), (GstBuffer*)buff);
        if(ret != GST_FLOW_OK)
        {
            LOGE("Failed to push gst buffer");
            goto EXIT;
        }

        if (eos)
        {
            g_signal_emit_by_name(core->appsrc, "end-of-stream", &ret);
        }
        initiative = false;
        continue;

EXIT:
    LOGI("status : in_buf : %p, codec_config : %d, eos : %d in feed_task", in_buf, codec_config, eos);
    }
    if(new_caps)
    {
        gst_caps_unref(new_caps);
    }
    LOGD("feed task finished");
    return NULL;
}

media_packet_h get_input_buffer(mc_gst_core_t *core)
{
    LOGD("waiting for input...");
    return mc_async_queue_pop(core->available_queue->input);
}

mc_ret_e mc_gst_prepare(mc_gst_core_t *core)
{
    int ret = MC_ERROR_NONE;

    if (!core)
        return MC_PARAM_ERROR;

    /* create basic core elements */
    ret = _mc_gst_create_pipeline(core);

    LOGD("initialized...");
    return ret;
}

mc_ret_e mc_gst_unprepare(mc_gst_core_t *core)
{
    int ret = MC_ERROR_NONE;

    if (!core)
        return ret;

    if(core->pipeline)
    {
        /* disconnect signal */
        if(core->fakesink && GST_IS_ELEMENT(core->fakesink))
        {
            if(g_signal_handler_is_connected(core->fakesink, core->signal_handoff))
                g_signal_handler_disconnect(core->fakesink, core->signal_handoff);
        }

        if(core->bus_whatch_id)
            g_source_remove(core->bus_whatch_id);

        MEDIACODEC_ELEMENT_SET_STATE(core->pipeline, GST_STATE_NULL);

        gst_object_unref(GST_OBJECT(core->pipeline));
    }
    return ret;

STATE_CHANGE_FAILED:
    if(core->pipeline)
        gst_object_unref(GST_OBJECT(core->pipeline));

    return MC_ERROR;
}

mc_ret_e mc_gst_process_input(mc_gst_core_t *core, media_packet_h inbuf, uint64_t timeOutUs)
{
    int ret = MC_ERROR_NONE;

    mc_async_queue_push(core->available_queue->input, inbuf);

    return ret;
}

mc_ret_e mc_gst_get_output(mc_gst_core_t *core, media_packet_h *outbuf, uint64_t timeOutUs)
{
    int ret = MC_ERROR_NONE;
    media_packet_h out_pkt = NULL;

    g_mutex_lock(core->ports[1]->mutex);

    if(!g_queue_is_empty(core->output_queue))
    {
        out_pkt = g_queue_pop_head(core->output_queue);
        LOGD("pop from output_queue : %p", core->output_queue);
    }
    else
    {
        ret = MC_OUTPUT_BUFFER_EMPTY;
        LOGD("output_queue is empty");
    }
    *outbuf = out_pkt;
    LOGD("output buf : %p", *outbuf);

    g_mutex_unlock(core->ports[1]->mutex);

    return ret;
}

static gboolean __mc_gst_init_gstreamer(mc_gst_core_t* core)
{
#if 1
    static gboolean initialized = FALSE;
    static const int max_argc = 50;
    gint* argc = NULL;
    gchar** argv = NULL;
    gchar** argv2 = NULL;
    GError *err = NULL;
    int i = 0;
    int arg_count = 0;

    if ( initialized )
    {
        LOGD("gstreamer already initialized.\n");
        return TRUE;
    }

    /* alloc */
    argc = malloc( sizeof(int) );
    argv = malloc( sizeof(gchar*) * max_argc );
    argv2 = malloc( sizeof(gchar*) * max_argc );

    if ( !argc || !argv )
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
#endif
    return FALSE;
}

mc_ret_e _mc_gst_create_pipeline(mc_gst_core_t* core)
{
    GstBus *bus = NULL;

    if (!__mc_gst_init_gstreamer(core))
    {
        LOGE ("gstreamer initialize fail");
        return MC_NOT_INITIALIZED;
    }
    core->codec = gst_element_factory_make(core->factory_name, NULL);

    if(!core->codec)
    {
        LOGE ("codec element create fail");
        goto ERROR;
    }

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

    /* add elements */
    gst_bin_add_many(GST_BIN(core->pipeline), core->appsrc, core->codec, core->fakesink, NULL);

    /* link elements */
    gst_element_link_many(core->appsrc, core->codec, core->fakesink, NULL);
    g_object_set (GST_OBJECT(core->codec), "byte-stream", TRUE, NULL);

    /* connect signals, bus watcher */
    bus = gst_pipeline_get_bus (GST_PIPELINE (core->pipeline));
    core->bus_whatch_id = gst_bus_add_watch (bus, __mc_gst_bus_call, core);
    gst_object_unref (bus);

    /* connect handoff */
    g_object_set (GST_OBJECT(core->fakesink), "signal-handoffs", TRUE, NULL);
    core->signal_handoff = g_signal_connect(core->fakesink, "handoff", G_CALLBACK(__mc_gst_buffer_add), core);

    /* set state PLAYING */
    MEDIACODEC_ELEMENT_SET_STATE(GST_ELEMENT_CAST(core->pipeline), GST_STATE_PLAYING);

    return MC_ERROR_NONE;

STATE_CHANGE_FAILED:
ERROR:
    if(core->codec)
        gst_object_unref(GST_OBJECT(core->codec));

    if(core->pipeline)
        gst_object_unref(GST_OBJECT(core->pipeline));

    if(core->appsrc)
        gst_object_unref(GST_OBJECT(core->appsrc));

    if(core->converter)
        gst_object_unref(GST_OBJECT(core->converter));

    if(core->fakesink)
        gst_object_unref(GST_OBJECT(core->fakesink));

    return MC_ERROR;
}

void __mc_gst_buffer_add (GstElement *element, GstBuffer *buffer, GstPad *pad, gpointer data)
{
    mc_gst_core_t* core = (mc_gst_core_t*) data;

    media_packet_h out_pkt = NULL;

    out_pkt = __mc_gst_gstbuffer_to_media_packet(core, buffer);

    /* push it to output buffer queue */
    g_queue_push_tail(core->output_queue, out_pkt);

    if (core->user_cb[_MEDIACODEC_EVENT_TYPE_FILLBUFFER])
    {
        ((mc_fill_buffer_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_FILLBUFFER])(out_pkt, core->user_data[_MEDIACODEC_EVENT_TYPE_FILLBUFFER]);
    }
}

int __mc_output_buffer_finalize_cb(media_packet_h packet, int error_code, void *user_data)
{
    //GstBuffer* buffer = NULL;
    void* buffer = NULL;
    media_packet_get_extra(packet, &buffer);
    LOGD("!!!! finalized_gst_buffer = %p", buffer);
    gst_buffer_unref((GstBuffer*)buffer);

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


mc_gst_buffer* _mc_gst_media_packet_to_gstbuffer(mc_gst_core_t* core, GstCaps **caps, media_packet_h pkt, bool codec_config)
{
    mc_gst_buffer* buff = NULL;
    void* buf_data = NULL;
    uint64_t buf_size = 0;
    uint64_t pts = 0, dur = 0;
    int ret = MEDIA_PACKET_ERROR_NONE;

    /* copy media_packet to new gstbuffer */
    ret = media_packet_get_buffer_size(pkt, &buf_size);
    if (ret != MEDIA_PACKET_ERROR_NONE)
    {
        LOGW("buffer size get fail");
    }

    ret = media_packet_get_buffer_data_ptr(pkt, &buf_data);
    if (ret != MEDIA_PACKET_ERROR_NONE)
    {
        LOGW("buffer size get fail");
        return NULL;
    }

    if (core->encoder && core->video && core->is_hw)
    {
        SCMN_IMGB *imgb = NULL;
        buff = __mc_gst_buffer_new(core);
        buff->pkt = pkt;
        GST_BUFFER_DATA(buff) = buf_data;
        GST_BUFFER_SIZE (buff) = buf_size;
        imgb = __mc_gst_make_tbm_buffer(core, pkt);

        if (!imgb)
        {
            LOGE("get imgb failed");
            return NULL;
        }
        GST_BUFFER_MALLOCDATA(buff) = (guint8 *)imgb;
    }
    else
    {
        buff = __mc_gst_buffer_new (core);
        buff->pkt = pkt;
        GST_BUFFER_DATA(buff) = buf_data;
        GST_BUFFER_SIZE (buff) = buf_size;
    }

    /* pts */
    media_packet_get_pts(pkt, &pts);
    GST_BUFFER_TIMESTAMP(buff) = (GstClockTime) pts;
    LOGD("input pts = %llu, GST_BUFFER_TIMESTAMP = %" GST_TIME_FORMAT, pts, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buff)));

    /* duration */
    media_packet_get_duration(pkt, &dur);
    GST_BUFFER_DURATION(buff) = dur;

    GST_BUFFER_CAPS(buff) = gst_caps_copy(*caps);

    return buff;
}

media_packet_h __mc_gst_gstbuffer_to_media_packet(mc_gst_core_t* core, GstBuffer* buffer)
{
    media_packet_h out_pkt = NULL;
    media_format_h out_fmt = NULL;
    void* pkt_data = NULL;

    if (!buffer)
        return NULL;

    out_fmt = core->output_fmt;

    if (!core->encoder && core->video && core->is_hw)
    {
        int i = 0;;
        int bo_num = 0;

        SCMN_IMGB *psimgb = NULL;
        tbm_surface_h tsurf = NULL;
        psimgb = (SCMN_IMGB*)GST_BUFFER_MALLOCDATA(buffer);
        /* create tbm surface */
        for (i = 0; i < SCMN_IMGB_MAX_PLANE; i++)
        {
            if (psimgb->bo[i])
            {
                bo_num++;
            }
        }

        if (bo_num > 0)
        {
            tsurf = tbm_surface_internal_create_with_bos(core->dec_info->width, core->dec_info->height, TBM_FORMAT_NV12, (tbm_bo *)psimgb->bo, bo_num);
            LOGD("tbm surface %p", tsurf);
        }

        if (tsurf)
        {
            media_packet_create_from_tbm_surface(out_fmt, tsurf, (media_packet_finalize_cb)__mc_output_buffer_finalize_cb, core, &out_pkt);
            LOGD("!!!! create_gst_buffer = %p", out_pkt);
            media_packet_set_extra(out_pkt, buffer);
            media_packet_set_buffer_size(out_pkt, GST_BUFFER_SIZE(buffer));
            media_packet_set_pts(out_pkt, GST_BUFFER_TIMESTAMP(buffer));
        }
    }
    else
    {
        /* create media_packet with given gstbuffer */
        media_packet_create_alloc(out_fmt, __mc_output_buffer_finalize_cb, core, &out_pkt);
        media_packet_set_extra(out_pkt, buffer);
        LOGD("!!!! create_gst_buffer = %p", buffer);

        // !!!! this time do memcpy because no way to set data ptr to media packet.
        media_packet_set_buffer_size(out_pkt, GST_BUFFER_SIZE(buffer));
        media_packet_get_buffer_data_ptr(out_pkt, &pkt_data);
        memcpy(pkt_data, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));
        media_packet_set_pts(out_pkt, GST_BUFFER_TIMESTAMP(buffer));
    }
    gst_buffer_ref(buffer);
    return out_pkt;
}

gboolean __mc_gst_bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
    int ret  = MC_ERROR_NONE;
    mc_gst_core_t *core = (mc_gst_core_t*)data;

    switch (GST_MESSAGE_TYPE (msg)) {

        case GST_MESSAGE_EOS:
        {
            LOGD ("End of stream\n");
            if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])
            {
                ((mc_eos_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EOS])(core->user_data[_MEDIACODEC_EVENT_TYPE_EOS]);
            }
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
    //gst_object_unref(msg);

    return TRUE;
}

static SCMN_IMGB * __mc_gst_make_tbm_buffer(mc_gst_core_t* core, media_packet_h pkt)
{
    tbm_surface_h surface = NULL;
    tbm_bo bo = NULL;

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

    /*psimgb->a[0] point to vitual address of output buffer*/
    psimgb->buf_share_method = BUF_SHARE_METHOD_TIZEN_BUFFER;
    psimgb->bo[0] = bo;
    psimgb->a[0] =  handle.ptr;
    psimgb->w[0] = core->enc_info->frame_width;
    psimgb->h[0] = core->enc_info->frame_height;
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
            sizeof (mc_gst_buffer),
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

static mc_gst_buffer* __mc_gst_buffer_new(mc_gst_core_t* core)
{
    mc_gst_buffer *ret = NULL;
    ret = (mc_gst_buffer *)gst_mini_object_new(GST_TYPE_MC_BUFFER);
    LOGD("creating buffer : %p", ret);
    ret->core = core;
    return ret;
}

static void __mc_gst_buffer_finalize(mc_gst_buffer *buffer)
{
    mc_gst_core_t *core = buffer->core;
    SCMN_IMGB *imgb = NULL;

    if (GST_BUFFER_MALLOCDATA(buffer))
    {
        imgb = (SCMN_IMGB *)GST_BUFFER_MALLOCDATA(buffer);
        if (imgb)
        {
            free(imgb);
            imgb = NULL;
            GST_BUFFER_MALLOCDATA(buffer) = NULL;
        }
    }

    GST_BUFFER_DATA(buffer) = NULL;

    if (core->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER])
    {
        ((mc_empty_buffer_cb) core->user_cb[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER])(buffer->pkt, core->user_data[_MEDIACODEC_EVENT_TYPE_EMPTYBUFFER]);
        LOGD("finalize, pkt : %p, buffer : %p", buffer->pkt, buffer);
    }

    if (GST_MINI_OBJECT_CLASS (mc_gst_buffer_parent_class)->finalize) {
        GST_MINI_OBJECT_CLASS (mc_gst_buffer_parent_class)->finalize (GST_MINI_OBJECT(buffer));
    }

    return;
}

static gint __gst_handle_core_error(mc_gst_core_t* core, int code )
{
    gint trans_err = MEDIACODEC_ERROR_NONE;

    g_return_val_if_fail(core, MEDIACODEC_ERROR_NOT_INITIALIZED);

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

    g_return_val_if_fail(core, MEDIACODEC_ERROR_NOT_INITIALIZED);

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

    g_return_val_if_fail(core, MEDIACODEC_ERROR_NOT_INITIALIZED);

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

    g_return_val_if_fail(core, MEDIACODEC_ERROR_NOT_INITIALIZED);
    g_return_val_if_fail(error, MEDIACODEC_ERROR_INVALID_PARAMETER);
    g_return_val_if_fail (message, MEDIACODEC_ERROR_INVALID_PARAMETER);

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
