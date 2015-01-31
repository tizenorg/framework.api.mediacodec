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

#ifndef __TIZEN_MEDIA_CODEC_QUEUE_H__
#define __TIZEN_MEDIA_CODEC_QUEUE_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _async_queue {
    GMutex *mutex;
    GCond *condition;
    GList *head;
    GList *tail;
    guint length;
    gboolean enabled;
} async_queue_t;

typedef struct _mc_aqueue {
    GThread *thread;
    gint running;
    async_queue_t *input;
    async_queue_t *output;
} mc_aqueue_t;

async_queue_t *mc_async_queue_new();

void mc_async_queue_free(async_queue_t *async_queue);

void mc_async_queue_push(async_queue_t *async_queue, gpointer data);

gpointer mc_async_queue_pop(async_queue_t *async_queue);

void mc_async_queue_disable (async_queue_t *async_queue);

void mc_async_queue_enable (async_queue_t *async_queue);

void mc_async_queue_flush(async_queue_t *async_queue);

#ifdef __cplusplus
}
#endif

#endif /* __TIZEN_MEDIA_CODEC_QUEUE_H__ */
