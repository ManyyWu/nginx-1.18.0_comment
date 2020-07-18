
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */

#include <ngx_config.h>
#include <ngx_core.h>

/*
由于指针是4的倍数,那么后两位一定为0,此时我们可以利用指针的后两位做标记,充分利用空间.
在nginx的slab中,我们使用ngx_slab_page_s结构体中的指针pre的后两位做标记,用于指示该page页面的slot块数与ngx_slab_exact_size的关系.
当page划分的slot块小于32时候,pre的后两位为NGX_SLAB_SMALL.
当page划分的slot块等于32时候,pre的后两位为NGX_SLAB_EXACT
当page划分的slot大于32块时候,pre的后两位为NGX_SLAB_BIG
当page页面不划分slot时候,即将整个页面分配给用户,pre的后两位为NGX_SLAB_PAGE
*/
#define NGX_SLAB_PAGE_MASK   3
#define NGX_SLAB_PAGE        0
#define NGX_SLAB_BIG         1
#define NGX_SLAB_EXACT       2
#define NGX_SLAB_SMALL       3

#if (NGX_PTR_SIZE == 4)

#define NGX_SLAB_PAGE_FREE   0
//标记这是连续分配多个page，并且我不是首page，例如一次分配3个page,分配的page为[1-3]，则page[1].slab=3  page[2].slab=page[3].slab=NGX_SLAB_PAGE_BUSY记录
#define NGX_SLAB_PAGE_BUSY   0xffffffff
#define NGX_SLAB_PAGE_START  0x80000000

#define NGX_SLAB_SHIFT_MASK  0x0000000f
#define NGX_SLAB_MAP_MASK    0xffff0000
#define NGX_SLAB_MAP_SHIFT   16

#define NGX_SLAB_BUSY        0xffffffff

#else /* (NGX_PTR_SIZE == 8) */

#define NGX_SLAB_PAGE_FREE   0
#define NGX_SLAB_PAGE_BUSY   0xffffffffffffffff
#define NGX_SLAB_PAGE_START  0x8000000000000000

#define NGX_SLAB_SHIFT_MASK  0x000000000000000f
#define NGX_SLAB_MAP_MASK    0xffffffff00000000
#define NGX_SLAB_MAP_SHIFT   32

#define NGX_SLAB_BUSY        0xffffffffffffffff

#endif


#define ngx_slab_slots(pool)                                                  \
    (ngx_slab_page_t *) ((u_char *) (pool) + sizeof(ngx_slab_pool_t))

#define ngx_slab_page_type(page)   ((page)->prev & NGX_SLAB_PAGE_MASK)

#define ngx_slab_page_prev(page)                                              \
    (ngx_slab_page_t *) ((page)->prev & ~NGX_SLAB_PAGE_MASK)

#define ngx_slab_page_addr(pool, page)                                        \
    ((((page) - (pool)->pages) << ngx_pagesize_shift)                         \
     + (uintptr_t) (pool)->start)


#if (NGX_DEBUG_MALLOC)

#define ngx_slab_junk(p, size)     ngx_memset(p, 0xA5, size)

#elif (NGX_HAVE_DEBUG_MALLOC)

#define ngx_slab_junk(p, size)                                                \
    if (ngx_debug_malloc)          ngx_memset(p, 0xA5, size)

#else

#define ngx_slab_junk(p, size)

#endif

static ngx_slab_page_t *ngx_slab_alloc_pages(ngx_slab_pool_t *pool,
    ngx_uint_t pages);
static void ngx_slab_free_pages(ngx_slab_pool_t *pool, ngx_slab_page_t *page,
    ngx_uint_t pages);
static void ngx_slab_error(ngx_slab_pool_t *pool, ngx_uint_t level,
    char *text);

//slab页面的大小,32位Linux中为4k,  
static ngx_uint_t  ngx_slab_max_size;//设置ngx_slab_max_size = 2048B。如果一个页要存放多个obj，则obj size要小于这个数值 
/*
由于指针是4的倍数,那么后两位一定为0,此时我们可以利用指针的后两位做标记,充分利用空间.
在nginx的slab中,我们使用ngx_slab_page_s结构体中的指针pre的后两位做标记,用于指示该page页面的slot块数与ngx_slab_exact_size的关系.
当page划分的slot块小于32时候,pre的后两位为NGX_SLAB_SMALL.
当page划分的slot块等于32时候,pre的后两位为NGX_SLAB_EXACT
当page划分的slot大于32块时候,pre的后两位为NGX_SLAB_BIG
当page页面不划分slot时候,即将整个页面分配给用户,pre的后两位为NGX_SLAB_PAGE
*/ //划分32个slot块,每个slot的大小就是ngx_slab_exact_size  
static ngx_uint_t  ngx_slab_exact_size;//设置ngx_slab_exact_size = 128B。分界是否要在缓存区分配额外空间给bitmap  
static ngx_uint_t  ngx_slab_exact_shift;//ngx_slab_exact_shift = 7，即128的位表示 //每个slot块大小的位移是ngx_slab_exact_shift  

