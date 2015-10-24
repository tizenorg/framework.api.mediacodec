#include <media_codec_queue.h>
#include <dlog.h>

async_queue_t *mc_async_queue_new(void)
{
    async_queue_t *async_queue;

    async_queue = g_slice_new0(async_queue_t);
    if (async_queue == NULL) {
        LOGE("async_queue initialization failed");
        return NULL;
    }

    g_cond_init(&async_queue->condition);
    g_mutex_init(&async_queue->mutex);
    async_queue->enabled = TRUE;

    return async_queue;
}

void mc_async_queue_free(async_queue_t *async_queue)
{
    g_cond_clear(&async_queue->condition);
    g_mutex_clear(&async_queue->mutex);

    g_list_free(async_queue->head);
    g_slice_free(async_queue_t, async_queue);
}

void mc_async_queue_push(async_queue_t *async_queue, gpointer data)
{
    g_mutex_lock(&async_queue->mutex);

    async_queue->tail = g_list_append(async_queue->tail, data);
    if (async_queue->tail->next)
        async_queue->tail = async_queue->tail->next;
    else
        async_queue->head = async_queue->tail;

    async_queue->length++;

    g_cond_signal(&async_queue->condition);
    /*LOGD("queue pushed : %p, %d, %p",queue, async_queue->length, data);*/

    g_mutex_unlock(&async_queue->mutex);
}

gpointer mc_async_queue_pop(async_queue_t *async_queue)
{
    gpointer data = NULL;

    g_mutex_lock(&async_queue->mutex);

    if (!async_queue->enabled) {
        /* g_warning ("not enabled!"); */
        goto leave;
    }

    if (!async_queue->head) {
        g_cond_wait(&async_queue->condition, &async_queue->mutex);
    }

    if (async_queue->head) {
        GList *node = async_queue->head;
        data = node->data;

        async_queue->head = node->next;

        if (async_queue->head)
            async_queue->head->prev = NULL;
        else
            async_queue->tail = NULL;
        async_queue->length--;
        /*LOGD("async queue poped : %p, %d, %p",queue, async_queue->length, data);*/
        g_list_free_1(node);
    }

leave:
    g_mutex_unlock(&async_queue->mutex);

    return data;
}

gpointer mc_async_queue_pop_forced(async_queue_t *async_queue)
{
    gpointer data = NULL;

    g_mutex_lock(&async_queue->mutex);

    if (async_queue->head) {
        GList *node = async_queue->head;
        data = node->data;

        async_queue->head = node->next;
        if (async_queue->head)
            async_queue->head->prev = NULL;
        else
            async_queue->tail = NULL;
        async_queue->length--;
        /*LOGD("async queue poped : %p, %d, %p",queue, async_queue->length, data);*/
        g_list_free_1(node);
    }

    g_mutex_unlock(&async_queue->mutex);

    return data;
}

void mc_async_queue_disable(async_queue_t *async_queue)
{
    g_mutex_lock(&async_queue->mutex);
    async_queue->enabled = FALSE;
    g_cond_broadcast(&async_queue->condition);
    g_mutex_unlock(&async_queue->mutex);
}

void mc_async_queue_enable(async_queue_t *async_queue)
{
    g_mutex_lock(&async_queue->mutex);
    async_queue->enabled = TRUE;
    g_mutex_unlock(&async_queue->mutex);
}

void mc_async_queue_flush(async_queue_t *async_queue)
{
    g_mutex_lock(&async_queue->mutex);

    g_list_free(async_queue->head);
    async_queue->head = async_queue->tail = NULL;
    async_queue->length = 0;

    g_mutex_unlock(&async_queue->mutex);
}

gboolean mc_async_queue_is_empty(async_queue_t *async_queue)
{
   return async_queue->head == NULL;
}

