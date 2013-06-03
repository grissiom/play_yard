/*
 * File      : app.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2013, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://openlab.rt-thread.com/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2013-06-03     Grissiom     first version
 */

#include <rtthread.h>

int ulRegTest1Counter;
int ulRegTest2Counter;

static rt_uint8_t test_thread_stack[512];
static struct rt_thread test_thread;
void vRegTestTask1(void*);

static rt_uint8_t test_thread_stack2[512];
static struct rt_thread test_thread2;
void vRegTestTask2(void*);

void rt_start_reg_test(int prio1, int prio2)
{
    rt_thread_init(&test_thread, "test1", vRegTestTask1, RT_NULL,
            test_thread_stack, sizeof(test_thread_stack), prio1, 20);
    rt_thread_startup(&test_thread);

    rt_thread_init(&test_thread2, "test2", vRegTestTask2, RT_NULL,
            test_thread_stack2, sizeof(test_thread_stack2), prio2, 20);
    rt_thread_startup(&test_thread2);
}

