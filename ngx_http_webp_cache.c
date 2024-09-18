#include "ngx_http_webp_module.h"

ngx_int_t
ngx_http_webp_handler(ngx_http_request_t *r)
{
    ngx_http_webp_loc_conf_t *conf;
    ngx_str_t src_path, dst_path, *uri;
    u_char hash[SHA_DIGEST_LENGTH * 2 + 1];
    ngx_str_t cache_key;
    ngx_uint_t quality;
    ngx_str_t res;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_webp_module);

    if (!conf->enable || !(r->method & NGX_HTTP_GET)) {
        return NGX_DECLINED;
    }

    uri = &r->uri;
    if (!ngx_strstr(uri->data, ".jpg") && !ngx_strstr(uri->data, ".jpeg") && 
        !ngx_strstr(uri->data, ".png") && !ngx_strstr(uri->data, ".avif") && 
        !ngx_strstr(uri->data, ".jxl")) {
        return NGX_DECLINED;
    }

    ngx_http_variable_value_t *accept = ngx_http_get_variable(r, &ngx_http_accept_header_key, ngx_hash_key(ngx_http_accept_header_key.data, ngx_http_accept_header_key.len));
    if (accept == NULL || ngx_strstr(accept->data, "image/webp") == NULL) {
        NGX_HTTP_WEBP_LOG(NGX_LOG_DEBUG, r->connection->log, 0,
                          "Client does not support WebP");
        return NGX_DECLINED;
    }

    if (conf->convert_if) {
        if (ngx_http_complex_value(r, conf->convert_if, &res) != NGX_OK) {
            return NGX_ERROR;
        }

        if (res.len == 0 || (res.len == 1 && res.data[0] == '0')) {
            return NGX_DECLINED;
        }
    }

    if (conf->quality_if) {
        if (ngx_http_complex_value(r, conf->quality_if, &res) != NGX_OK) {
            return NGX_ERROR;
        }

        quality = ngx_atoi(res.data, res.len);
        if (quality == (ngx_uint_t) NGX_ERROR || quality > 100) {
            NGX_HTTP_WEBP_LOG(NGX_LOG_ERR, r->connection->log, 0,
                              "Invalid quality value: \"%V\"", &res);
            return NGX_ERROR;
        }
    } else {
        quality = conf->quality;
    }

    if (ngx_http_webp_limit_req(r) != NGX_OK) {
        return NGX_HTTP_TOO_MANY_REQUESTS;
    }

    ngx_sha1_t sha1;
    ngx_sha1_init(&sha1);
    ngx_sha1_update(&sha1, r->unparsed_uri.data, r->unparsed_uri.len);
    ngx_sha1_final(hash, &sha1);

    src_path = *uri;
    dst_path.len = conf->cache_dir.len + sizeof(hash) + 5;
    dst_path.data = ngx_pcalloc(r->pool, dst_path.len);
    ngx_sprintf(dst_path.data, "%V/%s.webp", &conf->cache_dir, hash);

    cache_key.data = hash;
    cache_key.len = sizeof(hash) - 1;

    if (ngx_http_webp_lookup_cache(r, &cache_key, &dst_path) == NGX_OK) {
        return ngx_http_webp_serve_file(r, &dst_path);
    }

    NGX_HTTP_WEBP_LOG(NGX_LOG_DEBUG, r->connection->log, 0,
                      "Converting image to WebP: %V", &src_path);

    ngx_int_t rc = ngx_http_webp_convert_image(r, &src_path, &dst_path);
    if (rc == NGX_ERROR) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    return ngx_http_webp_serve_file(r, &dst_path);
}

ngx_int_t
ngx_http_webp_lookup_cache(ngx_http_request_t *r, ngx_str_t *cache_key, ngx_str_t *file_path)
{
    ngx_http_webp_loc_conf_t *conf = ngx_http_get_module_loc_conf(r, ngx_http_webp_module);
    ngx_http_webp_shm_ctx_t *ctx = (ngx_http_webp_shm_ctx_t *)conf->cache_zone->data;
    ngx_slab_pool_t *shpool = (ngx_slab_pool_t *)conf->cache_zone->shm.addr;
    ngx_http_webp_cache_entry_t *entry;
    uint32_t hash;
    ngx_rbtree_node_t *node, *sentinel;

    hash = ngx_crc32_long(cache_key->data, cache_key->len);

    ngx_shmtx_lock(&shpool->mutex);

    node = ctx->rbtree.root;
    sentinel = ctx->rbtree.sentinel;

    while (node != sentinel) {
        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        entry = (ngx_http_webp_cache_entry_t *) node;

        if (ngx_memcmp(entry->key, cache_key->data, cache_key->len) == 0) {
            if (entry->expire < ngx_time()) {
                ngx_queue_remove(&entry->queue);
                ngx_rbtree_delete(&ctx->rbtree, node);
                ngx_slab_free_locked(shpool, entry);
                ngx_shmtx_unlock(&shpool->mutex);
                return NGX_DECLINED;
            }

            *file_path = entry->path;
            ngx_queue_remove(&entry->queue);
            ngx_queue_insert_head(&ctx->queue, &entry->queue);
            ngx_shmtx_unlock(&shpool->mutex);
            return NGX_OK;
        }

        break;
    }

    ngx_shmtx_unlock(&shpool->mutex);
    return NGX_DECLINED;
}

