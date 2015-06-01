#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define HELLO_WORLD "hello world\n"

static char *ngx_http_hello_world(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_hello_world_handler(ngx_http_request_t *r);

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

static u_char ngx_hello_world[] = HELLO_WORLD;

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

static void create_ev(ngx_event_t *rev, ngx_msec_t time) {
    ngx_add_timer(rev, time);
}

static void delete_ev(ngx_event_t *rev) {
    if(rev->timer_set) {
        ngx_del_timer(rev);
    }
}

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

    if(ctx->chunks_to_send > 0 && !ctx->request->connection->write->error) {
        printf("Adding timer\n");
        delete_ev(&ctx->wev);
        create_ev(&ctx->wev, (ngx_msec_t)1000);
    } else {
    //    ctx->request->main->count--;
        ngx_http_finalize_request(r, NGX_DONE);
    }
}

static void
ngx_http_hello_cleanup(void *data)
{

printf("3.1. XXXXXX ngx_http_hello_cleanup\n");
    ngx_http_echo_ctx_t *ctx = (ngx_http_echo_ctx_t *) data;
    ctx->wev.timedout = 0;
    delete_ev(&ctx->wev);
}

ngx_http_echo_ctx_t *
ngx_http_echo_create_ctx(ngx_http_request_t *r)
{
    ngx_http_echo_ctx_t         *ctx;

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_echo_ctx_t));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->request = r;
    ctx->chunks_to_send = 10;

    ngx_http_cleanup_t *cln = ngx_http_cleanup_add(r, 0);
    cln->handler = ngx_http_hello_cleanup;
    cln->data = ctx;

    return ctx;
}

static ngx_int_t ngx_http_hello_world_handler(ngx_http_request_t *r)
{
    r->request_body_no_buffering = 1;
    r->buffered = 0;

    ngx_http_echo_ctx_t *ctx;
    ctx = ngx_http_echo_create_ctx(r);

    ctx->wev.handler   = echo_reading_callback;
    ctx->wev.data      = ctx;
    ctx->wev.log       = r->connection->log;

    create_ev(&ctx->wev, (ngx_msec_t)1000);

    r->main->count++;
    return NGX_AGAIN;
} /* ngx_http_hello_world_handler */

static char *ngx_http_hello_world(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t *clcf; /* pointer to core location configuration */

    /* Install the hello world handler. */
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_hello_world_handler;

    return NGX_CONF_OK;
} /* ngx_http_hello_world */