/*
注意，在ngx_slab_pool_t里面有两种类型的slab page，虽然都是ngx_slab_page_t定义的结构，但是功能不尽相同。一种是slots，用来表示存
放较小obj的内存块(如果页大小是 4096B，则是<2048B的obj，即小于1/2页)，另一种来表示所要分配的空间在缓存区的位置。Nginx把缓存obj分
成大的 (>=2048B)和小的(<2048B)。每次给大的obj分配一整个页，而把多个小obj存放在一个页中间，用bitmap等方法来表示 其占用情况。而小
的obj又分为3种：小于128B，等于128B，大于128B且小于2048B。其中小于128B的obj需要在实际缓冲区额外分配 bitmap空间来表示内存使用情况
(因为slab成员只有4个byte即32bit，一个缓存页4KB可以存放的obj超过32个，所以不能用slab 来表示)，这样会造成一定的空间损失。等于或大
于128B的obj因为可以用一个32bit的整形来表示其状态，所以就可以直接用slab成员。每次分配 的空间是2^n，最小是8byte，8，16，32，64，
128，256，512，1024，2048。小于2^i且大于2^(i-1)的obj会被分 配一个2^i的空间，比如56byte的obj就会分配一个64byte的空间。
*/ //http://adam281412.blog.163.com/blog/static/33700067201111283235134/
/*
共享内存的其实地址开始处数据:ngx_slab_pool_t + 9 * sizeof(ngx_slab_page_t)(slots_m[]) + pages * sizeof(ngx_slab_page_t)(pages_m[]) +pages*ngx_pagesize(这是实际的数据部分，
每个ngx_pagesize都由前面的一个ngx_slab_page_t进行管理，并且每个ngx_pagesize最前端第一个obj存放的是一个或者多个int类型bitmap，用于管理每块分配出去的内存)
*/

    //图形化理解参考:http://blog.csdn.net/u013009575/article/details/17743261
void
ngx_slab_sizes_init(void) //pool指向的是整个共享内存空间的起始地址   slab结构是配合共享内存使用的 可以以limit req模块为例，参考ngx_http_limit_req_module
{
    ngx_uint_t  n;

    /*
     //假设每个page是4KB  
    //设置ngx_slab_max_size = 2048B。如果一个页要存放多个obj，则obj size要小于这个数值  
    //设置ngx_slab_exact_size = 128B。分界是否要在缓存区分配额外空间给bitmap  
    //ngx_slab_exact_shift = 7，即128的位表示  
     */
    /* STUB */
    ngx_slab_max_size = ngx_pagesize / 2;
    ngx_slab_exact_size = ngx_pagesize / (8 * sizeof(uintptr_t));
    for (n = ngx_slab_exact_size; n >>= 1; ngx_slab_exact_shift++) {
        /* void */
    }
}


