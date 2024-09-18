#include "ngx_http_webp_module.h"

static ngx_int_t
ngx_http_webp_init_process(ngx_cycle_t *cycle)
{
    ngx_http_webp_loc_conf_t *conf;
    ngx_http_conf_ctx_t *ctx;
    ngx_event_t *ev;
    
    ctx = (ngx_http_conf_ctx_t *)cycle->conf_ctx[ngx_http_module.index];
    conf = ctx->loc_conf[ngx_http_webp_module.ctx_index];
    
    if (ngx_http_webp_init_shm_zone(conf->cache_zone, NULL) != NGX_OK) {
        return NGX_ERROR;
    }
    
    // Ensure cache directory exists and has correct permissions
    if (ngx_create_dir(conf->cache_dir.data, 0700) == NGX_FILE_ERROR) {
        if (ngx_errno != NGX_EEXIST) {
            ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                          "Failed to create WebP cache directory: %V", &conf->cache_dir);
            return NGX_ERROR;
        }
    }

    // Set restrictive permissions on existing directory
    if (chmod((const char *)conf->cache_dir.data, 0700) == -1) {
        ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_errno,
                      "Failed to set permissions on WebP cache directory: %V", &conf->cache_dir);
        return NGX_ERROR;
    }

    ev = ngx_pcalloc(cycle->pool, sizeof(ngx_event_t));
    if (ev == NULL) {
        return NGX_ERROR;
    }
    
    ev->handler = ngx_http_webp_cleanup_cache;
    ev->data = conf;
    ev->log = cycle->log;
    
    ngx_add_timer(ev, conf->cache_time * 1000 / conf->files_per_cleanup);
    
    return NGX_OK;
}

static ngx_int_t
ngx_http_webp_init_shm_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_http_webp_shm_ctx_t *ctx;
    ngx_slab_pool_t *shpool = (ngx_slab_pool_t *)shm_zone->shm.addr;

    if (data) {
        shm_zone->data = data;
        return NGX_OK;
    }

    ctx = ngx_slab_alloc(shpool, sizeof(ngx_http_webp_shm_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_rbtree_init(&ctx->rbtree, &ctx->sentinel, ngx_rbtree_insert_value);
    ngx_queue_init(&ctx->queue);

    shm_zone->data = ctx;
    return NGX_OK;
}

static void
ngx_http_webp_cleanup_cache(ngx_event_t *ev)
{
    ngx_http_webp_loc_conf_t *conf = ev->data;
    ngx_dir_t dir;
    time_t now = ngx_time();
    ngx_uint_t files_processed = 0;
    size_t total_size = 0;
    ngx_uint_t batch_size = 100;  // Process 100 files per batch
    
    if (ngx_open_dir(conf->cache_dir.data, &dir) != NGX_OK) {
        ngx_log_error(NGX_LOG_ERR, ev->log, 0, "Failed to open WebP cache directory: %V", &conf->cache_dir);
        goto done;
    }
    
    while (files_processed < conf->files_per_cleanup) {
        for (ngx_uint_t i = 0; i < batch_size && files_processed < conf->files_per_cleanup; i++) {
            ngx_err_t err = ngx_read_dir(&dir);
            if (err != 0) {
                break;
            }
            
            if (ngx_de_name(&dir)[0] == '.') {
                continue;
            }
            
            u_char *file_path = ngx_palloc(ev->pool, conf->cache_dir.len + ngx_strlen(ngx_de_name(&dir)) + 2);
            if (file_path == NULL) {
                continue;
            }
            
            ngx_sprintf(file_path, "%V/%s", &conf->cache_dir, ngx_de_name(&dir));
            
            ngx_file_info_t fi;
            if (ngx_file_info(file_path, &fi) == NGX_FILE_ERROR) {
                continue;
            }
            
            total_size += ngx_file_size(&fi);
            
            if (now - ngx_file_mtime(&fi) > conf->cache_time || total_size > conf->max_cache_size) {
                ngx_log_debug1(NGX_LOG_DEBUG_HTTP, ev->log, 0,
                               "Deleting WebP cache file: %s", file_path);
                ngx_delete_file(file_path);
                total_size -= ngx_file_size(&fi);
            }
            
            files_processed++;
        }
        
        // Yield to allow other events to be processed
        ngx_event_process_posted((ngx_cycle_t *) ngx_cycle);
    }
    
    ngx_close_dir(&dir);

done:
    // Schedule the next cleanup
    ev->handler = ngx_http_webp_cleanup_cache;
    ev->data = conf;
    ngx_add_timer(ev, conf->cache_time * 1000 / conf->files_per_cleanup);
}

static ngx_int_t
ngx_http_webp_limit_req(ngx_http_request_t *r)
{
    ngx_http_webp_loc_conf_t *conf;
    static ngx_atomic_t request_count = 0;
    static ngx_msec_t last_checked = 0;
    ngx_msec_t now, elapsed;
    ngx_int_t excess;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_webp_module);
    now = ngx_current_msec;
    elapsed = now - last_checked;

    if (elapsed > 1000) {
        request_count = conf->rate_limit * elapsed / 1000 - request_count;
        if ((ngx_int_t)request_count < 0) {
            request_count = 0;
        }
        last_checked = now;
    }

    excess = request_count - conf->rate_limit;

    if (excess < conf->burst_limit) {
        request_count++;
        return NGX_OK;
    } else {
        ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
                      "Limiting requests, excess: %i", excess);
        return NGX_HTTP_TOO_MANY_REQUESTS;
    }
}

static char *
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