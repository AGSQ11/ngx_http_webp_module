#ifndef _NGX_HTTP_WEBP_MODULE_H_INCLUDED_
#define _NGX_HTTP_WEBP_MODULE_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <webp/encode.h>
#include <webp/decode.h>
#include <avif/avif.h>
#include <jxl/encode.h>
#include <jxl/decode.h>

#define NGX_HTTP_WEBP_LOG(level, log, err, fmt, ...) \
    ngx_log_error(level, log, err, "[ngx_http_webp_module] " fmt, ##__VA_ARGS__)

typedef struct {
    ngx_flag_t enable;
    ngx_uint_t quality;
    ngx_uint_t cache_time;
    ngx_str_t cache_dir;
    size_t max_image_size;
    size_t max_cache_size;
    ngx_uint_t files_per_cleanup;
    ngx_shm_zone_t *cache_zone;
    ngx_http_complex_value_t *convert_if;
    ngx_http_complex_value_t *quality_if;
    ngx_uint_t rate_limit;
    ngx_uint_t burst_limit;
} ngx_http_webp_loc_conf_t;

typedef struct {
    ngx_rbtree_t rbtree;
    ngx_rbtree_node_t sentinel;
    ngx_queue_t queue;
} ngx_http_webp_shm_ctx_t;

typedef struct {
    ngx_rbtree_node_t node;
    ngx_queue_t queue;
    u_char key[SHA256_DIGEST_LENGTH * 2];
    ngx_str_t path;
    time_t expire;
} ngx_http_webp_cache_entry_t;

typedef struct {
    ngx_str_t src_path;
    ngx_str_t dst_path;
    u_char *image_data;
    size_t image_size;
    ngx_uint_t quality;
    uint8_t *webp_data;
    size_t webp_size;
    ngx_int_t result;
    ngx_pool_t *pool;
} ngx_http_webp_convert_ctx_t;

// Function prototypes
static void *ngx_http_webp_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_webp_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_webp_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_webp_handler(ngx_http_request_t *r);
static void ngx_http_webp_cleanup_cache(ngx_event_t *ev);
static ngx_int_t ngx_http_webp_init_process(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_webp_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data);
static void ngx_http_webp_convert_thread_handler(void *data, ngx_log_t *log);
static ngx_int_t ngx_http_webp_convert_image(ngx_http_request_t *r, ngx_str_t *src_path, ngx_str_t *dst_path);
static ngx_int_t ngx_http_webp_lookup_cache(ngx_http_request_t *r, ngx_str_t *cache_key, ngx_str_t *file_path);
static ngx_int_t ngx_http_webp_store_cache(ngx_http_request_t *r, ngx_str_t *webp_path, uint8_t *webp_data, size_t webp_size);
static ngx_int_t ngx_http_webp_serve_file(ngx_http_request_t *r, ngx_str_t *path);
static char *ngx_http_webp_set_complex_value_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_webp_limit_req(ngx_http_request_t *r);
static ngx_int_t ngx_http_webp_invalidate_cache(ngx_http_request_t *r);

#endif /* _NGX_HTTP_WEBP_MODULE_H_INCLUDED_ */