void
ngx_slab_init(ngx_slab_pool_t *pool)
{
    u_char           *p;
    size_t            size;
    ngx_int_t         m;
    ngx_uint_t        i, n, pages;
    ngx_slab_page_t  *slots, *page;

    pool->min_size = (size_t) 1 << pool->min_shift; //最小分配的空间是8byte  

    slots = ngx_slab_slots(pool); //共享内存前面的sizeof(ngx_slab_pool_t)是给slab poll的

    p = (u_char *) slots;
    size = pool->end - p; //size是总的共享内存 - sizeof(ngx_slab_pool_t)字节后的长度

    ngx_slab_junk(p, size);

    n = ngx_pagesize_shift - pool->min_shift; //12-3=9

    /*
    这些slab page是给大小为8，16，32，64，128，256，512，1024，2048byte的内存块 这些slab page的位置是在pool->pages的前面初始化  
    共享内存的其实地址开始处数据:ngx_slab_pool_t + 9 * sizeof(ngx_slab_page_t)(slots_m[]) + pages * sizeof(ngx_slab_page_t)(pages_m[]) +pages*ngx_pagesize(这是实际的数据部分，
    每个ngx_pagesize都由前面的一个ngx_slab_page_t进行管理，并且每个ngx_pagesize最前端第一个obj存放的是一个或者多个int类型bitmap，用于管理每块分配出去的内存)
    */
    for (i = 0; i < n; i++) { //这9个slots[]由9 * sizeof(ngx_slab_page_t)指向
        /* only "next" is used in list head */
        slots[i].slab = 0;
        slots[i].next = &slots[i];
        slots[i].prev = 0;
    }

    p += n * sizeof(ngx_slab_page_t); //跳过上面那些slab page  

    pool->stats = (ngx_slab_stat_t *) p;
    ngx_memzero(pool->stats, n * sizeof(ngx_slab_stat_t));

    p += n * sizeof(ngx_slab_stat_t);

    size -= n * (sizeof(ngx_slab_page_t) + sizeof(ngx_slab_stat_t));
	
    //**计算这个空间总共可以分配的缓存页(4KB)的数量，每个页的overhead是一个slab page的大小  
    //**这儿的overhead还不包括之后给<128B物体分配的bitmap的损耗  
    //这里 + sizeof(ngx_slab_page_t)的原因是每个ngx_pagesize都有对应的ngx_slab_page_t进行管理
    pages = (ngx_uint_t) (size / (ngx_pagesize + sizeof(ngx_slab_page_t))); //这里的size是不是应该把头部n * sizeof(ngx_slab_page_t)减去后在做计算更加准确?
    //把每个缓存页最前面的sizeof(ngx_slab_page_t)字节对应的slab page归0  

    pool->pages = (ngx_slab_page_t *) p;
    ngx_memzero(pool->pages, pages * sizeof(ngx_slab_page_t));

    page = pool->pages;
    //初始化free，free.next是下次分配页时候的入口  
    /* only "next" is used in list head */
    pool->free.slab = 0;
    pool->free.next = page;
    pool->free.prev = 0;

    //更新第一个slab page的状态，这儿slab成员记录了整个缓存区的页数目  
    page->slab = pages;  //第一个pages->slab指定了共享内存中除去头部外剩余页的个数
    page->next = &pool->free;
    page->prev = (uintptr_t) &pool->free;

    //实际缓存区(页)的开头，对齐   //因为对齐的原因,使得m_page数组和数据区域之间可能有些内存无法使用,  
    pool->start = ngx_align_ptr(p + pages * sizeof(ngx_slab_page_t),
                                ngx_pagesize);

    //根据实际缓存区的开始和结尾再次更新内存页的数目  
    //由于内存对齐操作(pool->start处内存对齐),可能导致pages减少,  
    //所以要调整.m为调整后page页面的减小量.  
    //其实下面几行代码就相当于:  
    // pages =(pool->end - pool->start) / ngx_pagesize  
    //pool->pages->slab = pages;  
    m = pages - (pool->end - pool->start) / ngx_pagesize;
    if (m > 0) {
        pages -= m;
        page->slab = pages;
    }

    //跳过pages * sizeof(ngx_slab_page_t)，也就是指向实际的数据页pages*ngx_pagesize
    pool->last = pool->pages + pages;
    pool->pfree = pages;

    pool->log_nomem = 1;
    pool->log_ctx = &pool->zero;
    pool->zero = '\0';
}

//由于是共享内存，所以在进程间需要用锁来保持同步
void *
ngx_slab_alloc(ngx_slab_pool_t *pool, size_t size)
{
    void  *p;

    ngx_shmtx_lock(&pool->mutex);

    p = ngx_slab_alloc_locked(pool, size);

    ngx_shmtx_unlock(&pool->mutex);

    return p;
}

/*
对于给定size,从slab_pool中分配内存.
1.如果size大于等于一页,那么从m_page中查找,如果有则直接返回,否则失败.
2.如果size小于一页,如果链表中有空余slot块.
     (1).如果size大于ngx_slab_exact_size,
a.设置slab的高16位(32位系统)存放solt对应的map,并且该对应为map的地位对应page中高位的slot块.例如1110对应为第1块slot是可用的,2-4块不可用.slab的低16为存储slot块大小的位移.
b.设置m_page元素的pre指针为NGX_SLAB_BIG.
c.如果page的全部slot都被使用了,那么将此页面从m_slot数组元素的链表中移除.
   (2).如果size等于ngx_slab_exact_size
a.设置slab存储slot的map,同样slab中的低位对应高位置的slot.
b.设置m_page元素的pre指针为NGX_SLAB_EXACT.
c.如果page的全部slot都被使用了,那么将此页面从m_slot数组元素的链表中移除.
   (3).如果size小于ngx_slab_exact_size
a.用page中的前几个slot存放slot的map,同样低位对应高位.
b.设置m_page元素的pre指针为NGX_SLAB_SMALL.
b.如果page的全部slot都被使用了,那么将此页面从m_slot数组元素的链表中移除.
 3.如果链表中没有空余的slot块,则在free链表中找到一个空闲的页面分配给m_slot数组元素中的链表.
   (1).如果size大于ngx_slab_exact_size,
a.设置slab的高16位(32位系统)存放solt对应的map,并且该对应为map的地位对应page中高位的slot块.slab的低16为存储slot块大小的位移.
b.设置m_page元素的pre指针为NGX_SLAB_BIG.
c.将分配的页面链入m_slot数组元素的链表中.
   (2).如果size等于ngx_slab_exact_size
a.设置slab存储slot的map,同样slab中的低位对应高位置的slot.
b.设置m_page元素的pre指针为NGX_SLAB_EXACT.
c.将分配的页面链入m_slot数组元素的链表中.
   (3).如果size小于ngx_slab_exact_size
a.用page中的前几个slot存放slot的map,同样低位对应高位.
b.设置m_page元素的pre指针为NGX_SLAB_SMALL.
c.将分配的页面链入m_slot数组元素的链表中.
4.成功则返回分配的内存块,即指针p,否则失败,返回null.

*/
//图形化理解参考:http://blog.csdn.net/u013009575/article/details/17743261
//返回的值是所要分配的空间在内存缓存区的位置  

