
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

#ifndef __TIZEN_MEDIA_CODEC_PORT_GST_H__
#define __TIZEN_MEDIA_CODEC_PORT_GST_H__

#include <unistd.h>
#include <tizen.h>
#include <media_codec.h>
#include <media_codec_private.h>
#include <media_codec_port.h>

#include <tbm_type.h>
#include <tbm_surface.h>
#include <tbm_bufmgr.h>
#include <tbm_surface_internal.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#define MMPLAYER_FENTER();          debug_fenter();
#define MMPLAYER_FLEAVE();          debug_fleave();
#else
#define MMPLAYER_FENTER();          LOGW("%s Enter",__FUNCTION__);
#define MMPLAYER_FLEAVE();          LOGW("%s Exit",__FUNCTION__);
#endif


#define GST_INIT_STRUCTURE(param) \
    memset(&(param), 0, sizeof(param));

#define MEDIACODEC_ELEMENT_SET_STATE( x_element, x_state ) \
LOGD("setting state [%s:%d] to [%s]\n", #x_state, x_state, GST_ELEMENT_NAME( x_element ) ); \
if ( GST_STATE_CHANGE_FAILURE == gst_element_set_state ( x_element, x_state) ) \
{ \
    LOGE("failed to set state %s to %s\n", #x_state, GST_ELEMENT_NAME( x_element )); \
    goto STATE_CHANGE_FAILED; \
}

#define SCMN_IMGB_MAX_PLANE 4
#define TBM_API_CHANGE                          //this need for temporary test - tbm_surface_internal_create_with_bos() API change
/* gst port layer */
typedef struct _mc_gst_port_t mc_gst_port_t;
typedef struct _mc_gst_core_t mc_gst_core_t;
typedef struct _mc_gst_buffer_t mc_gst_buffer_t;

typedef enum {
    BUF_SHARE_METHOD_PADDR = 0,
    BUF_SHARE_METHOD_FD,
    BUF_SHARE_METHOD_TIZEN_BUFFER,
    BUF_SHARE_METHOD_FLUSH_BUFFER
} buf_share_method_t;

#ifdef TIZEN_PROFILE_LITE
struct ion_mmu_data {
       int fd_buffer;
       unsigned long iova_addr;
       size_t iova_size;
};
#endif

typedef struct
{
    int w[SCMN_IMGB_MAX_PLANE];     /* width of each image plane */
    int h[SCMN_IMGB_MAX_PLANE];     /* height of each image plane */
    int s[SCMN_IMGB_MAX_PLANE];     /* stride of each image plane */
    int e[SCMN_IMGB_MAX_PLANE];     /* elevation of each image plane */
    void *a[SCMN_IMGB_MAX_PLANE];       /* user space address of each image plane */
    void *p[SCMN_IMGB_MAX_PLANE];       /* physical address of each image plane, if needs */
    int cs;     /* color space type of image */
    int x;      /* left postion, if needs */
    int y;      /* top position, if needs */
    int __dummy2;       /* to align memory */
    int data[16];       /* arbitrary data */
    int dma_buf_fd[SCMN_IMGB_MAX_PLANE];        /* DMABUF fd of each image plane */
    /* buffer share method */
    int buf_share_method;   /* will be 2(BUF_SHARE_METHOD_TIZEN_BUFFER)*/
    int y_size;     /* Y plane size in case of ST12 */
    /* UV plane size in case of ST12 */
    int uv_size;        /* UV plane size in case of ST12 */
    tbm_bo bo[SCMN_IMGB_MAX_PLANE];     /* Tizen buffer object of each image plane */
    void *jpeg_data;        /* JPEG data */
    int jpeg_size;      /* JPEG size */
    int tz_enable;      /* TZ memory buffer */
} SCMN_IMGB;

struct _mc_gst_port_t
{
    mc_gst_core_t *core;
    unsigned int num_buffers;
    unsigned int buffer_size;
    unsigned int index;
    bool is_allocated;
    media_packet_h *buffers;
    //GSem
    GQueue *queue;
    GMutex mutex;
    GCond buffer_cond;
};

struct _mc_gst_core_t
{
    int(**vtable)();
    const char *mime;
    int format;
    GstElement* pipeline;
    GstElement* appsrc;
    GstElement* fakesink;
    GstElement* codec;
    SCMN_IMGB *psimgb;

    GMainContext *thread_default;
    gulong signal_handoff;
    gint bus_whatch_id;
    gint probe_id;

    GMutex eos_mutex;
    GMutex eos_wait_mutex;
    GMutex drain_mutex;
    GMutex prepare_lock;
    GCond eos_cond;
    GCond eos_waiting_cond;
    //mc_sem_t *push_sem;
    //mc_sem_t *pop_sem;

    GstState state;
    bool output_allocated;
    bool encoder;
    bool video;
    bool is_hw;
    bool eos;
    bool eos_waiting;
    bool codec_config;
    bool need_feed;
    int prepare_count;
    int num_live_buffers;

    mediacodec_codec_type_e codec_id;
    media_format_h output_fmt;
    mc_gst_port_t *ports[2];

    mc_aqueue_t *available_queue;
    GQueue *output_queue;

    void *codec_info;

    void* user_cb[_MEDIACODEC_EVENT_TYPE_NUM];
    void* user_data[_MEDIACODEC_EVENT_TYPE_NUM];

};

struct _mc_gst_buffer_t
{
    GstBuffer buffer;
    mc_gst_core_t* core;
    media_packet_h pkt;
};

enum { fill_inbuf, fill_outbuf, create_caps };

int __mc_fill_input_buffer(mc_gst_core_t *core, mc_gst_buffer_t *buff);
int __mc_fill_output_buffer(mc_gst_core_t *core, GstBuffer *buff, media_packet_h *out_pkt);
int __mc_create_caps(mc_gst_core_t *core, GstCaps **caps);

int __mc_fill_inbuf_with_bo(mc_gst_core_t *core, mc_gst_buffer_t *buff);
int __mc_fill_inbuf_with_packet(mc_gst_core_t *core, mc_gst_buffer_t *buff);

int __mc_fill_outbuf_with_bo(mc_gst_core_t *core, GstBuffer *buff, media_packet_h *out_pkt);
int __mc_fill_outbuf_with_packet(mc_gst_core_t *core, GstBuffer *buff, media_packet_h *out_pkt);

int __mc_venc_caps(mc_gst_core_t *core, GstCaps **caps);
int __mc_vdec_caps(mc_gst_core_t *core, GstCaps **caps);
int __mc_aenc_caps(mc_gst_core_t *core, GstCaps **caps);
int __mc_adec_caps(mc_gst_core_t *core, GstCaps **caps);
int __mc_mp3dec_caps(mc_gst_core_t *core, GstCaps **caps);
int __mc_h263enc_caps(mc_gst_core_t *core, GstCaps **caps);

mc_gst_core_t *mc_gst_core_new();
void mc_gst_core_free(mc_gst_core_t *core);

mc_gst_port_t *mc_gst_port_new(mc_gst_core_t *core);
void mc_gst_port_free(mc_gst_port_t *port);

mc_ret_e mc_gst_prepare(mc_handle_t *mc_handle);
mc_ret_e mc_gst_unprepare(mc_handle_t *mc_handle);

mc_ret_e mc_gst_process_input(mc_handle_t *mc_handle, media_packet_h inbuf, uint64_t timeOutUs);
mc_ret_e mc_gst_get_output(mc_handle_t *mc_handle, media_packet_h *outbuf, uint64_t timeOutUs);

#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_PORT_GST_H__ */
