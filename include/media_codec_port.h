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
#include <media_codec_bitstream.h>
#include <media_codec_spec_emul.h>
#include <media_codec_ini.h>


/*===========================================================================================
|                                                                                                                                                                                      |
|  GLOBAL DEFINITIONS AND DECLARATIONS FOR MODULE                                                                                      |
|                                                                                                                                                                                      |
========================================================================================== */

/*---------------------------------------------------------------------------
|    GLOBAL #defines:                                                                                                                    |
---------------------------------------------------------------------------*/
#define CHECK_BIT(x, y) (((x) >> (y)) & 0x01)
#define GET_IS_ENCODER(x) CHECK_BIT(x, 0)
#define GET_IS_DECODER(x) CHECK_BIT(x, 1)
#define GET_IS_HW(x) CHECK_BIT(x, 2)
#define GET_IS_SW(x) CHECK_BIT(x, 3)

//#define GET_IS_OMX(x) CHECK_BIT(x, 4)
//#define GET_IS_GEN(x) CHECK_BIT(x, 5)

#if 1
#define MEDIACODEC_FENTER();
#define MEDIACODEC_FLEAVE();
#else
#define MEDIACODEC_FENTER();          LOGW("%s Enter",__FUNCTION__);
#define MEDIACODEC_FLEAVE();          LOGW("%s Exit",__FUNCTION__);
#endif

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
    MC_COURRPTED_INI            =   -19,
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

typedef void (*mc_dequeue_input_buffer_cb)(media_packet_h pkt, void *user_data);
typedef void (*mc_empty_buffer_cb)(media_packet_h pkt, void *user_data);
typedef void (*mc_fill_buffer_cb)(media_packet_h pkt, void *user_data);
typedef void (*mc_error_cb)(mediacodec_error_e error, void *user_data);
typedef void (*mc_eos_cb)(void *user_data);
typedef void (*mc_buffer_status_cb)(mediacodec_status_e status, void *user_data);
typedef void (*mc_supported_codec_cb)(mediacodec_codec_type_e codec_type, void *user_data);

int (*mc_sniff_bitstream)(mc_handle_t *handle, media_packet_h pkt);

int mc_sniff_h264_bitstream(mc_handle_t *handle, media_packet_h pkt);
int mc_sniff_mpeg4_bitstream(mc_handle_t *handle, media_packet_h pkt);
int mc_sniff_h263_bitstream(mc_handle_t *handle, media_packet_h pkt);
int mc_sniff_yuv(mc_handle_t *handle, media_packet_h pkt);

typedef enum {
    _MEDIACODEC_EVENT_TYPE_COMPLETE,
    _MEDIACODEC_EVENT_TYPE_EMPTYBUFFER,
    _MEDIACODEC_EVENT_TYPE_FILLBUFFER,
    _MEDIACODEC_EVENT_TYPE_ERROR,
    _MEDIACODEC_EVENT_TYPE_EOS,
    _MEDIACODEC_EVENT_TYPE_BUFFER_STATUS,
    _MEDIACODEC_EVENT_TYPE_INTERNAL_FILLBUFFER,
    _MEDIACODEC_EVENT_TYPE_SUPPORTED_CODEC,
    _MEDIACODEC_EVENT_TYPE_NUM
} _mediacodec_event_e;


typedef enum _mc_codec_port_type_e
{
    CODEC_PORT_TYPE_GENERAL,
    CODEC_PORT_TYPE_OMX,
    CODEC_PORT_TYPE_GST,
    CODEC_PORT_TYPE_MAX,
} mc_codec_port_type_e;

typedef enum _mc_vendor_e
{
    MC_VENDOR_DEFAULT,
    MC_VENDOR_SLSI_SEC,
    MC_VENDOR_SLSI_EXYNOS,
    MC_VENDOR_QCT,
    MC_VENDOR_SPRD
} mc_vendor_e;

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
    int width;
    int height;
    int bitrate;
    int format;
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

/* Codec Private data */
struct _mc_handle_t
{
    int state;                                  /**<  mc current state */
    bool is_encoder;
    bool is_video;
    bool is_hw;
    bool is_prepared;

    GList *supported_codecs;
    mediacodec_port_type_e port_type;
    mediacodec_codec_type_e codec_id;
    mc_vendor_e vendor;

    void *ports[2];
    void *core;

    union
    {
        mc_decoder_info_t decoder;
        mc_encoder_info_t encoder;
    } info;

    /* for process done cb */
    void* user_cb[_MEDIACODEC_EVENT_TYPE_NUM];
    void* user_data[_MEDIACODEC_EVENT_TYPE_NUM];

    mc_codec_map_t encoder_map[MEDIA_CODEC_MAX_CODEC_TYPE];
    mc_codec_map_t decoder_map[MEDIA_CODEC_MAX_CODEC_TYPE];

    int num_supported_codecs;
    int num_supported_decoder;
    int num_supported_encoder;

    mc_ini_t ini;
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

int mc_process_input(MMHandleType mediacodec, media_packet_h inbuf, uint64_t timeOutUs);
int mc_get_output(MMHandleType mediacodec, media_packet_h *outbuf, uint64_t timeOutUs);

int mc_flush_buffers(MMHandleType mediacodec);
int mc_get_supported_type(MMHandleType mediacodec, mediacodec_codec_type_e codec_type, bool encoder, int *support_type);

int mc_set_empty_buffer_cb(MMHandleType mediacodec, mediacodec_input_buffer_used_cb callback, void* user_data);
int mc_unset_empty_buffer_cb(MMHandleType mediacodec);

int mc_set_fill_buffer_cb(MMHandleType mediacodec, mediacodec_output_buffer_available_cb callback, void* user_data);
int mc_unset_fill_buffer_cb(MMHandleType mediacodec);

int mc_set_error_cb(MMHandleType mediacodec, mediacodec_error_cb callback, void* user_data);
int mc_unset_error_cb(MMHandleType mediacodec);

int mc_set_eos_cb(MMHandleType mediacodec, mediacodec_eos_cb callback, void* user_data);
int mc_unset_eos_cb(MMHandleType mediacodec);

int mc_set_buffer_status_cb(MMHandleType mediacodec, mediacodec_buffer_status_cb callback, void* user_data);
int mc_unset_buffer_status_cb(MMHandleType mediacodec);


#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_PORT_H__ */
