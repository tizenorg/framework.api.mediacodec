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

#ifndef __TIZEN_MEDIA_CODEC_EMUL_H__
#define __TIZEN_MEDIA_CODEC_EMUL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <media_codec_private.h>

#define MC_MAX_NUM_CODEC 4
#define MC_MAX_NUM_MAP  4
#define MC_MAX_FACTORY_STRLEN   20

enum { DECODER, ENCODER };
enum { SOFTWARE, HARDWARE };

//typedef struct _mc_codec_mimetype_t mc_codec_mimetype_t;
typedef struct _mc_codec_spec_t mc_codec_spec_t;
typedef struct _mc_codec_map_t mc_codec_map_t;
typedef struct _mc_codec_type_t mc_codec_type_t;

struct _mc_codec_spec_t
{
    mediacodec_codec_type_e mime;
    mediacodec_support_type_e codec_type;
    mediacodec_support_type_e support_type;
    mediacodec_port_type_e port_type;
};

struct _mc_codec_type_t
{
    char *factory_name;
    media_format_mimetype_e out_format;
    char *mime;
};

struct _mc_codec_map_t
{
    mediacodec_codec_type_e id;
    bool hardware;
    mc_codec_type_t type;
};

static const mc_codec_spec_t spec_emul[MC_MAX_NUM_CODEC] =
{
    {MEDIACODEC_H264,  MEDIACODEC_DECODER, MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_H263,  MEDIACODEC_ENCODER, MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_AAC,  MEDIACODEC_ENCODER, MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST},
    {MEDIACODEC_AAC,  MEDIACODEC_DECODER, MEDIACODEC_SUPPORT_TYPE_SW, MEDIACODEC_PORT_TYPE_GST}
};

static const mc_codec_map_t encoder_map[] =
{
#ifdef ENABLE_FFMPEG_CODEC
    {MEDIACODEC_H263, SOFTWARE, {"ffenc_h263p", MEDIA_FORMAT_H263P, "video/x-raw-yuv"}},
    {MEDIACODEC_AAC,  SOFTWARE, {"ffenc_aac", MEDIA_FORMAT_AAC, "audio/x-raw-int"}}
#else
    {MEDIACODEC_H263, SOFTWARE, {"maru_h263penc", MEDIA_FORMAT_H263P, "video/x-raw-yuv"}},
    {MEDIACODEC_AAC,  SOFTWARE, {"maru_aacenc", MEDIA_FORMAT_AAC, "audio/x-raw-int"}}
#endif
};

static const mc_codec_map_t decoder_map[] =
{
#ifdef ENABLE_FFMPEG_CODEC
    {MEDIACODEC_H264, SOFTWARE, {"ffdec_h264", MEDIA_FORMAT_I420, "video/x-h264"}},
    {MEDIACODEC_AAC,  SOFTWARE, {"ffdec_aac", MEDIA_FORMAT_PCM, "audio/mpeg"}}
#else
    {MEDIACODEC_H264, SOFTWARE, {"maru_h264dec", MEDIA_FORMAT_I420, "video/x-h264"}},
    {MEDIACODEC_AAC,  SOFTWARE, {"maru_aacdec", MEDIA_FORMAT_PCM, "audio/mpeg"}}
#endif
};

#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_EMUL_H__ */
