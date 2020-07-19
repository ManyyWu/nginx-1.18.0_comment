
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Maxim Dounin
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_MODULE_H_INCLUDED_
#define _NGX_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <nginx.h>


#define NGX_MODULE_UNSET_INDEX  (ngx_uint_t) -1


#define NGX_MODULE_SIGNATURE_0                                                \
    ngx_value(NGX_PTR_SIZE) ","                                               \
    ngx_value(NGX_SIG_ATOMIC_T_SIZE) ","                                      \
    ngx_value(NGX_TIME_T_SIZE) ","

#if (NGX_HAVE_KQUEUE)
#define NGX_MODULE_SIGNATURE_1   "1"
#else
#define NGX_MODULE_SIGNATURE_1   "0"
#endif

#if (NGX_HAVE_IOCP)
#define NGX_MODULE_SIGNATURE_2   "1"
#else
#define NGX_MODULE_SIGNATURE_2   "0"
#endif

#if (NGX_HAVE_FILE_AIO || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_3   "1"
#else
#define NGX_MODULE_SIGNATURE_3   "0"
#endif

#if (NGX_HAVE_AIO_SENDFILE || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_4   "1"
#else
#define NGX_MODULE_SIGNATURE_4   "0"
#endif

#if (NGX_HAVE_EVENTFD)
#define NGX_MODULE_SIGNATURE_5   "1"
#else
#define NGX_MODULE_SIGNATURE_5   "0"
#endif

#if (NGX_HAVE_EPOLL)
#define NGX_MODULE_SIGNATURE_6   "1"
#else
#define NGX_MODULE_SIGNATURE_6   "0"
#endif

#if (NGX_HAVE_KEEPALIVE_TUNABLE)
#define NGX_MODULE_SIGNATURE_7   "1"
#else
#define NGX_MODULE_SIGNATURE_7   "0"
#endif

#if (NGX_HAVE_INET6)
#define NGX_MODULE_SIGNATURE_8   "1"
#else
#define NGX_MODULE_SIGNATURE_8   "0"
#endif

#define NGX_MODULE_SIGNATURE_9   "1"
#define NGX_MODULE_SIGNATURE_10  "1"

#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
#define NGX_MODULE_SIGNATURE_11  "1"
#else
#define NGX_MODULE_SIGNATURE_11  "0"
#endif

#define NGX_MODULE_SIGNATURE_12  "1"

#if (NGX_HAVE_SETFIB)
#define NGX_MODULE_SIGNATURE_13  "1"
#else
#define NGX_MODULE_SIGNATURE_13  "0"
#endif

#if (NGX_HAVE_TCP_FASTOPEN)
#define NGX_MODULE_SIGNATURE_14  "1"
#else
#define NGX_MODULE_SIGNATURE_14  "0"
#endif

#if (NGX_HAVE_UNIX_DOMAIN)
#define NGX_MODULE_SIGNATURE_15  "1"
#else
#define NGX_MODULE_SIGNATURE_15  "0"
#endif

#if (NGX_HAVE_VARIADIC_MACROS)
#define NGX_MODULE_SIGNATURE_16  "1"
#else
#define NGX_MODULE_SIGNATURE_16  "0"
#endif

#define NGX_MODULE_SIGNATURE_17  "0"
#define NGX_MODULE_SIGNATURE_18  "0"

#if (NGX_HAVE_OPENAT)
#define NGX_MODULE_SIGNATURE_19  "1"
#else
#define NGX_MODULE_SIGNATURE_19  "0"
#endif

#if (NGX_HAVE_ATOMIC_OPS)
#define NGX_MODULE_SIGNATURE_20  "1"
#else
#define NGX_MODULE_SIGNATURE_20  "0"
#endif

#if (NGX_HAVE_POSIX_SEM)
#define NGX_MODULE_SIGNATURE_21  "1"
#else
#define NGX_MODULE_SIGNATURE_21  "0"
#endif

#if (NGX_THREADS || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_22  "1"
#else
#define NGX_MODULE_SIGNATURE_22  "0"
#endif

#if (NGX_PCRE)
#define NGX_MODULE_SIGNATURE_23  "1"
#else
#define NGX_MODULE_SIGNATURE_23  "0"
#endif

#if (NGX_HTTP_SSL || NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_24  "1"
#else
#define NGX_MODULE_SIGNATURE_24  "0"
#endif