/*
在一个page页中获取小与2048的obj块的时候，都会设置page->next = &slots[slot]; page->prev = &slots[slot]，slots[slot].next = page;也就是作为obj块的页
page[]都会和对应的slots[]关联，如果是分配大于2048的空间，则会分配整个页，其page[]和slots就没有关系
当该page页用完后，则会重新把page[]的next和prev置为NULL，同时把对应的slot[]的next和prev指向slot[]本身
当page用完后释放其中一个obj后，有恢复为page->next = &slots[slot]; page->prev = &slots[slot]，slots[slot].next = page;
*/
void *
ngx_slab_alloc_locked(ngx_slab_pool_t *pool, size_t size)
{ //这儿假设page_size是4KB  
    size_t            s;
    uintptr_t         p, m, mask, *bitmap;
    ngx_uint_t        i, n, slot, shift, map;
    ngx_slab_page_t  *page, *prev, *slots;

    if (size > ngx_slab_max_size) { //如果是large obj, size >= 2048B  

        ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0,
                       "slab alloc: %uz", size);

        //分配1个或多个内存页  
        page = ngx_slab_alloc_pages(pool, (size >> ngx_pagesize_shift)
                                          + ((size % ngx_pagesize) ? 1 : 0)); //例如size刚好是4K,则page=1,如果是4K+1，则page=2
        if (page) {
            //获得page向对于page[0]的偏移量由于m_page和page数组是相互对应的,即m_page[0]管理page[0]页面,m_page[1]管理page[1]页面.  
            //所以获得page相对于m_page[0]的偏移量就可以根据start得到相应页面的偏移量.  
            p = ngx_slab_page_addr(pool, page);

        } else {
            p = 0;
        }

        goto done;
    }

    //较小的obj, size < 2048B根据需要分配的size来确定在slots的位置，每个slot存放一种大小的obj的集合，如slots[0]表示8byte的空间，
    //slots[3]表示64byte的空间如果obj过小(<1B)，slot的位置是1B空间的位置，即最小分配1B  
    if (size > pool->min_size) { //计算使用哪个slots[]，也就是需要分配的空间是多少  例如size=9,则会使用slot[1]，也就是16字节
        shift = 1;
        for (s = size - 1; s >>= 1; shift++) { /* void */ }
        slot = shift - pool->min_shift;

    } else {
        shift = pool->min_shift;
        slot = 0;
    }

    pool->stats[slot].reqs++;
	
    //ngx_slab_pool_t + 9 * sizeof(ngx_slab_page_t) + pages * sizeof(ngx_slab_page_t) +pages*ngx_pagesize(这是实际的数据部分)
    ngx_log_debug2(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0,
                   "slab alloc: %uz slot: %ui", size, slot);

    //指向9 * sizeof(ngx_slab_page_t) ，也就是slots[0-8]数组 = 8 - 2048
    slots = ngx_slab_slots(pool);
    //在ngx_slab_init中slots[]->next默认是指向自己的
    page = slots[slot].next;//根据m_slot[slot]获得相应的m_page元素,然后找到相应页面.  

    if (page->next != page) { //如果之前已经有此类大小obj且那个已经分配的内存缓存页还未满  9 个ngx_slab_page_t都还没有用完

        if (shift < ngx_slab_exact_shift) { //小obj，size < 128B，更新内存缓存页中的bitmap，并返回待分配的空间在缓存的位置  

            bitmap = (uintptr_t *) ngx_slab_page_addr(pool, page);

                /*  
               例如要分配的size为54字节，则在前面计算出的shift对应的字节数应该是64字节，由于一个页面全是64字节obj大小，所以一共有64
               个64字节的obj，64个obj至少需要64位来表示每个obj是否已使用，因此至少需要64位(也就是8字节，2个int),所以至少要暂用一个64
               字节obj来存储该bitmap信息，第一个64字节obj实际上只用了8字节，其他56字节未用
               */
                //计算需要多少个字节来存储bitmap  
            map = (ngx_pagesize >> shift) / (8 * sizeof(uintptr_t));

            for (n = 0; n < map; n++) {

                    if (bitmap[n] != NGX_SLAB_BUSY) {//如果page的obj块空闲,也就是bitmap指向的32个obj是否都已经被分配出去了  
                        //如果整个page页的所有slab已经用完，则会在后面的ngx_slab_alloc_pages从新获取空间并划分slab，然后分配
                        for (m = 1, i = 0; m; m <<= 1, i++) {//如果obj块被占用,继续查找  从这32个obj中找出第一个未被分配出去的obj
                        if (bitmap[n] & m) {
                            continue;
                        }

                            //找到了，该bitmap对应的第n个(注意是位移操作后的m)未使用，使用他，同时置位该位，表示该bitmp[n]已经不能再被分配，因为已经本次分配出去了
                            bitmap[n] |= m;

                        i = (n * 8 * sizeof(uintptr_t) + i) << shift;  //该bit所处的整个bitmap中的第几位(例如需要3个bitmap表示所有的slab块，则现在是第三个bitmap的第一位，则i=32+32+1-1,bit从0开始)

                        p = (uintptr_t) bitmap + i;

                        pool->stats[slot].used++;

                        if (bitmap[n] == NGX_SLAB_BUSY) {  //如果该32位图在这次取到最后第31位(0-31)的时候，前面的bitmap[n] |= m后;使其刚好NGX_SLAB_BUSY，也就是位图填满
                            for (n = n + 1; n < map; n++) {
                                if (bitmap[n] != NGX_SLAB_BUSY) { //如果该bitmap后面的几个bitmap还没有用完，则直接返回该bitmap地址
                                    goto done;
                                }
                            }
                            //& ~NGX_SLAB_PAGE_MASK这个的原因是需要恢复原来的地址，因为低两位在第一次获取空间的时候，做了特殊意义处理
                            prev = ngx_slab_page_prev(page);  //也就是该obj对应的slot_m[]

                            //pages_m[i]和slot_m[i]取消对应的引用关系，因为该pages_m[i]指向的页page已经用完了
                            prev->next = page->next;
                            page->next->prev = page->prev; //slot_m[i]结构的next和prev指向自己

                            page->next = NULL; //page的next和prev指向NULL，表示不再可用来分配slot[]对应的obj
                            page->prev = NGX_SLAB_SMALL;
                        }

                        goto done;
                    }
                }
            }

        } else if (shift == ngx_slab_exact_shift) {
            //size == 128B，因为一个页可以放32个，用slab page的slab成员来标注每块内存的占用情况，不需要另外在内存缓存区分配bitmap，
            //并返回待分配的空间在缓存的位置  

            for (m = 1, i = 0; m; m <<= 1, i++) { //如果整个page页的所有slab已经用完，则会在后面的ngx_slab_alloc_pages从新获取空间并划分slab，然后分配
                if (page->slab & m) {
                    continue;
                }

                page->slab |= m; //标记第m个slab现在分配出去了

                if (page->slab == NGX_SLAB_BUSY) { //执行完page->slab |= m;后，有可能page->slab == NGX_SLAB_BUSY，表示最后一个obj已经分配出去了
                    //pages_m[i]和slot_m[i]取消对应的引用关系，因为该pages_m[i]指向的页page已经用完了
                    prev = ngx_slab_page_prev(page);
                    prev->next = page->next;
                    page->next->prev = page->prev;

                    page->next = NULL;
                    page->prev = NGX_SLAB_EXACT;
                }

                p = ngx_slab_page_addr(pool, page) + (i << shift);

                pool->stats[slot].used++;

                goto done;
            }

        } else { /* shift > ngx_slab_exact_shift */

            mask = ((uintptr_t) 1 << (ngx_pagesize >> shift)) - 1;
            mask <<= NGX_SLAB_MAP_SHIFT;

            for (m = (uintptr_t) 1 << NGX_SLAB_MAP_SHIFT, i = 0;
                 m & mask;
                 m <<= 1, i++)
            {
                if (page->slab & m) {
                    continue;
                }

                page->slab |= m;
				//size > 128B，也是更新slab page的slab成员，但是需要预先设置slab的部分bit，因为一个页的obj数量小于32个，并返回待分配的空间在缓存的位置
                //如果整个page页的所有slab已经用完，则会在后面的ngx_slab_alloc_pages从新获取空间并划分slab，然后分配
                if ((page->slab & NGX_SLAB_MAP_MASK) == mask) {
                    prev = ngx_slab_page_prev(page);
                    prev->next = page->next;
                    page->next->prev = page->prev;

                    page->next = NULL;
                    page->prev = NGX_SLAB_BIG;
                }

                p = ngx_slab_page_addr(pool, page) + (i << shift);

                pool->stats[slot].used++;

                goto done;
            }
        }

        ngx_slab_error(pool, NGX_LOG_ALERT, "ngx_slab_alloc(): page is busy");
        ngx_debug_point();
    }

    //分出一页加入到m_slot数组对应元素中  
    page = ngx_slab_alloc_pages(pool, 1);

    /*  
       例如要分配的size为54字节，则在前面计算出的shift对应的字节数应该是64字节，由于一个页面全是64字节obj大小，所以一共有64
       个64字节的obj，64个obj至少需要64位来表示每个obj是否已使用，因此至少需要64位(也就是8字节，2个int),所以至少要暂用一个64
       字节obj来存储该bitmap信息，第一个64字节obj实际上只用了8字节，其他56字节未用
     */
    if (page) {
        //size<128
        if (shift < ngx_slab_exact_shift) {
            bitmap = (uintptr_t *) ngx_slab_page_addr(pool, page);  //slot块的map存储在page的slot中定位到对应的page  page页的起始地址的一个uintptr_t类型4字节用来存储bitmap信息

            /*  
            例如要分配的size为54字节，则在前面计算出的shift对应的字节数应该是64字节，由于一个页面全是64字节obj大小，所以一共有64
            个64字节的obj，64个obj至少需要64位来表示每个obj是否已使用，因此至少需要64位(也就是8字节，2个int),所以至少要暂用一个64
            字节obj来存储该bitmap信息，第一个64字节obj实际上只用了8字节，其他56字节未用
            */
            n = (ngx_pagesize >> shift) / ((1 << shift) * 8); //计算bitmap需要多少个slot obj块

            if (n == 0) {
                n = 1; //至少需要一个4M页面大小的一个obj(2<<shift字节)来存储bitmap信息
            }

            /* "n" elements for bitmap, plus one requested */
            //设置对应的slot块的map,对于存放map的slot设置为1,表示已使用并且设置第一个可用的slot块(不是用于记录map的slot块)
            //标记为1,因为本次即将使用.
            for (i = 0; i < (n + 1) / (8 * sizeof(uintptr_t)); i++) {
                bitmap[i] = NGX_SLAB_BUSY;
            }

            m = ((uintptr_t) 1 << ((n + 1) % (8 * sizeof(uintptr_t)))) - 1;
            bitmap[i] = m; //这里是获取该页的第二个obj，因为第一个已经用于bitmap了，所以第一个和第二个在这里表示已用，bitmap[0]=3

            //计算所有obj的位图需要多少个uintptr_t存储。例如每个obj大小为64字节，则4K里面有64个obj，也就需要8字节，两个bitmap
            map = (ngx_pagesize >> shift) / (8 * sizeof(uintptr_t));

            for (i = i + 1; i < map; i++) {
                bitmap[i] = 0; //从第二个bitmap开始的所有位先清0
            }

            page->slab = shift; //该页的一个obj对应的字节移位数大小
            /* ngx_slab_pool_t + 9 * sizeof(ngx_slab_page_t)(slots_m[]) + pages * sizeof(ngx_slab_page_t)(pages_m[]) +pages*ngx_pagesize */
            page->next = &slots[slot];//指向上面的slots_m[i],例如obj大小64字节，则指向slots[2]   slots[0-8] -----8-2048
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_SMALL; //标记该页中存储的是小与128的obj

            slots[slot].next = page;

            pool->stats[slot].total += (ngx_pagesize >> shift) - n;

            p = ngx_slab_page_addr(pool, page) + (n << shift);
            //返回对应地址.  例如为64字节obj，则返回的start为第二个开始处obj，下次分配从第二个开始获取地址空间obj
            pool->stats[slot].used++;

            goto done;

        } else if (shift == ngx_slab_exact_shift) {

            page->slab = 1;//slab设置为1   page->slab存储obj的bitmap,例如这里为1，表示说第一个obj分配出去了 4K有32个128字节obj,因此一个slab位图刚好可以表示这32个obj
            page->next = &slots[slot];
            //用指针的后两位做标记,用NGX_SLAB_SMALL表示slot块小于ngx_slab_exact_shift  
            /*
                设置slab的高16位(32位系统)存放solt对应的map,并且该对应为map的地位对应page中高位的slot块.slab的低16为存储slot块大小的位移.
               */ 
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_EXACT;

            slots[slot].next = page;

            pool->stats[slot].total += 8 * sizeof(uintptr_t);

            p = ngx_slab_page_addr(pool, page);

            pool->stats[slot].used++;

            goto done;

        } else { /* shift > ngx_slab_exact_shift */

            page->slab = ((uintptr_t) 1 << NGX_SLAB_MAP_SHIFT) | shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_BIG;

            slots[slot].next = page;

            pool->stats[slot].total += ngx_pagesize >> shift;

            p = ngx_slab_page_addr(pool, page);

            pool->stats[slot].used++;

            goto done;
        }
    }

    p = 0;

    pool->stats[slot].fails++;

