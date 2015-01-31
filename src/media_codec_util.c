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
