
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
    /* width of each image plane */
    int w[SCMN_IMGB_MAX_PLANE];
    /* height of each image plane */
    int h[SCMN_IMGB_MAX_PLANE];
    /* stride of each image plane */
    int s[SCMN_IMGB_MAX_PLANE];
    /* elevation of each image plane */
    int e[SCMN_IMGB_MAX_PLANE];
    /* user space address of each image plane */
    void *a[SCMN_IMGB_MAX_PLANE];
    /* physical address of each image plane, if needs */
    void *p[SCMN_IMGB_MAX_PLANE];
    /* color space type of image */
    int cs;
    /* left postion, if needs */
    int x;
    /* top position, if needs */
    int y;
    /* to align memory */
    int __dummy2;
    /* arbitrary data */
    int data[16];
    /* DMABUF fd of each image plane */
    int dma_buf_fd[SCMN_IMGB_MAX_PLANE];
    /* buffer share method */
    int buf_share_method;   /* will be 2(BUF_SHARE_METHOD_TIZEN_BUFFER)*/
    /* Y plane size in case of ST12 */
    int y_size;
    /* UV plane size in case of ST12 */
    int uv_size;
    /* Tizen buffer object of each image plane */
    tbm_bo bo[SCMN_IMGB_MAX_PLANE];
    /* JPEG data */
    void *jpeg_data;
    /* JPEG size */
    int jpeg_size;
	/* TZ memory buffer */
    int tz_enable;
} SCMN_IMGB;

typedef struct {
    GstBuffer buffer;
    mc_gst_core_t* core;
    media_packet_h pkt;
} mc_gst_buffer;

typedef void (*mediacodec_internal_fillbuffer_cb)(media_packet_h pkt, bool avail_output, void *user_data);

mc_gst_core_t *mc_gst_core_new();
void mc_gst_core_free(mc_gst_core_t *core);
mc_gst_port_t *mc_gst_port_new(mc_gst_core_t *core);
void mc_gst_port_free(mc_gst_port_t *port);

mc_ret_e mc_gst_prepare(mc_gst_core_t *core);
mc_ret_e mc_gst_unprepare(mc_gst_core_t *core);

mc_ret_e mc_gst_creat_handle(mc_gst_core_t *core);
mc_ret_e mc_gst_delete(mc_gst_core_t *core);

mc_ret_e mc_gst_process_input(mc_gst_core_t *core, media_packet_h inbuf, uint64_t timeOutUs);
mc_ret_e mc_gst_get_output(mc_gst_core_t *core, media_packet_h *outbuf, uint64_t timeOutUs);

mc_ret_e mc_gst_set_vdec_info(mc_gst_core_t *core);
mc_ret_e mc_gst_set_venc_info(mc_gst_core_t *core);

#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_PORT_GST_H__ */