done:

    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0,
                   "slab alloc: %p", (void *) p);

    return (void *) p;
}


void *
ngx_slab_calloc(ngx_slab_pool_t *pool, size_t size)
{
    void  *p;

    ngx_shmtx_lock(&pool->mutex);

    p = ngx_slab_calloc_locked(pool, size);

    ngx_shmtx_unlock(&pool->mutex);

    return p;
}


void *
ngx_slab_calloc_locked(ngx_slab_pool_t *pool, size_t size)
{
    void  *p;

    p = ngx_slab_alloc_locked(pool, size);
    if (p) {
        ngx_memzero(p, size);
    }

    return p;
}


void
ngx_slab_free(ngx_slab_pool_t *pool, void *p)
{
    ngx_shmtx_lock(&pool->mutex);

    ngx_slab_free_locked(pool, p);

    ngx_shmtx_unlock(&pool->mutex);
}


void
ngx_slab_free_locked(ngx_slab_pool_t *pool, void *p)
{
    size_t            size;
    uintptr_t         slab, m, *bitmap;
    ngx_uint_t        i, n, type, slot, shift, map;
    ngx_slab_page_t  *slots, *page;

    ngx_log_debug1(NGX_LOG_DEBUG_ALLOC, ngx_cycle->log, 0, "slab free: %p", p);

    if ((u_char *) p < pool->start || (u_char *) p > pool->end) {
        ngx_slab_error(pool, NGX_LOG_ALERT, "ngx_slab_free(): outside of pool");
        goto fail;
    }

    n = ((u_char *) p - pool->start) >> ngx_pagesize_shift;
    page = &pool->pages[n];
    slab = page->slab;
    type = ngx_slab_page_type(page);

    switch (type) {

    case NGX_SLAB_SMALL:

        shift = slab & NGX_SLAB_SHIFT_MASK;
        size = (size_t) 1 << shift;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        n = ((uintptr_t) p & (ngx_pagesize - 1)) >> shift;
        m = (uintptr_t) 1 << (n % (8 * sizeof(uintptr_t)));
        n /= 8 * sizeof(uintptr_t);
        bitmap = (uintptr_t *)
                             ((uintptr_t) p & ~((uintptr_t) ngx_pagesize - 1));

        if (bitmap[n] & m) {
            slot = shift - pool->min_shift;

            if (page->next == NULL) {
                slots = ngx_slab_slots(pool);

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_SMALL;
                page->next->prev = (uintptr_t) page | NGX_SLAB_SMALL;
            }

            bitmap[n] &= ~m;

            n = (ngx_pagesize >> shift) / ((1 << shift) * 8);

            if (n == 0) {
                n = 1;
            }

            i = n / (8 * sizeof(uintptr_t));
            m = ((uintptr_t) 1 << (n % (8 * sizeof(uintptr_t)))) - 1;

            if (bitmap[i] & ~m) {
                goto done;
            }

            map = (ngx_pagesize >> shift) / (8 * sizeof(uintptr_t));

            for (i = i + 1; i < map; i++) {
                if (bitmap[i]) {
                    goto done;
                }
            }

            ngx_slab_free_pages(pool, page, 1);

            pool->stats[slot].total -= (ngx_pagesize >> shift) - n;

            goto done;
        }

        goto chunk_already_free;

    case NGX_SLAB_EXACT:

        m = (uintptr_t) 1 <<
                (((uintptr_t) p & (ngx_pagesize - 1)) >> ngx_slab_exact_shift);
        size = ngx_slab_exact_size;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        if (slab & m) {
            slot = ngx_slab_exact_shift - pool->min_shift;

            if (slab == NGX_SLAB_BUSY) {
                slots = ngx_slab_slots(pool);

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_EXACT;
                page->next->prev = (uintptr_t) page | NGX_SLAB_EXACT;
            }

            page->slab &= ~m;

            if (page->slab) {
                goto done;
            }

            ngx_slab_free_pages(pool, page, 1);

            pool->stats[slot].total -= 8 * sizeof(uintptr_t);

            goto done;
        }

        goto chunk_already_free;

    case NGX_SLAB_BIG:

        shift = slab & NGX_SLAB_SHIFT_MASK;
        size = (size_t) 1 << shift;

        if ((uintptr_t) p & (size - 1)) {
            goto wrong_chunk;
        }

        m = (uintptr_t) 1 << ((((uintptr_t) p & (ngx_pagesize - 1)) >> shift)
                              + NGX_SLAB_MAP_SHIFT);

        if (slab & m) {
            slot = shift - pool->min_shift;

            if (page->next == NULL) {
                slots = ngx_slab_slots(pool);

                page->next = slots[slot].next;
                slots[slot].next = page;

                page->prev = (uintptr_t) &slots[slot] | NGX_SLAB_BIG;
                page->next->prev = (uintptr_t) page | NGX_SLAB_BIG;
            }

            page->slab &= ~m;

            if (page->slab & NGX_SLAB_MAP_MASK) {
                goto done;
            }

            ngx_slab_free_pages(pool, page, 1);

            pool->stats[slot].total -= ngx_pagesize >> shift;

            goto done;
        }

        goto chunk_already_free;

    case NGX_SLAB_PAGE:

        if ((uintptr_t) p & (ngx_pagesize - 1)) {
            goto wrong_chunk;
        }

        if (!(slab & NGX_SLAB_PAGE_START)) {
            ngx_slab_error(pool, NGX_LOG_ALERT,
                           "ngx_slab_free(): page is already free");
            goto fail;
        }

        if (slab == NGX_SLAB_PAGE_BUSY) {
            ngx_slab_error(pool, NGX_LOG_ALERT,
                           "ngx_slab_free(): pointer to wrong page");
            goto fail;
        }

        size = slab & ~NGX_SLAB_PAGE_START;

        ngx_slab_free_pages(pool, page, size);

        ngx_slab_junk(p, size << ngx_pagesize_shift);

        return;
    }

    /* not reached */

    return;

done:

    pool->stats[slot].used--;

    ngx_slab_junk(p, size);

    return;

wrong_chunk:

    ngx_slab_error(pool, NGX_LOG_ALERT,
                   "ngx_slab_free(): pointer to wrong chunk");

    goto fail;

chunk_already_free:

    ngx_slab_error(pool, NGX_LOG_ALERT,
                   "ngx_slab_free(): chunk is already free");

fail:

    return;
}


