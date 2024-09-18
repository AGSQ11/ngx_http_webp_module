#include "ngx_http_webp_module.h"

static ngx_command_t ngx_http_webp_commands[] = {
    { ngx_string("ENGIWBP"),
      NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_webp_loc_conf_t, enable),
      NULL },

    { ngx_string("webp_quality"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_webp_loc_conf_t, quality),
      NULL },

    { ngx_string("webp_cache_time"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_sec_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_webp_loc_conf_t, cache_time),
      NULL },

    { ngx_string("webp_cache_dir"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_webp_loc_conf_t, cache_dir),
      NULL },

    { ngx_string("webp_max_image_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_webp_loc_conf_t, max_image_size),
      NULL },

    { ngx_string("webp_max_cache_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_webp_loc_conf_t, max_cache_size),
      NULL },

    { ngx_string("webp_files_per_cleanup"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_webp_loc_conf_t, files_per_cleanup),
      NULL },

    { ngx_string("webp_convert_if"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_webp_set_complex_value_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_webp_loc_conf_t, convert_if),
      NULL },

    { ngx_string("webp_quality_if"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_webp_set_complex_value_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_webp_loc_conf_t, quality_if),
      NULL },

    { ngx_string("webp_rate_limit"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_webp_loc_conf_t, rate_limit),
      NULL },

    { ngx_string("webp_burst_limit"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_num_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_webp_loc_conf_t, burst_limit),
      NULL },

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
    ngx_http_webp_loc_conf_t  *conf;

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
    conf->rate_limit = NGX_CONF_UNSET_UINT;
    conf->burst_limit = NGX_CONF_UNSET_UINT;

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
    ngx_conf_merge_uint_value(conf->rate_limit, prev->rate_limit, 10);
    ngx_conf_merge_uint_value(conf->burst_limit, prev->burst_limit, 20);

    if (conf->convert_if == NULL) {
        conf->convert_if = prev->convert_if;
    }

    if (conf->quality_if == NULL) {
        conf->quality_if = prev->quality_if;
    }

    return NGX_CONF_OK;
}

ngx_int_t
ngx_http_webp_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_webp_handler;

    return NGX_OK;
}

char *
ngx_http_webp_set_complex_value_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    char *p = conf;
    ngx_http_complex_value_t **cv = (ngx_http_complex_value_t **) (p + cmd->offset);
    ngx_str_t *value;
    ngx_http_compile_complex_value_t ccv;

    value = cf->args->elts;

    if (*cv != NULL) {
        return "is duplicate";
    }

*cv = ngx_palloc(cf->pool, sizeof(ngx_http_complex_value_t));
    if (*cv == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = *cv;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}