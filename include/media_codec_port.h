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

#ifndef __TIZEN_MEDIA_CODEC_PORT_H__
#define __TIZEN_MEDIA_CODEC_PORT_H__

#include <tizen.h>
#include <glib.h>
#include <dlfcn.h>
#include <gst/gst.h>

#include <media_codec.h>
#include <media_codec_queue.h>
#include <media_codec_spec_emul.h>


/*===========================================================================================
|                                                                                                                                                                                      |
|  GLOBAL DEFINITIONS AND DECLARATIONS FOR MODULE                                                                                      |
|                                                                                                                                                                                      |
========================================================================================== */

/*---------------------------------------------------------------------------
|    GLOBAL #defines:                                                                                                                    |
---------------------------------------------------------------------------*/
#define OUT_BUF_SIZE    9000000
#define CHECK_BIT(x, y) (((x) >> (y)) & 0x01)
#define GET_IS_ENCODER(x) CHECK_BIT(x, 0)
#define GET_IS_DECODER(x) CHECK_BIT(x, 1)
#define GET_IS_HW(x) CHECK_BIT(x, 2)
#define GET_IS_SW(x) CHECK_BIT(x, 3)

//#define GET_IS_OMX(x) CHECK_BIT(x, 4)
//#define GET_IS_GEN(x) CHECK_BIT(x, 5)


/*---------------------------------------------------------------------------
|    GLOBAL CONSTANT DEFINITIONS:                                                                                          |
---------------------------------------------------------------------------*/
/**
 * @brief Enumerations of media codec port's retrun value
 */
typedef enum
{
    MC_ERROR_NONE               =    0,
    MC_ERROR                    =   -1,     /**< codec happens error */
    MC_MEMORY_ERROR             =   -2,     /**< codec memory is not enough */
    MC_PARAM_ERROR              =   -3,     /**< codec parameter is error */
    MC_INVALID_ARG              =   -4,     /** < codec has invalid arguments */
    MC_PERMISSION_DENIED        =   -5,
    MC_INVALID_STATUS           =   -6,     /**< codec works at invalid status */
    MC_NOT_SUPPORTED            =   -7,     /**< codec can't support this specific video format */
    MC_INVALID_IN_BUF           =   -8,
    MC_INVALID_OUT_BUF          =   -9,
    MC_INTERNAL_ERROR           =   -10,
    MC_HW_ERROR                 =   -11,    /**< codec happens hardware error */
    MC_NOT_INITIALIZED          =   -12,
    MC_INVALID_STREAM           =   -13,
    MC_CODEC_NOT_FOUND          =   -14,
    MC_ERROR_DECODE             =   -15,
    MC_OUTPUT_BUFFER_EMPTY      =   -16,
    MC_OUTPUT_BUFFER_OVERFLOW   =   -17,    /**< codec output buffer is overflow */
    MC_MEMORY_ALLOCED           =   -18,    /**< codec has got memory and can decode one frame */
} mc_ret_e;

/*---------------------------------------------------------------------------
|    GLOBAL DATA TYPE DEFINITIONS:                                                                                         |
---------------------------------------------------------------------------*/
/**
 * @brief Called when the dequeue input buffer done
 * @details It will be invoked when mediacodec has released dequeue buffer.
 * @param[in]   user_data  The user data passed from the callback registration function
 * @pre It will be invoked when dequeue buffer process completed if you register this callback using mediacodec_set_dequeue_input_buffer_cb().
 * @see mediacodec_set_dequeue_input_buffer_cb()
 * @see mediacodec_unset_dequeue_input_buffer_cb()
 */

typedef struct _mc_decoder_info_t mc_decoder_info_t;
typedef struct _mc_encoder_info_t mc_encoder_info_t;

typedef struct _mc_handle_t mc_handle_t;

/* gst port layer */
typedef struct _mc_gst_port_t mc_gst_port_t;
typedef struct _mc_gst_core_t mc_gst_core_t;

typedef void (*mc_dequeue_input_buffer_cb)(media_packet_h pkt, void *user_data);
typedef void (*mc_empty_buffer_cb)(media_packet_h pkt, void *user_data);
typedef void (*mc_fill_buffer_cb)(media_packet_h pkt, void *user_data);
typedef void (*mc_error_cb)(mediacodec_error_e error, void *user_data);
typedef void (*mc_eos_cb)(void *user_data);

typedef enum {
    _MEDIACODEC_EVENT_TYPE_COMPLETE,
    _MEDIACODEC_EVENT_TYPE_EMPTYBUFFER,
    _MEDIACODEC_EVENT_TYPE_FILLBUFFER,
    _MEDIACODEC_EVENT_TYPE_ERROR,
    _MEDIACODEC_EVENT_TYPE_EOS,
    _MEDIACODEC_EVENT_TYPE_INTERNAL_FILLBUFFER,
    _MEDIACODEC_EVENT_TYPE_NUM
} _mediacodec_event_e;


