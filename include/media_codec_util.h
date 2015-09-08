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

#ifndef __TIZEN_MEDIA_CODEC_UTIL_H__
#define __TIZEN_MEDIA_CODEC_UTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <tizen.h>
//#include <mm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 1
#define MEDIACODEC_FENTER();          LOGI("%s Enter",__FUNCTION__);
#define MEDIACODEC_FLEAVE();          LOGI("%s Exit",__FUNCTION__);
#else
#define MEDIACODEC_FENTER();          LOGD("%s Enter",__FUNCTION__);
#define MEDIACODEC_FLEAVE();          LOGD("%s Exit",__FUNCTION__);
#endif


typedef enum
{
    CODEC_RET_SUCCESS         =  0,
    CODEC_RET_FAIL            = -1,
    CODEC_RET_NOT_SUPPORT     = -2,
    CODEC_RET_INVALID_ARG     = -3,
    CODEC_RET_INVALID_CONFIG  = -4,
    CODEC_RET_CORRUPTED_BS    = -5,
    CODEC_RET_SMALL_IMG_BUF   = -6,
    CODEC_RET_HW_ERROR        = -7,
    CODEC_RET_NOT_AVAILABLE   = -8,
    CODEC_RET_NOT_EXPECTED    = -9,
    CODEC_RET_UNKNOWN_ERR     = -100,
} CodecRet;

typedef struct _mc_sem_t mc_sem_t;

struct _mc_sem_t
{
    GCond cond;
    GMutex mutex;
    int counter;
};

void *mc_aligned_malloc(int size, int alignment);
void mc_aligned_free(void *mem);

mc_sem_t *mc_sem_new();
void mc_sem_free(mc_sem_t *sem);
void mc_sem_down(mc_sem_t *sem);
void mc_sem_up(mc_sem_t *sem);
void mc_hex_dump(char *desc, void *addr, int len);

#define MC_FREEIF(x) \
if ( x ) \
    g_free( x ); \
x = NULL;




#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_UTIL_H__ */