ngx_int_t
ngx_http_webp_store_cache(ngx_http_request_t *r, ngx_str_t *webp_path, uint8_t *webp_data, size_t webp_size)
{
    ngx_http_webp_loc_conf_t *conf = ngx_http_get_module_loc_conf(r, ngx_http_webp_module);
    ngx_http_webp_shm_ctx_t *ctx = (ngx_http_webp_shm_ctx_t *)conf->cache_zone->data;
    ngx_slab_pool_t *shpool = (ngx_slab_pool_t *)conf->cache_zone->shm.addr;
    ngx_http_webp_cache_entry_t *entry;
    ngx_rbtree_node_t *node;
    u_char key[SHA_DIGEST_LENGTH * 2];

    ngx_sha1_t sha1;
    ngx_sha1_init(&sha1);
    ngx_sha1_update(&sha1, webp_path->data, webp_path->len);
    ngx_sha1_final(key, &sha1);

    ngx_shmtx_lock(&shpool->mutex);

    entry = ngx_slab_alloc_locked(shpool, sizeof(ngx_http_webp_cache_entry_t));
    if (entry == NULL) {
        ngx_shmtx_unlock(&shpool->mutex);
        return NGX_ERROR;
    }

    node = (ngx_rbtree_node_t *)entry;
    node->key = ngx_crc32_long(key, sizeof(key));

    ngx_memcpy(entry->key, key, sizeof(key));
    entry->path = *webp_path;
    entry->expire = ngx_time() + conf->cache_time;

    ngx_rbtree_insert(&ctx->rbtree, node);
    ngx_queue_insert_head(&ctx->queue, &entry->queue);

    ngx_shmtx_unlock(&shpool->mutex);

    // Write the WebP file to disk
    ngx_file_t file;
    ngx_memzero(&file, sizeof(ngx_file_t));
    file.name = *webp_path;
    file.fd = ngx_open_file(webp_path->data, NGX_FILE_WRONLY, NGX_FILE_CREATE_OR_OPEN, 0644);

    if (file.fd == NGX_INVALID_FILE) {
        NGX_HTTP_WEBP_LOG(NGX_LOG_ERR, r->connection->log, ngx_errno,
                          "Failed to create WebP file: %V", webp_path);
        return NGX_ERROR;
    }

    ssize_t written = ngx_write_file(&file, webp_data, webp_size, 0);
    ngx_close_file(file.fd);

    if (written != webp_size) {
        NGX_HTTP_WEBP_LOG(NGX_LOG_ERR, r->connection->log, ngx_errno,
                          "Failed to write WebP file: %V", webp_path);
        return NGX_ERROR;
    }

    return NGX_OK;
}

ngx_int_t
ngx_http_webp_invalidate_cache(ngx_http_request_t *r)
{
    ngx_http_webp_loc_conf_t *conf = ngx_http_get_module_loc_conf(r, ngx_http_webp_module);
    ngx_http_webp_shm_ctx_t *ctx = (ngx_http_webp_shm_ctx_t *)conf->cache_zone->data;
    ngx_slab_pool_t *shpool = (ngx_slab_pool_t *)conf->cache_zone->shm.addr;
    ngx_str_t cache_key;
    ngx_http_webp_cache_entry_t *entry;
    ngx_rbtree_node_t *node, *sentinel;
    uint32_t hash;

    if (ngx_http_arg(r, (u_char *) "cache_key", 9, &cache_key) != NGX_OK) {
        return NGX_HTTP_BAD_REQUEST;
    }

    hash = ngx_crc32_long(cache_key.data, cache_key.len);

    ngx_shmtx_lock(&shpool->mutex);

    node = ctx->rbtree.root;
    sentinel = ctx->rbtree.sentinel;

    while (node != sentinel) {
        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        entry = (ngx_http_webp_cache_entry_t *) node;

        if (ngx_memcmp(entry->key, cache_key.data, cache_key.len) == 0) {
            ngx_queue_remove(&entry->queue);
            ngx_rbtree_delete(&ctx->rbtree, node);
            ngx_slab_free_locked(shpool, entry);
            ngx_shmtx_unlock(&shpool->mutex);

            // Delete the file from disk
            if (ngx_delete_file(entry->path.data) == NGX_FILE_ERROR) {
                NGX_HTTP_WEBP_LOG(NGX_LOG_ERR, r->connection->log, ngx_errno,
                                  "Failed to delete cache file: %V", &entry->path);
            }

            return NGX_HTTP_OK;
        }

        break;
    }

    ngx_shmtx_unlock(&shpool->mutex);
    return NGX_HTTP_NOT_FOUND;
}