static ngx_slab_page_t *
ngx_slab_alloc_pages(ngx_slab_pool_t *pool, ngx_uint_t pages)
{
    ngx_slab_page_t  *page, *p;

    for (page = pool->free.next; page != &pool->free; page = page->next) {

        if (page->slab >= pages) {

            if (page->slab > pages) {
                page[page->slab - 1].prev = (uintptr_t) &page[pages];

                page[pages].slab = page->slab - pages;
                page[pages].next = page->next;
                page[pages].prev = page->prev;

                p = (ngx_slab_page_t *) page->prev;
                p->next = &page[pages];
                page->next->prev = (uintptr_t) &page[pages];

            } else {
                p = (ngx_slab_page_t *) page->prev;
                p->next = page->next;
                page->next->prev = page->prev;
            }

            page->slab = pages | NGX_SLAB_PAGE_START;
            page->next = NULL;
            page->prev = NGX_SLAB_PAGE;

            pool->pfree -= pages;

            if (--pages == 0) {
                return page;
            }

            for (p = page + 1; pages; pages--) {
                p->slab = NGX_SLAB_PAGE_BUSY;
                p->next = NULL;
                p->prev = NGX_SLAB_PAGE;
                p++;
            }

            return page;
        }
    }

    if (pool->log_nomem) {
        ngx_slab_error(pool, NGX_LOG_CRIT,
                       "ngx_slab_alloc() failed: no memory");
    }

    return NULL;
}


