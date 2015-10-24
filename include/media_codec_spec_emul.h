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

enum { DECODER, ENCODER };
enum { SOFTWARE, HARDWARE };

typedef struct _mc_codec_spec_t mc_codec_spec_t;
typedef struct _mc_codec_map_t mc_codec_map_t;
typedef struct _mc_codec_type_t mc_codec_type_t;

struct _mc_codec_spec_t
{
    mediacodec_codec_type_e codec_id;
    mediacodec_support_type_e codec_type;
    mediacodec_port_type_e port_type;
};

struct _mc_codec_type_t
{
    char *factory_name;
    char *mime;
    media_format_mimetype_e out_format;
};

struct _mc_codec_map_t
{
    mediacodec_codec_type_e id;
    bool hardware;
    mc_codec_type_t type;
};


#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_EMUL_H__ */