#define NGX_MODULE_SIGNATURE_25  "1"

#if (NGX_HTTP_GZIP)
#define NGX_MODULE_SIGNATURE_26  "1"
#else
#define NGX_MODULE_SIGNATURE_26  "0"
#endif

#define NGX_MODULE_SIGNATURE_27  "1"

#if (NGX_HTTP_X_FORWARDED_FOR)
#define NGX_MODULE_SIGNATURE_28  "1"
#else
#define NGX_MODULE_SIGNATURE_28  "0"
#endif

#if (NGX_HTTP_REALIP)
#define NGX_MODULE_SIGNATURE_29  "1"
#else
#define NGX_MODULE_SIGNATURE_29  "0"
#endif

#if (NGX_HTTP_HEADERS)
#define NGX_MODULE_SIGNATURE_30  "1"
#else
#define NGX_MODULE_SIGNATURE_30  "0"
#endif

#if (NGX_HTTP_DAV)
#define NGX_MODULE_SIGNATURE_31  "1"
#else
#define NGX_MODULE_SIGNATURE_31  "0"
#endif

#if (NGX_HTTP_CACHE)
#define NGX_MODULE_SIGNATURE_32  "1"
#else
#define NGX_MODULE_SIGNATURE_32  "0"
#endif

#if (NGX_HTTP_UPSTREAM_ZONE)
#define NGX_MODULE_SIGNATURE_33  "1"
#else
#define NGX_MODULE_SIGNATURE_33  "0"
#endif

#if (NGX_COMPAT)
#define NGX_MODULE_SIGNATURE_34  "1"
#else
#define NGX_MODULE_SIGNATURE_34  "0"
#endif

#define NGX_MODULE_SIGNATURE                                                  \
    NGX_MODULE_SIGNATURE_0 NGX_MODULE_SIGNATURE_1 NGX_MODULE_SIGNATURE_2      \
    NGX_MODULE_SIGNATURE_3 NGX_MODULE_SIGNATURE_4 NGX_MODULE_SIGNATURE_5      \
    NGX_MODULE_SIGNATURE_6 NGX_MODULE_SIGNATURE_7 NGX_MODULE_SIGNATURE_8      \
    NGX_MODULE_SIGNATURE_9 NGX_MODULE_SIGNATURE_10 NGX_MODULE_SIGNATURE_11    \
    NGX_MODULE_SIGNATURE_12 NGX_MODULE_SIGNATURE_13 NGX_MODULE_SIGNATURE_14   \
    NGX_MODULE_SIGNATURE_15 NGX_MODULE_SIGNATURE_16 NGX_MODULE_SIGNATURE_17   \
    NGX_MODULE_SIGNATURE_18 NGX_MODULE_SIGNATURE_19 NGX_MODULE_SIGNATURE_20   \
    NGX_MODULE_SIGNATURE_21 NGX_MODULE_SIGNATURE_22 NGX_MODULE_SIGNATURE_23   \
    NGX_MODULE_SIGNATURE_24 NGX_MODULE_SIGNATURE_25 NGX_MODULE_SIGNATURE_26   \
    NGX_MODULE_SIGNATURE_27 NGX_MODULE_SIGNATURE_28 NGX_MODULE_SIGNATURE_29   \
    NGX_MODULE_SIGNATURE_30 NGX_MODULE_SIGNATURE_31 NGX_MODULE_SIGNATURE_32   \
    NGX_MODULE_SIGNATURE_33 NGX_MODULE_SIGNATURE_34

/*
下面的ctx_index、index、spare0、spare1、spare2、spare3、version变量不需要在定义时赋值,可以用Nginx准备好的宏NGX_MODULE_V1来定义,
它已经定义好了这7个值.
*/
#define NGX_MODULE_V1                                                         \
    NGX_MODULE_UNSET_INDEX, NGX_MODULE_UNSET_INDEX,                           \
    NULL, 0, 0, nginx_version, NGX_MODULE_SIGNATURE
//这些值在ngx_module_s中的spare_hook0 到 spare_hook7中使用
#define NGX_MODULE_V1_PADDING  0, 0, 0, 0, 0, 0, 0, 0

