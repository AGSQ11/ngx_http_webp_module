#include "ngx_http_webp_module.h"

static void
ngx_http_webp_convert_thread_handler(void *data, ngx_log_t *log)
{
    ngx_http_webp_convert_ctx_t *ctx = data;
    uint8_t *raw_data = NULL;
    int width, height;
    uint8_t *webp_data = NULL;
    size_t webp_size;

    if (ngx_strstr(ctx->src_path.data, ".png")) {
        raw_data = WebPDecodePNG(ctx->image_data, ctx->image_size, &width, &height);
    } else if (ngx_strstr(ctx->src_path.data, ".jpg") || ngx_strstr(ctx->src_path.data, ".jpeg")) {
        raw_data = WebPDecodeJPEG(ctx->image_data, ctx->image_size, &width, &height);
    } else if (ngx_strstr(ctx->src_path.data, ".avif")) {
        avifDecoder *decoder = avifDecoderCreate();
        avifResult result = avifDecoderSetIOMemory(decoder, ctx->image_data, ctx->image_size);
        if (result == AVIF_RESULT_OK) {
            result = avifDecoderParse(decoder);
            if (result == AVIF_RESULT_OK) {
                avifRGBImage rgb;
                avifRGBImageSetDefaults(&rgb, decoder->image);
                rgb.format = AVIF_RGB_FORMAT_RGBA;
                rgb.depth = 8;
                raw_data = ngx_palloc(ctx->pool, rgb.rowBytes * rgb.height);
                rgb.pixels = raw_data;
                result = avifImageYUVToRGB(decoder->image, &rgb);
                if (result == AVIF_RESULT_OK) {
                    width = rgb.width;
                    height = rgb.height;
                }
            }
        }
        avifDecoderDestroy(decoder);
    }
#ifdef NGX_HTTP_WEBP_JXL_ENABLED
    else if (ngx_strstr(ctx->src_path.data, ".jxl")) {
        JxlDecoder *decoder = JxlDecoderCreate(NULL);
        JxlBasicInfo info;
        JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};
        JxlDecoderSetInput(decoder, ctx->image_data, ctx->image_size);
        if (JxlDecoderGetBasicInfo(decoder, &info) == JXL_DEC_SUCCESS) {
            size_t buffer_size = info.xsize * info.ysize * 4;
            raw_data = ngx_palloc(ctx->pool, buffer_size);
            JxlDecoderSetImageOutBuffer(decoder, &format, raw_data, buffer_size);
            JxlDecoderProcessInput(decoder);
            width = info.xsize;
            height = info.ysize;
        }
        JxlDecoderDestroy(decoder);
    }
#endif

    if (raw_data == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "Failed to decode image: %V", &ctx->src_path);
        ctx->result = NGX_ERROR;
        return;
    }

    webp_size = WebPEncodeRGBA(raw_data, width, height, width * 4, ctx->quality, &webp_data);
    if (raw_data != ctx->image_data) {
        ngx_pfree(ctx->pool, raw_data);
    }

    if (webp_data == NULL) {
        ngx_log_error(NGX_LOG_ERR, log, 0, "Failed to encode WebP image: %V", &ctx->src_path);
        ctx->result = NGX_ERROR;
        return;
    }

    ctx->webp_data = webp_data;
    ctx->webp_size = webp_size;
    ctx->result = NGX_OK;
}

ngx_int_t
ngx_http_webp_convert_image(ngx_http_request_t *r, ngx_str_t *src_path, ngx_str_t *dst_path)
{
    ngx_http_webp_loc_conf_t *conf;
    ngx_http_webp_convert_ctx_t *ctx;
    ngx_thread_task_t *task;
    ngx_thread_pool_t *tp;
    ngx_file_t file;
    size_t size;
    ssize_t n;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_webp_module);

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_webp_convert_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    file.name = *src_path;
    file.log = r->connection->log;
    file.fd = ngx_open_file(src_path->data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);

    if (file.fd == NGX_INVALID_FILE) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "Failed to open image file: %V", src_path);
        return NGX_ERROR;
    }

    if (ngx_file_info(file.name.data, &file.info) == NGX_FILE_ERROR) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "Failed to stat file: %V", src_path);
        ngx_close_file(file.fd);
        return NGX_ERROR;
    }

    size = ngx_file_size(&file.info);
    if (size > conf->max_image_size) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "Image file too large: %V, size: %uz, max allowed: %uz",
                      src_path, size, conf->max_image_size);
        ngx_close_file(file.fd);
        return NGX_HTTP_REQUEST_ENTITY_TOO_LARGE;
    }

    ctx->image_data = ngx_pnalloc(r->pool, size);
    if (ctx->image_data == NULL) {
        ngx_close_file(file.fd);
        return NGX_ERROR;
    }

    n = ngx_read_file(&file, ctx->image_data, size, 0);
    ngx_close_file(file.fd);

    if (n == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "Failed to read image file: %V", src_path);
        return NGX_ERROR;
    }

    ctx->src_path = *src_path;
    ctx->dst_path = *dst_path;
    ctx->image_size = size;
    ctx->quality = conf->quality;
    ctx->pool = r->pool;

    task = ngx_thread_task_alloc(r->pool, sizeof(ngx_http_webp_convert_ctx_t));
    if (task == NULL) {
        return NGX_ERROR;
    }

    task->handler = ngx_http_webp_convert_thread_handler;
    task->ctx = ctx;

    tp = ngx_thread_pool_get((ngx_cycle_t *) ngx_cycle, NULL);
    if (tp == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "Failed to get thread pool");
        return NGX_ERROR;
    }

    if (ngx_thread_task_post(tp, task) != NGX_OK) {
        return NGX_ERROR;
    }

    r->main->blocked++;
    r->aio = 1;

    return NGX_AGAIN;
}

ngx_int_t
ngx_http_webp_serve_file(ngx_http_request_t *r, ngx_str_t *path)
{
    ngx_int_t rc;
    ngx_buf_t *b;
    ngx_chain_t out;
    ngx_open_file_info_t of;
    ngx_http_core_loc_conf_t *clcf;

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