typedef enum _mc_codec_port_type_e
{
	CODEC_PORT_TYPE_GENERAL,
	CODEC_PORT_TYPE_OMX,
	CODEC_PORT_TYPE_GST,
	CODEC_PORT_TYPE_MAX,
} mc_codec_port_type_e;

struct _mc_decoder_info_t
{
    int width;
    int height;
    int actwidth;
    int actheight;

    int samplerate;
    int channel;
    int bit;
};

struct _mc_encoder_info_t
{
    int frame_width;
    int frame_height;
    int bitrate;
    int fps;
    int qp_min;
    int qp_max;
    int vbvbuffer_size;
    int level;
    int profile;

    int samplerate;
    int channel;
    int bit;
};

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
    GMutex *mutex;
    GCond *buffer_cond;
};

struct _mc_gst_core_t
{
    GstState state;
    bool output_allocated;
    bool encoder;
    bool video;
    bool is_hw;

    mediacodec_codec_type_e codec_id;
    media_format_h output_fmt;
    mc_gst_port_t *ports[2];

    /* gst stuffs */
    GstElement* pipeline;
    GstElement* appsrc;
    GstElement* converter;
    GstElement* fakesink;
    GstElement* codec;

    gulong signal_handoff;
    gint bus_whatch_id;

    mc_aqueue_t *available_queue;
    GQueue *output_queue;

    mc_decoder_info_t *dec_info;
    mc_encoder_info_t *enc_info;

    void* user_cb[_MEDIACODEC_EVENT_TYPE_NUM];
    void* user_data[_MEDIACODEC_EVENT_TYPE_NUM];

    gchar *factory_name;
    gchar *mime;
};

/* Codec Private data */
struct _mc_handle_t
{
    void *hcodec;                               /**< codec handle */
    int state;                                  /**<  mc current state */
    mediacodec_port_type_e port_type;
    bool is_encoder;
    bool is_video;
    bool is_hw;
    bool is_codec_config;                           /** < codec config data for first frame(SPS - using in AVC) */
    bool output_allocated;
    bool is_prepared;

    int frame_count;
    int out_buf_cnt;
    int *out_buf_ref;

    mediacodec_codec_type_e codec_id;

    /* for gst port */
    mc_gst_port_t *gst_ports[2];
    mc_gst_core_t *gst_core;

    /* for Decoder */
    mc_decoder_info_t *dec_info;

    /* for Encoder */
    mc_encoder_info_t *enc_info;

    /* for process done cb */
    void* user_cb[_MEDIACODEC_EVENT_TYPE_NUM];
    void* user_data[_MEDIACODEC_EVENT_TYPE_NUM];

    mc_codec_spec_t g_media_codec_spec_emul[MC_MAX_NUM_CODEC];
};

/*===========================================================================================
|                                                                                                                                                                                      |
|  GLOBAL FUNCTION PROTOTYPES                                                                                                                            |
|                                                                                                                                                                                      |
========================================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

int mc_create(MMHandleType *mediacodec);
int mc_destroy(MMHandleType mediacodec);

int mc_set_codec(MMHandleType mediacodec, mediacodec_codec_type_e codec_id, mediacodec_support_type_e flags);

int mc_set_vdec_info(MMHandleType mediacodec, int width, int height);
int mc_set_venc_info(MMHandleType mediacodec, int width, int height, int fps, int target_bits);

int mc_set_adec_info(MMHandleType mediacodec, int samplerate, int channel, int bit);
int mc_set_aenc_info(MMHandleType mediacodec, int samplerate, int channel, int bit,  int bitrate);

int mc_prepare(MMHandleType mediacodec);
int mc_unprepare(MMHandleType mediacodec);
int mc_reset(MMHandleType mediacodec);

int mc_process_input(MMHandleType mediacodec, media_packet_h inbuf, uint64_t timeOutUs);
int mc_get_output(MMHandleType mediacodec, media_packet_h *outbuf, uint64_t timeOutUs);

int mc_set_empty_buffer_cb(MMHandleType mediacodec, mediacodec_input_buffer_used_cb callback, void* user_data);
int mc_unset_empty_buffer_cb(MMHandleType mediacodec);

int mc_set_fill_buffer_cb(MMHandleType mediacodec, mediacodec_output_buffer_available_cb callback, void* user_data);
int mc_unset_fill_buffer_cb(MMHandleType mediacodec);

int mc_set_error_cb(MMHandleType mediacodec, mediacodec_error_cb callback, void* user_data);
int mc_unset_error_cb(MMHandleType mediacodec);

int mc_set_eos_cb(MMHandleType mediacodec, mediacodec_eos_cb callback, void* user_data);
int mc_unset_eos_cb(MMHandleType mediacodec);

#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_PORT_H__ */
