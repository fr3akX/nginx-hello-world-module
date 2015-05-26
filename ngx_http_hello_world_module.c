/**
 * @file   ngx_http_hello_world_module.c
 * @author António P. P. Almeida <appa@perusio.net>
 * @date   Wed Aug 17 12:06:52 2011
 *
 * @brief  A hello world module for Nginx.
 *
 * @section LICENSE
 *
 * Copyright (C) 2011 by Dominic Fallows, António P. P. Almeida <appa@perusio.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define HELLO_WORLD "hello world\n"

static char *ngx_http_hello_world(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_hello_world_handler(ngx_http_request_t *r);

/**
 * This module provided directive: hello world.
 *
 */
static ngx_command_t ngx_http_hello_world_commands[] = {

    { ngx_string("hello_world"), /* directive */
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS, /* location context and takes
                                            no arguments*/
      ngx_http_hello_world, /* configuration setup function */
      0, /* No offset. Only one context is supported. */
      0, /* No offset when storing the module configuration on struct. */
      NULL},

    ngx_null_command /* command termination */
};

/* The hello world string. */
static u_char ngx_hello_world[] = HELLO_WORLD;

/* The module context. */
static ngx_http_module_t ngx_http_hello_world_module_ctx = {
    NULL, /* preconfiguration */
    NULL, /* postconfiguration */

    NULL, /* create main configuration */
    NULL, /* init main configuration */

    NULL, /* create server configuration */
    NULL, /* merge server configuration */

    NULL, /* create location configuration */
    NULL /* merge location configuration */
};

/* Module definition. */
ngx_module_t ngx_http_hello_world_module = {
    NGX_MODULE_V1,
    &ngx_http_hello_world_module_ctx, /* module context */
    ngx_http_hello_world_commands, /* module directives */
    NGX_HTTP_MODULE, /* module type */
    NULL, /* init master */
    NULL, /* init module */
    NULL, /* init process */
    NULL, /* init thread */
    NULL, /* exit thread */
    NULL, /* exit process */
    NULL, /* exit master */
    NGX_MODULE_V1_PADDING
};

typedef struct {
    ngx_event_t wev;
    ngx_http_request_t *request;
    unsigned int chunks_to_send;
} ngx_http_echo_ctx_t;

static
void echo_reading_callback(ngx_event_t *wev)
{

    ngx_http_request_t *r;
    ngx_http_echo_ctx_t *ctx;
    ngx_buf_t *b;
    ngx_chain_t out;

    ctx = (ngx_http_echo_ctx_t *) wev->data;
    r = ctx->request;

    printf("In callvback, cunks to send: %u\n", ctx->chunks_to_send);

    if(!r->header_sent) {
        printf("Sending headers\n");

        /* Set the Content-Type header. */
        r->headers_out.content_type.len = sizeof("text/plain") - 1;
        r->headers_out.content_type.data = (u_char *) "text/plain";

        /* Sending the headers for the reply. */
        r->headers_out.status = NGX_HTTP_OK; /* 200 status code */
        /* Get the content length of the body. */
        r->headers_out.content_length_n = sizeof(ngx_hello_world) * ctx->chunks_to_send;
        ngx_http_send_header(r); /* Send the headers */
    }

    ctx->chunks_to_send--;

    /* Allocate a new buffer for sending out the reply. */
    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));

    /* Insertion in the buffer chain. */
    out.buf = b;
    out.next = NULL; /* just one buffer */

    b->pos = ngx_hello_world; /* first position in memory of the data */
    b->last = ngx_hello_world + sizeof(ngx_hello_world); /* last position in memory of the data */
    b->memory = 1;
    b->flush = 1;
    b->last_buf = ctx->chunks_to_send == 0; /* there will be no more buffers in the request */

    /* Send the body, and return the status code of the output filter chain. */
    ngx_http_output_filter(r, &out);

    if(ctx->chunks_to_send > 0) {
        printf("Adding timer\n");
        ngx_add_timer(&ctx->wev, (ngx_msec_t)1000);
    } else {
        printf("Finalizing request\n");
        ngx_http_finalize_request(r, NGX_OK);
    }

}

static void
ngx_http_hello_cleanup(void *data)
{
    ngx_http_echo_ctx_t *state = (ngx_http_echo_ctx_t *) data;

    if(state->wev.timer_set) {
        printf("Deleting timer\n");
        ngx_del_timer(&state->wev);
        return;
    }else{
        printf("Not deleting timer\n");
    }
}

ngx_http_echo_ctx_t *
ngx_http_echo_create_ctx(ngx_http_request_t *r)
{
    ngx_http_echo_ctx_t         *ctx;

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_echo_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->wev.handler   = echo_reading_callback;
    ctx->wev.data      = ctx;
    ctx->wev.log       = r->connection->log;
    ctx->request = r;
    ctx->chunks_to_send = 10;

    ngx_http_cleanup_t *cln = ngx_http_cleanup_add(r, 0);
    cln->handler = ngx_http_hello_cleanup;
    cln->data = ctx;

    return ctx;
}

/**
 * Content handler.
 *
 * @param r
 *   Pointer to the request structure. See http_request.h.
 * @return
 *   The status of the response generation.
 */
static ngx_int_t ngx_http_hello_world_handler(ngx_http_request_t *r)
{
    r->request_body_no_buffering = 1;
    r->buffered = 0;

    ngx_http_echo_ctx_t *ctx;
    ctx = ngx_http_echo_create_ctx(r);
    ngx_add_timer(&ctx->wev, (ngx_msec_t)1000);
    r->main->count++;
    return NGX_DONE;
} /* ngx_http_hello_world_handler */

/**
 * Configuration setup function that installs the content handler.
 *
 * @param cf
 *   Module configuration structure pointer.
 * @param cmd
 *   Module directives structure pointer.
 * @param conf
 *   Module configuration structure pointer.
 * @return string
 *   Status of the configuration setup.
 */
static char *ngx_http_hello_world(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf; /* pointer to core location configuration */

    /* Install the hello world handler. */
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_hello_world_handler;

    return NGX_CONF_OK;
} /* ngx_http_hello_world */
