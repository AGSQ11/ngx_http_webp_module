# NGINX ENGIWBP Module - Still pre-Alpha - lots of bugs. :)

## Overview

The NGINX ENGIWBP (ENGINYRING WebP) module is a dynamic image conversion and optimization solution for NGINX. It provides on-the-fly conversion of JPEG, PNG, and AVIF images to WebP format, with intelligent caching and serving capabilities. JPEG XL support is optional and can be enabled or disabled during compilation.

## Features

- On-the-fly conversion of JPEG, PNG, and AVIF to WebP
- Optional JPEG XL support (can be enabled/disabled during compilation)
- Configurable WebP quality settings
- Intelligent caching mechanism
- Rate limiting to prevent abuse
- Conditional conversion based on client capabilities
- Custom header for easy identification of processed images

## Requirements

- NGINX (version 1.x.x or higher)
- libwebp
- libavif
- PCRE library
- OpenSSL library
- zlib library
- (Optional) libjxl for JPEG XL support

## Installation

1. Install the required dependencies:

   ```bash
   sudo apt-get update
   sudo apt-get install build-essential libpcre3 libpcre3-dev zlib1g zlib1g-dev libssl-dev libwebp-dev libavif-dev
   ```

2. (Optional) Install JPEG XL support:
   
   If you want JPEG XL support, install libjxl-dev if available in your repositories:
   
   ```bash
   sudo apt-get install libjxl-dev
   ```
   
   If not available, you may need to compile it from source. Refer to the JPEG XL project documentation for instructions.

3. Download and extract the NGINX source:

   ```bash
   wget http://nginx.org/download/nginx-x.x.x.tar.gz
   tar -xzvf nginx-x.x.x.tar.gz
   cd nginx-x.x.x
   ```

   Replace `x.x.x` with your NGINX version.

4. Clone the ENGIWBP module repository:

   ```bash
   git clone https://github.com/AGSQ11/ngx_http_webp_module.git
   ```

5. Configure NGINX with the module:

   With JPEG XL support (default if libjxl is available):
   ```bash
   ./configure --add-module=../ngx_http_webp_module --with-threads
   ```

   Without JPEG XL support:
   ```bash
   ./configure --add-module=../ngx_http_webp_module --with-threads --with-http_webp_jxl=no
   ```

   Add any other configuration options you typically use.

6. Compile and install NGINX:

   ```bash
   make
   sudo make install
   ```
   
## Configuration

Add the following directives to your NGINX configuration file within a `server` or `location` block:

```nginx
ENGIWBP on;
webp_quality 80;
webp_convert_if $http_accept ~* "image/webp";
webp_quality_if $arg_quality 90;
webp_cache_time 1h;
webp_cache_dir /path/to/cache;
webp_max_image_size 15M;
webp_max_cache_size 1G;
webp_files_per_cleanup 100;
webp_rate_limit 10r/s;
```

### Directive Descriptions

- `ENGIWBP on|off`: Enables or disables the module.
- `webp_quality`: Sets the default WebP quality (0-100).
- `webp_convert_if`: Specifies a condition for conversion.
- `webp_quality_if`: Allows dynamic quality setting based on a condition.
- `webp_cache_time`: Sets the cache duration for converted images.
- `webp_cache_dir`: Specifies the directory for caching WebP images.
- `webp_max_image_size`: Sets the maximum size of images to convert.
- `webp_max_cache_size`: Sets the maximum size of the cache.
- `webp_files_per_cleanup`: Sets the number of files to process in each cleanup cycle.
- `webp_rate_limit`: Sets a limit on conversion requests per second.

## Usage Example

```nginx
http {
    ...

    server {
        listen 80;
        server_name example.com;

        location /images/ {
            ENGIWBP on;
            webp_quality 80;
            webp_convert_if $http_accept ~* "image/webp";
            webp_quality_if $arg_quality 90;
            webp_cache_time 1h;
            webp_cache_dir /var/cache/nginx/webp;
            webp_max_image_size 15M;
            webp_max_cache_size 1G;
            webp_files_per_cleanup 100;
            webp_rate_limit 10r/s;
        }
    }
}
```

This configuration will convert images in the `/images/` location to WebP format if the client supports it, cache the results, and serve the WebP version on subsequent requests.

## Testing

After configuring the module, you can test it by:

1. Placing some test images (JPEG, PNG, AVIF, and JPEG XL if supported) in your `/images/` directory.
2. Accessing these images through a web browser that supports WebP.
3. Checking the response headers for the `X-Optimization` header, which should be set to `ENGINYRING WebP Conversion Module 1.0`.
4. Verifying that the images are served in WebP format when supported.

## Troubleshooting

- Ensure NGINX has read permissions for source images and write permissions for the cache directory.
- Check NGINX error logs for any module-specific issues.
- Verify that all required libraries are correctly installed and linked.
- If you're having issues with JPEG XL support, consider building the module without JPEG XL support as described in the installation section.

## Contributing

Contributions to the ENGIWBP module are welcome! Please submit pull requests or open issues on the GitHub repository.

## License

See LICENSE file.

## Support

For support, please open an issue on the GitHub repository or contact us.
