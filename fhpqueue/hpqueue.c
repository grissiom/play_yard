#include <rthw.h>
#include <rtthread.h>

#include "hpqueue.h"

struct rt_hpqueue_item {
    rt_uint16_t seq;
    rt_uint16_t prio;
    void        *data;
    rt_size_t   size;
};

struct rt_hpqueue_prio_info {
    rt_uint16_t seq;
    rt_uint16_t base;
};

rt_inline int _isgreater(struct rt_hpqueue *hpq,
                         rt_size_t i, rt_size_t j)
{
    rt_uint16_t si, sj, p;

    if (hpq->data[i].prio > hpq->data[j].prio)
        return 1;
    if (hpq->data[i].prio < hpq->data[j].prio)
        return 0;

    p = hpq->data[i].prio;
    si = hpq->data[i].seq + hpq->pinfo[p].base;
    sj = hpq->data[j].seq + hpq->pinfo[p].base;
    if (si > sj)
        return 1;
    else if (si < sj)
        return 0;
    else
        RT_ASSERT(0);
    return -1;
}

rt_inline void _exch(struct rt_hpqueue *hpq,
                     rt_size_t i, rt_size_t j)
{
    struct rt_hpqueue_item item;

    item = hpq->data[i];
    hpq->data[i] = hpq->data[j];
    hpq->data[j] = item;
}

rt_inline rt_size_t _left_child(rt_size_t parent)
{
    return (parent + 1) * 2 - 1;
}

rt_inline rt_size_t _parent(rt_size_t child)
{
    return (child - 1) / 2;
}

static void _swim(struct rt_hpqueue *hpq,
                  rt_size_t idx)
{
    while (idx > 0 && _isgreater(hpq, idx, _parent(idx)))
    {
        _exch(hpq, _parent(idx), idx);
        idx = _parent(idx);
    }
}

static void _sink(struct rt_hpqueue *hpq,
                  rt_size_t pnt)
{
    rt_size_t chld = _left_child(pnt);

    for (chld = _left_child(pnt);
         chld < hpq->used_size;
         chld = _left_child(pnt))
    {
        /* get the biggest child */
        if (chld < hpq->used_size - 1 &&
            _isgreater(hpq, chld+1, chld))
            chld = chld + 1;
        if (_isgreater(hpq, pnt, chld))
            break;

        _exch(hpq, chld, pnt);
        pnt = chld;
    }
}

static void _do_push(struct rt_hpqueue *hpq,
                     rt_uint16_t prio,
                     void *data,
                     rt_size_t size)
{
    rt_uint16_t seq = hpq->pinfo[prio].seq;

    hpq->pinfo[prio].seq--;

    hpq->data[hpq->used_size].prio = prio;
    hpq->data[hpq->used_size].seq  = seq;
    hpq->data[hpq->used_size].data = data;
    hpq->data[hpq->used_size].size = size;
    _swim(hpq, hpq->used_size);
    hpq->used_size++;
}

static void _do_pop(struct rt_hpqueue *hpq,
                    void **datap,
                    rt_size_t *sizep)
{
    *datap = hpq->data[0].data;
    *sizep = hpq->data[0].size;

    hpq->pinfo[hpq->data[0].prio].base++;

    hpq->used_size--;
    hpq->data[0] = hpq->data[hpq->used_size];
    _sink(hpq, 0);

    return;
}

rt_err_t rt_hpq_init(struct rt_hpqueue *hpq,
                     rt_uint16_t max_prio,
                     void *data,
                     rt_size_t data_size)
{
    int tmp, pinfosz;

    RT_ASSERT(hpq);
    RT_ASSERT(data);
    RT_ASSERT(data_size);

    rt_list_init(&(hpq->suspended_push_list));
    rt_list_init(&(hpq->suspended_pop_list));

    pinfosz = (max_prio + 1) * sizeof(struct rt_hpqueue_prio_info);
    tmp = data_size - pinfosz;
    if (tmp < 0)
        return -RT_ERROR;

    hpq->heap_size = (data_size - pinfosz) / sizeof(struct rt_hpqueue_item);
    if (hpq->heap_size == 0)
        return -RT_ERROR;
    if (hpq->heap_size < max_prio + 1)
        rt_kprintf("hpq(%p): warning: item number is smaller than priority number\n",
                   hpq);

    hpq->used_size = 0;
    hpq->max_prio  = max_prio;
    hpq->pinfo     = (struct rt_hpqueue_prio_info*)data;
    hpq->data      = (struct rt_hpqueue_item*)
                         (((struct rt_hpqueue_prio_info*)data)+max_prio+1);
    for (tmp = 0; tmp <= max_prio; tmp++)
    {
        hpq->pinfo[tmp].seq  = -1;
        hpq->pinfo[tmp].base =  0;
    }
}