/*
在执行configure命令时仅使用-add-module参数添加了第三方HTTP过滤模块. 这里没有把默认未编译进Nginx的官方HTTP过滤模块考虑进去. 这样,在
configure执行完毕后,Nginx各HTTP过滤模块的执行顺序就确定了. 默认HTTP过滤模块间的顺序必须如图6-1所示,因为它们是“写死”在
auto/modules(auto/sources)脚本中的. 读者可以通过阅读这个modules脚本的源代码了解Nginx是如何根据各官方过滤模块功能的不同来决定它们的顺序的

默认即编译进Nginx的HTTP过滤模块
┏━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃默认即编译进Nginx的HTTP过滤模块     ┃    功能                                                          ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP头部做处理. 在返回200成功时,根据请求中If-              ┃
┃                                    ┃Modified-Since或者If-Unmodified-Since头部取得浏览器缓存文件的时   ┃
┃ngx_http_not_modified filter module ┃                                                                  ┃
┃                                    ┃间,再分析返回用户文件的最后修改时间,以此决定是否直接发送304     ┃
┃                                    ┃ Not Modified响应给用户                                           ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  处理请求中的Range信息,根据Range中的要求返回文件的一部分给      ┃
┃ngx_http_range_body_filter_module   ┃                                                                  ┃
┃                                    ┃用户                                                              ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP包体做处理. 将用户发送的ngx_chain_t结构的HTTP包         ┃
┃                                    ┃体复制到新的ngx_chain_t结构中（都是各种指针的复制,不包括实际     ┃
┃ngx_http_copy_filter_module         ┃                                                                  ┃
┃                                    ┃HTTP响应内容）,后续的HTTP过滤模块处埋的ngx_chain_t类型的成       ┃
┃                                    ┃员都是ngx_http_copy_filter_module模块处理后的变量                 ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP头部做处理. 允许通过修改nginx.conf配置文件,在返回      ┃
┃ngx_http_headers filter module      ┃                                                                  ┃
┃                                    ┃给用户的响应中添加任意的HTTP头部                                  ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP头部做处理. 这就是执行configure命令时提到的http_        ┃
┃ngx_http_userid filter module       ┃                                                                  ┃
┃                                    ┃userid module模块,它基于cookie提供了简单的认证管理功能           ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  可以将文本类型返回给用户的响应包,按照nginx．conf中的配置重新   ┃
┃ngx_http_charset filter module      ┃                                                                  ┃
┃                                    ┃进行编码,再返回给用户                                            ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  支持SSI（Server Side Include,服务器端嵌入）功能,将文件内容包  ┃
┃ngx_http_ssi_filter module          ┃                                                                  ┃
┃                                    ┃含到网页中并返回给用户                                            ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP包体做处理.                              它仅应用于     ┃
┃ngx_http_postpone_filter module     ┃subrequest产生的子请求. 它使得多个子请求同时向客户端发送响应时    ┃
┃                                    ┃能够有序,所谓的“有序”是揩按照构造子请求的顺序发送响应          ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  对特定的HTTP响应包体（如网页或者文本文件）进行gzip压缩,再      ┃
┃ngx_http_gzip_filter_module         ┃                                                                  ┃
┃                                    ┃把压缩后的内容返回给用户                                          ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_http_range_header_filter module ┃  支持range协议                                                   ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_http_chunked filter module      ┃  支持chunk编码                                                   ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃                                    ┃  仅对HTTP头部做处理. 该过滤模块将会把r->headers out结构体        ┃
┃                                    ┃中的成员序列化为返回给用户的HTTP响应字符流,包括响应行(如         ┃
┃ngx_http_header filter module       ┃                                                                  ┃
┃                                    ┃HTTP/I.1 200 0K)和响应头部,并通过调用ngx_http_write filter       ┃
┃                                    ┃ module过滤模块中的过滤方法直接将HTTP包头发送到客户端             ┃
┣━━━━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ngx_http_write filter module        ┃  仅对HTTP包体做处理. 该模块负责向客户端发送HTTP响应              ┃
┗━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
第三方过滤模块为何要在ngx_http_headers_filter_module模块之后、ngx_http_userid_filter_module横块之前
*/

