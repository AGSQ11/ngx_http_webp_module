#include "ngx_http_webp_module.h"

static ngx_int_t
ngx_http_webp_add_custom_header(ngx_http_request_t *r)
{
    ngx_table_elt_t   *h;

    h = ngx_list_push(&r->headers_out.headers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    h->hash = 1;
    ngx_str_set(&h->key, "X-Optimization");
    ngx_str_set(&h->value, "ENGINYRING WebP Conversion Module 1.0");

    return NGX_OK;
}

ngx_int_t
ngx_http_webp_serve_file(ngx_http_request_t *r, ngx_str_t *path)
{
    ngx_int_t rc;
    ngx_buf_t *b;
    ngx_chain_t out;
    ngx_open_file_info_t of;
    ngx_http_core_loc_conf_t *clcf;

    // Add custom header
    rc = ngx_http_webp_add_custom_header(r);
    if (rc != NGX_OK) {
        return rc;
    }

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

    ngx_memzero(&of, sizeof(ngx_open_file_info_t));

    of.read_ahead = clcf->read_ahead;
    of.directio = clcf->directio;
    of.valid = clcf->open_file_cache_valid;
    of.min_uses = clcf->open_file_cache_min_uses;
    of.errors = clcf->open_file_cache_errors;
    of.events = clcf->open_file_cache_events;

    if (ngx_open_cached_file(clcf->open_file_cache, path, &of, r->pool) != NGX_OK) {
        switch (of.err) {
        case 0:
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        case NGX_ENOENT:
        case NGX_ENOTDIR:
        case NGX_ENAMETOOLONG:
            return NGX_HTTP_NOT_FOUND;
        case NGX_EACCES:
            return NGX_HTTP_FORBIDDEN;
        default:
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    r->root_tested = !r->error_page;

    if (!of.is_file) {
        return NGX_DECLINED;
    }

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = of.size;
    r->headers_out.last_modified_time = of.mtime;

    if (ngx_http_set_etag(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_http_set_content_type(r) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
    if (b->file == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    b->file_pos = 0;
    b->file_last = of.size;

    b->in_file = b->file_last ? 1 : 0;
    b->last_buf = (r == r->main) ? 1 : 0;
    b->last_in_chain = 1;

    b->file->fd = of.fd;
    b->file->name = *path;
    b->file->log = r->connection->log;
    b->file->directio = of.is_directio;

    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}