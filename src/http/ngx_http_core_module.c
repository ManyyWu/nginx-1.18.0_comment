
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    u_char    *name;
    uint32_t   method;
} ngx_http_method_name_t;

//client_body_in_file_only  on | off | clean
#define NGX_HTTP_REQUEST_BODY_FILE_OFF    0
#define NGX_HTTP_REQUEST_BODY_FILE_ON     1
#define NGX_HTTP_REQUEST_BODY_FILE_CLEAN  2


static ngx_int_t ngx_http_core_auth_delay(ngx_http_request_t *r);
static void ngx_http_core_auth_delay_handler(ngx_http_request_t *r);

static ngx_int_t ngx_http_core_find_location(ngx_http_request_t *r);
static ngx_int_t ngx_http_core_find_static_location(ngx_http_request_t *r,
    ngx_http_location_tree_node_t *node);

static ngx_int_t ngx_http_core_preconfiguration(ngx_conf_t *cf);
static ngx_int_t ngx_http_core_postconfiguration(ngx_conf_t *cf);
static void *ngx_http_core_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_core_init_main_conf(ngx_conf_t *cf, void *conf);
static void *ngx_http_core_create_srv_conf(ngx_conf_t *cf);
static char *ngx_http_core_merge_srv_conf(ngx_conf_t *cf,
    void *parent, void *child);
static void *ngx_http_core_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_core_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);

static char *ngx_http_core_server(ngx_conf_t *cf, ngx_command_t *cmd,
    void *dummy);
static char *ngx_http_core_location(ngx_conf_t *cf, ngx_command_t *cmd,
    void *dummy);
static ngx_int_t ngx_http_core_regex_location(ngx_conf_t *cf,
    ngx_http_core_loc_conf_t *clcf, ngx_str_t *regex, ngx_uint_t caseless);