/*
配置模块与核心模块都是与Nginx框架密切相关的,是其他模块的基础. 而事件模块则是HTTP模块和mail模块的基础,原因参见8.2.2节.
HTTP模块和mail模块的“地位”相似,它们都更关注于应用层面. 在事件模块中,ngx_event_core module事件模块是其他所有事件模块
的基础；在HTTP模块中,ngx_http_core module模块是其他所有HTTP模块的基础；在mail模块中,ngx_mail_core module模块是其他所
有mail模块的基础.
*/
struct ngx_module_s {//相关空间初始化,赋值等可以参考ngx_http_block
    /*
    对于一类模块（由下面的type成员决定类别）而言,ctx_index表示当前模块在这类模块中的序号. 这个成员常常是由管理这类模块的一个
    Nginx核心模块设置的,对于所有的HTTP模块而言,ctx_index是由核心模块ngx_http_module设置的. ctx_index非常重要,Nginx的模块化
    设计非常依赖于各个模块的顺序,它们既用于表达优先级,也用于表明每个模块的位置,借以帮助Nginx框架快速获得某个模块的数据（）
    */
    //ctx index表明了模块在相同类型模块中的顺序
    ngx_uint_t            ctx_index; //初始化赋值见ngx_http_block, 这个值是按照在http_modules中的位置顺序来排序的,见ngx_http_block

    /*
    index表示当前模块在ngx_modules数组中的序号. 注意,ctx_index表示的是当前模块在一类模块中的序号,而index表示当前模块在所有模块中的序号,
    它同样关键. Nginx启动时会根据ngx_modules数组设置各模块的index值. 例如：
    ngx_max_module = 0;
    for (i = 0; ngx_modules[i]; i++) {
        ngx_modules[i]->index = ngx_max_module++;
    }
    */
    ngx_uint_t            index; //模块在所有模块中的序号,是第几个模块

    //spare系列的保留变量,暂未使用
    char                 *name;

    ngx_uint_t            spare0;
    ngx_uint_t            spare1;

    //模块的版本,便于将来的扩展. 目前只有一种,默认为1
    ngx_uint_t            version;
    const char           *signature;

    /*
    ctx用于指向一类模块的上下文结构体,为什么需要ctx呢？因为前面说过,Nginx模块有许多种类,不同类模块之间的功能差别很大. 例如,
    事件类型的模块主要处理I/O事件相关的功能,HTTP类型的模块主要处理HTTP应用层的功能. 这样,每个模块都有了自己的特性,而ctx将会
    指向特定类型模块的公共接口. 例如,在HTTP模块中,ctx需要指向ngx_http_module_t结构体,可以参考例如ngx_http_core_module,
    event模块中,指向ngx_event_module_t
    */
    void                 *ctx; //HTTP框架初始化时完成的
    ngx_command_t        *commands; //commands将处理nginx.conf中的配置项

    /*
    结构体中的type字段决定了该模块的模块类型：
    core module对应的值为NGX_CORE_MODULE
    http module对应的值为NGX_HTTP_MODULE
    mail module对应的值为NGX_MAIL_MODULE
    event module对应的值为NGX_EVENT_MODULE
    每个大模块中都有一些具体功能实现的子模块,如ngx_lua模块就是http module中的子模块.
    type表示该模块的类型,它与ctx指针是紧密相关的. 在官方Nginx中,它的取值范围是以下5种：NGX_HTTP_MODULE、NGX_CORE_MODULE、
    NGX_CONF_MODULE、NGX_EVENT_MODULE、NGX_MAIL_MODULE. 这5种模块间的关系参考图8-2. 实际上,还可以自定义新的模块类型
    */
    ngx_uint_t            type;

    /*
    在Nginx的启动、停止过程中,以下7个函数指针表示有7个执行点会分别调用这7种方法（  ）. 对于任一个方法而言,
    如果不需要Nginx在某个时刻执行它,那么简单地把它设为NULL空指针即可
    */
    /*
    对于下列回调方法：init_module、init_process、exit_process、exit_master,调用它们的是Nginx的框架代码. 换句话说,这4个回调方法
    与HTTP框架无关,即使nginx.conf中没有配置http {...}这种开启HTTP功能的配置项,这些回调方法仍然会被调用. 因此,通常开发HTTP模块
    时都把它们设为NULL空指针. 这样,当Nginx不作为Web服务器使用时,不会执行HTTP模块的任何代码.
    */

    /*虽然从字面上理解应当在master进程启动时回调init_master,但到目前为止,框架代码从来不会调用它,因此,可将init_master设为NULL */
    ngx_int_t           (*init_master)(ngx_log_t *log); //实际上没用

    /*init_module回调方法在初始化所有模块时被调用. 在master/worker模式下,这个阶段将在启动worker子进程前完成*/
    ngx_int_t           (*init_module)(ngx_cycle_t *cycle); //ngx_init_cycle中调用,在解析玩所有的nginx.conf配置后才会调用模块的ngx_conf_parse

