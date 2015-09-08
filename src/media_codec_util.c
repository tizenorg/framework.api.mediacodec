#include <media_codec_util.h>

void *mc_aligned_malloc(int size, int alignment)
{
    unsigned char *pMem;
    unsigned char *tmp;

    if((tmp = (unsigned char *)malloc(size + alignment)) != NULL)
    {
        pMem = (unsigned char*)((unsigned int)(tmp + alignment - 1) & (~(unsigned int)(alignment -1)));

        if(pMem == tmp)
            pMem += alignment;

        *(pMem - 1) = (unsigned int)(pMem - tmp);

        return ((void*) pMem);
    }
    return NULL;
}

void mc_aligned_free(void *mem)
{
    unsigned char *ptr;

    if(mem == NULL)
        return;

    ptr = mem;
    ptr -= *(ptr-1);

    free(ptr);
}

mc_sem_t *mc_sem_new()
{
    mc_sem_t *sem;
    sem = g_new(mc_sem_t, 1);
    //sem->cond = g_cond_new();
    g_cond_init(&sem->cond);
    //sem->mutex = g_mutex_new();
    g_mutex_init(&sem->mutex);
    sem->counter = 0;

    return sem;
}

void mc_sem_free(mc_sem_t *sem)
{
    //g_cond_free(sem->cond);
    g_cond_clear(&sem->cond);
    //g_mutex_free(sem->mutex);
    g_mutex_clear(&sem->mutex);
    g_free(sem);
}

void mc_sem_down(mc_sem_t *sem)
{
    g_mutex_lock(&sem->mutex);

    while(sem->counter == 0)
        g_cond_wait(&sem->cond, &sem->mutex);
    sem->counter--;

    g_mutex_unlock(&sem->mutex);
}

void mc_sem_up(mc_sem_t *sem)
{
    g_mutex_lock(&sem->mutex);

    sem->counter++;
    g_cond_signal(&sem->cond);

    g_mutex_unlock(&sem->mutex);

}

void mc_hex_dump(char *desc, void *addr, int len)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    if (desc != NULL)
        printf("%s:\n", desc);

    for (i = 0; i < len; i++)
    {

        if ((i % 16) == 0)
        {
            if (i != 0)
                printf("  %s\n", buff);

            printf("  %04x ", i);
        }

        printf(" %02x", pc[i]);

        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    while ((i % 16) != 0) {
        printf("   ");
        i++;
    }
    printf("  %s\n", buff);
}

