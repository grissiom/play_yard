/*
 * A priority queue that has FIFO behavior among the items with same priority.
 * 
 * O(N) in space and O(log(N)) in both push and pop.
 */
#include <rthw.h>
#include <rtthread.h>

struct rt_hpqueue {
    rt_uint32_t heap_size;
    rt_uint32_t used_size;

    struct rt_hpqueue_prio_info *pinfo;
    struct rt_hpqueue_item *data;

    rt_list_t suspended_push_list;
    rt_list_t suspended_pop_list;

    rt_uint16_t max_prio;
};

rt_err_t rt_hpq_init(struct rt_hpqueue *hpq,
                     rt_uint16_t max_prio,
                     void *data,
                     rt_size_t heap_size);
rt_err_t rt_hpq_push(struct rt_hpqueue *hpq,
                     rt_uint16_t prio,
                     void *data,
                     rt_size_t size,
                     rt_tick_t timeout);
rt_err_t rt_hpq_pop(struct rt_hpqueue *hpq,
                    void **datap,
                    rt_size_t *sizep,
                    rt_tick_t timeout);
