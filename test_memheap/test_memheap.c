#include <rtthread.h>
#include <board.h>

static void _tst_realloc(void *p)
{
    int size = 0;
    int increment = 1;
    char *ptr = RT_NULL;
    unsigned int cnt = 0;
    while (1)
    {
        char *nptr;

        /*
         *rt_kprintf("t%d: realloc 0x%p cnt:%d size:%d\n",
         *        (int)p, ptr, cnt, size);
         */

        nptr = rt_realloc(ptr, size);
        RT_ASSERT(nptr + size < (char*)EFM32_EXT_HEAP_END);

        ptr = nptr;

        size += increment;
        if (size > (int)p)
        {
            size = (int)p/2;
            increment = -increment;
        }
        else if (size < 0)
        {
            size = 0;
            increment = -increment;
        }

        cnt++;
    }
}

void test_mem(void)
{
    rt_thread_t tid;

    tid = rt_thread_create("tm2k",
                           _tst_realloc,
                           (void*)2048,
                           1024,
                           21,
                           20);
    if (tid != RT_NULL)
        rt_thread_startup(tid);

    tid = rt_thread_create("tm4k",
                           _tst_realloc,
                           (void*)4129,
                           1024,
                           21,
                           17);
    if (tid != RT_NULL)
        rt_thread_startup(tid);
}
