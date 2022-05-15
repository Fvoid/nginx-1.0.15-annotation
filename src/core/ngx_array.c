
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>

// 初始化array
// 在内存池中先分配ngx_array的结构空间
// 然后分配ngx_array的数据空间
// 由于nginx是多线程执行，两次分配的空间可能会是不连贯的
ngx_array_t *
ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size)
{
    ngx_array_t *a;

    a = ngx_palloc(p, sizeof(ngx_array_t));
    if (a == NULL) {
        return NULL;
    }

    a->elts = ngx_palloc(p, n * size);
    if (a->elts == NULL) {
        return NULL;
    }

    a->nelts = 0;
    a->size = size;
    a->nalloc = n;
    a->pool = p;

    return a;
}

// 销毁ngx_array
// 由于ngx_array的结构数据和元素数据可能不连在一起
// 若array的元素起始地址 + 元素长度 == pool的最后使用的地址，则将pool的结束地址设为元素起始地址
// 以上操作则将元素数据销毁，而当array数据后面有别的数据时，则不会执行此操作，避免了对别的数据无意地也销毁了
// 若array的结构数据结束地址 == pool的最后使用的地址
void
ngx_array_destroy(ngx_array_t *a)
{
    ngx_pool_t  *p;

    p = a->pool;

    if ((u_char *) a->elts + a->size * a->nalloc == p->d.last) {
        p->d.last -= a->size * a->nalloc;
    }

    if ((u_char *) a + sizeof(ngx_array_t) == p->d.last) {
        p->d.last = (u_char *) a;
    }
}

// 向ngx_array添加一个元素
// 有两种扩容方式
// 当a的元素个数等于a的可分配元素个数时
// 第一种：若当前array最尾的数据地址 == pool最后使用的地址 并且 pool最后使用的地址 + 一个元素的大小 小于 pool的最尾地址
//         则增加pool的最尾地址，并增加一给array的可分配元素个数
// 第二种：在pool上扩容至当前size 的两倍，并将当前内存数据copy到新的地址
//         并更新array数据结构的信息，修改新内存的数据起始地址 和 可分配元素个数
void *
ngx_array_push(ngx_array_t *a)
{
    void        *elt, *new;
    size_t       size;
    ngx_pool_t  *p;

    if (a->nelts == a->nalloc) {

        /* the array is full */

        size = a->size * a->nalloc;

        p = a->pool;

        if ((u_char *) a->elts + size == p->d.last
            && p->d.last + a->size <= p->d.end)
        {
            /*
             * the array allocation is the last in the pool
             * and there is space for new allocation
             */

            p->d.last += a->size;
            a->nalloc++;

        } else {
            /* allocate a new array */

            new = ngx_palloc(p, 2 * size);
            if (new == NULL) {
                return NULL;
            }

            ngx_memcpy(new, a->elts, size);
            a->elts = new;
            a->nalloc *= 2;
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts++;

    return elt;
}

// 增加n个元素到array
// 扩容机制与以上相似
void *
ngx_array_push_n(ngx_array_t *a, ngx_uint_t n)
{
    void        *elt, *new;
    size_t       size;
    ngx_uint_t   nalloc;
    ngx_pool_t  *p;

    size = n * a->size;

    if (a->nelts + n > a->nalloc) {

        /* the array is full */

        p = a->pool;

        if ((u_char *) a->elts + a->size * a->nalloc == p->d.last
            && p->d.last + size <= p->d.end)
        {
            /*
             * the array allocation is the last in the pool
             * and there is space for new allocation
             */

            p->d.last += size;
            a->nalloc += n;

        } else {
            /* allocate a new array */

            nalloc = 2 * ((n >= a->nalloc) ? n : a->nalloc);

            new = ngx_palloc(p, nalloc * a->size);
            if (new == NULL) {
                return NULL;
            }

            ngx_memcpy(new, a->elts, a->nelts * a->size);
            a->elts = new;
            a->nalloc = nalloc;
        }
    }

    elt = (u_char *) a->elts + a->size * a->nelts;
    a->nelts += n;

    return elt;
}