rt_err_t rt_hpq_push(struct rt_hpqueue *hpq,
                     rt_uint16_t prio,
                     void *data,
                     rt_size_t size,
                     rt_tick_t timeout)
{
    rt_ubase_t  level;

    RT_ASSERT(hpq);

    if (prio > hpq->max_prio)
        return -RT_ERROR;

    level = rt_hw_interrupt_disable();
    while (hpq->used_size == hpq->heap_size)
    {
        rt_thread_t thread;
        if (timeout == 0)
        {
            rt_hw_interrupt_enable(level);
            return -RT_ETIMEOUT;
        }

        RT_DEBUG_NOT_IN_INTERRUPT;

        thread = rt_thread_self();
        thread->error = RT_EOK;
        rt_thread_suspend(thread);
        rt_list_insert_before(&(hpq->suspended_push_list),
                              &(thread->tlist));
        /* start timeout timer */
        if (timeout > 0)
        {
            /* reset the timeout of thread timer and start it */
            rt_timer_control(&(thread->thread_timer),
                             RT_TIMER_CTRL_SET_TIME,
                             &timeout);
            rt_timer_start(&(thread->thread_timer));
        }

        /* enable interrupt */
        rt_hw_interrupt_enable(level);

        /* do schedule */
        rt_schedule();

        /* thread is waked up */
        if (thread->error != RT_EOK)
            return thread->error;
        level = rt_hw_interrupt_disable();
    }

    _do_push(hpq, prio, data, size);

    if (!rt_list_isempty(&(hpq->suspended_pop_list)))
    {
        rt_thread_t thread;

        /* get thread entry */
        thread = rt_list_entry(hpq->suspended_pop_list.next,
                               struct rt_thread,
                               tlist);
        /* resume it */
        rt_thread_resume(thread);
        rt_hw_interrupt_enable(level);

        /* perform a schedule */
        rt_schedule();

        return RT_EOK;
    }

    return RT_EOK;
}

rt_err_t rt_hpq_pop(struct rt_hpqueue *hpq,
                    void **datap,
                    rt_size_t *sizep,
                    rt_tick_t timeout)
{
    rt_ubase_t level;

    RT_ASSERT(hpq);
    RT_ASSERT(datap);
    RT_ASSERT(sizep);

    level = rt_hw_interrupt_disable();
    while (hpq->used_size == 0)
    {
        rt_thread_t thread;

        if (timeout == 0)
        {
            rt_hw_interrupt_enable(level);
            return -RT_ETIMEOUT;
        }

        RT_DEBUG_NOT_IN_INTERRUPT;

        thread = rt_thread_self();
        thread->error = RT_EOK;
        rt_thread_suspend(thread);

        rt_list_insert_before(&(hpq->suspended_pop_list), &(thread->tlist));

        if (timeout > 0)
        {
            rt_timer_control(&(thread->thread_timer),
                             RT_TIMER_CTRL_SET_TIME,
                             &timeout);
            rt_timer_start(&(thread->thread_timer));
        }

        rt_hw_interrupt_enable(level);

        rt_schedule();

        /* thread is waked up */
        if (thread->error != RT_EOK)
            return thread->error;
        level = rt_hw_interrupt_disable();
    }

    _do_pop(hpq, datap, sizep);

    if (!rt_list_isempty(&(hpq->suspended_push_list)))
    {
        rt_thread_t thread;

        /* get thread entry */
        thread = rt_list_entry(hpq->suspended_push_list.next,
                               struct rt_thread,
                               tlist);
        /* resume it */
        rt_thread_resume(thread);
        rt_hw_interrupt_enable(level);

        /* perform a schedule */
        rt_schedule();

        return RT_EOK;
    }

    return RT_EOK;
}

void rt_hpq_dump(struct rt_hpqueue *hpq)
{
    rt_size_t base = 0;
    int level = 0;

    for (level = 0; ; level++)
    {
        int idx;
        for (idx = 0; idx < (1 << level); idx++)
        {
            if (base + idx >= hpq->used_size)
                break;
            rt_kprintf("%d(%p, %d), ",
                       hpq->data[base + idx].prio,
                       hpq->data[base + idx].data,
                       hpq->data[base + idx].size);
        }
        base += idx;
        if (base >= hpq->used_size)
            break;
        rt_kprintf("\n");
    }
    rt_kprintf("\nseq base: ");
    for (level = 0; level <= hpq->max_prio; level++)
    {
        rt_kprintf("%d, ", hpq->pinfo[level].base);
    }
    rt_kprintf("\n");
}

