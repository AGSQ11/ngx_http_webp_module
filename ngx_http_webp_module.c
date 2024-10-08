#include "ngx_http_webp_module.h"

static ngx_command_t ngx_http_webp_commands[] = {
    {
        ngx_string("ENGIWBP"),
        NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_webp_loc_conf_t, enable),
        NULL
    },
    {
        ngx_string("webp_quality"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_webp_loc_conf_t, quality),
        NULL
    },
    {
        ngx_string("webp_cache_time"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_sec_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_webp_loc_conf_t, cache_time),
        NULL
    },
    {
        ngx_string("webp_cache_dir"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_webp_loc_conf_t, cache_dir),
        NULL
    },
    {
        ngx_string("webp_max_image_size"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_size_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_webp_loc_conf_t, max_image_size),
        NULL
    },
    {
        ngx_string("webp_max_cache_size"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_size_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_webp_loc_conf_t, max_cache_size),
        NULL
    },
    {
        ngx_string("webp_files_per_cleanup"),
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_webp_loc_conf_t, files_per_cleanup),
        NULL
    },
    ngx_null_command
};

static ngx_http_module_t ngx_http_webp_module_ctx = {
    NULL,                          /* preconfiguration */
    ngx_http_webp_init,            /* postconfiguration */
    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */
    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */
    ngx_http_webp_create_loc_conf, /* create location configuration */
    ngx_http_webp_merge_loc_conf   /* merge location configuration */
};

ngx_module_t ngx_http_webp_module = {
    NGX_MODULE_V1,
    &ngx_http_webp_module_ctx,    /* module context */
    ngx_http_webp_commands,       /* module directives */
    NGX_HTTP_MODULE,              /* module type */
    NULL,                         /* init master */
    NULL,                         /* init module */
    ngx_http_webp_init_process,   /* init process */
    NULL,                         /* init thread */
    NULL,                         /* exit thread */
    NULL,                         /* exit process */
    NULL,                         /* exit master */
    NGX_MODULE_V1_PADDING
};

void *
ngx_http_webp_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_webp_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_webp_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->enable = NGX_CONF_UNSET;
    conf->quality = NGX_CONF_UNSET_UINT;
    conf->cache_time = NGX_CONF_UNSET_UINT;
    conf->max_image_size = NGX_CONF_UNSET_SIZE;
    conf->max_cache_size = NGX_CONF_UNSET_SIZE;
    conf->files_per_cleanup = NGX_CONF_UNSET_UINT;

    return conf;
}

char *
ngx_http_webp_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_webp_loc_conf_t *prev = parent;
    ngx_http_webp_loc_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_uint_value(conf->quality, prev->quality, 75);
    ngx_conf_merge_uint_value(conf->cache_time, prev->cache_time, 3600);
    ngx_conf_merge_str_value(conf->cache_dir, prev->cache_dir, "/var/cache/nginx/webp");
    ngx_conf_merge_size_value(conf->max_image_size, prev->max_image_size, 10 * 1024 * 1024);
    ngx_conf_merge_size_value(conf->max_cache_size, prev->max_cache_size, 1024 * 1024 * 1024);
    ngx_conf_merge_uint_value(conf->files_per_cleanup, prev->files_per_cleanup, 100);

    return NGX_CONF_OK;
}

ngx_int_t
ngx_http_webp_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt *h;
    ngx_http_core_main_conf_t *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_webp_handler;

    return NGX_OK;
}

ngx_int_t
ngx_http_webp_init_process(ngx_cycle_t *cycle)
{
    // Initialize any process-specific resources here
    return NGX_OK;
}

ngx_int_t
ngx_http_webp_limit_req(ngx_http_request_t *r)
{
    // Implement a simple rate limiting logic
    static ngx_atomic_t request_count = 0;
    static ngx_msec_t last_checked = 0;
    ngx_msec_t now = ngx_current_msec;
    ngx_msec_t elapsed = now - last_checked;
    ngx_int_t limit = 10; // Allow 10 requests per second

    if (elapsed > 1000) {
        request_count = 0;
        last_checked = now;
    }

    if (ngx_atomic_fetch_add(&request_count, 1) > limit) {
        return NGX_HTTP_TOO_MANY_REQUESTS;
    }

    return NGX_OK;
}