static char *ngx_http_core_types(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_type(ngx_conf_t *cf, ngx_command_t *dummy,
    void *conf);

static char *ngx_http_core_listen(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_server_name(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_root(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_core_limit_except(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_set_aio(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_directio(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_error_page(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_open_file_cache(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_error_log(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_keepalive(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_internal(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static char *ngx_http_core_resolver(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#if (NGX_HTTP_GZIP)
static ngx_int_t ngx_http_gzip_accept_encoding(ngx_str_t *ae);
static ngx_uint_t ngx_http_gzip_quantity(u_char *p, u_char *last);
static char *ngx_http_gzip_disable(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#endif
static ngx_int_t ngx_http_get_forwarded_addr_internal(ngx_http_request_t *r,
    ngx_addr_t *addr, u_char *xff, size_t xfflen, ngx_array_t *proxies,
    int recursive);
#if (NGX_HAVE_OPENAT)
static char *ngx_http_disable_symlinks(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
#endif

static char *ngx_http_core_lowat_check(ngx_conf_t *cf, void *post, void *data);
static char *ngx_http_core_pool_size(ngx_conf_t *cf, void *post, void *data);

static ngx_conf_post_t  ngx_http_core_lowat_post =
    { ngx_http_core_lowat_check };

static ngx_conf_post_handler_pt  ngx_http_core_pool_size_p =
    ngx_http_core_pool_size;


static ngx_conf_enum_t  ngx_http_core_request_body_in_file[] = {
    { ngx_string("off"), NGX_HTTP_REQUEST_BODY_FILE_OFF },
    { ngx_string("on"), NGX_HTTP_REQUEST_BODY_FILE_ON },
    { ngx_string("clean"), NGX_HTTP_REQUEST_BODY_FILE_CLEAN },
    { ngx_null_string, 0 }
};


static ngx_conf_enum_t  ngx_http_core_satisfy[] = {
    { ngx_string("all"), NGX_HTTP_SATISFY_ALL },
    { ngx_string("any"), NGX_HTTP_SATISFY_ANY },
    { ngx_null_string, 0 }
};


static ngx_conf_enum_t  ngx_http_core_lingering_close[] = {
    { ngx_string("off"), NGX_HTTP_LINGERING_OFF },
    { ngx_string("on"), NGX_HTTP_LINGERING_ON },
    { ngx_string("always"), NGX_HTTP_LINGERING_ALWAYS },
    { ngx_null_string, 0 }
};


static ngx_conf_enum_t  ngx_http_core_server_tokens[] = {
    { ngx_string("off"), NGX_HTTP_SERVER_TOKENS_OFF },
    { ngx_string("on"), NGX_HTTP_SERVER_TOKENS_ON },
    { ngx_string("build"), NGX_HTTP_SERVER_TOKENS_BUILD },
    { ngx_null_string, 0 }
};


static ngx_conf_enum_t  ngx_http_core_if_modified_since[] = {
    { ngx_string("off"), NGX_HTTP_IMS_OFF },
    { ngx_string("exact"), NGX_HTTP_IMS_EXACT },
    { ngx_string("before"), NGX_HTTP_IMS_BEFORE },
    { ngx_null_string, 0 }
};


static ngx_conf_bitmask_t  ngx_http_core_keepalive_disable[] = {
    { ngx_string("none"), NGX_HTTP_KEEPALIVE_DISABLE_NONE },
    { ngx_string("msie6"), NGX_HTTP_KEEPALIVE_DISABLE_MSIE6 },
    { ngx_string("safari"), NGX_HTTP_KEEPALIVE_DISABLE_SAFARI },
    { ngx_null_string, 0 }
};


static ngx_path_init_t  ngx_http_client_temp_path = {
    ngx_string(NGX_HTTP_CLIENT_TEMP_PATH), { 0, 0, 0 }
};


#if (NGX_HTTP_GZIP)

static ngx_conf_enum_t  ngx_http_gzip_http_version[] = {
    { ngx_string("1.0"), NGX_HTTP_VERSION_10 },
    { ngx_string("1.1"), NGX_HTTP_VERSION_11 },
    { ngx_null_string, 0 }
};


static ngx_conf_bitmask_t  ngx_http_gzip_proxied_mask[] = {
    { ngx_string("off"), NGX_HTTP_GZIP_PROXIED_OFF },
    { ngx_string("expired"), NGX_HTTP_GZIP_PROXIED_EXPIRED },
    { ngx_string("no-cache"), NGX_HTTP_GZIP_PROXIED_NO_CACHE },
    { ngx_string("no-store"), NGX_HTTP_GZIP_PROXIED_NO_STORE },
    { ngx_string("private"), NGX_HTTP_GZIP_PROXIED_PRIVATE },
    { ngx_string("no_last_modified"), NGX_HTTP_GZIP_PROXIED_NO_LM },
    { ngx_string("no_etag"), NGX_HTTP_GZIP_PROXIED_NO_ETAG },
    { ngx_string("auth"), NGX_HTTP_GZIP_PROXIED_AUTH },
    { ngx_string("any"), NGX_HTTP_GZIP_PROXIED_ANY },
    { ngx_null_string, 0 }
};


static ngx_str_t  ngx_http_gzip_no_cache = ngx_string("no-cache");
static ngx_str_t  ngx_http_gzip_no_store = ngx_string("no-store");
static ngx_str_t  ngx_http_gzip_private = ngx_string("private");

#endif

//相关配置见ngx_event_core_commands ngx_http_core_commands ngx_stream_commands ngx_http_core_commands ngx_core_commands  ngx_mail_commands
static ngx_command_t  ngx_http_core_commands[] = {
    /* 确定桶的个数,越大冲突概率越小. variables_hash_max_size是每个桶中对应的散列信息个数
        配置块:http server location
     */
    { ngx_string("variables_hash_max_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_core_main_conf_t, variables_hash_max_size),
      NULL },

    /* server_names_hash_max_size 32 | 64 |128 ,为了提个寻找server_name的能力,nginx使用散列表来存储server name.
        这个是设置每个散列桶咋弄的内存大小,注意和variables_hash_max_size区别
        配置块:http server location
    */
    { ngx_string("variables_hash_bucket_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_core_main_conf_t, variables_hash_bucket_size),
      NULL },
    /* server_names_hash_max_size 32 | 64 |128 ,为了提个寻找server_name的能力,nginx使用散列表来存储server name.
    */
    { ngx_string("server_names_hash_max_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_core_main_conf_t, server_names_hash_max_size),
      NULL },

    { ngx_string("server_names_hash_bucket_size"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_MAIN_CONF_OFFSET,
      offsetof(ngx_http_core_main_conf_t, server_names_hash_bucket_size),
      NULL },

    { ngx_string("server"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_http_core_server,
      0,
      0,
      NULL },

    /*
    connection_pool_size
    语法：connection_pool_size size;
    默认：connection_pool_size 256;
    配置块：http、server
    Nginx对于每个建立成功的TCP连接会预先分配一个内存池,上面的size配置项将指定这个内存池的初始大小（即ngx_connection_t结构体中的pool内存池初始大小,
    9.8.1节将介绍这个内存池是何时分配的）,用于减少内核对于小块内存的分配次数. 需慎重设置,因为更大的size会使服务器消耗的内存增多,而更小的size则会引发更多的内存分配次数.
    */
    { ngx_string("connection_pool_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, connection_pool_size),
      &ngx_http_core_pool_size_p },

/*
语法：request_pool_size size;
默认：request_pool_size 4k;
配置块：http、server
Nginx开始处理HTTP请求时,将会为每个请求都分配一个内存池,size配置项将指定这个内存池的初始大小（即ngx_http_request_t结构体中的pool内存池初始大小,
11.3节将介绍这个内存池是何时分配的）,用于减少内核对于小块内存的分配次数. TCP连接关闭时会销毁connection_pool_size指定的连接内存池,HTTP请求结束
时会销毁request_pool_size指定的HTTP请求内存池,但它们的创建、销毁时间并不一致,因为一个TCP连接可能被复用于多个HTTP请求.
*/
    { ngx_string("request_pool_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, request_pool_size),
      &ngx_http_core_pool_size_p },
    /*
    读取HTTP头部的超时时间
    语法：client_header_timeout time（默认单位：秒）;
    默认：client_header_timeout 60;
    配置块：http、server、location
    客户端与服务器建立连接后将开始接收HTTP头部,在这个过程中,如果在一个时间间隔（超时时间）内没有读取到客户端发来的字节,则认为超时,
    并向客户端返回408 ("Request timed out")响应.
    */
    { ngx_string("client_header_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, client_header_timeout),
      NULL },

/*
语法:  client_header_buffer_size size;
默认值:  client_header_buffer_size 1k;
上下文:  http, server

设置读取客户端请求头部的缓冲容量.  对于大多数请求,1K的缓冲足矣.  但如果请求中含有的cookie很长,或者请求来自WAP的客户端,可能
请求头不能放在1K的缓冲中.  如果从请求行,或者某个请求头开始不能完整的放在这块空间中,那么nginx将按照 large_client_header_buffers
指令的配置分配更多更大的缓冲来存放.
*/
    { ngx_string("client_header_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, client_header_buffer_size),
      NULL },

/*
存储超大HTTP头部的内存buffer大小
语法：large_client_header_buffers number size;
默认：large_client_header_buffers 4 8k;
配置块：http、server
large_client_header_buffers定义了Nginx接收一个超大HTTP头部请求的buffer个数和每个buffer的大小. 如果HTTP请求行（如GET /index HTTP/1.1）
的大小超过上面的单个buffer,则返回"Request URI too large" (414). 请求中一般会有许多header,每一个header的大小也不能超过单个buffer的大小,
否则会返回"Bad request" (400). 当然,请求行和请求头部的总和也不可以超过buffer个数*buffer大小.

设置读取客户端请求超大请求的缓冲最大number(数量)和每块缓冲的size(容量).  HTTP请求行的长度不能超过一块缓冲的容量,否则nginx返回错误414
(Request-URI Too Large)到客户端.  每个请求头的长度也不能超过一块缓冲的容量,否则nginx返回错误400 (Bad Request)到客户端.  缓冲仅在必需
是才分配,默认每块的容量是8K字节.  即使nginx处理完请求后与客户端保持入长连接,nginx也会释放这些缓冲.
*/ //当client_header_buffer_size不够存储头部行的时候,用large_client_header_buffers再次分配空间存储
    { ngx_string("large_client_header_buffers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE2,
      ngx_conf_set_bufs_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, large_client_header_buffers),
      NULL },

/*
忽略不合法的HTTP头部
语法：ignore_invalid_headers on | off;
默认：ignore_invalid_headers on;
配置块：http、server
如果将其设置为off,那么当出现不合法的HTTP头部时,Nginx会拒绝服务,并直接向用户发送400（Bad Request）错误. 如果将其设置为on,则会忽略此HTTP头部.
*/
    { ngx_string("ignore_invalid_headers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, ignore_invalid_headers),
      NULL },

/*
merge_slashes
语法：merge_slashes on | off;
默认：merge_slashes on;
配置块：http、server、location
此配置项表示是否合并相邻的“/”,例如,//test///a.txt,在配置为on时,会将其匹配为location /test/a.txt;如果配置为off,则不会匹配,URI将仍然是//test///a.txt.
*/
    { ngx_string("merge_slashes"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, merge_slashes),
      NULL },

/*
HTTP头部是否允许下画线
语法：underscores_in_headers on | off;
默认：underscores_in_headers off;
配置块：http、server
默认为off,表示HTTP头部的名称中不允许带“_”（下画线）.
*/
    { ngx_string("underscores_in_headers"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_SRV_CONF_OFFSET,
      offsetof(ngx_http_core_srv_conf_t, underscores_in_headers),
      NULL },
    /*
        location [= ~ ~* ^~ @ ] /uri/ {....} location会尝试根据用户请求中url来匹配上面的/url表达式,如果可以匹配
        就选择location{}块中的配置来处理用户请求. 当然,匹配方式是多样的,如下:
        1) = 表示把url当做字符串,以便于参数中的url做完全匹配. 例如
            localtion = / {
                #只有当用户请求是/时,才会使用该location下的配置.
            }
        2) ~表示匹配url时是字母大小写敏感的.
        3) ~*表示匹配url时忽略字母大小写问题
        4) ^~表示匹配url时指需要其前半部分与url参数匹配即可,例如:
            location ^~ /images/ {
                #以/images/开通的请求都会被匹配上
            }
        5) @表示仅用于nginx服务器内部请求之间的重定向,带有@的location不直接处理用户请求. 当然,在url参数里是可以用
            正则表达式的,例如:
            location ~* \.(gif|jpg|jpeg)$ {
                #匹配以.gif .jpg .jpeg结尾的请求.
            }

        上面这些方式表达为"如果匹配,则...",如果要实现"如果不匹配,则....",可以在最后一个location中使用/作为参数,它会匹配所有
        的HTTP请求,这样就可以表示如果不能匹配前面的所有location,则由"/"这个location处理. 例如:
            location / {
                # /可以匹配所有请求.
            }

         完全匹配 > 前缀匹配 > 正则表达式 > /
    */ //location {}配置查找可以参考ngx_http_core_find_config_phase->ngx_http_core_find_location
    { ngx_string("location"), 
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_BLOCK|NGX_CONF_TAKE12,
      ngx_http_core_location,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },
    //lisen [ip:port | ip(不知道端口默认为80) | 端口 | *:端口 | localhost:端口] [ngx_http_core_listen函数中的参数]
    { ngx_string("listen"),  //
      NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_core_listen,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },
      
    /* server_name www.a.com www.b.com,可以跟多个服务器域名
    在开始处理一个HTTP请求是,nginx会取出header头中的Host,与每个server的server_name进行匹配,一次决定到底由那一个server
    块来处理这个请求. 有可能一个host与多个server块中的多个server_name匹配,这是就会根据匹配优先级来选择实际处理的server块.
    server_name与HOst的匹配优先级如下:
    1 首先匹配字符串完全匹配的servername,如www.aaa.com
    2 其次选择通配符在前面的servername,如*.aaa.com
    3 再次选择通配符在后面的servername,如www.aaa.*
    4 最新选择使用正则表达式才匹配的servername.

    如果都不匹配,按照下面顺序选择处理server块:
    1 优先选择在listen配置项后加入[default|default_server]的server块.
    2 找到匹配listen端口的第一个server块

    如果server_name后面跟着空字符串,如server_name ""表示匹配没有host这个HTTP头部的请求
    该参数默认为server_name ""
    server_name_in_redirect on | off 该配置需要配合server_name使用. 在使用on打开后,表示在重定向请求时会使用
    server_name里的第一个主机名代替原先请求中的Host头部,而使用off关闭时,表示在重定向请求时使用请求本身的HOST头部
    */ //官方详细文档参考http://nginx.org/en/docs/http/server_names.html
    { ngx_string("server_name"),
      NGX_HTTP_SRV_CONF|NGX_CONF_1MORE,
      ngx_http_core_server_name,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

/*
types_hash_max_size
语法：types_hash_max_size size;
默认：types_hash_max_size 1024;
配置块：http、server、location
types_hash_max_size影响散列表的冲突率. types_hash_max_size越大,就会消耗更多的内存,但散列key的冲突率会降低,检索速度就更快. types_hash_max_size越小,消耗的内存就越小,但散列key的冲突率可能上升.
*/
    { ngx_string("types_hash_max_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, types_hash_max_size),
      NULL },

/*
types_hash_bucket_size
语法：types_hash_bucket_size size;
默认：types_hash_bucket_size 32|64|128;
配置块：http、server、location
为了快速寻找到相应MIME type,Nginx使用散列表来存储MIME type与文件扩展名. types_hash_bucket_size 设置了每个散列桶占用的内存大小.
*/
    { ngx_string("types_hash_bucket_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, types_hash_bucket_size),
      NULL },

    /*
    下面是MIME类型的设置配置项.
    MIME type与文件扩展的映射
    语法：type {...};
    配置块：http、server、location
    定义MIME type到文件扩展名的映射. 多个扩展名可以映射到同一个MIME type. 例如：
    types {
     text/html    html;
     text/html    conf;
     image/gif    gif;
     image/jpeg   jpg;
    }
    */ //types和default_type对应
//types {}配置ngx_http_core_type首先存在与该数组中,然后在ngx_http_core_merge_loc_conf存入types_hash中,真正生效见ngx_http_set_content_type
    { ngx_string("types"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF
                                          |NGX_CONF_BLOCK|NGX_CONF_NOARGS,
      ngx_http_core_types,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
/*
默认MIME type
语法：default_type MIME-type;
默认：default_type text/plain;
配置块：http、server、location
当找不到相应的MIME type与文件扩展名之间的映射时,使用默认的MIME type作为HTTP header中的Content-Type.
*/ //types和default_type对应
    { ngx_string("default_type"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, default_type),
      NULL },

    /* 
       nginx指定文件路径有两种方式root和alias,这两者的用法区别,使用方法总结了下,方便大家在应用过程中,快速响应. root与alias主要区别在于nginx如何解释location后面的uri,这会使两者分别以不同的方式将请求映射到服务器文件上.
       [root]
       语法：root path
       默认值：root html
       配置段：http、server、location、if
       [alias]
       语法：alias path
       配置段：location
       实例：
       
       location ~ ^/weblogs/ {
        root /data/weblogs/www.ttlsa.com;
        autoindex on;
        auth_basic            "Restricted";
        auth_basic_user_file  passwd/weblogs;
       }
       如果一个请求的URI是/weblogs/httplogs/www.ttlsa.com-access.log时,web服务器将会返回服务器上的/data/weblogs/www.ttlsa.com/weblogs/httplogs/www.ttlsa.com-access.log的文件.
       [info]root会根据完整的URI请求来映射,也就是/path/uri. [/info]
       因此,前面的请求映射为path/weblogs/httplogs/www.ttlsa.com-access.log.
       
       
       location ^~ /binapp/ {  
        limit_conn limit 4;
        limit_rate 200k;
        internal;  
        alias /data/statics/bin/apps/;
       }
       alias会把location后面配置的路径丢弃掉,把当前匹配到的目录指向到指定的目录. 如果一个请求的URI是/binapp/a.ttlsa.com/favicon时,web服务器将会返回服务器上的/data/statics/bin/apps/a.ttlsa.com/favicon.jgp的文件.
       [warning]1. 使用alias时,目录名后面一定要加"/".
       2. alias可以指定任何名称.
       3. alias在使用正则匹配时,必须捕捉要匹配的内容并在指定的内容处使用.
       4. alias只能位于location块中. [/warning]
       如需转载请注明出处：  http://www.ttlsa.com/html/2907.html


       定义资源文件路径. 默认root html.配置块:http  server location  if, 如:
        location /download/ {
            root /opt/web/html/;
        } 
        如果有一个请求的url是/download/index/aa.html,那么WEB将会返回服务器上/opt/web/html/download/index/aa.html文件的内容
    */
    { ngx_string("root"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_core_root,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /*
    以alias方式设置资源路径
    语法：alias path;
    配置块：location
    
    alias也是用来设置文件资源路径的,它与root的不同点主要在于如何解读紧跟location后面的uri参数,这将会致使alias与root以不同的方式将用户请求映射到真正的磁盘文件上. 例如,如果有一个请求的URI是/conf/nginx.conf,而用户实际想访问的文件在/usr/local/nginx/conf/nginx.conf,那么想要使用alias来进行设置的话,可以采用如下方式：
    location /conf {
       alias /usr/local/nginx/conf/;   
    }
    
    如果用root设置,那么语句如下所示：
    location /conf {
       root /usr/local/nginx/;       
    }
    
    使用alias时,在URI向实际文件路径的映射过程中,已经把location后配置的/conf这部分字符串丢弃掉,
    因此,/conf/nginx.conf请求将根据alias path映射为path/nginx.conf. root则不然,它会根据完整的URI
    请求来映射,因此,/conf/nginx.conf请求会根据root path映射为path/conf/nginx.conf. 这也是root
    可以放置到http、server、location或if块中,而alias只能放置到location块中的原因.
    
    alias后面还可以添加正则表达式,例如：
    location ~ ^/test/(\w+)\.(\w+)$ {
        alias /usr/local/nginx/$2/$1.$2;
    }
    
    这样,请求在访问/test/nginx.conf时,Nginx会返回/usr/local/nginx/conf/nginx.conf文件中的内容.

    nginx指定文件路径有两种方式root和alias,这两者的用法区别,使用方法总结了下,方便大家在应用过程中,快速响应. root与alias主要区别在于nginx如何解释location后面的uri,这会使两者分别以不同的方式将请求映射到服务器文件上.
[root]
语法：root path
默认值：root html
配置段：http、server、location、if
[alias]
语法：alias path
配置段：location
实例：

location ~ ^/weblogs/ {
 root /data/weblogs/www.ttlsa.com;
 autoindex on;
 auth_basic            "Restricted";
 auth_basic_user_file  passwd/weblogs;
}
如果一个请求的URI是/weblogs/httplogs/www.ttlsa.com-access.log时,web服务器将会返回服务器上的/data/weblogs/www.ttlsa.com/weblogs/httplogs/www.ttlsa.com-access.log的文件.
[info]root会根据完整的URI请求来映射,也就是/path/uri. [/info]
因此,前面的请求映射为path/weblogs/httplogs/www.ttlsa.com-access.log.


location ^~ /binapp/ {  
 limit_conn limit 4;
 limit_rate 200k;
 internal;  
 alias /data/statics/bin/apps/;
}
alias会把location后面配置的路径丢弃掉,把当前匹配到的目录指向到指定的目录. 如果一个请求的URI是/binapp/a.ttlsa.com/favicon时,web服务器将会返回服务器上的/data/statics/bin/apps/a.ttlsa.com/favicon.jgp的文件.
[warning]1. 使用alias时,目录名后面一定要加"/".
2. alias可以指定任何名称.
3. alias在使用正则匹配时,必须捕捉要匹配的内容并在指定的内容处使用.
4. alias只能位于location块中. [/warning]
如需转载请注明出处：  http://www.ttlsa.com/html/2907.html
    */
    { ngx_string("alias"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_core_root,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    /*
按HTTP方法名限制用户请求
语法:  limit_except method ... { ... }
默认值:  —  
上下文:  location
 
允许按请求的HTTP方法限制对某路径的请求. method用于指定不由这些限制条件进行过滤的HTTP方法,可选值有 GET、 HEAD、 POST、 PUT、
DELETE、 MKCOL、 COPY、 MOVE、 OPTIONS、 PROPFIND、 PROPPATCH、 LOCK、 UNLOCK 或者 PATCH.  指定method为GET方法的同时,
nginx会自动添加HEAD方法.  那么其他HTTP方法的请求就会由指令引导的配置块中的ngx_http_access_module 模块和ngx_http_auth_basic_module
模块的指令来限制访问. 如：

limit_except GET {
    allow 192.168.1.0/32;
    deny  all;
}

请留意上面的例子将对除GET和HEAD方法以外的所有HTTP方法的请求进行访问限制.
    */
    { ngx_string("limit_except"),
      NGX_HTTP_LOC_CONF|NGX_CONF_BLOCK|NGX_CONF_1MORE,
      ngx_http_core_limit_except,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
/*
HTTP请求包体的最大值
语法：client_max_body_size size;
默认：client_max_body_size 1m;
配置块：http、server、location
浏览器在发送含有较大HTTP包体的请求时,其头部会有一个Content-Length字段,client_max_body_size是用来限制Content-Length所示值的大小的. 因此,
这个限制包体的配置非常有用处,因为不用等Nginx接收完所有的HTTP包体—这有可能消耗很长时间—就可以告诉用户请求过大不被接受. 例如,用户试图
上传一个10GB的文件,Nginx在收完包头后,发现Content-Length超过client_max_body_size定义的值,就直接发送413 ("Request Entity Too Large")响应给客户端.
*/
    { ngx_string("client_max_body_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_off_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_max_body_size),
      NULL },

/*
存储HTTP头部的内存buffer大小
语法：client_header_buffer_size size;
默认：client_header_buffer_size 1k;
配置块：http、server
上面配置项定义了正常情况下Nginx接收用户请求中HTTP header部分（包括HTTP行和HTTP头部）时分配的内存buffer大小. 有时,
请求中的HTTP header部分可能会超过这个大小,这时large_client_header_buffers定义的buffer将会生效.
*/
    { ngx_string("client_body_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_buffer_size),
      NULL },

    /*
    读取HTTP包体的超时时间
    语法：client_body_timeout time（默认单位：秒）;
    默认：client_body_timeout 60;
    配置块：http、server、location
    此配置项与client_header_timeout相似,只是这个超时时间只在读取HTTP包体时才有效.
    */
    { ngx_string("client_body_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_timeout),
      NULL },

/*
HTTP包体的临时存放目录
语法：client_body_temp_path dir-path [ level1 [ level2 [ level3 ]]]
默认：client_body_temp_path client_body_temp;
配置块：http、server、location
上面配置项定义HTTP包体存放的临时目录. 在接收HTTP包体时,如果包体的大小大于client_body_buffer_size,则会以一个递增的整数命名并存放到
client_body_temp_path指定的目录中. 后面跟着的level1、level2、level3,是为了防止一个目录下的文件数量太多,从而导致性能下降,
因此使用了level参数,这样可以按照临时文件名最多再加三层目录. 例如：
client_body_temp_path  /opt/nginx/client_temp 1 2;
如果新上传的HTTP 包体使用00000123456作为临时文件名,就会被存放在这个目录中.
/opt/nginx/client_temp/6/45/00000123456
*/
    { ngx_string("client_body_temp_path"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1234,
      ngx_conf_set_path_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_temp_path),
      NULL },

    /*
    HTTP包体只存储到磁盘文件中
    语法：client_body_in_file_only on | clean | off;
    默认：client_body_in_file_only off;
    配置块：http、server、location
    当值为非off时,用户请求中的HTTP包体一律存储到磁盘文件中,即使只有0字节也会存储为文件. 当请求结束时,如果配置为on,则这个文件不会
    被删除（该配置一般用于调试、定位问题）,但如果配置为clean,则会删除该文件.
   */
    { ngx_string("client_body_in_file_only"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_in_file_only),
      &ngx_http_core_request_body_in_file },

/*
HTTP包体尽量写入到一个内存buffer中
语法：client_body_in_single_buffer on | off;
默认：client_body_in_single_buffer off;
配置块：http、server、location
用户请求中的HTTP包体一律存储到内存唯一同一个buffer中. 当然,如果HTTP包体的大小超过了下面client_body_buffer_size设置的值,包体还是会写入到磁盘文件中.
*/
    { ngx_string("client_body_in_single_buffer"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, client_body_in_single_buffer),
      NULL },

/*
sendfile系统调用
语法：sendfile on | off;
默认：sendfile off;
配置块：http、server、location
可以启用Linux上的sendfile系统调用来发送文件,它减少了内核态与用户态之间的两次内存复制,这样就会从磁盘中读取文件后直接在内核态发送到网卡设备,
提高了发送文件的效率.
*/ 
    /*
    When both AIO and sendfile are enabled on Linux, AIO is used for files that are larger than or equal to the size specified in the 
    directio directive, while sendfile is used for files of smaller sizes or when directio is disabled. 
    如果aio on; sendfile都配置了,并且执行了b->file->directio = of.is_directio(并且of.is_directio要为1)这几个模块,
    则当文件大小大于等于directio指定size(默认512)的时候使用aio,当小于size或者directio off的时候使用sendfile
    生效见ngx_open_and_stat_file  if (of->directio <= ngx_file_size(&fi)) { ngx_directio_on } 以及ngx_output_chain_copy_buf

    不过不满足上面的条件,如果aio on; sendfile都配置了,则还是以sendfile为准
    */ //ngx_output_chain_as_is  ngx_output_chain_copy_buf是aio和sendfile和普通文件读写的分支点  ngx_linux_sendfile_chain是sendfile发送和普通write发送的分界点
    { ngx_string("sendfile"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_FLAG,
//一般大缓存文件用aio发送,小文件用sendfile,因为aio是异步的,不影响其他流程,但是sendfile是同步的,太大的话可能需要多次sendfile才能发送完,有种阻塞感觉
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, sendfile),
      NULL },

    { ngx_string("sendfile_max_chunk"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, sendfile_max_chunk),
      NULL },

    { ngx_string("subrequest_output_buffer_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, subrequest_output_buffer_size),
      NULL },

/*
AIO系统调用
语法：aio on | off;
默认：aio off;
配置块：http、server、location
此配置项表示是否在FreeBSD或Linux系统上启用内核级别的异步文件I/O功能. 注意,它与sendfile功能是互斥的.

Syntax:  aio on | off | threads[=pool];
 
Default:  aio off; 
Context:  http, server, location
 
Enables or disables the use of asynchronous file I/O (AIO) on FreeBSD and Linux: 

location /video/ {
    aio            on;
    output_buffers 1 64k;
}

On FreeBSD, AIO can be used starting from FreeBSD 4.3. AIO can either be linked statically into a kernel: 

options VFS_AIO
or loaded dynamically as a kernel loadable module: 

kldload aio

On Linux, AIO can be used starting from kernel version 2.6.22. Also, it is necessary to enable directio, or otherwise reading will be blocking: 

location /video/ {
    aio            on;
    directio       512;
    output_buffers 1 128k;
}

On Linux, directio can only be used for reading blocks that are aligned on 512-byte boundaries (or 4K for XFS). File’s unaligned end is 
read in blocking mode. The same holds true for byte range requests and for FLV requests not from the beginning of a file: reading of 
unaligned data at the beginning and end of a file will be blocking. 

When both AIO and sendfile are enabled on Linux, AIO is used for files that are larger than or equal to the size specified in the directio 
directive, while sendfile is used for files of smaller sizes or when directio is disabled. 

location /video/ {
    sendfile       on;
    aio            on;
    directio       8m;
}

Finally, files can be read and sent using multi-threading (1.7.11), without blocking a worker process: 

location /video/ {
    sendfile       on;
    aio            threads;
}
Read and send file operations are offloaded to threads of the specified pool. If the pool name is omitted, the pool with the name “default” 
is used. The pool name can also be set with variables: 

aio threads=pool$disk;
By default, multi-threading is disabled, it should be enabled with the --with-threads configuration parameter. Currently, multi-threading is 
compatible only with the epoll, kqueue, and eventport methods. Multi-threaded sending of files is only supported on Linux. 
*/
/*
When both AIO and sendfile are enabled on Linux, AIO is used for files that are larger than or equal to the size specified in the 
directio directive, while sendfile is used for files of smaller sizes or when directio is disabled. 
如果aio on; sendfile都配置了,并且执行了b->file->directio = of.is_directio(并且of.is_directio要为1)这几个模块,
则当文件大小大于等于directio指定size(默认512)的时候使用aio,当小于size或者directio off的时候使用sendfile
生效见ngx_open_and_stat_file  if (of->directio <= ngx_file_size(&fi)) { ngx_directio_on } 以及ngx_output_chain_copy_buf

不过不满足上面的条件,如果aio on; sendfile都配置了,则还是以sendfile为准
*/ //ngx_output_chain_as_is  ngx_output_chain_align_file_buf  ngx_output_chain_copy_buf是aio和sendfile和普通文件读写的分支点  ngx_linux_sendfile_chain是sendfile发送和普通write发送的分界点
    { ngx_string("aio"),  
//一般大缓存文件用aio发送,小文件用sendfile,因为aio是异步的,不影响其他流程,但是sendfile是同步的,太大的话可能需要多次sendfile才能发送完,有种阻塞感觉
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_core_set_aio,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("aio_write"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, aio_write),
      NULL },

    { ngx_string("read_ahead"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, read_ahead),
      NULL },

/*
语法：directio size | off;
默认：directio off;
配置块：http、server、location
当文件大小大于该值的时候,可以此配置项在FreeBSD和Linux系统上使用O_DIRECT选项去读取文件,通常对大文件的读取速度有优化作用. 注意,它与sendfile功能是互斥的.
*/
/*
When both AIO and sendfile are enabled on Linux, AIO is used for files that are larger than or equal to the size specified in the 
directio directive, while sendfile is used for files of smaller sizes or when directio is disabled. 
如果aio on; sendfile都配置了,并且执行了b->file->directio = of.is_directio(并且of.is_directio要为1)这几个模块,
则当文件大小大于等于directio指定size(默认512)的时候使用aio,当小于size或者directio off的时候使用sendfile
生效见ngx_open_and_stat_file  if (of->directio <= ngx_file_size(&fi)) { ngx_directio_on } 以及ngx_output_chain_copy_buf

不过不满足上面的条件,如果aio on; sendfile都配置了,则还是以sendfile为准


当读入长度大于等于指定size的文件时,开启DirectIO功能. 具体的做法是,在FreeBSD或Linux系统开启使用O_DIRECT标志,在MacOS X系统开启
使用F_NOCACHE标志,在Solaris系统开启使用directio()功能. 这条指令自动关闭sendfile(0.7.15版). 它在处理大文件时
*/ //ngx_output_chain_as_is  ngx_output_chain_align_file_buf  ngx_output_chain_copy_buf是aio和sendfile和普通文件读写的分支点  ngx_linux_sendfile_chain是sendfile发送和普通write发送的分界点
  //生效见ngx_open_and_stat_file  if (of->directio <= ngx_file_size(&fi)) { ngx_directio_on }

    /* 数据在文件里面,并且程序有走到了 b->file->directio = of.is_directio(并且of.is_directio要为1)这几个模块,
        并且文件大小大于directio xxx中的大小才才会生效,见ngx_output_chain_align_file_buf  ngx_output_chain_as_is */
    { ngx_string("directio"), //在获取缓存文件内容的时候,只有文件大小大与等于directio的时候才会生效ngx_directio_on
//一般大缓存文件用aio发送,小文件用sendfile,因为aio是异步的,不影响其他流程,太大的话可能需要多次sendfile才能发送完,有种阻塞感觉
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_core_directio,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
directio_alignment
语法：directio_alignment size;
默认：directio_alignment 512;  它与directio配合使用,指定以directio方式读取文件时的对齐方式
配置块：http、server、location
它与directio配合使用,指定以directio方式读取文件时的对齐方式. 一般情况下,512B已经足够了,但针对一些高性能文件系统,如Linux下的XFS文件系统,
可能需要设置到4KB作为对齐方式.
*/ // 默认512   在ngx_output_chain_get_buf生效,表示分配内存空间的时候,空间起始地址需要按照这个值对齐
    { ngx_string("directio_alignment"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_off_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, directio_alignment),
      NULL },
/*
tcp_nopush
官方:

tcp_nopush
Syntax: tcp_nopush on | off
Default: off
Context: http
server
location
Reference: tcp_nopush
This directive permits or forbids the use of thesocket options TCP_NOPUSH on FreeBSD or TCP_CORK on Linux. This option is 
onlyavailable when using sendfile.
Setting this option causes nginx to attempt to sendit’s HTTP response headers in one packet on Linux and FreeBSD 4.x
You can read more about the TCP_NOPUSH and TCP_CORKsocket options here.

 
linux 下是tcp_cork,上面的意思就是说,当使用sendfile函数时,tcp_nopush才起作用,它和指令tcp_nodelay是互斥的. tcp_cork是linux下
tcp/ip传输的一个标准了,这个标准的大概的意思是,一般情况下,在tcp交互的过程中,当应用程序接收到数据包后马上传送出去,不等待,
而tcp_cork选项是数据包不会马上传送出去,等到数据包最大时,一次性的传输出去,这样有助于解决网络堵塞,已经是默认了.

也就是说tcp_nopush = on 会设置调用tcp_cork方法,这个也是默认的,结果就是数据包不会马上传送出去,等到数据包最大时,一次性的传输出去,
这样有助于解决网络堵塞.

以快递投递举例说明一下（以下是我的理解,也许是不正确的）,当快递东西时,快递员收到一个包裹,马上投递,这样保证了即时性,但是会
耗费大量的人力物力,在网络上表现就是会引起网络堵塞,而当快递收到一个包裹,把包裹放到集散地,等一定数量后统一投递,这样就是tcp_cork的
选项干的事情,这样的话,会最大化的利用网络资源,虽然有一点点延迟.

对于nginx配置文件中的tcp_nopush,默认就是tcp_nopush,不需要特别指定,这个选项对于www,ftp等大文件很有帮助

tcp_nodelay
        TCP_NODELAY和TCP_CORK基本上控制了包的“Nagle化”,Nagle化在这里的含义是采用Nagle算法把较小的包组装为更大的帧.  John Nagle是Nagle算法的发明人,后者就是用他的名字来命名的,他在1984年首次用这种方法来尝试解决福特汽车公司的网络拥塞问题（欲了解详情请参看IETF RFC 896）. 他解决的问题就是所谓的silly window syndrome,中文称“愚蠢窗口症候群”,具体含义是,因为普遍终端应用程序每产生一次击键操作就会发送一个包,而典型情况下一个包会拥有一个字节的数据载荷以及40个字节长的包头,于是产生4000%的过载,很轻易地就能令网络发生拥塞,.  Nagle化后来成了一种标准并且立即在因特网上得以实现. 它现在已经成为缺省配置了,但在我们看来,有些场合下把这一选项关掉也是合乎需要的.

       现在让我们假设某个应用程序发出了一个请求,希望发送小块数据. 我们可以选择立即发送数据或者等待产生更多的数据然后再一次发送两种策略. 如果我们马上发送数据,那么交互性的以及客户/服务器型的应用程序将极大地受益. 如果请求立即发出那么响应时间也会快一些. 以上操作可以通过设置套接字的TCP_NODELAY = on 选项来完成,这样就禁用了Nagle 算法.

       另外一种情况则需要我们等到数据量达到最大时才通过网络一次发送全部数据,这种数据传输方式有益于大量数据的通信性能,典型的应用就是文件服务器. 应用 Nagle算法在这种情况下就会产生问题. 但是,如果你正在发送大量数据,你可以设置TCP_CORK选项禁用Nagle化,其方式正好同 TCP_NODELAY相反（TCP_CORK和 TCP_NODELAY是互相排斥的）.



tcp_nopush
语法：tcp_nopush on | off;
默认：tcp_nopush off;
配置块：http、server、location
在打开sendfile选项时,确定是否开启FreeBSD系统上的TCP_NOPUSH或Linux系统上的TCP_CORK功能. 打开tcp_nopush后,将会在发送响应时把整个响应包头放到一个TCP包中发送.
*/ // tcp_nopush on | off;只有开启sendfile,nopush才生效,通过设置TCP_CORK实现
    { ngx_string("tcp_nopush"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, tcp_nopush),
      NULL },

      /*
      在元数据操作等小包传送时,发现性能不好,通过调试发现跟socket的TCP_NODELAY有很大关系.
      TCP_NODELAY 和 TCP_CORK,
      这两个选项都对网络连接的行为具有重要的作用. 许多UNIX系统都实现了TCP_NODELAY选项,但是,TCP_CORK则是Linux系统所独有的而且相对较新;它首先在内核版本2.4上得以实现.
      此外,其他UNIX系统版本也有功能类似的选项,值得注意的是,在某种由BSD派生的系统上的 TCP_NOPUSH选项其实就是TCP_CORK的一部分具体实现.
      TCP_NODELAY和TCP_CORK基本上控制了包的“Nagle化”,Nagle化在这里的含义是采用Nagle算法把较小的包组装为更大的帧.  John Nagle是Nagle算法的发明人,
      后者就是用他的名字来命名的,他在1984年首次用这种方法来尝试解决福特汽车公司的网络拥塞问题（欲了解详情请参看IETF RFC 896）. 他解决的问题就是所谓的silly
      window syndrome ,中文称“愚蠢窗口症候群”,具体含义是,因为普遍终端应用程序每产生一次击键操作就会发送一个包,而典型情况下一个包会拥有
      一个字节的数据载荷以及40 个字节长的包头,于是产生4000%的过载,很轻易地就能令网络发生拥塞,.  Nagle化后来成了一种标准并且立即在因特网上得以实现.
      它现在已经成为缺省配置了,但在我们看来,有些场合下把这一选项关掉也是合乎需要的.
      现在让我们假设某个应用程序发出了一个请求,希望发送小块数据. 我们可以选择立即发送数据或者等待产生更多的数据然后再一次发送两种策略. 如果我们马上发送数据,
      那么交互性的以及客户/服务器型的应用程序将极大地受益. 例如,当我们正在发送一个较短的请求并且等候较大的响应时,相关过载与传输的数据总量相比就会比较低,
      而且,如果请求立即发出那么响应时间也会快一些. 以上操作可以通过设置套接字的TCP_NODELAY选项来完成,这样就禁用了Nagle 算法.
      另外一种情况则需要我们等到数据量达到最大时才通过网络一次发送全部数据,这种数据传输方式有益于大量数据的通信性能,典型的应用就是文件服务器.
      应用 Nagle算法在这种情况下就会产生问题. 但是,如果你正在发送大量数据,你可以设置TCP_CORK选项禁用Nagle化,其方式正好同 TCP_NODELAY相反
      （TCP_CORK 和 TCP_NODELAY 是互相排斥的）. 下面就让我们仔细分析下其工作原理.
      假设应用程序使用sendfile()函数来转移大量数据. 应用协议通常要求发送某些信息来预先解释数据,这些信息其实就是报头内容. 典型情况下报头很小,
      而且套接字上设置了TCP_NODELAY. 有报头的包将被立即传输,在某些情况下（取决于内部的包计数器）,因为这个包成功地被对方收到后需要请求对方确认.
      这样,大量数据的传输就会被推迟而且产生了不必要的网络流量交换.
      但是,如果我们在套接字上设置了TCP_CORK（可以比喻为在管道上插入“塞子”）选项,具有报头的包就会填补大量的数据,所有的数据都根据大小自动地通过包传输出去.
      当数据传输完成时,最好取消TCP_CORK 选项设置给连接“拔去塞子”以便任一部分的帧都能发送出去. 这同“塞住”网络连接同等重要.
      总而言之,如果你肯定能一起发送多个数据集合（例如HTTP响应的头和正文）,那么我们建议你设置TCP_CORK选项,这样在这些数据之间不存在延迟.
      能极大地有益于WWW、FTP以及文件服务器的性能,同时也简化了你的工作. 示例代码如下：
      
      intfd, on = 1; 
      … 
      此处是创建套接字等操作,出于篇幅的考虑省略
      … 
      setsockopt (fd, SOL_TCP, TCP_CORK, &on, sizeof (on));  cork 
      write (fd, …); 
      fprintf (fd, …); 
      sendfile (fd, …); 
      write (fd, …); 
      sendfile (fd, …); 
      … 
      on = 0; 
      setsockopt (fd, SOL_TCP, TCP_CORK, &on, sizeof (on));  拔去塞子 
      或者
      setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(char*)&yes,sizeof(int));
      */
    /*
    语法：tcp_nodelay on | off;
    默认：tcp_nodelay on;
    配置块：http、server、location
    确定对keepalive连接是否使用TCP_NODELAY选项.  TCP_NODEALY其实就是禁用naggle算法,即使是小包也立即发送,TCP_CORK则和他相反,只有填充满后才发送
    */
    { ngx_string("tcp_nodelay"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, tcp_nodelay),
      NULL },

/*    reset_timeout_connection (感觉很多nginx源码没这个参数)
     
    语法：reset_timeout_connection on | off;
    
    默认：reset_timeout_connection off;
    
    配置块：http、server、location
    
    连接超时后将通过向客户端发送RST包来直接重置连接. 这个选项打开后,Nginx会在某个连接超时后,不是使用正常情形下的四次握手关闭TCP连接,
    而是直接向用户发送RST重置包,不再等待用户的应答,直接释放Nginx服务器上关于这个套接字使用的所有缓存（如TCP滑动窗口）. 相比正常的关闭方式,
    它使得服务器避免产生许多处于FIN_WAIT_1、FIN_WAIT_2、TIME_WAIT状态的TCP连接.
    
    注意,使用RST重置包关闭连接会带来一些问题,默认情况下不会开启.
*/         
    /*
    发送响应的超时时间
    语法：send_timeout time;
    默认：send_timeout 60;
    配置块：http、server、location
    这个超时时间是发送响应的超时时间,即Nginx服务器向客户端发送了数据包,但客户端一直没有去接收这个数据包. 如果某个连接超过
    send_timeout定义的超时时间,那么Nginx将会关闭这个连接
    */
    { ngx_string("send_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, send_timeout),
      NULL },

    { ngx_string("send_lowat"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, send_lowat),
      &ngx_http_core_lowat_post },
/* 
clcf->postpone_output：由于处理postpone_output指令,用于设置延时输出的阈值. 比如指令“postpone s”,当输出内容的size小于s, 默认1460
并且不是最后一个buffer,也不需要flush,那么就延时输出. 见ngx_http_write_filter
*/
    { ngx_string("postpone_output"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, postpone_output),
      NULL },

/* 
语法:  limit_rate rate;
默认值:  limit_rate 0;
上下文:  http, server, location, if in location

限制向客户端传送响应的速率限制. 参数rate的单位是字节/秒,设置为0将关闭限速.  nginx按连接限速,所以如果某个客户端同时开启了两个连接,
那么客户端的整体速率是这条指令设置值的2倍.

也可以利用$limit_rate变量设置流量限制. 如果想在特定条件下限制响应传输速率,可以使用这个功能：
server {

    if ($slow) {
        set $limit_rate 4k;
    }
    ...
}

此外,也可以通过“X-Accel-Limit-Rate”响应头来完成速率限制.  这种机制可以用proxy_ignore_headers指令和 fastcgi_ignore_headers指令关闭.
*/
    { ngx_string("limit_rate"), //limit_rate限制包体的发送速度,limit_req限制连接请求连理速度
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_set_complex_value_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, limit_rate),
      NULL },

/*

语法:  limit_rate_after size;
默认值:  limit_rate_after 0;
上下文:  http, server, location, if in location
 

设置不限速传输的响应大小. 当传输量大于此值时,超出部分将限速传送.
比如: 
location /flv/ {
    flv;
    limit_rate_after 500k;
    limit_rate       50k;
}
*/
    { ngx_string("limit_rate_after"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_http_set_complex_value_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, limit_rate_after),
      NULL },
        /*
        keepalive超时时间
        语法：keepalive_timeout time（默认单位：秒）;
        默认：keepalive_timeout 75;
        配置块：http、server、location
        一个keepalive 连接在闲置超过一定时间后（默认的是75秒）,服务器和浏览器都会去关闭这个连接. 当然,keepalive_timeout配置项是用
        来约束Nginx服务器的,Nginx也会按照规范把这个时间传给浏览器,但每个浏览器对待keepalive的策略有可能是不同的.
        */ //注意和ngx_http_upstream_keepalive_commands中keepalive的区别
    { ngx_string("keepalive_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_core_keepalive,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
/*
一个keepalive长连接上允许承载的请求最大数
语法：keepalive_requests n;
默认：keepalive_requests 100;
配置块：http、server、location
一个keepalive连接上默认最多只能发送100个请求.  设置通过一个长连接可以处理的最大请求数.  请求数超过此值,长连接将关闭.
*/
    { ngx_string("keepalive_requests"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, keepalive_requests),
      NULL },
/*
对某些浏览器禁用keepalive功能
语法：keepalive_disable [ msie6 | safari | none ]...
默认：keepalive_disable  msie6 safari
配置块：http、server、location
HTTP请求中的keepalive功能是为了让多个请求复用一个HTTP长连接,这个功能对服务器的性能提高是很有帮助的. 但有些浏览器,如IE 6和Safari,
它们对于使用keepalive功能的POST请求处理有功能性问题. 因此,针对IE 6及其早期版本、Safari浏览器默认是禁用keepalive功能的.
*/
    { ngx_string("keepalive_disable"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, keepalive_disable),
      &ngx_http_core_keepalive_disable },

       /*
        相对于NGX HTTP ACCESS PHASE阶段处理方法,satisfy配置项参数的意义
        ┏━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
        ┃satisfy的参数 ┃    意义                                                                              ┃
        ┣━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
        ┃              ┃    NGX HTTP ACCESS PHASE阶段可能有很多HTTP模块都对控制请求的访问权限感兴趣,         ┃
        ┃              ┃那么以哪一个为准呢？当satisfy的参数为all时,这些HTTP模块必须同时发生作用,即以该阶    ┃
        ┃all           ┃                                                                                      ┃
        ┃              ┃段中全部的handler方法共同决定请求的访问权限,换句话说,这一阶段的所有handler方法必    ┃
        ┃              ┃须全部返回NGX OK才能认为请求具有访问权限                                              ┃
        ┣━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
        ┃              ┃  与all相反,参数为any时意味着在NGX—HTTP__ ACCESS—PHASE阶段只要有任意一个           ┃
        ┃              ┃HTTP模块认为请求合法,就不用再调用其他HTTP模块继续检查了,可以认为请求是具有访问      ┃
        ┃              ┃权限的. 实际上,这时的情况有些复杂：如果其中任何一个handler方法返回NGX二OK,则认为    ┃
        ┃              ┃请求具有访问权限;如果某一个handler方法返回403戎者401,则认为请求没有访问权限,还     ┃
        ┃any           ┃                                                                                      ┃
        ┃              ┃需要检查NGX—HTTP—ACCESS—PHASE阶段的其他handler方法. 也就是说,any配置项下任        ┃
        ┃              ┃何一个handler方法一旦认为请求具有访问权限,就认为这一阶段执行成功,继续向下执行;如   ┃
        ┃              ┃果其中一个handler方法认为没有访问权限,则未必以此为准,还需要检测其他的hanlder方法.   ┃
        ┃              ┃all和any有点像“&&”和“¨”的关系                                                    ┃
        ┗━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
        */
    { ngx_string("satisfy"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, satisfy),
      &ngx_http_core_satisfy },

    { ngx_string("auth_delay"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, auth_delay),
      NULL },
     /*
     internal 
     语法：internal 
     默认值：no 
     使用字段： location 
     internal指令指定某个location只能被“内部的”请求调用,外部的调用请求会返回"Not found" (404)
     “内部的”是指下列类型：
     ·指令error_page重定向的请求.
     ·ngx_http_ssi_module模块中使用include virtual指令创建的某些子请求.
     ·ngx_http_rewrite_module模块中使用rewrite指令修改的请求.
     
     一个防止错误页面被用户直接访问的例子：
     
     error_page 404 /404.html;
     location  /404.html {  //表示匹配/404.html的location必须uri是重定向后的uri
       internal;
     }
     */ 
     /* 该location{}必须是内部重定向(index重定向 、error_pages等重定向调用ngx_http_internal_redirect)后匹配的location{},否则不让访问该location */
     //在location{}中配置了internal,表示匹配该uri的location{}必须是进行重定向后匹配的该location,如果不满足条件直接返回NGX_HTTP_NOT_FOUND,
     //生效地方见ngx_http_core_find_config_phase   
    { ngx_string("internal"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_core_internal,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
/*
lingering_close
语法：lingering_close off | on | always;
默认：lingering_close on;
配置块：http、server、location
该配置控制Nginx关闭用户连接的方式. always表示关闭用户连接前必须无条件地处理连接上所有用户发送的数据. off表示关闭连接时完全不管连接
上是否已经有准备就绪的来自用户的数据. on是中间值,一般情况下在关闭连接前都会处理连接上的用户发送的数据,除了有些情况下在业务上认定这之后的数据是不必要的.
*/
    { ngx_string("lingering_close"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, lingering_close),
      &ngx_http_core_lingering_close },

    /*
    lingering_time
    语法：lingering_time time;
    默认：lingering_time 30s;
    配置块：http、server、location
    lingering_close启用后,这个配置项对于上传大文件很有用. 上文讲过,当用户请求的Content-Length大于max_client_body_size配置时,
    Nginx服务会立刻向用户发送413（Request entity too large）响应. 但是,很多客户端可能不管413返回值,仍然持续不断地上传HTTP body,
    这时,经过了lingering_time设置的时间后,Nginx将不管用户是否仍在上传,都会把连接关闭掉.
    */
    { ngx_string("lingering_time"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, lingering_time),
      NULL },
/*
lingering_timeout
语法：lingering_timeout time;
默认：lingering_timeout 5s;
配置块：http、server、location
lingering_close生效后,在关闭连接前,会检测是否有用户发送的数据到达服务器,如果超过lingering_timeout时间后还没有数据可读,
就直接关闭连接;否则,必须在读取完连接缓冲区上的数据并丢弃掉后才会关闭连接.
*/
    { ngx_string("lingering_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, lingering_timeout),
      NULL },

    { ngx_string("reset_timedout_connection"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, reset_timedout_connection),
      NULL },

    { ngx_string("absolute_redirect"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, absolute_redirect),
      NULL },
    /*  server_name_in_redirect on | off 该配置需要配合server_name使用. 在使用on打开后,表示在重定向请求时会使用
    server_name里的第一个主机名代替原先请求中的Host头部,而使用off关闭时,表示在重定向请求时使用请求本身的HOST头部 */
    { ngx_string("server_name_in_redirect"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, server_name_in_redirect),
      NULL },

    { ngx_string("port_in_redirect"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, port_in_redirect),
      NULL },

    { ngx_string("msie_padding"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, msie_padding),
      NULL },

    { ngx_string("msie_refresh"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, msie_refresh),
      NULL },

/*
文件未找到时是否记录到error日志
语法：log_not_found on | off;
默认：log_not_found on;
配置块：http、server、location
此配置项表示当处理用户请求且需要访问文件时,如果没有找到文件,是否将错误日志记录到error.log文件中. 这仅用于定位问题.
*/
    { ngx_string("log_not_found"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, log_not_found),
      NULL },

    { ngx_string("log_subrequest"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, log_subrequest),
      NULL },

    /* 
    是否允许递归使用error_page
    语法：recursive_error_pages [on | off];
    默认：recursive_error_pages off;
    配置块：http、server、location
    确定是否允许递归地定义error_page.
    */
    { ngx_string("recursive_error_pages"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, recursive_error_pages),
      NULL },

/*
返回错误页面时是否在Server中注明Nginx版本
语法：server_tokens on | off;
默认：server_tokens on;
配置块：http、server、location
表示处理请求出错时是否在响应的Server头部中标明Nginx版本,这是为了方便定位问题.
*/
    { ngx_string("server_tokens"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, server_tokens),
      &ngx_http_core_server_tokens },

/*
对If-Modified-Since头部的处理策略
语法：if_modified_since [off|exact|before];
默认：if_modified_since exact;
配置块：http、server、location
出于性能考虑,Web浏览器一般会在客户端本地缓存一些文件,并存储当时获取的时间. 这样,下次向Web服务器获取缓存过的资源时,
就可以用If-Modified-Since头部把上次获取的时间捎带上,而if_modified_since将根据后面的参数决定如何处理If-Modified-Since头部.

相关参数说明如下.
off：表示忽略用户请求中的If-Modified-Since头部. 这时,如果获取一个文件,那么会正常地返回文件内容. HTTP响应码通常是200.
exact：将If-Modified-Since头部包含的时间与将要返回的文件上次修改的时间做精确比较,如果没有匹配上,则返回200和文件的实际内容,如果匹配上,
则表示浏览器缓存的文件内容已经是最新的了,没有必要再返回文件从而浪费时间与带宽了,这时会返回304 Not Modified,浏览器收到后会直接读取自己的本地缓存.
before：是比exact更宽松的比较. 只要文件的上次修改时间等于或者早于用户请求中的If-Modified-Since头部的时间,就会向客户端返回304 Not Modified.
*/ //生效见ngx_http_test_if_modified
    { ngx_string("if_modified_since"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, if_modified_since),
      &ngx_http_core_if_modified_since },

    { ngx_string("max_ranges"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, max_ranges),
      NULL },

    { ngx_string("chunked_transfer_encoding"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, chunked_transfer_encoding),
      NULL },

    //生效见ngx_http_set_etag
    { ngx_string("etag"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, etag),
      NULL },

    /*
    根据HTTP返回码重定向页面
    语法：error_page code [ code... ] [ = | =answer-code ] uri | @named_location
    配置块：http、server、location、if 
    
    当对于某个请求返回错误码时,如果匹配上了error_page中设置的code,则重定向到新的URI中. 例如：
    error_page   404          /404.html;
    error_page   502 503 504  /50x.html;
    error_page   403          http://example.com/forbidden.html;
    error_page   404          = @fetch;
    
    注意,虽然重定向了URI,但返回的HTTP错误码还是与原来的相同. 用户可以通过“=”来更改返回的错误码,例如：
    error_page 404 =200 /empty.gif;
    error_page 404 =403 /forbidden.gif;
    
    也可以不指定确切的返回错误码,而是由重定向后实际处理的真实结果来决定,这时,只要把“=”后面的错误码去掉即可,例如：
    error_page 404 = /empty.gif;
    
    如果不想修改URI,只是想让这样的请求重定向到另一个location中进行处理,那么可以这样设置：
    location / (
        error_page 404 @fallback;
    )
     
    location @fallback (
        proxy_pass http://backend;
    )
    
    这样,返回404的请求会被反向代理到http://backend上游服务器中处理
    */ //try_files和error_page都有重定向功能  //error_page错误码必须must be between 300 and 599,并且不能为499,见ngx_http_core_error_page
    { ngx_string("error_page"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_2MORE,
      ngx_http_core_error_page,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("post_action"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, post_action),
      NULL },
    
//    error_log file [ debug | info | notice | warn | error | crit ] 
    { ngx_string("error_log"), //ngx_errlog_module中的error_log配置只能全局配置,ngx_http_core_module在http{} server{} local{}中配置
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_core_error_log,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
打开文件缓存

语法：open_file_cache max = N [inactive = time] | off;

默认：open_file_cache off;

配置块：http、server、location

文件缓存会在内存中存储以下3种信息：

文件句柄、文件大小和上次修改时间.

已经打开过的目录结构.

没有找到的或者没有权限操作的文件信息.

这样,通过读取缓存就减少了对磁盘的操作.

该配置项后面跟3种参数.
max：表示在内存中存储元素的最大个数. 当达到最大限制数量后,将采用LRU（Least Recently Used）算法从缓存中淘汰最近最少使用的元素.
inactive：表示在inactive指定的时间段内没有被访问过的元素将会被淘汰. 默认时间为60秒.
off：关闭缓存功能.
例如：
open_file_cache max=1000 inactive=20s; //如果20s内有请求到该缓存,则该缓存继续生效,如果20s内都没有请求该缓存,则20s外请求,会重新获取原文件并生成缓存
*/

/*
   注意open_file_cache inactive=20s和fastcgi_cache_valid 20s的区别,前者指的是如果客户端在20s内没有请求到来,则会把该缓存文件对应的fd stat属性信息
   从ngx_open_file_cache_t->rbtree(expire_queue)中删除(客户端第一次请求该uri对应的缓存文件的时候会把该文件对应的stat信息节点ngx_cached_open_file_s添加到
   ngx_open_file_cache_t->rbtree(expire_queue)中),从而提高获取缓存文件的效率
   fastcgi_cache_valid指的是何时缓存文件过期,过期则删除,定时执行ngx_cache_manager_process_handler->ngx_http_file_cache_manager
*/

/* 
   如果没有配置open_file_cache max=1000 inactive=20s;,也就是说没有缓存cache缓存文件对应的文件stat信息,则每次都要从新打开文件获取文件stat信息,
   如果有配置open_file_cache,则会把打开的cache缓存文件stat信息按照ngx_crc32_long做hash后添加到ngx_cached_open_file_t->rbtree中,这样下次在请求该
   uri,则就不用再次open文件后在stat获取文件属性了,这样可以提高效率,参考ngx_open_cached_file

   创建缓存文件stat节点node后,每次新来请求的时候都会更新accessed时间,因此只要inactive时间内有请求,就不会删除缓存stat节点,见ngx_expire_old_cached_files
   inactive时间内没有新的请求则会从红黑树中删除该节点,同时关闭该文件见ngx_open_file_cleanup  ngx_close_cached_file  ngx_expire_old_cached_files
   */ //可以参考ngx_open_file_cache_t  参考ngx_open_cached_file 
   
    { ngx_string("open_file_cache"), 
//open_file_cache inactive 30主要用于是否在30s内有新请求,没有则删除缓存,而open_file_cache_min_uses表示只要缓存在红黑树中,并且遍历该文件次数达到指定次数,则不会close文件,也就不会从新获取stat信息
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_core_open_file_cache,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache),
      NULL },

/*
检验缓存中元素有效性的频率
语法：open_file_cache_valid time;
默认：open_file_cache_valid 60s;
配置块：http、server、location
设置检查open_file_cache缓存stat信息的元素的时间间隔.
*/ 
//表示60s后来的第一个请求要对文件stat信息做一次检查,检查是否发送变化,如果发送变化则从新获取文件stat信息或者从新创建该阶段,
    //生效在ngx_open_cached_file中的(&& now - file->created < of->valid ) 
    { ngx_string("open_file_cache_valid"), 
    //open_file_cache_min_uses后者是判断是否需要close描述符,然后重新打开获取fd和stat信息,open_file_cache_valid只是定期对stat(超过该配置时间后,在来一个的时候会进行判断)进行更新
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_sec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache_valid),
      NULL },

/*
不被淘汰的最小访问次数
语法：open_file_cache_min_uses number;
默认：open_file_cache_min_uses 1;
配置块：http、server、location
 
设置在由open_file_cache指令的inactive参数配置的超时时间内,文件应该被访问的最小number(次数). 如果访问次数大于等于此值,文件描
述符会保留在缓存中,否则从缓存中删除.
*/  //例如open_file_cache max=102400 inactive=20s; 只要该缓存文件被遍历次数超过open_file_cache_min_uses次请求,则缓存中的文件更改信息不变,不会close文件
    //这时候的情况是:请求带有If-Modified-Since,得到的是304且Last-Modified时间没变
/*
file->uses >= min_uses表示只要在inactive时间内该ngx_cached_open_file_s file节点被遍历到的次数达到min_uses次,则永远不会关闭文件(也就是不用重新获取文件stat信息),
除非该cache node失效,缓存超时inactive后会从红黑树中删除该file node节点,同时关闭文件等见ngx_open_file_cleanup  ngx_close_cached_file  ngx_expire_old_cached_files
*/    { ngx_string("open_file_cache_min_uses"), 
//只要缓存匹配次数达到这么多次,就不会重新关闭close该文件缓存,下次也就不会从新打开文件获取文件描述符,除非缓存时间inactive内都没有新请求,则会删除节点并关闭文件
//open_file_cache inactive 30主要用于是否在30s内有新请求,没有则删除缓存,而open_file_cache_min_uses表示只要缓存在红黑树中,并且遍历该文件次数达到指定次数,则不会close文件,也就不会从新获取stat信息

//open_file_cache_min_uses后者是判断是否需要close描述符,然后重新打开获取fd和stat信息,open_file_cache_valid只是定期对stat(超过该配置时间后,在来一个的时候会进行判断)进行更新

      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache_min_uses),
      NULL },

/*
是否缓存打开文件错误的信息
语法：open_file_cache_errors on | off;
默认：open_file_cache_errors off;
配置块：http、server、location
此配置项表示是否在文件缓存中缓存打开文件时出现的找不到路径、没有权限等错误信息.
*/
    { ngx_string("open_file_cache_errors"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache_errors),
      NULL },

    { ngx_string("open_file_cache_events"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, open_file_cache_events),
      NULL },

/*
DNS解析地址
语法：resolver address ...;
配置块：http、server、location
设置DNS名字解析服务器的地址,例如：
resolver 127.0.0.1 192.0.2.1;
*/
    { ngx_string("resolver"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_core_resolver,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

/*
DNS解析的超时时间
语法：resolver_timeout time;
默认：resolver_timeout 30s;
配置块：http、server、location
此配置项表示DNS解析的超时时间.
*/ //产考:http://theantway.com/2013/09/understanding_the_dns_resolving_in_nginx/         Nginx的DNS解析过程分析 
    { ngx_string("resolver_timeout"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_msec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, resolver_timeout),
      NULL },

#if (NGX_HTTP_GZIP)

    { ngx_string("gzip_vary"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, gzip_vary),
      NULL },

    { ngx_string("gzip_http_version"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, gzip_http_version),
      &ngx_http_gzip_http_version },

    { ngx_string("gzip_proxied"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_core_loc_conf_t, gzip_proxied),
      &ngx_http_gzip_proxied_mask },

    { ngx_string("gzip_disable"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_gzip_disable,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

#endif

#if (NGX_HAVE_OPENAT)

/*
语法:  disable_symlinks off;
disable_symlinks on | if_not_owner [from=part];
 
默认值:  disable_symlinks off; 
上下文:  http, server, location
 
决定nginx打开文件时如何处理符号链接： 

off
默认行为,允许路径中出现符号链接,不做检查.
on
如果文件路径中任何组成部分中含有符号链接,拒绝访问该文件.
if_not_owner
如果文件路径中任何组成部分中含有符号链接,且符号链接和链接目标的所有者不同,拒绝访问该文件.
from=part
当nginx进行符号链接检查时(参数on和参数if_not_owner),路径中所有部分默认都会被检查. 而使用from=part参数可以避免对路径开始部分进行符号链接检查,
而只检查后面的部分路径. 如果某路径不是以指定值开始,整个路径将被检查,就如同没有指定这个参数一样. 如果某路径与指定值完全匹配,将不做检查. 这
个参数的值可以包含变量.

比如： 
disable_symlinks on from=$document_root;
这条指令只在有openat()和fstatat()接口的系统上可用. 当然,现在的FreeBSD、Linux和Solaris都支持这些接口.
参数on和if_not_owner会带来处理开销.
只在那些不支持打开目录查找文件的系统中,使用这些参数需要工作进程有这些被检查目录的读权限.
*/
    { ngx_string("disable_symlinks"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_disable_symlinks,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

#endif

      ngx_null_command
};

/*
任何一个HTTP模块的server相关的配置项都是可能出现在main级别中,而location相关的配置项可能出现在main、srv级别中


main server location配置定义如下:
1.main配置项:只能在http{}内 server外的配置. 例如 http{aaa; server{ location{} }}   aaa为main配置项
2.server配置项:可以在http内,server外配置,也可以在server内配置.  例如 http{bbb; server{bbb; location{} }}   bbb为server配置项
2.server配置项:可以在http内,server外配置,也可以在server内配置,也可以出现在location中.  例如 http{ccc; server{ccc; location{ccc} }}   ccc为location配置项

这样区分main srv local_create的原因是:
例如
http {
    sss;
    xxx; 
    server {
        sss;
        xxx; 
        location {
            xxx;
        } 
    }
},
其中的xxx可以同时出现在http server 和location中,所以在解析到http{}行的时候需要创建main来存储NGX_HTTP_MAIN_CONF配置.
那么为什么还需要调用sev和loc对应的create呢?
因为server类配置可能同时出现在main中,所以需要存储这写配置,所以要创建srv来存储他们,就是上面的sss配置.
同理location类配置可能同时出现在main中,所以需要存储这写配置,所以要创建loc来存储他们,就是上面的sss配置.

在解析到server{}的时候,由于location配置也可能出现在server{}内,就是上面server{}中的xxx;所以解析到server{}的时候
需要调动srv和loc create;

所以最终要决定使用那个sss配置和xxx配置,这就需要把http和server的sss合并, http、server和location中的xxx合并
*/
static ngx_http_module_t  ngx_http_core_module_ctx = {
    ngx_http_core_preconfiguration,        /* preconfiguration */ //在解析http{}内的配置项前回调
    ngx_http_core_postconfiguration,       /* postconfiguration */ //解析完http{}内的所有配置项后回调

    ////解析到http{}行时,在ngx_http_block执行. 该函数创建的结构体成员只能出现在http中,不会出现在server和location中
    ngx_http_core_create_main_conf,        /* create main configuration */
    //http{}的所有项解析完后执行
    ngx_http_core_init_main_conf,          /* init main configuration */ //解析完main配置项后回调

    //解析server{}   local{}项时,会执行
    //创建用于存储可同时出现在main、srv级别配置项的结构体,该结构体中的成员与server配置是相关联的
    ngx_http_core_create_srv_conf,         /* create server configuration */
    /* merge_srv_conf方法可以把出现在main级别中的配置项值合并到srv级别配置项中 */
    ngx_http_core_merge_srv_conf,          /* merge server configuration */

    //解析到http{}  server{}  local{}行时,会执行
    //创建用于存储可同时出现在main、srv、loc级别配置项的结构体,该结构体中的成员与location配置是相关联的
    ngx_http_core_create_loc_conf,         /* create location configuration */
    //把出现在main、srv级别的配置项值合并到loc级别的配置项中
    ngx_http_core_merge_loc_conf           /* merge location configuration */
};

//在解析到http{}行的时候,会根据ngx_http_block来执行ngx_http_core_module_ctx中的相关create来创建存储配置项目的空间
ngx_module_t  ngx_http_core_module = { //http{}内部 和server location都属于这个模块,他们的main_create  srv_create loc_ctreate都是一样的
//http{}相关配置结构创建首先需要执行ngx_http_core_module,而后才能执行对应的http子模块,这里有个顺序关系在里面. 因为
//ngx_http_core_loc_conf_t ngx_http_core_srv_conf_t ngx_http_core_main_conf_t的相关
    NGX_MODULE_V1,
    &ngx_http_core_module_ctx,             /* module context */
    ngx_http_core_commands,                /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


ngx_str_t  ngx_http_core_get_method = { 3, (u_char *) "GET" };

//ngx_http_process_request->ngx_http_handler->ngx_http_core_run_phases
//ngx_http_run_posted_requests->ngx_http_handler
void
ngx_http_handler(ngx_http_request_t *r) /* 执行11个阶段的指定阶段 */
{
    ngx_http_core_main_conf_t  *cmcf;

    r->connection->log->action = NULL;

/*
    检查ngx_http_request_t结构体的internal标志位,如果internal为0,则从头部phase_handler执行;如果internal标志位为1,则表示请求当前需要做内部跳转,
将要把结构体中的phase_handler序号置为server_rewrite_index. 注意ngx_http_phase_engine_t结构体中的handlers动态数组中保存了请求需要经历的所有
回调方法,而server_rewrite_index则是handlers数组中NGX_HTTP_SERVER_REWRITE_PHASE处理阶段的第一个ngx_http_phase_handler_t回调方法所处的位置.
    究竟handlers数组是怎么使用的呢？事实上,它要配合着ngx_http_request_t结构体的phase_handler序号使用,由phase_handler指定着请求将要执行
的handlers数组中的方法位置. 注意,handlers数组中的方法都是由各个HTTP模块实现的,这就是所有HTTP模块能够共同处理请求的原因.
 */
    if (!r->internal) {
        
        switch (r->headers_in.connection_type) {
        case 0:
            r->keepalive = (r->http_version > NGX_HTTP_VERSION_10); //指明在1.0以上版本默认是长连接
            break;

        case NGX_HTTP_CONNECTION_CLOSE:
            r->keepalive = 0;
            break;

        case NGX_HTTP_CONNECTION_KEEP_ALIVE:
            r->keepalive = 1;
            break;
        }
    
        r->lingering_close = (r->headers_in.content_length_n > 0
                              || r->headers_in.chunked); 
        /*
       当internal标志位为0时,表示不需要重定向（如刚开始处理请求时）,将phase_handler序号置为0,意味着从ngx_http_phase_engine_t指定数组
       的第一个回调方法开始执行（了解ngx_http_phase_engine_t是如何将各HTTP模块的回调方法构造成handlers数组的）.
          */
        r->phase_handler = 0;

    } else {
/* 
在这一步骤中,把phase_handler序号设为server_rewrite_index,这意味着无论之前执行到哪一个阶段,马上都要重新从NGX_HTTP_SERVER_REWRITE_PHASE
阶段开始再次执行,这是Nginx的请求可以反复rewrite重定向的基础.
*/
        cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);
        r->phase_handler = cmcf->phase_engine.server_rewrite_index;
    }

    r->valid_location = 1;
#if (NGX_HTTP_GZIP)
    r->gzip_tested = 0;
    r->gzip_ok = 0;
    r->gzip_vary = 0;
#endif

    r->write_event_handler = ngx_http_core_run_phases;
    ngx_http_core_run_phases(r);  
}

/*  
    每个ngx_http_phases阶段对应的checker函数,处于同一个阶段的checker函数相同,见ngx_http_init_phase_handlers
    NGX_HTTP_SERVER_REWRITE_PHASE  -------  ngx_http_core_rewrite_phase
    NGX_HTTP_FIND_CONFIG_PHASE     -------  ngx_http_core_find_config_phase
    NGX_HTTP_REWRITE_PHASE         -------  ngx_http_core_rewrite_phase
    NGX_HTTP_POST_REWRITE_PHASE    -------  ngx_http_core_post_rewrite_phase
    NGX_HTTP_ACCESS_PHASE          -------  ngx_http_core_access_phase
    NGX_HTTP_POST_ACCESS_PHASE     -------  ngx_http_core_post_access_phase
    NGX_HTTP_TRY_FILES_PHASE       -------  NGX_HTTP_TRY_FILES_PHASE
    NGX_HTTP_CONTENT_PHASE         -------  ngx_http_core_content_phase
    其他几个阶段                   -------  ngx_http_core_generic_phase

    HTTP框架为11个阶段实现的checker方法  赋值见ngx_http_init_phase_handlers
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━┓
┃    阶段名称                  ┃    checker方法                   ┃
┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━┓
┃   NGX_HTTP_POST_READ_PHASE   ┃    ngx_http_core_generic_phase   ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP SERVER REWRITE PHASE ┃ngx_http_core_rewrite_phase       ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP FIND CONFIG PHASE    ┃ngx_http_core find config_phase   ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP REWRITE PHASE        ┃ngx_http_core_rewrite_phase       ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP POST REWRITE PHASE   ┃ngx_http_core_post_rewrite_phase  ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP PREACCESS PHASE      ┃ngx_http_core_generic_phase       ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP ACCESS PHASE         ┃ngx_http_core_access_phase        ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP POST ACCESS PHASE    ┃ngx_http_core_post_access_phase   ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP TRY FILES PHASE      ┃ngx_http_core_try_files_phase     ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP CONTENT PHASE        ┃ngx_http_core_content_phase       ┃
┣━━━━━━━━━━━━━━━╋━━━━━━━━━━━━━━━━━┫
┃NGX HTTP LOG PHASE            ┃ngx_http_core_generic_phase       ┃
┗━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━┛
*/
/*
通常来说,在接收完HTTP头部后,是无法在一次Nginx框架的调度中处理完一个请求的. 在第一次接收完HTTP头部后,HTTP框架将调度
ngx_http_process_request方法开始处理请求,如果某个checker方法返回了NGX_OK,则将会把控制权交还给Nginx框架. 当这个请求
上对应的事件再次触发时,HTTP框架将不会再调度ngx_http_process_request方法处理请求,而是由ngx_http_request_handler方法
开始处理请求. 例如recv虽然把头部行内容读取完毕,并能解析完成,但是可能有携带请求内容,内容可能没有读完
*/
//通过执行当前r->phase_handler所指向的阶段的checker函数
//ngx_http_process_request->ngx_http_handler->ngx_http_core_run_phases
void
ngx_http_core_run_phases(ngx_http_request_t *r) //执行该请求对于的阶段的checker(),并获取返回值
{
    ngx_int_t                   rc;
    ngx_http_phase_handler_t   *ph;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    ph = cmcf->phase_engine.handlers;

    while (ph[r->phase_handler].checker) { //处于同一ngx_http_phases阶段的所有ngx_http_phase_handler_t的checker指向相同的函数,见ngx_http_init_phase_handlers
/*
handler方法其实仅能在checker方法中被调用,而且checker方法由HTTP框架实现,所以可以控制各HTTP模块实现的处理方法在不同的阶段中采用不同的调用行为

ngx_http_request_t结构体中的phase_handler成员将决定执行到哪一阶段,以及下一阶段应当执行哪个HTTP模块实现的内容. 可以看到请求的phase_handler成员
会被重置,而HTTP框架实现的checker穷法也会修改phase_handler成员的值

当checker方法的返回值非NGX_OK时,意味着向下执行phase_engine中的各处理方法;反之,当任何一个checker方法返回NGX_OK时,意味着把控制权交还
给Nginx的事件模块,由它根据事件（网络事件、定时器事件、异步I/O事件等）再次调度请求. 然而,一个请求多半需要Nginx事件模块多次地调度HTTP模
块处理,也就是在该函数外设置的读/写事件的回调方法ngx_http_request_handler
*/
        
        rc = ph[r->phase_handler].checker(r, &ph[r->phase_handler]);

 /* 直接返回NGX OK会使待HTTP框架立刻把控制权交还给epoll事件框架,不再处理当前请求,唯有这个请求上的事件再次被触发才会继续执行. */
        if (rc == NGX_OK) { //执行phase_handler阶段的hecker  handler方法后,返回值为NGX_OK,则直接退出,否则继续循环执行checker handler
            return;
        }
    }
}


/*
//NGX_HTTP_POST_READ_PHASE   NGX_HTTP_PREACCESS_PHASE  NGX_HTTP_LOG_PHASE默认都是该函数盼段下HTTP模块的ngx_http_handler_pt方法返回值意义
┏━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    返回值    ┃    意义                                                                          ┃
┣━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃              ┃  执行下一个ngx_http_phases阶段中的第一个ngx_http_handler_pt处理方法. 这意味着两  ┃
┃              ┃点：①即使当前阶段中后续还有一曲HTTP模块设置了ngx_http_handler_pt处理方法,返回   ┃
┃NGX_OK        ┃                                                                                  ┃
┃              ┃NGX_OK之后它们也是得不到执行机会的;②如果下一个ngx_http_phases阶段中没有任何     ┃
┃              ┃HTTP模块设置了ngx_http_handler_pt处理方法,将再次寻找之后的阶段,如此循环下去     ┃
┣━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX_DECLINED  ┃  按照顺序执行下一个ngx_http_handler_pt处理方法. 这个顺序就是ngx_http_phase_      ┃
┃              ┃engine_t中所有ngx_http_phase_handler_t结构体组成的数组的顺序                      ┃
┣━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX_AGAIN     ┃  当前的ngx_http_handler_pt处理方法尚未结束,这意味着该处理方法在当前阶段有机会   ┃
┃              ┃再次被调用. 这时一般会把控制权交还给事件模块,当下次可写事件发生时会再次执行到该  ┃
┣━━━━━━━┫                                                                                  ┃
┃NGX_DONE      ┃ngx_http_handler_pt处理方法                                                       ┃
┣━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃ NGX_ERROR    ┃                                                                                  ┃
┃              ┃  需要调用ngx_http_finalize_request结束请求                                       ┃
┣━━━━━━━┫                                                                                  ┃
┃其他          ┃                                                                                  ┃
┗━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
*/ 
/* 
有3个HTTP阶段都使用了ngx_http_core_generic_phase作为它们的checker方法,这意味着任何试图在NGX_HTTP_POST_READ_PHASE   NGX_HTTP_PREACCESS_PHASE
NGX_HTTP_LOG_PHASE这3个阶段处理请求的HTTP模块都需要了解ngx_http_core_generic_phase方法 
*/ //所有阶段的checker在ngx_http_core_run_phases中调用
//NGX_HTTP_POST_READ_PHASE   NGX_HTTP_PREACCESS_PHASE  NGX_HTTP_LOG_PHASE默认都是该函数
//当HTTP框架在建立的TCP连接上接收到客户发送的完整HTTP请求头部时,开始执行NGX_HTTP_POST_READ_PHASE阶段的checker方法
ngx_int_t
ngx_http_core_generic_phase(ngx_http_request_t *r, ngx_http_phase_handler_t *ph)
{
    ngx_int_t  rc;

    /*
     * generic phase checker,
     * used by the post read and pre-access phases
     */

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "generic phase: %ui", r->phase_handler);

    rc = ph->handler(r); //调用这一阶段中各HTTP模块添加的handler处理方法

    if (rc == NGX_OK) {//如果handler方法返回NGX OK,之后将进入下一个阶段处理,而不佥理会当前阶段中是否还有其他的处理方法
        r->phase_handler = ph->next; //直接指向下一个处理阶段的第一个方法
        return NGX_AGAIN;
    }

//如果handler方法返回NGX_DECLINED,那么将进入下一个处理方法,这个处理方法既可能属于当前阶段,也可能属于下一个阶段. 注意返回
//NGX_OK与NGX_DECLINED之间的区别
    if (rc == NGX_DECLINED) {
        r->phase_handler++;//紧接着的下一个处理方法
        return NGX_AGAIN;
    }
/*
如果handler方法返回NGX_AGAIN或者NGX_DONE,则意味着刚才的handler方法无法在这一次调度中处理完这一个阶段,它需要多次调度才能完成,
也就是说,刚刚执行过的handler方法希望：如果请求对应的事件再次被触发时,将由ngx_http_request_handler通过ngx_http_core_ run_phases再次
调用这个handler方法. 直接返回NGX_OK会使待HTTP框架立刻把控制权交还给epoll事件框架,不再处理当前请求,唯有这个请求上的事件再次被触发才会继续执行.
*/
//如果handler方法返回NGX_AGAIN或者NGX_DONE,那么当前请求将仍然停留在这一个处理阶段中
    if (rc == NGX_AGAIN || rc == NGX_DONE) { //phase_handler没有发生变化,当这个请求上的事件再次触发的时候继续在该阶段执行
        return NGX_OK;
    }

    /* rc == NGX_ERROR || rc == NGX_HTTP_...  */
    //如果handler方法返回NGX_ERROR或者类似NGX_HTTP开头的返回码,则调用ngx_http_finalize_request结束请求
    ngx_http_finalize_request(r, rc);

    return NGX_OK;
}

/*
NGX_HTTP_SERVER_REWRITE_PHASE  NGX_HTTP_REWRITE_PHASE阶段的checker方法是ngx_http_core_rewrite_phase. 表10-2总结了该阶段
下ngx_http_handler_pt处理方法的返回值是如何影响HTTP框架执行的,注意,这个阶段中不存在返回值可以使请求直接跳到下一个阶段执行.
NGX_HTTP_REWRITE_PHASE  NGX_HTTP_POST_REWRITE_PHASE阶段HTTP模块的ngx_http_handler_pt方法返回值意义
┏━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
┃    返回值    ┃    意义                                                                            ┃
┣━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃              ┃  当前的ngx_http_handler_pt处理方法尚未结束,这意味着该处理方法在当前阶段中有机会   ┃
┃NGX DONE      ┃                                                                                    ┃
┃              ┃再次被调用                                                                          ┃
┣━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃              ┃  当前ngx_http_handler_pt处理方法执行完毕,按照顺序执行下一个ngx_http_handler_pt处  ┃
┃NGX DECLINED  ┃                                                                                    ┃
┃              ┃理方法                                                                              ┃
┣━━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
┃NGX AGAIN     ┃                                                                                    ┃
┣━━━━━━━┫                                                                                    ┃
┃NGX DONE      ┃                                                                                    ┃
┃              ┃  需要调用ngx_http_finalize_request结束请求                                         ┃
┣━━━━━━━┫                                                                                    ┃
┃ NGX ERROR    ┃                                                                                    ┃
┣━━━━━━━┫                                                                                    ┃
┃其他          ┃                                                                                    ┃
┗━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

*/ //所有阶段的checker在ngx_http_core_run_phases中调用
ngx_int_t
ngx_http_core_rewrite_phase(ngx_http_request_t *r, ngx_http_phase_handler_t *ph)
{
    ngx_int_t  rc;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "rewrite phase: %ui", r->phase_handler);

    rc = ph->handler(r);//ngx_http_rewrite_handler

/* 将phase_handler加1表示将要执行下一个回调方法. 注意,此时返回的是NGX AGAIN,HTTP框架不会把进程控制权交还给epoll事件框架,而
是继续立刻执行请求的下一个回调方法.  */
    if (rc == NGX_DECLINED) {
        r->phase_handler++;
        return NGX_AGAIN;
    }

    /*
     如果handler方法返回NGX_DONE,则意味着刚才的handler方法无法在这一次调度中处理完这一个阶段,它需要多次的调度才能完成. 注意,此
     时返回NGX_OK,它会使得HTTP框架立刻把控制权交还给epoll等事件模块,不再处理当前请求,唯有这个请求上的事件再次被触发时才会继续执行.
     */
    if (rc == NGX_DONE) { //phase_handler没有发生变化,因此如果该请求的事件再次触发,还会接着上次的handler执行
        return NGX_OK;
    }

    /*
    为什么该checker执行handler没有NGX_DECLINED(r- >phase_handler  =  ph- >next) ?????
    答:ngx_http_core_rewrite_phase方法与ngx_http_core_generic_phase方法有一个显著的不同点：前者永远不会导致跨过同
一个HTTP阶段的其他处理方法,就直接跳到下一个阶段来处理请求. 原因其实很简单,可能有许多HTTP模块在NGX_HTTP_REWRITE_PHASE和
NGX_HTTP_POST_REWRITE_PHASE阶段同时处理重写URL这样的业务,HTTP框架认为这两个盼段的HTTP模块是完全平等的,序号靠前的HTTP模块优先
级并不会更高,它不能决定序号靠后的HTTP模块是否可以再次重写URL. 因此,ngx_http_core_rewrite_phase方法绝对不会把phase_handler直接
设置到下一个阶段处理方法的流程中,即不可能存在类似下面的代码: r- >phase_handler  =  ph- >next ;
     */
    

    /* NGX_OK, NGX_AGAIN, NGX_ERROR, NGX_HTTP_...  */

    ngx_http_finalize_request(r, rc);

    return NGX_OK;
}


/*
NGXHTTP—FIND—CONFIG—PHASE阶段上不能挂载任何回调函数,因为它们永远也不会被执行,该阶段完成的是Nginx的特定任务,即进行Location定位
*/
//所有阶段的checker在ngx_http_core_run_phases中调用
ngx_int_t
ngx_http_core_find_config_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{
    u_char                    *p;
    size_t                     len;
    ngx_int_t                  rc;
    ngx_http_core_loc_conf_t  *clcf;
    
    
    r->content_handler = NULL; //首先初始化content_handler,这个会在ngx_http_core_content_phase里面使用
    r->uri_changed = 0;


    rc = ngx_http_core_find_location(r);//解析完HTTP{}块后,ngx_http_init_static_location_trees函数会创建一颗三叉树,以加速配置查找.
	//找到所属的location,并且loc_conf也已经更新了r->loc_conf了.

    if (rc == NGX_ERROR) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_OK;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);//用刚找到的loc_conf,得到其http_core模块的位置配置.

    /* 该location{}必须是内部重定向(index重定向 、error_pages等重定向调用ngx_http_internal_redirect)后匹配的location{},否则不让访问该location */
    if (!r->internal && clcf->internal) { //是否是i在内部重定向,如果是,中断吗
        ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
        return NGX_OK;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "using configuration \"%s%V\"",
                   (clcf->noname ? "*" : (clcf->exact_match ? "=" : "")),
                   &clcf->name);

    ngx_http_update_location_config(r);

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http cl:%O max:%O",
                   r->headers_in.content_length_n, clcf->client_max_body_size);

    if (r->headers_in.content_length_n != -1
        && !r->discard_body
        && clcf->client_max_body_size
        && clcf->client_max_body_size < r->headers_in.content_length_n)
    {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "client intended to send too large body: %O bytes",
                      r->headers_in.content_length_n);

        r->expect_tested = 1;
        (void) ngx_http_discard_request_body(r);
        ngx_http_finalize_request(r, NGX_HTTP_REQUEST_ENTITY_TOO_LARGE);
        return NGX_OK;
    }

    if (rc == NGX_DONE) {
        ngx_http_clear_location(r);

        r->headers_out.location = ngx_list_push(&r->headers_out.headers);
        if (r->headers_out.location == NULL) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return NGX_OK;
        }

        r->headers_out.location->hash = 1;
        ngx_str_set(&r->headers_out.location->key, "Location");

        if (r->args.len == 0) {
            r->headers_out.location->value = clcf->name;

        } else {
            len = clcf->name.len + 1 + r->args.len;
            p = ngx_pnalloc(r->pool, len);

            if (p == NULL) {
                ngx_http_clear_location(r);
                ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
                return NGX_OK;
            }

            r->headers_out.location->value.len = len;
            r->headers_out.location->value.data = p;

            p = ngx_cpymem(p, clcf->name.data, clcf->name.len);
            *p++ = '?';
            ngx_memcpy(p, r->args.data, r->args.len);
        }

        ngx_http_finalize_request(r, NGX_HTTP_MOVED_PERMANENTLY);
        return NGX_OK;
    }

    r->phase_handler++;
    return NGX_AGAIN;
}


ngx_int_t
ngx_http_core_post_rewrite_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{
    ngx_http_core_srv_conf_t  *cscf;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post rewrite phase: %ui", r->phase_handler);

    if (!r->uri_changed) {
        r->phase_handler++;
        return NGX_AGAIN;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "uri changes: %d", r->uri_changes);

    /*
     * gcc before 3.3 compiles the broken code for
     *     if (r->uri_changes-- == 0)
     * if the r->uri_changes is defined as
     *     unsigned  uri_changes:4
     */

    r->uri_changes--;

    if (r->uri_changes == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "rewrite or internal redirection cycle "
                      "while processing \"%V\"", &r->uri);

        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_OK;
    }

    r->phase_handler = ph->next;

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    r->loc_conf = cscf->ctx->loc_conf;

    return NGX_AGAIN;
}


ngx_int_t
ngx_http_core_access_phase(ngx_http_request_t *r, ngx_http_phase_handler_t *ph)
{
    ngx_int_t                  rc;
    ngx_http_core_loc_conf_t  *clcf;

    if (r != r->main) {
        r->phase_handler = ph->next;
        return NGX_AGAIN;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "access phase: %ui", r->phase_handler);

    rc = ph->handler(r);

    if (rc == NGX_DECLINED) {
        r->phase_handler++;
        return NGX_AGAIN;
    }

    if (rc == NGX_AGAIN || rc == NGX_DONE) {
        return NGX_OK;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (clcf->satisfy == NGX_HTTP_SATISFY_ALL) {

        if (rc == NGX_OK) {
            r->phase_handler++;
            return NGX_AGAIN;
        }

    } else {
        if (rc == NGX_OK) {
            r->access_code = 0;

            if (r->headers_out.www_authenticate) {
                r->headers_out.www_authenticate->hash = 0;
            }

            r->phase_handler = ph->next;
            return NGX_AGAIN;
        }

        if (rc == NGX_HTTP_FORBIDDEN || rc == NGX_HTTP_UNAUTHORIZED) {
            if (r->access_code != NGX_HTTP_UNAUTHORIZED) {
                r->access_code = rc;
            }

            r->phase_handler++;
            return NGX_AGAIN;
        }
    }

    /* rc == NGX_ERROR || rc == NGX_HTTP_...  */

    if (rc == NGX_HTTP_UNAUTHORIZED) {
        return ngx_http_core_auth_delay(r);
    }

    ngx_http_finalize_request(r, rc);
    return NGX_OK;
}


ngx_int_t
ngx_http_core_post_access_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{
    ngx_int_t  access_code;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "post access phase: %ui", r->phase_handler);

    access_code = r->access_code;

    if (access_code) {
        r->access_code = 0;

        if (access_code == NGX_HTTP_FORBIDDEN) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "access forbidden by rule");
        }

        if (access_code == NGX_HTTP_UNAUTHORIZED) {
            return ngx_http_core_auth_delay(r);
        }

        ngx_http_finalize_request(r, access_code);
        return NGX_OK;
    }

    r->phase_handler++;
    return NGX_AGAIN;
}


static ngx_int_t
ngx_http_core_auth_delay(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (clcf->auth_delay == 0) {
        ngx_http_finalize_request(r, NGX_HTTP_UNAUTHORIZED);
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                  "delaying unauthorized request");

    if (ngx_handle_read_event(r->connection->read, 0) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    r->read_event_handler = ngx_http_test_reading;
    r->write_event_handler = ngx_http_core_auth_delay_handler;

    r->connection->write->delayed = 1;
    ngx_add_timer(r->connection->write, clcf->auth_delay);

    /*
     * trigger an additional event loop iteration
     * to ensure constant-time processing
     */

    ngx_post_event(r->connection->write, &ngx_posted_next_events);

    return NGX_OK;
}


static void
ngx_http_core_auth_delay_handler(ngx_http_request_t *r)
{
    ngx_event_t  *wev;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "auth delay handler");

    wev = r->connection->write;

    if (wev->delayed) {

        if (ngx_handle_write_event(wev, 0) != NGX_OK) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        }

        return;
    }

    ngx_http_finalize_request(r, NGX_HTTP_UNAUTHORIZED);
}


ngx_int_t
ngx_http_core_content_phase(ngx_http_request_t *r,
    ngx_http_phase_handler_t *ph)
{
    size_t     root;
    ngx_int_t  rc;
    ngx_str_t  path;

    if (r->content_handler) {
        r->write_event_handler = ngx_http_request_empty_handler;
        ngx_http_finalize_request(r, r->content_handler(r));
        return NGX_OK;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "content phase: %ui", r->phase_handler);

    rc = ph->handler(r);

    if (rc != NGX_DECLINED) {
        ngx_http_finalize_request(r, rc);
        return NGX_OK;
    }

    /* rc == NGX_DECLINED */

    ph++;

    if (ph->checker) {
        r->phase_handler++;
        return NGX_AGAIN;
    }

    /* no content handler was found */

    if (r->uri.data[r->uri.len - 1] == '/') {

        if (ngx_http_map_uri_to_path(r, &path, &root, 0) != NULL) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "directory index of \"%s\" is forbidden", path.data);
        }

        ngx_http_finalize_request(r, NGX_HTTP_FORBIDDEN);
        return NGX_OK;
    }

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "no handler found");

    ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
    return NGX_OK;
}


void
ngx_http_update_location_config(ngx_http_request_t *r)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (r->method & clcf->limit_except) {
        r->loc_conf = clcf->limit_except_loc_conf;
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
    }

    if (r == r->main) {
        ngx_set_connection_log(r->connection, clcf->error_log);
    }

    if ((ngx_io.flags & NGX_IO_SENDFILE) && clcf->sendfile) {
        r->connection->sendfile = 1;

    } else {
        r->connection->sendfile = 0;
    }

    if (clcf->client_body_in_file_only) {
        r->request_body_in_file_only = 1;
        r->request_body_in_persistent_file = 1;
        r->request_body_in_clean_file =
            clcf->client_body_in_file_only == NGX_HTTP_REQUEST_BODY_FILE_CLEAN;
        r->request_body_file_log_level = NGX_LOG_NOTICE;

    } else {
        r->request_body_file_log_level = NGX_LOG_WARN;
    }

    r->request_body_in_single_buf = clcf->client_body_in_single_buffer;

    if (r->keepalive) {
        if (clcf->keepalive_timeout == 0) {
            r->keepalive = 0;

        } else if (r->connection->requests >= clcf->keepalive_requests) {
            r->keepalive = 0;

        } else if (r->headers_in.msie6
                   && r->method == NGX_HTTP_POST
                   && (clcf->keepalive_disable
                       & NGX_HTTP_KEEPALIVE_DISABLE_MSIE6))
        {
            /*
             * MSIE may wait for some time if an response for
             * a POST request was sent over a keepalive connection
             */
            r->keepalive = 0;

        } else if (r->headers_in.safari
                   && (clcf->keepalive_disable
                       & NGX_HTTP_KEEPALIVE_DISABLE_SAFARI))
        {
            /*
             * Safari may send a POST request to a closed keepalive
             * connection and may stall for some time, see
             *     https://bugs.webkit.org/show_bug.cgi?id=5760
             */
            r->keepalive = 0;
        }
    }

    if (!clcf->tcp_nopush) {
        /* disable TCP_NOPUSH/TCP_CORK use */
        r->connection->tcp_nopush = NGX_TCP_NOPUSH_DISABLED;
    }

    if (clcf->handler) {
        r->content_handler = clcf->handler;
    }
}


/*
 * NGX_OK       - exact or regex match
 * NGX_DONE     - auto redirect
 * NGX_AGAIN    - inclusive match
 * NGX_ERROR    - regex error
 * NGX_DECLINED - no match
 */

static ngx_int_t
ngx_http_core_find_location(ngx_http_request_t *r)
{
    ngx_int_t                  rc;
    ngx_http_core_loc_conf_t  *pclcf;
#if (NGX_PCRE)
    ngx_int_t                  n;
    ngx_uint_t                 noregex;
    ngx_http_core_loc_conf_t  *clcf, **clcfp;

    noregex = 0;
#endif

    pclcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    rc = ngx_http_core_find_static_location(r, pclcf->static_locations);

    if (rc == NGX_AGAIN) {

#if (NGX_PCRE)
        clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

        noregex = clcf->noregex;
#endif

        /* look up nested locations */

        rc = ngx_http_core_find_location(r);
    }

    if (rc == NGX_OK || rc == NGX_DONE) {
        return rc;
    }

    /* rc == NGX_DECLINED or rc == NGX_AGAIN in nested location */

#if (NGX_PCRE)

    if (noregex == 0 && pclcf->regex_locations) {

        for (clcfp = pclcf->regex_locations; *clcfp; clcfp++) {

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "test location: ~ \"%V\"", &(*clcfp)->name);

            n = ngx_http_regex_exec(r, (*clcfp)->regex, &r->uri);

            if (n == NGX_OK) {
                r->loc_conf = (*clcfp)->loc_conf;

                /* look up nested locations */

                rc = ngx_http_core_find_location(r);

                return (rc == NGX_ERROR) ? rc : NGX_OK;
            }

            if (n == NGX_DECLINED) {
                continue;
            }

            return NGX_ERROR;
        }
    }
#endif

    return rc;
}


/*
 * NGX_OK       - exact match
 * NGX_DONE     - auto redirect
 * NGX_AGAIN    - inclusive match
 * NGX_DECLINED - no match
 */

static ngx_int_t
ngx_http_core_find_static_location(ngx_http_request_t *r,
    ngx_http_location_tree_node_t *node)
{
    u_char     *uri;
    size_t      len, n;
    ngx_int_t   rc, rv;

    len = r->uri.len;
    uri = r->uri.data;

    rv = NGX_DECLINED;

    for ( ;; ) {

        if (node == NULL) {
            return rv;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "test location: \"%*s\"",
                       (size_t) node->len, node->name);

        n = (len <= (size_t) node->len) ? len : node->len;

        rc = ngx_filename_cmp(uri, node->name, n);

        if (rc != 0) {
            node = (rc < 0) ? node->left : node->right;

            continue;
        }

        if (len > (size_t) node->len) {

            if (node->inclusive) {

                r->loc_conf = node->inclusive->loc_conf;
                rv = NGX_AGAIN;

                node = node->tree;
                uri += n;
                len -= n;

                continue;
            }

            /* exact only */

            node = node->right;

            continue;
        }

        if (len == (size_t) node->len) {

            if (node->exact) {
                r->loc_conf = node->exact->loc_conf;
                return NGX_OK;

            } else {
                r->loc_conf = node->inclusive->loc_conf;
                return NGX_AGAIN;
            }
        }

        /* len < node->len */

        if (len + 1 == (size_t) node->len && node->auto_redirect) {

            r->loc_conf = (node->exact) ? node->exact->loc_conf:
                                          node->inclusive->loc_conf;
            rv = NGX_DONE;
        }

        node = node->left;
    }
}


void *
ngx_http_test_content_type(ngx_http_request_t *r, ngx_hash_t *types_hash)
{
    u_char      c, *lowcase;
    size_t      len;
    ngx_uint_t  i, hash;

    if (types_hash->size == 0) {
        return (void *) 4;
    }

    if (r->headers_out.content_type.len == 0) {
        return NULL;
    }

    len = r->headers_out.content_type_len;

    if (r->headers_out.content_type_lowcase == NULL) {

        lowcase = ngx_pnalloc(r->pool, len);
        if (lowcase == NULL) {
            return NULL;
        }

        r->headers_out.content_type_lowcase = lowcase;

        hash = 0;

        for (i = 0; i < len; i++) {
            c = ngx_tolower(r->headers_out.content_type.data[i]);
            hash = ngx_hash(hash, c);
            lowcase[i] = c;
        }

        r->headers_out.content_type_hash = hash;
    }

    return ngx_hash_find(types_hash, r->headers_out.content_type_hash,
                         r->headers_out.content_type_lowcase, len);
}


ngx_int_t
ngx_http_set_content_type(ngx_http_request_t *r)
{
    u_char                     c, *exten;
    ngx_str_t                 *type;
    ngx_uint_t                 i, hash;
    ngx_http_core_loc_conf_t  *clcf;

    if (r->headers_out.content_type.len) {
        return NGX_OK;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (r->exten.len) {

        hash = 0;

        for (i = 0; i < r->exten.len; i++) {
            c = r->exten.data[i];

            if (c >= 'A' && c <= 'Z') {

                exten = ngx_pnalloc(r->pool, r->exten.len);
                if (exten == NULL) {
                    return NGX_ERROR;
                }

                hash = ngx_hash_strlow(exten, r->exten.data, r->exten.len);

                r->exten.data = exten;

                break;
            }

            hash = ngx_hash(hash, c);
        }

        type = ngx_hash_find(&clcf->types_hash, hash,
                             r->exten.data, r->exten.len);

        if (type) {
            r->headers_out.content_type_len = type->len;
            r->headers_out.content_type = *type;

            return NGX_OK;
        }
    }

    r->headers_out.content_type_len = clcf->default_type.len;
    r->headers_out.content_type = clcf->default_type;

    return NGX_OK;
}


void
ngx_http_set_exten(ngx_http_request_t *r)
{
    ngx_int_t  i;

    ngx_str_null(&r->exten);

    for (i = r->uri.len - 1; i > 1; i--) {
        if (r->uri.data[i] == '.' && r->uri.data[i - 1] != '/') {

            r->exten.len = r->uri.len - i - 1;
            r->exten.data = &r->uri.data[i + 1];

            return;

        } else if (r->uri.data[i] == '/') {
            return;
        }
    }

    return;
}


ngx_int_t
ngx_http_set_etag(ngx_http_request_t *r)
{
    ngx_table_elt_t           *etag;
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (!clcf->etag) {
        return NGX_OK;
    }

    etag = ngx_list_push(&r->headers_out.headers);
    if (etag == NULL) {
        return NGX_ERROR;
    }

    etag->hash = 1;
    ngx_str_set(&etag->key, "ETag");

    etag->value.data = ngx_pnalloc(r->pool, NGX_OFF_T_LEN + NGX_TIME_T_LEN + 3);
    if (etag->value.data == NULL) {
        etag->hash = 0;
        return NGX_ERROR;
    }

    etag->value.len = ngx_sprintf(etag->value.data, "\"%xT-%xO\"",
                                  r->headers_out.last_modified_time,
                                  r->headers_out.content_length_n)
                      - etag->value.data;

    r->headers_out.etag = etag;

    return NGX_OK;
}


void
ngx_http_weak_etag(ngx_http_request_t *r)
{
    size_t            len;
    u_char           *p;
    ngx_table_elt_t  *etag;

    etag = r->headers_out.etag;

    if (etag == NULL) {
        return;
    }

    if (etag->value.len > 2
        && etag->value.data[0] == 'W'
        && etag->value.data[1] == '/')
    {
        return;
    }

    if (etag->value.len < 1 || etag->value.data[0] != '"') {
        r->headers_out.etag->hash = 0;
        r->headers_out.etag = NULL;
        return;
    }

    p = ngx_pnalloc(r->pool, etag->value.len + 2);
    if (p == NULL) {
        r->headers_out.etag->hash = 0;
        r->headers_out.etag = NULL;
        return;
    }

    len = ngx_sprintf(p, "W/%V", &etag->value) - p;

    etag->value.data = p;
    etag->value.len = len;
}


ngx_int_t
ngx_http_send_response(ngx_http_request_t *r, ngx_uint_t status,
    ngx_str_t *ct, ngx_http_complex_value_t *cv)
{
    ngx_int_t     rc;
    ngx_str_t     val;
    ngx_buf_t    *b;
    ngx_chain_t   out;

    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }

    r->headers_out.status = status;

    if (ngx_http_complex_value(r, cv, &val) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (status == NGX_HTTP_MOVED_PERMANENTLY
        || status == NGX_HTTP_MOVED_TEMPORARILY
        || status == NGX_HTTP_SEE_OTHER
        || status == NGX_HTTP_TEMPORARY_REDIRECT
        || status == NGX_HTTP_PERMANENT_REDIRECT)
    {
        ngx_http_clear_location(r);

        r->headers_out.location = ngx_list_push(&r->headers_out.headers);
        if (r->headers_out.location == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        r->headers_out.location->hash = 1;
        ngx_str_set(&r->headers_out.location->key, "Location");
        r->headers_out.location->value = val;

        return status;
    }

    r->headers_out.content_length_n = val.len;

    if (ct) {
        r->headers_out.content_type_len = ct->len;
        r->headers_out.content_type = *ct;

    } else {
        if (ngx_http_set_content_type(r) != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    if (r->method == NGX_HTTP_HEAD || (r != r->main && val.len == 0)) {
        return ngx_http_send_header(r);
    }

    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b->pos = val.data;
    b->last = val.data + val.len;
    b->memory = val.len ? 1 : 0;
    b->last_buf = (r == r->main) ? 1 : 0;
    b->last_in_chain = 1;

    out.buf = b;
    out.next = NULL;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}


ngx_int_t
ngx_http_send_header(ngx_http_request_t *r)
{
    if (r->post_action) {
        return NGX_OK;
    }

    if (r->header_sent) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "header already sent");
        return NGX_ERROR;
    }

    if (r->err_status) {
        r->headers_out.status = r->err_status;
        r->headers_out.status_line.len = 0;
    }

    return ngx_http_top_header_filter(r);
}


ngx_int_t
ngx_http_output_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_int_t          rc;
    ngx_connection_t  *c;

    c = r->connection;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http output filter \"%V?%V\"", &r->uri, &r->args);

    rc = ngx_http_top_body_filter(r, in);

    if (rc == NGX_ERROR) {
        /* NGX_ERROR may be returned by any filter */
        c->error = 1;
    }

    return rc;
}


u_char *
ngx_http_map_uri_to_path(ngx_http_request_t *r, ngx_str_t *path,
    size_t *root_length, size_t reserved)
{
    u_char                    *last;
    size_t                     alias;
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    alias = clcf->alias;

    if (alias && !r->valid_location) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
                      "\"alias\" cannot be used in location \"%V\" "
                      "where URI was rewritten", &clcf->name);
        return NULL;
    }

    if (clcf->root_lengths == NULL) {

        *root_length = clcf->root.len;

        path->len = clcf->root.len + reserved + r->uri.len - alias + 1;

        path->data = ngx_pnalloc(r->pool, path->len);
        if (path->data == NULL) {
            return NULL;
        }

        last = ngx_copy(path->data, clcf->root.data, clcf->root.len);

    } else {

        if (alias == NGX_MAX_SIZE_T_VALUE) {
            reserved += r->add_uri_to_alias ? r->uri.len + 1 : 1;

        } else {
            reserved += r->uri.len - alias + 1;
        }

        if (ngx_http_script_run(r, path, clcf->root_lengths->elts, reserved,
                                clcf->root_values->elts)
            == NULL)
        {
            return NULL;
        }

        if (ngx_get_full_name(r->pool, (ngx_str_t *) &ngx_cycle->prefix, path)
            != NGX_OK)
        {
            return NULL;
        }

        *root_length = path->len - reserved;
        last = path->data + *root_length;

        if (alias == NGX_MAX_SIZE_T_VALUE) {
            if (!r->add_uri_to_alias) {
                *last = '\0';
                return last;
            }

            alias = 0;
        }
    }

    last = ngx_copy(last, r->uri.data + alias, r->uri.len - alias);
    *last = '\0';

    return last;
}


ngx_int_t
ngx_http_auth_basic_user(ngx_http_request_t *r)
{
    ngx_str_t   auth, encoded;
    ngx_uint_t  len;

    if (r->headers_in.user.len == 0 && r->headers_in.user.data != NULL) {
        return NGX_DECLINED;
    }

    if (r->headers_in.authorization == NULL) {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    encoded = r->headers_in.authorization->value;

    if (encoded.len < sizeof("Basic ") - 1
        || ngx_strncasecmp(encoded.data, (u_char *) "Basic ",
                           sizeof("Basic ") - 1)
           != 0)
    {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    encoded.len -= sizeof("Basic ") - 1;
    encoded.data += sizeof("Basic ") - 1;

    while (encoded.len && encoded.data[0] == ' ') {
        encoded.len--;
        encoded.data++;
    }

    if (encoded.len == 0) {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    auth.len = ngx_base64_decoded_length(encoded.len);
    auth.data = ngx_pnalloc(r->pool, auth.len + 1);
    if (auth.data == NULL) {
        return NGX_ERROR;
    }

    if (ngx_decode_base64(&auth, &encoded) != NGX_OK) {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    auth.data[auth.len] = '\0';

    for (len = 0; len < auth.len; len++) {
        if (auth.data[len] == ':') {
            break;
        }
    }

    if (len == 0 || len == auth.len) {
        r->headers_in.user.data = (u_char *) "";
        return NGX_DECLINED;
    }

    r->headers_in.user.len = len;
    r->headers_in.user.data = auth.data;
    r->headers_in.passwd.len = auth.len - len - 1;
    r->headers_in.passwd.data = &auth.data[len + 1];

    return NGX_OK;
}


#if (NGX_HTTP_GZIP)

ngx_int_t
ngx_http_gzip_ok(ngx_http_request_t *r)
{
    time_t                     date, expires;
    ngx_uint_t                 p;
    ngx_array_t               *cc;
    ngx_table_elt_t           *e, *d, *ae;
    ngx_http_core_loc_conf_t  *clcf;

    r->gzip_tested = 1;

    if (r != r->main) {
        return NGX_DECLINED;
    }

    ae = r->headers_in.accept_encoding;
    if (ae == NULL) {
        return NGX_DECLINED;
    }

    if (ae->value.len < sizeof("gzip") - 1) {
        return NGX_DECLINED;
    }

    /*
     * test first for the most common case "gzip,...":
     *   MSIE:    "gzip, deflate"
     *   Firefox: "gzip,deflate"
     *   Chrome:  "gzip,deflate,sdch"
     *   Safari:  "gzip, deflate"
     *   Opera:   "gzip, deflate"
     */

    if (ngx_memcmp(ae->value.data, "gzip,", 5) != 0
        && ngx_http_gzip_accept_encoding(&ae->value) != NGX_OK)
    {
        return NGX_DECLINED;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    if (r->headers_in.msie6 && clcf->gzip_disable_msie6) {
        return NGX_DECLINED;
    }

    if (r->http_version < clcf->gzip_http_version) {
        return NGX_DECLINED;
    }

    if (r->headers_in.via == NULL) {
        goto ok;
    }

    p = clcf->gzip_proxied;

    if (p & NGX_HTTP_GZIP_PROXIED_OFF) {
        return NGX_DECLINED;
    }

    if (p & NGX_HTTP_GZIP_PROXIED_ANY) {
        goto ok;
    }

    if (r->headers_in.authorization && (p & NGX_HTTP_GZIP_PROXIED_AUTH)) {
        goto ok;
    }

    e = r->headers_out.expires;

    if (e) {

        if (!(p & NGX_HTTP_GZIP_PROXIED_EXPIRED)) {
            return NGX_DECLINED;
        }

        expires = ngx_parse_http_time(e->value.data, e->value.len);
        if (expires == NGX_ERROR) {
            return NGX_DECLINED;
        }

        d = r->headers_out.date;

        if (d) {
            date = ngx_parse_http_time(d->value.data, d->value.len);
            if (date == NGX_ERROR) {
                return NGX_DECLINED;
            }

        } else {
            date = ngx_time();
        }

        if (expires < date) {
            goto ok;
        }

        return NGX_DECLINED;
    }

    cc = &r->headers_out.cache_control;

    if (cc->elts) {

        if ((p & NGX_HTTP_GZIP_PROXIED_NO_CACHE)
            && ngx_http_parse_multi_header_lines(cc, &ngx_http_gzip_no_cache,
                                                 NULL)
               >= 0)
        {
            goto ok;
        }

        if ((p & NGX_HTTP_GZIP_PROXIED_NO_STORE)
            && ngx_http_parse_multi_header_lines(cc, &ngx_http_gzip_no_store,
                                                 NULL)
               >= 0)
        {
            goto ok;
        }

        if ((p & NGX_HTTP_GZIP_PROXIED_PRIVATE)
            && ngx_http_parse_multi_header_lines(cc, &ngx_http_gzip_private,
                                                 NULL)
               >= 0)
        {
            goto ok;
        }

        return NGX_DECLINED;
    }

    if ((p & NGX_HTTP_GZIP_PROXIED_NO_LM) && r->headers_out.last_modified) {
        return NGX_DECLINED;
    }

    if ((p & NGX_HTTP_GZIP_PROXIED_NO_ETAG) && r->headers_out.etag) {
        return NGX_DECLINED;
    }

ok:

#if (NGX_PCRE)

    if (clcf->gzip_disable && r->headers_in.user_agent) {

        if (ngx_regex_exec_array(clcf->gzip_disable,
                                 &r->headers_in.user_agent->value,
                                 r->connection->log)
            != NGX_DECLINED)
        {
            return NGX_DECLINED;
        }
    }

#endif

    r->gzip_ok = 1;

    return NGX_OK;
}


/*
 * gzip is enabled for the following quantities:
 *     "gzip; q=0.001" ... "gzip; q=1.000"
 * gzip is disabled for the following quantities:
 *     "gzip; q=0" ... "gzip; q=0.000", and for any invalid cases
 */

static ngx_int_t
ngx_http_gzip_accept_encoding(ngx_str_t *ae)
{
    u_char  *p, *start, *last;

    start = ae->data;
    last = start + ae->len;

    for ( ;; ) {
        p = ngx_strcasestrn(start, "gzip", 4 - 1);
        if (p == NULL) {
            return NGX_DECLINED;
        }

        if (p == start || (*(p - 1) == ',' || *(p - 1) == ' ')) {
            break;
        }

        start = p + 4;
    }

    p += 4;

    while (p < last) {
        switch (*p++) {
        case ',':
            return NGX_OK;
        case ';':
            goto quantity;
        case ' ':
            continue;
        default:
            return NGX_DECLINED;
        }
    }

    return NGX_OK;

quantity:

    while (p < last) {
        switch (*p++) {
        case 'q':
        case 'Q':
            goto equal;
        case ' ':
            continue;
        default:
            return NGX_DECLINED;
        }
    }

    return NGX_OK;

equal:

    if (p + 2 > last || *p++ != '=') {
        return NGX_DECLINED;
    }

    if (ngx_http_gzip_quantity(p, last) == 0) {
        return NGX_DECLINED;
    }

    return NGX_OK;
}


static ngx_uint_t
ngx_http_gzip_quantity(u_char *p, u_char *last)
{
    u_char      c;
    ngx_uint_t  n, q;

    c = *p++;

    if (c != '0' && c != '1') {
        return 0;
    }

    q = (c - '0') * 100;

    if (p == last) {
        return q;
    }

    c = *p++;

    if (c == ',' || c == ' ') {
        return q;
    }

    if (c != '.') {
        return 0;
    }

    n = 0;

    while (p < last) {
        c = *p++;

        if (c == ',' || c == ' ') {
            break;
        }

        if (c >= '0' && c <= '9') {
            q += c - '0';
            n++;
            continue;
        }

        return 0;
    }

    if (q > 100 || n > 3) {
        return 0;
    }

    return q;
}

#endif


ngx_int_t
ngx_http_subrequest(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args, ngx_http_request_t **psr,
    ngx_http_post_subrequest_t *ps, ngx_uint_t flags)
{
    ngx_time_t                    *tp;
    ngx_connection_t              *c;
    ngx_http_request_t            *sr;
    ngx_http_core_srv_conf_t      *cscf;
    ngx_http_postponed_request_t  *pr, *p;

    if (r->subrequests == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "subrequests cycle while processing \"%V\"", uri);
        return NGX_ERROR;
    }

    /*
     * 1000 is reserved for other purposes.
     */
    if (r->main->count >= 65535 - 1000) {
        ngx_log_error(NGX_LOG_CRIT, r->connection->log, 0,
                      "request reference counter overflow "
                      "while processing \"%V\"", uri);
        return NGX_ERROR;
    }

    if (r->subrequest_in_memory) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "nested in-memory subrequest \"%V\"", uri);
        return NGX_ERROR;
    }

    sr = ngx_pcalloc(r->pool, sizeof(ngx_http_request_t));
    if (sr == NULL) {
        return NGX_ERROR;
    }

    sr->signature = NGX_HTTP_MODULE;

    c = r->connection;
    sr->connection = c;

    sr->ctx = ngx_pcalloc(r->pool, sizeof(void *) * ngx_http_max_module);
    if (sr->ctx == NULL) {
        return NGX_ERROR;
    }

    if (ngx_list_init(&sr->headers_out.headers, r->pool, 20,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (ngx_list_init(&sr->headers_out.trailers, r->pool, 4,
                      sizeof(ngx_table_elt_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    sr->main_conf = cscf->ctx->main_conf;
    sr->srv_conf = cscf->ctx->srv_conf;
    sr->loc_conf = cscf->ctx->loc_conf;

    sr->pool = r->pool;

    sr->headers_in = r->headers_in;

    ngx_http_clear_content_length(sr);
    ngx_http_clear_accept_ranges(sr);
    ngx_http_clear_last_modified(sr);

    sr->request_body = r->request_body;

#if (NGX_HTTP_V2)
    sr->stream = r->stream;
#endif

    sr->method = NGX_HTTP_GET;
    sr->http_version = r->http_version;

    sr->request_line = r->request_line;
    sr->uri = *uri;

    if (args) {
        sr->args = *args;
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, c->log, 0,
                   "http subrequest \"%V?%V\"", uri, &sr->args);

    sr->subrequest_in_memory = (flags & NGX_HTTP_SUBREQUEST_IN_MEMORY) != 0;
    sr->waited = (flags & NGX_HTTP_SUBREQUEST_WAITED) != 0;
    sr->background = (flags & NGX_HTTP_SUBREQUEST_BACKGROUND) != 0;

    sr->unparsed_uri = r->unparsed_uri;
    sr->method_name = ngx_http_core_get_method;
    sr->http_protocol = r->http_protocol;
    sr->schema = r->schema;

    ngx_http_set_exten(sr);

    sr->main = r->main;
    sr->parent = r;
    sr->post_subrequest = ps;
    sr->read_event_handler = ngx_http_request_empty_handler;
    sr->write_event_handler = ngx_http_handler;

    sr->variables = r->variables;

    sr->log_handler = r->log_handler;

    if (sr->subrequest_in_memory) {
        sr->filter_need_in_memory = 1;
    }

    if (!sr->background) {
        if (c->data == r && r->postponed == NULL) {
            c->data = sr;
        }

        pr = ngx_palloc(r->pool, sizeof(ngx_http_postponed_request_t));
        if (pr == NULL) {
            return NGX_ERROR;
        }

        pr->request = sr;
        pr->out = NULL;
        pr->next = NULL;

        if (r->postponed) {
            for (p = r->postponed; p->next; p = p->next) { /* void */ }
            p->next = pr;

        } else {
            r->postponed = pr;
        }
    }

    sr->internal = 1;

    sr->discard_body = r->discard_body;
    sr->expect_tested = 1;
    sr->main_filter_need_in_memory = r->main_filter_need_in_memory;

    sr->uri_changes = NGX_HTTP_MAX_URI_CHANGES + 1;
    sr->subrequests = r->subrequests - 1;

    tp = ngx_timeofday();
    sr->start_sec = tp->sec;
    sr->start_msec = tp->msec;

    r->main->count++;

    *psr = sr;

    if (flags & NGX_HTTP_SUBREQUEST_CLONE) {
        sr->method = r->method;
        sr->method_name = r->method_name;
        sr->loc_conf = r->loc_conf;
        sr->valid_location = r->valid_location;
        sr->valid_unparsed_uri = r->valid_unparsed_uri;
        sr->content_handler = r->content_handler;
        sr->phase_handler = r->phase_handler;
        sr->write_event_handler = ngx_http_core_run_phases;

#if (NGX_PCRE)
        sr->ncaptures = r->ncaptures;
        sr->captures = r->captures;
        sr->captures_data = r->captures_data;
        sr->realloc_captures = 1;
        r->realloc_captures = 1;
#endif

        ngx_http_update_location_config(sr);
    }

    return ngx_http_post_request(sr, NULL);
}


ngx_int_t
ngx_http_internal_redirect(ngx_http_request_t *r,
    ngx_str_t *uri, ngx_str_t *args)
{
    ngx_http_core_srv_conf_t  *cscf;

    r->uri_changes--;

    if (r->uri_changes == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "rewrite or internal redirection cycle "
                      "while internally redirecting to \"%V\"", uri);

        r->main->count++;
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_DONE;
    }

    r->uri = *uri;

    if (args) {
        r->args = *args;

    } else {
        ngx_str_null(&r->args);
    }

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "internal redirect: \"%V?%V\"", uri, &r->args);

    ngx_http_set_exten(r);

    /* clear the modules contexts */
    ngx_memzero(r->ctx, sizeof(void *) * ngx_http_max_module);

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);
    r->loc_conf = cscf->ctx->loc_conf;

    ngx_http_update_location_config(r);

#if (NGX_HTTP_CACHE)
    r->cache = NULL;
#endif

    r->internal = 1;
    r->valid_unparsed_uri = 0;
    r->add_uri_to_alias = 0;
    r->main->count++;

    ngx_http_handler(r);

    return NGX_DONE;
}


ngx_int_t
ngx_http_named_location(ngx_http_request_t *r, ngx_str_t *name)
{
    ngx_http_core_srv_conf_t    *cscf;
    ngx_http_core_loc_conf_t   **clcfp;
    ngx_http_core_main_conf_t   *cmcf;

    r->main->count++;
    r->uri_changes--;

    if (r->uri_changes == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "rewrite or internal redirection cycle "
                      "while redirect to named location \"%V\"", name);

        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_DONE;
    }

    if (r->uri.len == 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "empty URI in redirect to named location \"%V\"", name);

        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return NGX_DONE;
    }

    cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

    if (cscf->named_locations) {

        for (clcfp = cscf->named_locations; *clcfp; clcfp++) {

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "test location: \"%V\"", &(*clcfp)->name);

            if (name->len != (*clcfp)->name.len
                || ngx_strncmp(name->data, (*clcfp)->name.data, name->len) != 0)
            {
                continue;
            }

            ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                           "using location: %V \"%V?%V\"",
                           name, &r->uri, &r->args);

            r->internal = 1;
            r->content_handler = NULL;
            r->uri_changed = 0;
            r->loc_conf = (*clcfp)->loc_conf;

            /* clear the modules contexts */
            ngx_memzero(r->ctx, sizeof(void *) * ngx_http_max_module);

            ngx_http_update_location_config(r);

            cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

            r->phase_handler = cmcf->phase_engine.location_rewrite_index;

            r->write_event_handler = ngx_http_core_run_phases;
            ngx_http_core_run_phases(r);

            return NGX_DONE;
        }
    }

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "could not find named location \"%V\"", name);

    ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);

    return NGX_DONE;
}


ngx_http_cleanup_t *
ngx_http_cleanup_add(ngx_http_request_t *r, size_t size)
{
    ngx_http_cleanup_t  *cln;

    r = r->main;

    cln = ngx_palloc(r->pool, sizeof(ngx_http_cleanup_t));
    if (cln == NULL) {
        return NULL;
    }

    if (size) {
        cln->data = ngx_palloc(r->pool, size);
        if (cln->data == NULL) {
            return NULL;
        }

    } else {
        cln->data = NULL;
    }

    cln->handler = NULL;
    cln->next = r->cleanup;

    r->cleanup = cln;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http cleanup add: %p", cln);

    return cln;
}


ngx_int_t
ngx_http_set_disable_symlinks(ngx_http_request_t *r,
    ngx_http_core_loc_conf_t *clcf, ngx_str_t *path, ngx_open_file_info_t *of)
{
#if (NGX_HAVE_OPENAT)
    u_char     *p;
    ngx_str_t   from;

    of->disable_symlinks = clcf->disable_symlinks;

    if (clcf->disable_symlinks_from == NULL) {
        return NGX_OK;
    }

    if (ngx_http_complex_value(r, clcf->disable_symlinks_from, &from)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    if (from.len == 0
        || from.len > path->len
        || ngx_memcmp(path->data, from.data, from.len) != 0)
    {
        return NGX_OK;
    }

    if (from.len == path->len) {
        of->disable_symlinks = NGX_DISABLE_SYMLINKS_OFF;
        return NGX_OK;
    }

    p = path->data + from.len;

    if (*p == '/') {
        of->disable_symlinks_from = from.len;
        return NGX_OK;
    }

    p--;

    if (*p == '/') {
        of->disable_symlinks_from = from.len - 1;
    }
#endif

    return NGX_OK;
}


ngx_int_t
ngx_http_get_forwarded_addr(ngx_http_request_t *r, ngx_addr_t *addr,
    ngx_array_t *headers, ngx_str_t *value, ngx_array_t *proxies,
    int recursive)
{
    ngx_int_t          rc;
    ngx_uint_t         i, found;
    ngx_table_elt_t  **h;

    if (headers == NULL) {
        return ngx_http_get_forwarded_addr_internal(r, addr, value->data,
                                                    value->len, proxies,
                                                    recursive);
    }

    i = headers->nelts;
    h = headers->elts;

    rc = NGX_DECLINED;

    found = 0;

    while (i-- > 0) {
        rc = ngx_http_get_forwarded_addr_internal(r, addr, h[i]->value.data,
                                                  h[i]->value.len, proxies,
                                                  recursive);

        if (!recursive) {
            break;
        }

        if (rc == NGX_DECLINED && found) {
            rc = NGX_DONE;
            break;
        }

        if (rc != NGX_OK) {
            break;
        }

        found = 1;
    }

    return rc;
}


static ngx_int_t
ngx_http_get_forwarded_addr_internal(ngx_http_request_t *r, ngx_addr_t *addr,
    u_char *xff, size_t xfflen, ngx_array_t *proxies, int recursive)
{
    u_char      *p;
    ngx_addr_t   paddr;
    ngx_uint_t   found;

    found = 0;

    do {

        if (ngx_cidr_match(addr->sockaddr, proxies) != NGX_OK) {
            return found ? NGX_DONE : NGX_DECLINED;
        }

        for (p = xff + xfflen - 1; p > xff; p--, xfflen--) {
            if (*p != ' ' && *p != ',') {
                break;
            }
        }

        for ( /* void */ ; p > xff; p--) {
            if (*p == ' ' || *p == ',') {
                p++;
                break;
            }
        }

        if (ngx_parse_addr_port(r->pool, &paddr, p, xfflen - (p - xff))
            != NGX_OK)
        {
            return found ? NGX_DONE : NGX_DECLINED;
        }

        *addr = paddr;
        found = 1;
        xfflen = p - 1 - xff;

    } while (recursive && p > xff);

    return NGX_OK;
}


static char *
ngx_http_core_server(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{
    char                        *rv;
    void                        *mconf;
    size_t                       len;
    u_char                      *p;
    ngx_uint_t                   i;
    ngx_conf_t                   pcf;
    ngx_http_module_t           *module;
    struct sockaddr_in          *sin;
    ngx_http_conf_ctx_t         *ctx, *http_ctx;
    ngx_http_listen_opt_t        lsopt;
    ngx_http_core_srv_conf_t    *cscf, **cscfp;
    ngx_http_core_main_conf_t   *cmcf;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    http_ctx = cf->ctx;
    ctx->main_conf = http_ctx->main_conf;

    /* the server{}'s srv_conf */

    ctx->srv_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->srv_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    /* the server{}'s loc_conf */

    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = cf->cycle->modules[i]->ctx;

        if (module->create_srv_conf) {
            mconf = module->create_srv_conf(cf);
            if (mconf == NULL) {
                return NGX_CONF_ERROR;
            }

            ctx->srv_conf[cf->cycle->modules[i]->ctx_index] = mconf;
        }

        if (module->create_loc_conf) {
            mconf = module->create_loc_conf(cf);
            if (mconf == NULL) {
                return NGX_CONF_ERROR;
            }

            ctx->loc_conf[cf->cycle->modules[i]->ctx_index] = mconf;
        }
    }


    /* the server configuration context */

    cscf = ctx->srv_conf[ngx_http_core_module.ctx_index];
    cscf->ctx = ctx;


    cmcf = ctx->main_conf[ngx_http_core_module.ctx_index];

    cscfp = ngx_array_push(&cmcf->servers);
    if (cscfp == NULL) {
        return NGX_CONF_ERROR;
    }

    *cscfp = cscf;


    /* parse inside server{} */

    pcf = *cf;
    cf->ctx = ctx;
    cf->cmd_type = NGX_HTTP_SRV_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = pcf;

    if (rv == NGX_CONF_OK && !cscf->listen) {
        ngx_memzero(&lsopt, sizeof(ngx_http_listen_opt_t));

        p = ngx_pcalloc(cf->pool, sizeof(struct sockaddr_in));
        if (p == NULL) {
            return NGX_CONF_ERROR;
        }

        lsopt.sockaddr = (struct sockaddr *) p;

        sin = (struct sockaddr_in *) p;

        sin->sin_family = AF_INET;
#if (NGX_WIN32)
        sin->sin_port = htons(80);
#else
        sin->sin_port = htons((getuid() == 0) ? 80 : 8000);
#endif
        sin->sin_addr.s_addr = INADDR_ANY;

        lsopt.socklen = sizeof(struct sockaddr_in);

        lsopt.backlog = NGX_LISTEN_BACKLOG;
        lsopt.rcvbuf = -1;
        lsopt.sndbuf = -1;
#if (NGX_HAVE_SETFIB)
        lsopt.setfib = -1;
#endif
#if (NGX_HAVE_TCP_FASTOPEN)
        lsopt.fastopen = -1;
#endif
        lsopt.wildcard = 1;

        len = NGX_INET_ADDRSTRLEN + sizeof(":65535") - 1;

        p = ngx_pnalloc(cf->pool, len);
        if (p == NULL) {
            return NGX_CONF_ERROR;
        }

        lsopt.addr_text.data = p;
        lsopt.addr_text.len = ngx_sock_ntop(lsopt.sockaddr, lsopt.socklen, p,
                                            len, 1);

        if (ngx_http_add_listen(cf, cscf, &lsopt) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return rv;
}


static char *
ngx_http_core_location(ngx_conf_t *cf, ngx_command_t *cmd, void *dummy)
{
    char                      *rv;
    u_char                    *mod;
    size_t                     len;
    ngx_str_t                 *value, *name;
    ngx_uint_t                 i;
    ngx_conf_t                 save;
    ngx_http_module_t         *module;
    ngx_http_conf_ctx_t       *ctx, *pctx;
    ngx_http_core_loc_conf_t  *clcf, *pclcf;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    pctx = cf->ctx;
    ctx->main_conf = pctx->main_conf;
    ctx->srv_conf = pctx->srv_conf;

    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = cf->cycle->modules[i]->ctx;

        if (module->create_loc_conf) {
            ctx->loc_conf[cf->cycle->modules[i]->ctx_index] =
                                                   module->create_loc_conf(cf);
            if (ctx->loc_conf[cf->cycle->modules[i]->ctx_index] == NULL) {
                return NGX_CONF_ERROR;
            }
        }
    }

    clcf = ctx->loc_conf[ngx_http_core_module.ctx_index];
    clcf->loc_conf = ctx->loc_conf;

    value = cf->args->elts;

    if (cf->args->nelts == 3) {

        len = value[1].len;
        mod = value[1].data;
        name = &value[2];

        if (len == 1 && mod[0] == '=') {

            clcf->name = *name;
            clcf->exact_match = 1;

        } else if (len == 2 && mod[0] == '^' && mod[1] == '~') {

            clcf->name = *name;
            clcf->noregex = 1;

        } else if (len == 1 && mod[0] == '~') {

            if (ngx_http_core_regex_location(cf, clcf, name, 0) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

        } else if (len == 2 && mod[0] == '~' && mod[1] == '*') {

            if (ngx_http_core_regex_location(cf, clcf, name, 1) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

        } else {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid location modifier \"%V\"", &value[1]);
            return NGX_CONF_ERROR;
        }

    } else {

        name = &value[1];

        if (name->data[0] == '=') {

            clcf->name.len = name->len - 1;
            clcf->name.data = name->data + 1;
            clcf->exact_match = 1;

        } else if (name->data[0] == '^' && name->data[1] == '~') {

            clcf->name.len = name->len - 2;
            clcf->name.data = name->data + 2;
            clcf->noregex = 1;

        } else if (name->data[0] == '~') {

            name->len--;
            name->data++;

            if (name->data[0] == '*') {

                name->len--;
                name->data++;

                if (ngx_http_core_regex_location(cf, clcf, name, 1) != NGX_OK) {
                    return NGX_CONF_ERROR;
                }

            } else {
                if (ngx_http_core_regex_location(cf, clcf, name, 0) != NGX_OK) {
                    return NGX_CONF_ERROR;
                }
            }

        } else {

            clcf->name = *name;

            if (name->data[0] == '@') {
                clcf->named = 1;
            }
        }
    }

    pclcf = pctx->loc_conf[ngx_http_core_module.ctx_index];

    if (cf->cmd_type == NGX_HTTP_LOC_CONF) {

        /* nested location */

#if 0
        clcf->prev_location = pclcf;
#endif

        if (pclcf->exact_match) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "location \"%V\" cannot be inside "
                               "the exact location \"%V\"",
                               &clcf->name, &pclcf->name);
            return NGX_CONF_ERROR;
        }

        if (pclcf->named) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "location \"%V\" cannot be inside "
                               "the named location \"%V\"",
                               &clcf->name, &pclcf->name);
            return NGX_CONF_ERROR;
        }

        if (clcf->named) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "named location \"%V\" can be "
                               "on the server level only",
                               &clcf->name);
            return NGX_CONF_ERROR;
        }

        len = pclcf->name.len;

#if (NGX_PCRE)
        if (clcf->regex == NULL
            && ngx_filename_cmp(clcf->name.data, pclcf->name.data, len) != 0)
#else
        if (ngx_filename_cmp(clcf->name.data, pclcf->name.data, len) != 0)
#endif
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "location \"%V\" is outside location \"%V\"",
                               &clcf->name, &pclcf->name);
            return NGX_CONF_ERROR;
        }
    }

    if (ngx_http_add_location(cf, &pclcf->locations, clcf) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    save = *cf;
    cf->ctx = ctx;
    cf->cmd_type = NGX_HTTP_LOC_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = save;

    return rv;
}


static ngx_int_t
ngx_http_core_regex_location(ngx_conf_t *cf, ngx_http_core_loc_conf_t *clcf,
    ngx_str_t *regex, ngx_uint_t caseless)
{
#if (NGX_PCRE)
    ngx_regex_compile_t  rc;
    u_char               errstr[NGX_MAX_CONF_ERRSTR];

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.pattern = *regex;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;

#if (NGX_HAVE_CASELESS_FILESYSTEM)
    rc.options = NGX_REGEX_CASELESS;
#else
    rc.options = caseless ? NGX_REGEX_CASELESS : 0;
#endif

    clcf->regex = ngx_http_regex_compile(cf, &rc);
    if (clcf->regex == NULL) {
        return NGX_ERROR;
    }

    clcf->name = *regex;

    return NGX_OK;

#else

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "using regex \"%V\" requires PCRE library",
                       regex);
    return NGX_ERROR;

#endif
}


static char *
ngx_http_core_types(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    char        *rv;
    ngx_conf_t   save;

    if (clcf->types == NULL) {
        clcf->types = ngx_array_create(cf->pool, 64, sizeof(ngx_hash_key_t));
        if (clcf->types == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    save = *cf;
    cf->handler = ngx_http_core_type;
    cf->handler_conf = conf;

    rv = ngx_conf_parse(cf, NULL);

    *cf = save;

    return rv;
}


static char *
ngx_http_core_type(ngx_conf_t *cf, ngx_command_t *dummy, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t       *value, *content_type, *old;
    ngx_uint_t       i, n, hash;
    ngx_hash_key_t  *type;

    value = cf->args->elts;

    if (ngx_strcmp(value[0].data, "include") == 0) {
        if (cf->args->nelts != 2) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid number of arguments"
                               " in \"include\" directive");
            return NGX_CONF_ERROR;
        }

        return ngx_conf_include(cf, dummy, conf);
    }

    content_type = ngx_palloc(cf->pool, sizeof(ngx_str_t));
    if (content_type == NULL) {
        return NGX_CONF_ERROR;
    }

    *content_type = value[0];

    for (i = 1; i < cf->args->nelts; i++) {

        hash = ngx_hash_strlow(value[i].data, value[i].data, value[i].len);

        type = clcf->types->elts;
        for (n = 0; n < clcf->types->nelts; n++) {
            if (ngx_strcmp(value[i].data, type[n].key.data) == 0) {
                old = type[n].value;
                type[n].value = content_type;

                ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                                   "duplicate extension \"%V\", "
                                   "content type: \"%V\", "
                                   "previous content type: \"%V\"",
                                   &value[i], content_type, old);
                goto next;
            }
        }


        type = ngx_array_push(clcf->types);
        if (type == NULL) {
            return NGX_CONF_ERROR;
        }

        type->key = value[i];
        type->key_hash = hash;
        type->value = content_type;

    next:
        continue;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_core_preconfiguration(ngx_conf_t *cf)
{
    return ngx_http_variables_add_core_vars(cf);
}


static ngx_int_t
ngx_http_core_postconfiguration(ngx_conf_t *cf)
{
    ngx_http_top_request_body_filter = ngx_http_request_body_save_filter;

    return NGX_OK;
}


static void *
ngx_http_core_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_core_main_conf_t));
    if (cmcf == NULL) {
        return NULL;
    }

    if (ngx_array_init(&cmcf->servers, cf->pool, 4,
                       sizeof(ngx_http_core_srv_conf_t *))
        != NGX_OK)
    {
        return NULL;
    }

    cmcf->server_names_hash_max_size = NGX_CONF_UNSET_UINT;
    cmcf->server_names_hash_bucket_size = NGX_CONF_UNSET_UINT;

    cmcf->variables_hash_max_size = NGX_CONF_UNSET_UINT;
    cmcf->variables_hash_bucket_size = NGX_CONF_UNSET_UINT;

    return cmcf;
}


static char *
ngx_http_core_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_core_main_conf_t *cmcf = conf;

    ngx_conf_init_uint_value(cmcf->server_names_hash_max_size, 512);
    ngx_conf_init_uint_value(cmcf->server_names_hash_bucket_size,
                             ngx_cacheline_size);

    cmcf->server_names_hash_bucket_size =
            ngx_align(cmcf->server_names_hash_bucket_size, ngx_cacheline_size);


    ngx_conf_init_uint_value(cmcf->variables_hash_max_size, 1024);
    ngx_conf_init_uint_value(cmcf->variables_hash_bucket_size, 64);

    cmcf->variables_hash_bucket_size =
               ngx_align(cmcf->variables_hash_bucket_size, ngx_cacheline_size);

    if (cmcf->ncaptures) {
        cmcf->ncaptures = (cmcf->ncaptures + 1) * 3;
    }

    return NGX_CONF_OK;
}


static void *
ngx_http_core_create_srv_conf(ngx_conf_t *cf)
{
    ngx_http_core_srv_conf_t  *cscf;

    cscf = ngx_pcalloc(cf->pool, sizeof(ngx_http_core_srv_conf_t));
    if (cscf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->client_large_buffers.num = 0;
     */

    if (ngx_array_init(&cscf->server_names, cf->temp_pool, 4,
                       sizeof(ngx_http_server_name_t))
        != NGX_OK)
    {
        return NULL;
    }

    cscf->connection_pool_size = NGX_CONF_UNSET_SIZE;
    cscf->request_pool_size = NGX_CONF_UNSET_SIZE;
    cscf->client_header_timeout = NGX_CONF_UNSET_MSEC;
    cscf->client_header_buffer_size = NGX_CONF_UNSET_SIZE;
    cscf->ignore_invalid_headers = NGX_CONF_UNSET;
    cscf->merge_slashes = NGX_CONF_UNSET;
    cscf->underscores_in_headers = NGX_CONF_UNSET;

    cscf->file_name = cf->conf_file->file.name.data;
    cscf->line = cf->conf_file->line;

    return cscf;
}


static char *
ngx_http_core_merge_srv_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_core_srv_conf_t *prev = parent;
    ngx_http_core_srv_conf_t *conf = child;

    ngx_str_t                name;
    ngx_http_server_name_t  *sn;

    /* TODO: it does not merge, it inits only */

    ngx_conf_merge_size_value(conf->connection_pool_size,
                              prev->connection_pool_size, 64 * sizeof(void *));
    ngx_conf_merge_size_value(conf->request_pool_size,
                              prev->request_pool_size, 4096);
    ngx_conf_merge_msec_value(conf->client_header_timeout,
                              prev->client_header_timeout, 60000);
    ngx_conf_merge_size_value(conf->client_header_buffer_size,
                              prev->client_header_buffer_size, 1024);
    ngx_conf_merge_bufs_value(conf->large_client_header_buffers,
                              prev->large_client_header_buffers,
                              4, 8192);

    if (conf->large_client_header_buffers.size < conf->connection_pool_size) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the \"large_client_header_buffers\" size must be "
                           "equal to or greater than \"connection_pool_size\"");
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_value(conf->ignore_invalid_headers,
                              prev->ignore_invalid_headers, 1);

    ngx_conf_merge_value(conf->merge_slashes, prev->merge_slashes, 1);

    ngx_conf_merge_value(conf->underscores_in_headers,
                              prev->underscores_in_headers, 0);

    if (conf->server_names.nelts == 0) {
        /* the array has 4 empty preallocated elements, so push cannot fail */
        sn = ngx_array_push(&conf->server_names);
#if (NGX_PCRE)
        sn->regex = NULL;
#endif
        sn->server = conf;
        ngx_str_set(&sn->name, "");
    }

    sn = conf->server_names.elts;
    name = sn[0].name;

#if (NGX_PCRE)
    if (sn->regex) {
        name.len++;
        name.data--;
    } else
#endif

    if (name.data[0] == '.') {
        name.len--;
        name.data++;
    }

    conf->server_name.len = name.len;
    conf->server_name.data = ngx_pstrdup(cf->pool, &name);
    if (conf->server_name.data == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static void *
ngx_http_core_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_core_loc_conf_t));
    if (clcf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     clcf->root = { 0, NULL };
     *     clcf->limit_except = 0;
     *     clcf->post_action = { 0, NULL };
     *     clcf->types = NULL;
     *     clcf->default_type = { 0, NULL };
     *     clcf->error_log = NULL;
     *     clcf->error_pages = NULL;
     *     clcf->client_body_path = NULL;
     *     clcf->regex = NULL;
     *     clcf->exact_match = 0;
     *     clcf->auto_redirect = 0;
     *     clcf->alias = 0;
     *     clcf->limit_rate = NULL;
     *     clcf->limit_rate_after = NULL;
     *     clcf->gzip_proxied = 0;
     *     clcf->keepalive_disable = 0;
     */

    clcf->client_max_body_size = NGX_CONF_UNSET;
    clcf->client_body_buffer_size = NGX_CONF_UNSET_SIZE;
    clcf->client_body_timeout = NGX_CONF_UNSET_MSEC;
    clcf->satisfy = NGX_CONF_UNSET_UINT;
    clcf->auth_delay = NGX_CONF_UNSET_MSEC;
    clcf->if_modified_since = NGX_CONF_UNSET_UINT;
    clcf->max_ranges = NGX_CONF_UNSET_UINT;
    clcf->client_body_in_file_only = NGX_CONF_UNSET_UINT;
    clcf->client_body_in_single_buffer = NGX_CONF_UNSET;
    clcf->internal = NGX_CONF_UNSET;
    clcf->sendfile = NGX_CONF_UNSET;
    clcf->sendfile_max_chunk = NGX_CONF_UNSET_SIZE;
    clcf->subrequest_output_buffer_size = NGX_CONF_UNSET_SIZE;
    clcf->aio = NGX_CONF_UNSET;
    clcf->aio_write = NGX_CONF_UNSET;
#if (NGX_THREADS)
    clcf->thread_pool = NGX_CONF_UNSET_PTR;
    clcf->thread_pool_value = NGX_CONF_UNSET_PTR;
#endif
    clcf->read_ahead = NGX_CONF_UNSET_SIZE;
    clcf->directio = NGX_CONF_UNSET;
    clcf->directio_alignment = NGX_CONF_UNSET;
    clcf->tcp_nopush = NGX_CONF_UNSET;
    clcf->tcp_nodelay = NGX_CONF_UNSET;
    clcf->send_timeout = NGX_CONF_UNSET_MSEC;
    clcf->send_lowat = NGX_CONF_UNSET_SIZE;
    clcf->postpone_output = NGX_CONF_UNSET_SIZE;
    clcf->keepalive_timeout = NGX_CONF_UNSET_MSEC;
    clcf->keepalive_header = NGX_CONF_UNSET;
    clcf->keepalive_requests = NGX_CONF_UNSET_UINT;
    clcf->lingering_close = NGX_CONF_UNSET_UINT;
    clcf->lingering_time = NGX_CONF_UNSET_MSEC;
    clcf->lingering_timeout = NGX_CONF_UNSET_MSEC;
    clcf->resolver_timeout = NGX_CONF_UNSET_MSEC;
    clcf->reset_timedout_connection = NGX_CONF_UNSET;
    clcf->absolute_redirect = NGX_CONF_UNSET;
    clcf->server_name_in_redirect = NGX_CONF_UNSET;
    clcf->port_in_redirect = NGX_CONF_UNSET;
    clcf->msie_padding = NGX_CONF_UNSET;
    clcf->msie_refresh = NGX_CONF_UNSET;
    clcf->log_not_found = NGX_CONF_UNSET;
    clcf->log_subrequest = NGX_CONF_UNSET;
    clcf->recursive_error_pages = NGX_CONF_UNSET;
    clcf->chunked_transfer_encoding = NGX_CONF_UNSET;
    clcf->etag = NGX_CONF_UNSET;
    clcf->server_tokens = NGX_CONF_UNSET_UINT;
    clcf->types_hash_max_size = NGX_CONF_UNSET_UINT;
    clcf->types_hash_bucket_size = NGX_CONF_UNSET_UINT;

    clcf->open_file_cache = NGX_CONF_UNSET_PTR;
    clcf->open_file_cache_valid = NGX_CONF_UNSET;
    clcf->open_file_cache_min_uses = NGX_CONF_UNSET_UINT;
    clcf->open_file_cache_errors = NGX_CONF_UNSET;
    clcf->open_file_cache_events = NGX_CONF_UNSET;

#if (NGX_HTTP_GZIP)
    clcf->gzip_vary = NGX_CONF_UNSET;
    clcf->gzip_http_version = NGX_CONF_UNSET_UINT;
#if (NGX_PCRE)
    clcf->gzip_disable = NGX_CONF_UNSET_PTR;
#endif
    clcf->gzip_disable_msie6 = 3;
#if (NGX_HTTP_DEGRADATION)
    clcf->gzip_disable_degradation = 3;
#endif
#endif

#if (NGX_HAVE_OPENAT)
    clcf->disable_symlinks = NGX_CONF_UNSET_UINT;
    clcf->disable_symlinks_from = NGX_CONF_UNSET_PTR;
#endif

    return clcf;
}


static ngx_str_t  ngx_http_core_text_html_type = ngx_string("text/html");
static ngx_str_t  ngx_http_core_image_gif_type = ngx_string("image/gif");
static ngx_str_t  ngx_http_core_image_jpeg_type = ngx_string("image/jpeg");

static ngx_hash_key_t  ngx_http_core_default_types[] = {
    { ngx_string("html"), 0, &ngx_http_core_text_html_type },
    { ngx_string("gif"), 0, &ngx_http_core_image_gif_type },
    { ngx_string("jpg"), 0, &ngx_http_core_image_jpeg_type },
    { ngx_null_string, 0, NULL }
};


static char *
ngx_http_core_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_core_loc_conf_t *prev = parent;
    ngx_http_core_loc_conf_t *conf = child;

    ngx_uint_t        i;
    ngx_hash_key_t   *type;
    ngx_hash_init_t   types_hash;

    if (conf->root.data == NULL) {

        conf->alias = prev->alias;
        conf->root = prev->root;
        conf->root_lengths = prev->root_lengths;
        conf->root_values = prev->root_values;

        if (prev->root.data == NULL) {
            ngx_str_set(&conf->root, "html");

            if (ngx_conf_full_name(cf->cycle, &conf->root, 0) != NGX_OK) {
                return NGX_CONF_ERROR;
            }
        }
    }

    if (conf->post_action.data == NULL) {
        conf->post_action = prev->post_action;
    }

    ngx_conf_merge_uint_value(conf->types_hash_max_size,
                              prev->types_hash_max_size, 1024);

    ngx_conf_merge_uint_value(conf->types_hash_bucket_size,
                              prev->types_hash_bucket_size, 64);

    conf->types_hash_bucket_size = ngx_align(conf->types_hash_bucket_size,
                                             ngx_cacheline_size);

    /*
     * the special handling of the "types" directive in the "http" section
     * to inherit the http's conf->types_hash to all servers
     */

    if (prev->types && prev->types_hash.buckets == NULL) {

        types_hash.hash = &prev->types_hash;
        types_hash.key = ngx_hash_key_lc;
        types_hash.max_size = conf->types_hash_max_size;
        types_hash.bucket_size = conf->types_hash_bucket_size;
        types_hash.name = "types_hash";
        types_hash.pool = cf->pool;
        types_hash.temp_pool = NULL;

        if (ngx_hash_init(&types_hash, prev->types->elts, prev->types->nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    if (conf->types == NULL) {
        conf->types = prev->types;
        conf->types_hash = prev->types_hash;
    }

    if (conf->types == NULL) {
        conf->types = ngx_array_create(cf->pool, 3, sizeof(ngx_hash_key_t));
        if (conf->types == NULL) {
            return NGX_CONF_ERROR;
        }

        for (i = 0; ngx_http_core_default_types[i].key.len; i++) {
            type = ngx_array_push(conf->types);
            if (type == NULL) {
                return NGX_CONF_ERROR;
            }

            type->key = ngx_http_core_default_types[i].key;
            type->key_hash =
                       ngx_hash_key_lc(ngx_http_core_default_types[i].key.data,
                                       ngx_http_core_default_types[i].key.len);
            type->value = ngx_http_core_default_types[i].value;
        }
    }

    if (conf->types_hash.buckets == NULL) {

        types_hash.hash = &conf->types_hash;
        types_hash.key = ngx_hash_key_lc;
        types_hash.max_size = conf->types_hash_max_size;
        types_hash.bucket_size = conf->types_hash_bucket_size;
        types_hash.name = "types_hash";
        types_hash.pool = cf->pool;
        types_hash.temp_pool = NULL;

        if (ngx_hash_init(&types_hash, conf->types->elts, conf->types->nelts)
            != NGX_OK)
        {
            return NGX_CONF_ERROR;
        }
    }

    if (conf->error_log == NULL) {
        if (prev->error_log) {
            conf->error_log = prev->error_log;
        } else {
            conf->error_log = &cf->cycle->new_log;
        }
    }

    if (conf->error_pages == NULL && prev->error_pages) {
        conf->error_pages = prev->error_pages;
    }

    ngx_conf_merge_str_value(conf->default_type,
                              prev->default_type, "text/plain");

    ngx_conf_merge_off_value(conf->client_max_body_size,
                              prev->client_max_body_size, 1 * 1024 * 1024);
    ngx_conf_merge_size_value(conf->client_body_buffer_size,
                              prev->client_body_buffer_size,
                              (size_t) 2 * ngx_pagesize);
    ngx_conf_merge_msec_value(conf->client_body_timeout,
                              prev->client_body_timeout, 60000);

    ngx_conf_merge_bitmask_value(conf->keepalive_disable,
                              prev->keepalive_disable,
                              (NGX_CONF_BITMASK_SET
                               |NGX_HTTP_KEEPALIVE_DISABLE_MSIE6));
    ngx_conf_merge_uint_value(conf->satisfy, prev->satisfy,
                              NGX_HTTP_SATISFY_ALL);
    ngx_conf_merge_msec_value(conf->auth_delay, prev->auth_delay, 0);
    ngx_conf_merge_uint_value(conf->if_modified_since, prev->if_modified_since,
                              NGX_HTTP_IMS_EXACT);
    ngx_conf_merge_uint_value(conf->max_ranges, prev->max_ranges,
                              NGX_MAX_INT32_VALUE);
    ngx_conf_merge_uint_value(conf->client_body_in_file_only,
                              prev->client_body_in_file_only,
                              NGX_HTTP_REQUEST_BODY_FILE_OFF);
    ngx_conf_merge_value(conf->client_body_in_single_buffer,
                              prev->client_body_in_single_buffer, 0);
    ngx_conf_merge_value(conf->internal, prev->internal, 0);
    ngx_conf_merge_value(conf->sendfile, prev->sendfile, 0);
    ngx_conf_merge_size_value(conf->sendfile_max_chunk,
                              prev->sendfile_max_chunk, 0);
    ngx_conf_merge_size_value(conf->subrequest_output_buffer_size,
                              prev->subrequest_output_buffer_size,
                              (size_t) ngx_pagesize);
    ngx_conf_merge_value(conf->aio, prev->aio, NGX_HTTP_AIO_OFF);
    ngx_conf_merge_value(conf->aio_write, prev->aio_write, 0);
#if (NGX_THREADS)
    ngx_conf_merge_ptr_value(conf->thread_pool, prev->thread_pool, NULL);
    ngx_conf_merge_ptr_value(conf->thread_pool_value, prev->thread_pool_value,
                             NULL);
#endif
    ngx_conf_merge_size_value(conf->read_ahead, prev->read_ahead, 0);
    ngx_conf_merge_off_value(conf->directio, prev->directio,
                              NGX_OPEN_FILE_DIRECTIO_OFF);
    ngx_conf_merge_off_value(conf->directio_alignment, prev->directio_alignment,
                              512);
    ngx_conf_merge_value(conf->tcp_nopush, prev->tcp_nopush, 0);
    ngx_conf_merge_value(conf->tcp_nodelay, prev->tcp_nodelay, 1);

    ngx_conf_merge_msec_value(conf->send_timeout, prev->send_timeout, 60000);
    ngx_conf_merge_size_value(conf->send_lowat, prev->send_lowat, 0);
    ngx_conf_merge_size_value(conf->postpone_output, prev->postpone_output,
                              1460);

    if (conf->limit_rate == NULL) {
        conf->limit_rate = prev->limit_rate;
    }

    if (conf->limit_rate_after == NULL) {
        conf->limit_rate_after = prev->limit_rate_after;
    }

    ngx_conf_merge_msec_value(conf->keepalive_timeout,
                              prev->keepalive_timeout, 75000);
    ngx_conf_merge_sec_value(conf->keepalive_header,
                              prev->keepalive_header, 0);
    ngx_conf_merge_uint_value(conf->keepalive_requests,
                              prev->keepalive_requests, 100);
    ngx_conf_merge_uint_value(conf->lingering_close,
                              prev->lingering_close, NGX_HTTP_LINGERING_ON);
    ngx_conf_merge_msec_value(conf->lingering_time,
                              prev->lingering_time, 30000);
    ngx_conf_merge_msec_value(conf->lingering_timeout,
                              prev->lingering_timeout, 5000);
    ngx_conf_merge_msec_value(conf->resolver_timeout,
                              prev->resolver_timeout, 30000);

    if (conf->resolver == NULL) {

        if (prev->resolver == NULL) {

            /*
             * create dummy resolver in http {} context
             * to inherit it in all servers
             */

            prev->resolver = ngx_resolver_create(cf, NULL, 0);
            if (prev->resolver == NULL) {
                return NGX_CONF_ERROR;
            }
        }

        conf->resolver = prev->resolver;
    }

    if (ngx_conf_merge_path_value(cf, &conf->client_body_temp_path,
                              prev->client_body_temp_path,
                              &ngx_http_client_temp_path)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_value(conf->reset_timedout_connection,
                              prev->reset_timedout_connection, 0);
    ngx_conf_merge_value(conf->absolute_redirect,
                              prev->absolute_redirect, 1);
    ngx_conf_merge_value(conf->server_name_in_redirect,
                              prev->server_name_in_redirect, 0);
    ngx_conf_merge_value(conf->port_in_redirect, prev->port_in_redirect, 1);
    ngx_conf_merge_value(conf->msie_padding, prev->msie_padding, 1);
    ngx_conf_merge_value(conf->msie_refresh, prev->msie_refresh, 0);
    ngx_conf_merge_value(conf->log_not_found, prev->log_not_found, 1);
    ngx_conf_merge_value(conf->log_subrequest, prev->log_subrequest, 0);
    ngx_conf_merge_value(conf->recursive_error_pages,
                              prev->recursive_error_pages, 0);
    ngx_conf_merge_value(conf->chunked_transfer_encoding,
                              prev->chunked_transfer_encoding, 1);
    ngx_conf_merge_value(conf->etag, prev->etag, 1);

    ngx_conf_merge_uint_value(conf->server_tokens, prev->server_tokens,
                              NGX_HTTP_SERVER_TOKENS_ON);

    ngx_conf_merge_ptr_value(conf->open_file_cache,
                              prev->open_file_cache, NULL);

    ngx_conf_merge_sec_value(conf->open_file_cache_valid,
                              prev->open_file_cache_valid, 60);

    ngx_conf_merge_uint_value(conf->open_file_cache_min_uses,
                              prev->open_file_cache_min_uses, 1);

    ngx_conf_merge_sec_value(conf->open_file_cache_errors,
                              prev->open_file_cache_errors, 0);

    ngx_conf_merge_sec_value(conf->open_file_cache_events,
                              prev->open_file_cache_events, 0);
#if (NGX_HTTP_GZIP)

    ngx_conf_merge_value(conf->gzip_vary, prev->gzip_vary, 0);
    ngx_conf_merge_uint_value(conf->gzip_http_version, prev->gzip_http_version,
                              NGX_HTTP_VERSION_11);
    ngx_conf_merge_bitmask_value(conf->gzip_proxied, prev->gzip_proxied,
                              (NGX_CONF_BITMASK_SET|NGX_HTTP_GZIP_PROXIED_OFF));

#if (NGX_PCRE)
    ngx_conf_merge_ptr_value(conf->gzip_disable, prev->gzip_disable, NULL);
#endif

    if (conf->gzip_disable_msie6 == 3) {
        conf->gzip_disable_msie6 =
            (prev->gzip_disable_msie6 == 3) ? 0 : prev->gzip_disable_msie6;
    }

#if (NGX_HTTP_DEGRADATION)

    if (conf->gzip_disable_degradation == 3) {
        conf->gzip_disable_degradation =
            (prev->gzip_disable_degradation == 3) ?
                 0 : prev->gzip_disable_degradation;
    }

#endif
#endif

#if (NGX_HAVE_OPENAT)
    ngx_conf_merge_uint_value(conf->disable_symlinks, prev->disable_symlinks,
                              NGX_DISABLE_SYMLINKS_OFF);
    ngx_conf_merge_ptr_value(conf->disable_symlinks_from,
                             prev->disable_symlinks_from, NULL);
#endif

    return NGX_CONF_OK;
}


static char *
ngx_http_core_listen(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_srv_conf_t *cscf = conf;

    ngx_str_t              *value, size;
    ngx_url_t               u;
    ngx_uint_t              n;
    ngx_http_listen_opt_t   lsopt;

    cscf->listen = 1;

    value = cf->args->elts;

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.url = value[1];
    u.listen = 1;
    u.default_port = 80;

    if (ngx_parse_url(cf->pool, &u) != NGX_OK) {
        if (u.err) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "%s in \"%V\" of the \"listen\" directive",
                               u.err, &u.url);
        }

        return NGX_CONF_ERROR;
    }

    ngx_memzero(&lsopt, sizeof(ngx_http_listen_opt_t));

    lsopt.backlog = NGX_LISTEN_BACKLOG;
    lsopt.rcvbuf = -1;
    lsopt.sndbuf = -1;
#if (NGX_HAVE_SETFIB)
    lsopt.setfib = -1;
#endif
#if (NGX_HAVE_TCP_FASTOPEN)
    lsopt.fastopen = -1;
#endif
#if (NGX_HAVE_INET6)
    lsopt.ipv6only = 1;
#endif

    for (n = 2; n < cf->args->nelts; n++) {

        if (ngx_strcmp(value[n].data, "default_server") == 0
            || ngx_strcmp(value[n].data, "default") == 0)
        {
            lsopt.default_server = 1;
            continue;
        }

        if (ngx_strcmp(value[n].data, "bind") == 0) {
            lsopt.set = 1;
            lsopt.bind = 1;
            continue;
        }

#if (NGX_HAVE_SETFIB)
        if (ngx_strncmp(value[n].data, "setfib=", 7) == 0) {
            lsopt.setfib = ngx_atoi(value[n].data + 7, value[n].len - 7);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.setfib == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid setfib \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }
#endif

#if (NGX_HAVE_TCP_FASTOPEN)
        if (ngx_strncmp(value[n].data, "fastopen=", 9) == 0) {
            lsopt.fastopen = ngx_atoi(value[n].data + 9, value[n].len - 9);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.fastopen == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid fastopen \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }
#endif

        if (ngx_strncmp(value[n].data, "backlog=", 8) == 0) {
            lsopt.backlog = ngx_atoi(value[n].data + 8, value[n].len - 8);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.backlog == NGX_ERROR || lsopt.backlog == 0) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid backlog \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[n].data, "rcvbuf=", 7) == 0) {
            size.len = value[n].len - 7;
            size.data = value[n].data + 7;

            lsopt.rcvbuf = ngx_parse_size(&size);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.rcvbuf == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid rcvbuf \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[n].data, "sndbuf=", 7) == 0) {
            size.len = value[n].len - 7;
            size.data = value[n].data + 7;

            lsopt.sndbuf = ngx_parse_size(&size);
            lsopt.set = 1;
            lsopt.bind = 1;

            if (lsopt.sndbuf == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid sndbuf \"%V\"", &value[n]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[n].data, "accept_filter=", 14) == 0) {
#if (NGX_HAVE_DEFERRED_ACCEPT && defined SO_ACCEPTFILTER)
            lsopt.accept_filter = (char *) &value[n].data[14];
            lsopt.set = 1;
            lsopt.bind = 1;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "accept filters \"%V\" are not supported "
                               "on this platform, ignored",
                               &value[n]);
#endif
            continue;
        }

        if (ngx_strcmp(value[n].data, "deferred") == 0) {
#if (NGX_HAVE_DEFERRED_ACCEPT && defined TCP_DEFER_ACCEPT)
            lsopt.deferred_accept = 1;
            lsopt.set = 1;
            lsopt.bind = 1;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "the deferred accept is not supported "
                               "on this platform, ignored");
#endif
            continue;
        }

        if (ngx_strncmp(value[n].data, "ipv6only=o", 10) == 0) {
#if (NGX_HAVE_INET6 && defined IPV6_V6ONLY)
            if (ngx_strcmp(&value[n].data[10], "n") == 0) {
                lsopt.ipv6only = 1;

            } else if (ngx_strcmp(&value[n].data[10], "ff") == 0) {
                lsopt.ipv6only = 0;

            } else {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid ipv6only flags \"%s\"",
                                   &value[n].data[9]);
                return NGX_CONF_ERROR;
            }

            lsopt.set = 1;
            lsopt.bind = 1;

            continue;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "ipv6only is not supported "
                               "on this platform");
            return NGX_CONF_ERROR;
#endif
        }

        if (ngx_strcmp(value[n].data, "reuseport") == 0) {
#if (NGX_HAVE_REUSEPORT)
            lsopt.reuseport = 1;
            lsopt.set = 1;
            lsopt.bind = 1;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "reuseport is not supported "
                               "on this platform, ignored");
#endif
            continue;
        }

        if (ngx_strcmp(value[n].data, "ssl") == 0) {
#if (NGX_HTTP_SSL)
            lsopt.ssl = 1;
            continue;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "the \"ssl\" parameter requires "
                               "ngx_http_ssl_module");
            return NGX_CONF_ERROR;
#endif
        }

        if (ngx_strcmp(value[n].data, "http2") == 0) {
#if (NGX_HTTP_V2)
            lsopt.http2 = 1;
            continue;
#else
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "the \"http2\" parameter requires "
                               "ngx_http_v2_module");
            return NGX_CONF_ERROR;
#endif
        }

        if (ngx_strcmp(value[n].data, "spdy") == 0) {
            ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                               "invalid parameter \"spdy\": "
                               "ngx_http_spdy_module was superseded "
                               "by ngx_http_v2_module");
            continue;
        }

        if (ngx_strncmp(value[n].data, "so_keepalive=", 13) == 0) {

            if (ngx_strcmp(&value[n].data[13], "on") == 0) {
                lsopt.so_keepalive = 1;

            } else if (ngx_strcmp(&value[n].data[13], "off") == 0) {
                lsopt.so_keepalive = 2;

            } else {

#if (NGX_HAVE_KEEPALIVE_TUNABLE)
                u_char     *p, *end;
                ngx_str_t   s;

                end = value[n].data + value[n].len;
                s.data = value[n].data + 13;

                p = ngx_strlchr(s.data, end, ':');
                if (p == NULL) {
                    p = end;
                }

                if (p > s.data) {
                    s.len = p - s.data;

                    lsopt.tcp_keepidle = ngx_parse_time(&s, 1);
                    if (lsopt.tcp_keepidle == (time_t) NGX_ERROR) {
                        goto invalid_so_keepalive;
                    }
                }

                s.data = (p < end) ? (p + 1) : end;

                p = ngx_strlchr(s.data, end, ':');
                if (p == NULL) {
                    p = end;
                }

                if (p > s.data) {
                    s.len = p - s.data;

                    lsopt.tcp_keepintvl = ngx_parse_time(&s, 1);
                    if (lsopt.tcp_keepintvl == (time_t) NGX_ERROR) {
                        goto invalid_so_keepalive;
                    }
                }

                s.data = (p < end) ? (p + 1) : end;

                if (s.data < end) {
                    s.len = end - s.data;

                    lsopt.tcp_keepcnt = ngx_atoi(s.data, s.len);
                    if (lsopt.tcp_keepcnt == NGX_ERROR) {
                        goto invalid_so_keepalive;
                    }
                }

                if (lsopt.tcp_keepidle == 0 && lsopt.tcp_keepintvl == 0
                    && lsopt.tcp_keepcnt == 0)
                {
                    goto invalid_so_keepalive;
                }

                lsopt.so_keepalive = 1;

#else

                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "the \"so_keepalive\" parameter accepts "
                                   "only \"on\" or \"off\" on this platform");
                return NGX_CONF_ERROR;

#endif
            }

            lsopt.set = 1;
            lsopt.bind = 1;

            continue;

#if (NGX_HAVE_KEEPALIVE_TUNABLE)
        invalid_so_keepalive:

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid so_keepalive value: \"%s\"",
                               &value[n].data[13]);
            return NGX_CONF_ERROR;
#endif
        }

        if (ngx_strcmp(value[n].data, "proxy_protocol") == 0) {
            lsopt.proxy_protocol = 1;
            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[n]);
        return NGX_CONF_ERROR;
    }

    for (n = 0; n < u.naddrs; n++) {
        lsopt.sockaddr = u.addrs[n].sockaddr;
        lsopt.socklen = u.addrs[n].socklen;
        lsopt.addr_text = u.addrs[n].name;
        lsopt.wildcard = ngx_inet_wildcard(lsopt.sockaddr);

        if (ngx_http_add_listen(cf, cscf, &lsopt) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_core_server_name(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_srv_conf_t *cscf = conf;

    u_char                   ch;
    ngx_str_t               *value;
    ngx_uint_t               i;
    ngx_http_server_name_t  *sn;

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {

        ch = value[i].data[0];

        if ((ch == '*' && (value[i].len < 3 || value[i].data[1] != '.'))
            || (ch == '.' && value[i].len < 2))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "server name \"%V\" is invalid", &value[i]);
            return NGX_CONF_ERROR;
        }

        if (ngx_strchr(value[i].data, '/')) {
            ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                               "server name \"%V\" has suspicious symbols",
                               &value[i]);
        }

        sn = ngx_array_push(&cscf->server_names);
        if (sn == NULL) {
            return NGX_CONF_ERROR;
        }

#if (NGX_PCRE)
        sn->regex = NULL;
#endif
        sn->server = cscf;

        if (ngx_strcasecmp(value[i].data, (u_char *) "$hostname") == 0) {
            sn->name = cf->cycle->hostname;

        } else {
            sn->name = value[i];
        }

        if (value[i].data[0] != '~') {
            ngx_strlow(sn->name.data, sn->name.data, sn->name.len);
            continue;
        }

#if (NGX_PCRE)
        {
        u_char               *p;
        ngx_regex_compile_t   rc;
        u_char                errstr[NGX_MAX_CONF_ERRSTR];

        if (value[i].len == 1) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "empty regex in server name \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

        value[i].len--;
        value[i].data++;

        ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

        rc.pattern = value[i];
        rc.err.len = NGX_MAX_CONF_ERRSTR;
        rc.err.data = errstr;

        for (p = value[i].data; p < value[i].data + value[i].len; p++) {
            if (*p >= 'A' && *p <= 'Z') {
                rc.options = NGX_REGEX_CASELESS;
                break;
            }
        }

        sn->regex = ngx_http_regex_compile(cf, &rc);
        if (sn->regex == NULL) {
            return NGX_CONF_ERROR;
        }

        sn->name = value[i];
        cscf->captures = (rc.captures > 0);
        }
#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "using regex \"%V\" "
                           "requires PCRE library", &value[i]);

        return NGX_CONF_ERROR;
#endif
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_core_root(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t                  *value;
    ngx_int_t                   alias;
    ngx_uint_t                  n;
    ngx_http_script_compile_t   sc;

    alias = (cmd->name.len == sizeof("alias") - 1) ? 1 : 0;

    if (clcf->root.data) {

        if ((clcf->alias != 0) == alias) {
            return "is duplicate";
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" directive is duplicate, "
                           "\"%s\" directive was specified earlier",
                           &cmd->name, clcf->alias ? "alias" : "root");

        return NGX_CONF_ERROR;
    }

    if (clcf->named && alias) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the \"alias\" directive cannot be used "
                           "inside the named location");

        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    if (ngx_strstr(value[1].data, "$document_root")
        || ngx_strstr(value[1].data, "${document_root}"))
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the $document_root variable cannot be used "
                           "in the \"%V\" directive",
                           &cmd->name);

        return NGX_CONF_ERROR;
    }

    if (ngx_strstr(value[1].data, "$realpath_root")
        || ngx_strstr(value[1].data, "${realpath_root}"))
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the $realpath_root variable cannot be used "
                           "in the \"%V\" directive",
                           &cmd->name);

        return NGX_CONF_ERROR;
    }

    clcf->alias = alias ? clcf->name.len : 0;
    clcf->root = value[1];

    if (!alias && clcf->root.len > 0
        && clcf->root.data[clcf->root.len - 1] == '/')
    {
        clcf->root.len--;
    }

    if (clcf->root.data[0] != '$') {
        if (ngx_conf_full_name(cf->cycle, &clcf->root, 0) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    n = ngx_http_script_variables_count(&clcf->root);

    ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));
    sc.variables = n;

#if (NGX_PCRE)
    if (alias && clcf->regex) {
        clcf->alias = NGX_MAX_SIZE_T_VALUE;
        n = 1;
    }
#endif

    if (n) {
        sc.cf = cf;
        sc.source = &clcf->root;
        sc.lengths = &clcf->root_lengths;
        sc.values = &clcf->root_values;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}


static ngx_http_method_name_t  ngx_methods_names[] = {
    { (u_char *) "GET",       (uint32_t) ~NGX_HTTP_GET },
    { (u_char *) "HEAD",      (uint32_t) ~NGX_HTTP_HEAD },
    { (u_char *) "POST",      (uint32_t) ~NGX_HTTP_POST },
    { (u_char *) "PUT",       (uint32_t) ~NGX_HTTP_PUT },
    { (u_char *) "DELETE",    (uint32_t) ~NGX_HTTP_DELETE },
    { (u_char *) "MKCOL",     (uint32_t) ~NGX_HTTP_MKCOL },
    { (u_char *) "COPY",      (uint32_t) ~NGX_HTTP_COPY },
    { (u_char *) "MOVE",      (uint32_t) ~NGX_HTTP_MOVE },
    { (u_char *) "OPTIONS",   (uint32_t) ~NGX_HTTP_OPTIONS },
    { (u_char *) "PROPFIND",  (uint32_t) ~NGX_HTTP_PROPFIND },
    { (u_char *) "PROPPATCH", (uint32_t) ~NGX_HTTP_PROPPATCH },
    { (u_char *) "LOCK",      (uint32_t) ~NGX_HTTP_LOCK },
    { (u_char *) "UNLOCK",    (uint32_t) ~NGX_HTTP_UNLOCK },
    { (u_char *) "PATCH",     (uint32_t) ~NGX_HTTP_PATCH },
    { NULL, 0 }
};


static char *
ngx_http_core_limit_except(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *pclcf = conf;

    char                      *rv;
    void                      *mconf;
    ngx_str_t                 *value;
    ngx_uint_t                 i;
    ngx_conf_t                 save;
    ngx_http_module_t         *module;
    ngx_http_conf_ctx_t       *ctx, *pctx;
    ngx_http_method_name_t    *name;
    ngx_http_core_loc_conf_t  *clcf;

    if (pclcf->limit_except) {
        return "is duplicate";
    }

    pclcf->limit_except = 0xffffffff;

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        for (name = ngx_methods_names; name->name; name++) {

            if (ngx_strcasecmp(value[i].data, name->name) == 0) {
                pclcf->limit_except &= name->method;
                goto next;
            }
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid method \"%V\"", &value[i]);
        return NGX_CONF_ERROR;

    next:
        continue;
    }

    if (!(pclcf->limit_except & NGX_HTTP_GET)) {
        pclcf->limit_except &= (uint32_t) ~NGX_HTTP_HEAD;
    }

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_conf_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    pctx = cf->ctx;
    ctx->main_conf = pctx->main_conf;
    ctx->srv_conf = pctx->srv_conf;

    ctx->loc_conf = ngx_pcalloc(cf->pool, sizeof(void *) * ngx_http_max_module);
    if (ctx->loc_conf == NULL) {
        return NGX_CONF_ERROR;
    }

    for (i = 0; cf->cycle->modules[i]; i++) {
        if (cf->cycle->modules[i]->type != NGX_HTTP_MODULE) {
            continue;
        }

        module = cf->cycle->modules[i]->ctx;

        if (module->create_loc_conf) {

            mconf = module->create_loc_conf(cf);
            if (mconf == NULL) {
                return NGX_CONF_ERROR;
            }

            ctx->loc_conf[cf->cycle->modules[i]->ctx_index] = mconf;
        }
    }


    clcf = ctx->loc_conf[ngx_http_core_module.ctx_index];
    pclcf->limit_except_loc_conf = ctx->loc_conf;
    clcf->loc_conf = ctx->loc_conf;
    clcf->name = pclcf->name;
    clcf->noname = 1;
    clcf->lmt_excpt = 1;

    if (ngx_http_add_location(cf, &pclcf->locations, clcf) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    save = *cf;
    cf->ctx = ctx;
    cf->cmd_type = NGX_HTTP_LMT_CONF;

    rv = ngx_conf_parse(cf, NULL);

    *cf = save;

    return rv;
}


static char *
ngx_http_core_set_aio(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t  *value;

    if (clcf->aio != NGX_CONF_UNSET) {
        return "is duplicate";
    }

#if (NGX_THREADS)
    clcf->thread_pool = NULL;
    clcf->thread_pool_value = NULL;
#endif

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "off") == 0) {
        clcf->aio = NGX_HTTP_AIO_OFF;
        return NGX_CONF_OK;
    }

    if (ngx_strcmp(value[1].data, "on") == 0) {
#if (NGX_HAVE_FILE_AIO)
        clcf->aio = NGX_HTTP_AIO_ON;
        return NGX_CONF_OK;
#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"aio on\" "
                           "is unsupported on this platform");
        return NGX_CONF_ERROR;
#endif
    }

#if (NGX_HAVE_AIO_SENDFILE)

    if (ngx_strcmp(value[1].data, "sendfile") == 0) {
        clcf->aio = NGX_HTTP_AIO_ON;

        ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                           "the \"sendfile\" parameter of "
                           "the \"aio\" directive is deprecated");
        return NGX_CONF_OK;
    }

#endif

    if (ngx_strncmp(value[1].data, "threads", 7) == 0
        && (value[1].len == 7 || value[1].data[7] == '='))
    {
#if (NGX_THREADS)
        ngx_str_t                          name;
        ngx_thread_pool_t                 *tp;
        ngx_http_complex_value_t           cv;
        ngx_http_compile_complex_value_t   ccv;

        clcf->aio = NGX_HTTP_AIO_THREADS;

        if (value[1].len >= 8) {
            name.len = value[1].len - 8;
            name.data = value[1].data + 8;

            ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

            ccv.cf = cf;
            ccv.value = &name;
            ccv.complex_value = &cv;

            if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            if (cv.lengths != NULL) {
                clcf->thread_pool_value = ngx_palloc(cf->pool,
                                    sizeof(ngx_http_complex_value_t));
                if (clcf->thread_pool_value == NULL) {
                    return NGX_CONF_ERROR;
                }

                *clcf->thread_pool_value = cv;

                return NGX_CONF_OK;
            }

            tp = ngx_thread_pool_add(cf, &name);

        } else {
            tp = ngx_thread_pool_add(cf, NULL);
        }

        if (tp == NULL) {
            return NGX_CONF_ERROR;
        }

        clcf->thread_pool = tp;

        return NGX_CONF_OK;
#else
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"aio threads\" "
                           "is unsupported on this platform");
        return NGX_CONF_ERROR;
#endif
    }

    return "invalid value";
}


static char *
ngx_http_core_directio(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t  *value;

    if (clcf->directio != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (ngx_strcmp(value[1].data, "off") == 0) {
        clcf->directio = NGX_OPEN_FILE_DIRECTIO_OFF;
        return NGX_CONF_OK;
    }

    clcf->directio = ngx_parse_offset(&value[1]);
    if (clcf->directio == (off_t) NGX_ERROR) {
        return "invalid value";
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_core_error_page(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    u_char                            *p;
    ngx_int_t                          overwrite;
    ngx_str_t                         *value, uri, args;
    ngx_uint_t                         i, n;
    ngx_http_err_page_t               *err;
    ngx_http_complex_value_t           cv;
    ngx_http_compile_complex_value_t   ccv;

    if (clcf->error_pages == NULL) {
        clcf->error_pages = ngx_array_create(cf->pool, 4,
                                             sizeof(ngx_http_err_page_t));
        if (clcf->error_pages == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    i = cf->args->nelts - 2;

    if (value[i].data[0] == '=') {
        if (i == 1) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid value \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

        if (value[i].len > 1) {
            overwrite = ngx_atoi(&value[i].data[1], value[i].len - 1);

            if (overwrite == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid value \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

        } else {
            overwrite = 0;
        }

        n = 2;

    } else {
        overwrite = -1;
        n = 1;
    }

    uri = value[cf->args->nelts - 1];

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &uri;
    ccv.complex_value = &cv;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    ngx_str_null(&args);

    if (cv.lengths == NULL && uri.len && uri.data[0] == '/') {
        p = (u_char *) ngx_strchr(uri.data, '?');

        if (p) {
            cv.value.len = p - uri.data;
            cv.value.data = uri.data;
            p++;
            args.len = (uri.data + uri.len) - p;
            args.data = p;
        }
    }

    for (i = 1; i < cf->args->nelts - n; i++) {
        err = ngx_array_push(clcf->error_pages);
        if (err == NULL) {
            return NGX_CONF_ERROR;
        }

        err->status = ngx_atoi(value[i].data, value[i].len);

        if (err->status == NGX_ERROR || err->status == 499) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "invalid value \"%V\"", &value[i]);
            return NGX_CONF_ERROR;
        }

        if (err->status < 300 || err->status > 599) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "value \"%V\" must be between 300 and 599",
                               &value[i]);
            return NGX_CONF_ERROR;
        }

        err->overwrite = overwrite;

        if (overwrite == -1) {
            switch (err->status) {
                case NGX_HTTP_TO_HTTPS:
                case NGX_HTTPS_CERT_ERROR:
                case NGX_HTTPS_NO_CERT:
                case NGX_HTTP_REQUEST_HEADER_TOO_LARGE:
                    err->overwrite = NGX_HTTP_BAD_REQUEST;
            }
        }

        err->value = cv;
        err->args = args;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_core_open_file_cache(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    time_t       inactive;
    ngx_str_t   *value, s;
    ngx_int_t    max;
    ngx_uint_t   i;

    if (clcf->open_file_cache != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    value = cf->args->elts;

    max = 0;
    inactive = 60;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "max=", 4) == 0) {

            max = ngx_atoi(value[i].data + 4, value[i].len - 4);
            if (max <= 0) {
                goto failed;
            }

            continue;
        }

        if (ngx_strncmp(value[i].data, "inactive=", 9) == 0) {

            s.len = value[i].len - 9;
            s.data = value[i].data + 9;

            inactive = ngx_parse_time(&s, 1);
            if (inactive == (time_t) NGX_ERROR) {
                goto failed;
            }

            continue;
        }

        if (ngx_strcmp(value[i].data, "off") == 0) {

            clcf->open_file_cache = NULL;

            continue;
        }

    failed:

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid \"open_file_cache\" parameter \"%V\"",
                           &value[i]);
        return NGX_CONF_ERROR;
    }

    if (clcf->open_file_cache == NULL) {
        return NGX_CONF_OK;
    }

    if (max == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                        "\"open_file_cache\" must have the \"max\" parameter");
        return NGX_CONF_ERROR;
    }

    clcf->open_file_cache = ngx_open_file_cache_init(cf->pool, max, inactive);
    if (clcf->open_file_cache) {
        return NGX_CONF_OK;
    }

    return NGX_CONF_ERROR;
}


static char *
ngx_http_core_error_log(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    return ngx_log_set_log(cf, &clcf->error_log);
}


static char *
ngx_http_core_keepalive(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t  *value;

    if (clcf->keepalive_timeout != NGX_CONF_UNSET_MSEC) {
        return "is duplicate";
    }

    value = cf->args->elts;

    clcf->keepalive_timeout = ngx_parse_time(&value[1], 0);

    if (clcf->keepalive_timeout == (ngx_msec_t) NGX_ERROR) {
        return "invalid value";
    }

    if (cf->args->nelts == 2) {
        return NGX_CONF_OK;
    }

    clcf->keepalive_header = ngx_parse_time(&value[2], 1);

    if (clcf->keepalive_header == (time_t) NGX_ERROR) {
        return "invalid value";
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_core_internal(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    if (clcf->internal != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    clcf->internal = 1;

    return NGX_CONF_OK;
}


static char *
ngx_http_core_resolver(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf = conf;

    ngx_str_t  *value;

    if (clcf->resolver) {
        return "is duplicate";
    }

    value = cf->args->elts;

    clcf->resolver = ngx_resolver_create(cf, &value[1], cf->args->nelts - 1);
    if (clcf->resolver == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


#if (NGX_HTTP_GZIP)

static char *
ngx_http_gzip_disable(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf = conf;

#if (NGX_PCRE)

    ngx_str_t            *value;
    ngx_uint_t            i;
    ngx_regex_elt_t      *re;
    ngx_regex_compile_t   rc;
    u_char                errstr[NGX_MAX_CONF_ERRSTR];

    if (clcf->gzip_disable == NGX_CONF_UNSET_PTR) {
        clcf->gzip_disable = ngx_array_create(cf->pool, 2,
                                              sizeof(ngx_regex_elt_t));
        if (clcf->gzip_disable == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.pool = cf->pool;
    rc.err.len = NGX_MAX_CONF_ERRSTR;
    rc.err.data = errstr;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strcmp(value[i].data, "msie6") == 0) {
            clcf->gzip_disable_msie6 = 1;
            continue;
        }

#if (NGX_HTTP_DEGRADATION)

        if (ngx_strcmp(value[i].data, "degradation") == 0) {
            clcf->gzip_disable_degradation = 1;
            continue;
        }

#endif

        re = ngx_array_push(clcf->gzip_disable);
        if (re == NULL) {
            return NGX_CONF_ERROR;
        }

        rc.pattern = value[i];
        rc.options = NGX_REGEX_CASELESS;

        if (ngx_regex_compile(&rc) != NGX_OK) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "%V", &rc.err);
            return NGX_CONF_ERROR;
        }

        re->regex = rc.regex;
        re->name = value[i].data;
    }

    return NGX_CONF_OK;

#else
    ngx_str_t   *value;
    ngx_uint_t   i;

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        if (ngx_strcmp(value[i].data, "msie6") == 0) {
            clcf->gzip_disable_msie6 = 1;
            continue;
        }

#if (NGX_HTTP_DEGRADATION)

        if (ngx_strcmp(value[i].data, "degradation") == 0) {
            clcf->gzip_disable_degradation = 1;
            continue;
        }

#endif

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "without PCRE library \"gzip_disable\" supports "
                           "builtin \"msie6\" and \"degradation\" mask only");

        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;

#endif
}

#endif


#if (NGX_HAVE_OPENAT)

static char *
ngx_http_disable_symlinks(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf = conf;

    ngx_str_t                         *value;
    ngx_uint_t                         i;
    ngx_http_compile_complex_value_t   ccv;

    if (clcf->disable_symlinks != NGX_CONF_UNSET_UINT) {
        return "is duplicate";
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strcmp(value[i].data, "off") == 0) {
            clcf->disable_symlinks = NGX_DISABLE_SYMLINKS_OFF;
            continue;
        }

        if (ngx_strcmp(value[i].data, "if_not_owner") == 0) {
            clcf->disable_symlinks = NGX_DISABLE_SYMLINKS_NOTOWNER;
            continue;
        }

        if (ngx_strcmp(value[i].data, "on") == 0) {
            clcf->disable_symlinks = NGX_DISABLE_SYMLINKS_ON;
            continue;
        }

        if (ngx_strncmp(value[i].data, "from=", 5) == 0) {
            value[i].len -= 5;
            value[i].data += 5;

            ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

            ccv.cf = cf;
            ccv.value = &value[i];
            ccv.complex_value = ngx_palloc(cf->pool,
                                           sizeof(ngx_http_complex_value_t));
            if (ccv.complex_value == NULL) {
                return NGX_CONF_ERROR;
            }

            if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            clcf->disable_symlinks_from = ccv.complex_value;

            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    if (clcf->disable_symlinks == NGX_CONF_UNSET_UINT) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"off\", \"on\" "
                           "or \"if_not_owner\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    if (cf->args->nelts == 2) {
        clcf->disable_symlinks_from = NULL;
        return NGX_CONF_OK;
    }

    if (clcf->disable_symlinks_from == NGX_CONF_UNSET_PTR) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "duplicate parameters \"%V %V\"",
                           &value[1], &value[2]);
        return NGX_CONF_ERROR;
    }

    if (clcf->disable_symlinks == NGX_DISABLE_SYMLINKS_OFF) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"from=\" cannot be used with \"off\" parameter");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

#endif


static char *
ngx_http_core_lowat_check(ngx_conf_t *cf, void *post, void *data)
{
#if (NGX_FREEBSD)
    ssize_t *np = data;

    if ((u_long) *np >= ngx_freebsd_net_inet_tcp_sendspace) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"send_lowat\" must be less than %d "
                           "(sysctl net.inet.tcp.sendspace)",
                           ngx_freebsd_net_inet_tcp_sendspace);

        return NGX_CONF_ERROR;
    }

#elif !(NGX_HAVE_SO_SNDLOWAT)
    ssize_t *np = data;

    ngx_conf_log_error(NGX_LOG_WARN, cf, 0,
                       "\"send_lowat\" is not supported, ignored");

    *np = 0;

#endif

    return NGX_CONF_OK;
}


static char *
ngx_http_core_pool_size(ngx_conf_t *cf, void *post, void *data)
{
    size_t *sp = data;

    if (*sp < NGX_MIN_POOL_SIZE) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the pool size must be no less than %uz",
                           NGX_MIN_POOL_SIZE);
        return NGX_CONF_ERROR;
    }

    if (*sp % NGX_POOL_ALIGNMENT) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "the pool size must be a multiple of %uz",
                           NGX_POOL_ALIGNMENT);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}
