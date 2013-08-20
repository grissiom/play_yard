#include <stdlib.h>

#include <rtthread.h>
#include <rtdevice.h>

static struct rt_semaphore _read_sem;

static rt_uint8_t _trans_buf[1024];

static rt_err_t _rx_ind(rt_device_t dev, rt_size_t size)
{
    /*rt_kprintf("pipe 0x%p rx\n", dev);*/
	return rt_sem_release(&_read_sem);
}

static void _read_portal(void *param)
{
    rt_err_t err;
    rt_device_t portal;

    RT_ASSERT(param);

    portal = rt_device_find((char*)param);

    RT_ASSERT(portal);

    err = rt_device_open(portal, RT_DEVICE_OFLAG_RDWR);
    if (err != RT_EOK)
    {
        rt_kprintf("open %s err: %d", param, err);
        return;
    }

    rt_sem_init(&_read_sem, "readsim", 0, 0);
    rt_device_set_rx_indicate(portal, _rx_ind);

    while (1)
    {
        unsigned int len, i, sum;
        unsigned char ch;

		if (rt_sem_take(&_read_sem, RT_WAITING_FOREVER) != RT_EOK)
            continue;
        rt_kprintf(" read thread get:");
        sum = 0;
        while (rt_device_read(portal, 0, &ch, 1) == 1)
        {
            /*rt_kprintf(" %02x", (unsigned int)ch);*/
            sum += ch;
        }
        rt_kprintf(" %d\n", sum);

        rt_kprintf(" read thread put:");
        sum = 0;
        len = rand() % sizeof(_trans_buf);
        for (i = 0; i < len; i++)
        {
            _trans_buf[i] = rand();
            sum += _trans_buf[i];
        }
        rt_kprintf(" %d\n", sum);

        rt_device_write(portal, 0, _trans_buf, len);
    }
}

static rt_err_t _wr_ind(rt_device_t dev, rt_size_t size)
{
    char ch;
    rt_kprintf("pipe 0x%p get: ", dev);
    while (rt_device_read(dev, 0, &ch, 1) == 1)
    {
        rt_kprintf("%02x", (unsigned int)ch);
    }
    rt_kprintf("\n");
	return RT_EOK;
}

static void _write_portal(void *param)
{
    rt_err_t err;
    rt_device_t portal;

    RT_ASSERT(param);

    portal = rt_device_find((char*)param);

    RT_ASSERT(portal);

    err = rt_device_open(portal, RT_DEVICE_OFLAG_RDWR);
    if (err != RT_EOK)
    {
        rt_kprintf("open %s err: %d", param, err);
        return;
    }
    /*rt_device_set_rx_indicate(portal, _wr_ind);*/

    while (1)
    {
        unsigned char ch;
        unsigned int len, i, sum;

        rt_kprintf("write thread put:");
        sum = 0;
        len = rand() % sizeof(_trans_buf);
        for (i = 0; i < len; i++)
        {
            _trans_buf[i] = rand();
            sum += _trans_buf[i];
        }
        rt_kprintf(" %d\n", sum);

        rt_device_write(portal, 0, _trans_buf, len);
        rt_thread_delay(100);

        rt_kprintf("write thread get:");
        sum = 0;
        while (rt_device_read(portal, 0, &ch, 1) == 1)
        {
            /*rt_kprintf(" %02x", (unsigned int)ch);*/
            sum += ch;
        }
        rt_kprintf(" %d\n", sum);
    }
}

void test_portal(void)
{
    rt_err_t err;
    rt_thread_t tid;

    err = rt_pipe_create("pipe0", RT_PIPE_FLAG_NONBLOCK_RDWR, 1024);
    if (err != RT_EOK)
    {
        rt_kprintf("create pipe0 failed: %d\n", err);
        return;
    }

    err = rt_pipe_create("pipe1", RT_PIPE_FLAG_NONBLOCK_RDWR, 1024);
    if (err != RT_EOK)
    {
        rt_kprintf("create pipe1 failed: %d\n", err);
        return;
    }

    err = rt_portal_create("port0", "pipe0", "pipe1");
    if (err != RT_EOK)
    {
        rt_kprintf("create port0 failed: %d\n", err);
        return;
    }

    err = rt_portal_create("port1", "pipe1", "pipe0");
    if (err != RT_EOK)
    {
        rt_kprintf("create port1 failed: %d\n", err);
        return;
    }

    tid = rt_thread_create("rdportal",
                           _read_portal, "port0",
                           1024, RT_THREAD_PRIORITY_MAX / 3, 20);
    if (tid != RT_NULL)
        rt_thread_startup(tid);

    tid = rt_thread_create("wrportal",
                           _write_portal, "port1",
                           1024, RT_THREAD_PRIORITY_MAX / 3 - 1, 20);
    if (tid != RT_NULL)
        rt_thread_startup(tid);
}