    /* init_process回调方法在正常服务前被调用. 在master/worker模式下,多个worker子进程已经产生,在每个worker进程
    的初始化过程会调用所有模块的init_process函数*/
    ngx_int_t           (*init_process)(ngx_cycle_t *cycle); //ngx_worker_process_init或者ngx_single_process_cycle中调用

    /* 由于Nginx暂不支持多线程模式,所以init_thread在框架代码中没有被调用过,设为NULL*/
    ngx_int_t           (*init_thread)(ngx_cycle_t *cycle); //实际上没用

    // 同上,exit_thread也不支持,设为NULL
    void                (*exit_thread)(ngx_cycle_t *cycle);//实际上没用

    /* exit_process回调方法在服务停止前调用. 在master/worker模式下,worker进程会在退出前调用它,见ngx_worker_process_exit*/
    void                (*exit_process)(ngx_cycle_t *cycle); //ngx_single_process_cycle 或者 ngx_worker_process_exit中调用
    // exit_master回调方法将在master进程退出前被调用
    void                (*exit_master)(ngx_cycle_t *cycle); //ngx_master_process_exit中调用


    /*以下8个spare_hook变量也是保留字段,目前没有使用,但可用Nginx提供的NGX_MODULE_V1_PADDING宏来填充. 看一下该宏的定义：
    #define NGX_MODULE_V1_PADDING  0, 0, 0, 0, 0, 0, 0, 0*/
    uintptr_t             spare_hook0;
    uintptr_t             spare_hook1;
    uintptr_t             spare_hook2;
    uintptr_t             spare_hook3;
    uintptr_t             spare_hook4;
    uintptr_t             spare_hook5;
    uintptr_t             spare_hook6;
    uintptr_t             spare_hook7;
};

/*
ngx_core_module_t上下文是以配置项的解析作为基础的,它提供了create_conf回调方法来创建存储配置顼的数据结构,
在读取nginx.conf配置文件时,会根据模块中的ngx_command_t把解析出的配置项存放在这个数据结构中；它还提供了init_conf
回调方法,用于在解析完配置文件后,使用解析出的配置项初始化核心模块功能. 除此以外,Nginx框架不会约束核心模块的接口、
功能,这种简洁、灵活的设计为Nginx实现动态可配置性、动态可扩展性、动态可定制性带来了极大的便利,这样,在每个模块的
功能实现中就会较少地考虑如何不停止服务、不重启服务来实现以上功能.
*/

/*
Nginx还定义了一种基础类型的模块：核心模块,它的模块类型叫做NGX_CORE_MODULE. 目前官方的核心类型模块中共有6个具体模块,分别
是ngx_core_module、ngx_errlog_module、ngx_events_module、ngx_openssl_module、ngx_http_module、ngx_mail_module模块
*/ //ngx_init_cycle中执行下面的函数,在解析相应核心模块NGX_CORE_MODULE前调用create_conf,解析完相应核心模块后调用init_conf

//所有的核心模块NGX_CORE_MODULE对应的上下文ctx为ngx_core_module_t,子模块,例如http{} NGX_HTTP_MODULE模块对应的为上下文为ngx_http_module_t
//events{} NGX_EVENT_MODULE模块对应的为上下文为ngx_event_module_t
typedef struct { //对应的是核心模块NGX_CORE_MODULE,在ngx_init_cycle中执行
    ngx_str_t             name;
    void               *(*create_conf)(ngx_cycle_t *cycle);

    //它还提供了init_conf回调方法,用于在解析完配置文件后,使用解析出的配置项初始化核心模块功能.
    char               *(*init_conf)(ngx_cycle_t *cycle, void *conf);
} ngx_core_module_t;


ngx_int_t ngx_preinit_modules(void);
ngx_int_t ngx_cycle_modules(ngx_cycle_t *cycle);
ngx_int_t ngx_init_modules(ngx_cycle_t *cycle);
ngx_int_t ngx_count_modules(ngx_cycle_t *cycle, ngx_uint_t type);


ngx_int_t ngx_add_module(ngx_conf_t *cf, ngx_str_t *file,
    ngx_module_t *module, char **order);


extern ngx_module_t  *ngx_modules[];
extern ngx_uint_t     ngx_max_module;

extern char          *ngx_module_names[];


#endif /* _NGX_MODULE_H_INCLUDED_ */