static void
ngx_slab_free_pages(ngx_slab_pool_t *pool, ngx_slab_page_t *page,
    ngx_uint_t pages)
{
    ngx_slab_page_t  *prev, *join;

    pool->pfree += pages;

    page->slab = pages--;

    if (pages) {
        ngx_memzero(&page[1], pages * sizeof(ngx_slab_page_t));
    }

    if (page->next) {
        prev = ngx_slab_page_prev(page);
        prev->next = page->next;
        page->next->prev = page->prev;
    }

    join = page + page->slab;

    if (join < pool->last) {

        if (ngx_slab_page_type(join) == NGX_SLAB_PAGE) {

            if (join->next != NULL) {
                pages += join->slab;
                page->slab += join->slab;

                prev = ngx_slab_page_prev(join);
                prev->next = join->next;
                join->next->prev = join->prev;

                join->slab = NGX_SLAB_PAGE_FREE;
                join->next = NULL;
                join->prev = NGX_SLAB_PAGE;
            }
        }
    }

    if (page > pool->pages) {
        join = page - 1;

        if (ngx_slab_page_type(join) == NGX_SLAB_PAGE) {

            if (join->slab == NGX_SLAB_PAGE_FREE) {
                join = ngx_slab_page_prev(join);
            }

            if (join->next != NULL) {
                pages += join->slab;
                join->slab += page->slab;

                prev = ngx_slab_page_prev(join);
                prev->next = join->next;
                join->next->prev = join->prev;

                page->slab = NGX_SLAB_PAGE_FREE;
                page->next = NULL;
                page->prev = NGX_SLAB_PAGE;

                page = join;
            }
        }
    }

    if (pages) {
        page[pages].prev = (uintptr_t) page;
    }

    page->prev = (uintptr_t) &pool->free;
    page->next = pool->free.next;

    page->next->prev = (uintptr_t) page;

    pool->free.next = page;
}


static void
ngx_slab_error(ngx_slab_pool_t *pool, ngx_uint_t level, char *text)
{
    ngx_log_error(level, ngx_cycle->log, 0, "%s%s", text, pool->log_ctx);
}
