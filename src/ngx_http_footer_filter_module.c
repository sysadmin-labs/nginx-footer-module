/*
 * Appends a piece of text to a response body.
 *
 * The directives are the ones the alibaba module of the same name has
 * carried since 2010, so a configuration written for it keeps working:
 * "footer" takes the text, which may contain variables, and
 * "footer_types" says which content types it applies to.
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_http_complex_value_t  *text;        /* NULL when not configured */
    ngx_flag_t                 off;         /* footer ""; turns it off */
    ngx_hash_t                 types;
    ngx_array_t               *types_keys;
} ngx_http_footer_loc_conf_t;


typedef struct {
    ngx_str_t   text;
    unsigned    appended:1;
} ngx_http_footer_ctx_t;


static char *ngx_http_footer_set(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static void *ngx_http_footer_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_footer_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);
static ngx_int_t ngx_http_footer_filter_init(ngx_conf_t *cf);


static ngx_command_t  ngx_http_footer_filter_commands[] = {

    { ngx_string("footer"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_footer_set,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("footer_types"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_types_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_footer_loc_conf_t, types_keys),
      &ngx_http_html_default_types[0] },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_footer_filter_module_ctx = {
    NULL,                               /* preconfiguration */
    ngx_http_footer_filter_init,        /* postconfiguration */

    NULL,                               /* create main configuration */
    NULL,                               /* init main configuration */

    NULL,                               /* create server configuration */
    NULL,                               /* merge server configuration */

    ngx_http_footer_create_loc_conf,    /* create location configuration */
    ngx_http_footer_merge_loc_conf      /* merge location configuration */
};


ngx_module_t  ngx_http_footer_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_footer_filter_module_ctx, /* module context */
    ngx_http_footer_filter_commands,    /* module directives */
    NGX_HTTP_MODULE,                    /* module type */
    NULL,                               /* init master */
    NULL,                               /* init module */
    NULL,                               /* init process */
    NULL,                               /* init thread */
    NULL,                               /* exit thread */
    NULL,                               /* exit process */
    NULL,                               /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


static ngx_int_t
ngx_http_footer_header_filter(ngx_http_request_t *r)
{
    ngx_str_t                    text;
    ngx_http_footer_ctx_t       *ctx;
    ngx_http_footer_loc_conf_t  *flcf;

    flcf = ngx_http_get_module_loc_conf(r, ngx_http_footer_filter_module);

    /*
     * A subrequest is left alone: its body is assembled into the main
     * response, and a footer per subrequest would appear in the middle of
     * it.  The statuses below carry no body at all, and a HEAD answer
     * carries only the header, whose length must still match what a GET
     * would send -- that is why the length is corrected rather than the
     * footer skipped.
     */
    if (flcf->text == NULL
        || r != r->main
        || r->headers_out.status == NGX_HTTP_NO_CONTENT
        || r->headers_out.status == NGX_HTTP_NOT_MODIFIED
        || ngx_http_test_content_type(r, &flcf->types) == NULL)
    {
        return ngx_http_next_header_filter(r);
    }

    if (ngx_http_complex_value(r, flcf->text, &text) != NGX_OK) {
        return NGX_ERROR;
    }

    if (text.len == 0) {
        return ngx_http_next_header_filter(r);
    }

    if (r->headers_out.content_length_n != -1) {
        r->headers_out.content_length_n += text.len;
    }

    if (r->headers_out.content_length) {
        r->headers_out.content_length->hash = 0;
        r->headers_out.content_length = NULL;
    }

    /*
     * The body is no longer the one the entity tag was computed over, and
     * a range of it can no longer be served from what the upstream knows.
     */
    ngx_http_clear_accept_ranges(r);
    ngx_http_weak_etag(r);

    if (r->header_only || (r->method & NGX_HTTP_HEAD)) {
        return ngx_http_next_header_filter(r);
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_footer_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ctx->text = text;
    ngx_http_set_ctx(r, ctx, ngx_http_footer_filter_module);

    return ngx_http_next_header_filter(r);
}


static ngx_int_t
ngx_http_footer_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_buf_t              *b;
    ngx_chain_t            *cl, *nl;
    ngx_http_footer_ctx_t  *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_footer_filter_module);

    /*
     * appended is what keeps a second copy out.  A filter is called again
     * with the same chain whenever the one below it could not take all of
     * it, and the buffer added here carries last_buf, so without the flag
     * the next pass would find it and append once more.
     */
    if (ctx == NULL || ctx->appended) {
        return ngx_http_next_body_filter(r, in);
    }

    for (cl = in; cl; cl = cl->next) {
        if (cl->buf->last_buf) {
            break;
        }
    }

    if (cl == NULL) {
        return ngx_http_next_body_filter(r, in);
    }

    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_ERROR;
    }

    b->pos = ctx->text.data;
    b->last = ctx->text.data + ctx->text.len;
    b->start = b->pos;
    b->end = b->last;
    b->memory = 1;
    b->last_buf = 1;
    b->last_in_chain = cl->buf->last_in_chain;

    nl = ngx_alloc_chain_link(r->pool);
    if (nl == NULL) {
        return NGX_ERROR;
    }

    nl->buf = b;
    nl->next = cl->next;
    cl->next = nl;

    /*
     * The buffer that ended the body is kept rather than replaced, so the
     * flags it carries survive.  It gives up last_buf to the footer, and
     * sync keeps an empty one from being dropped before the footer behind
     * it is written.
     */
    cl->buf->last_buf = 0;
    cl->buf->last_in_chain = 0;
    cl->buf->sync = 1;

    ctx->appended = 1;

    return ngx_http_next_body_filter(r, in);
}


static char *
ngx_http_footer_set(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_footer_loc_conf_t *flcf = conf;

    ngx_str_t                          *value;
    ngx_http_compile_complex_value_t    ccv;

    if (flcf->text != NULL || flcf->off != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    /*
     * An empty argument is how the module has always been switched off in
     * a location below one that sets a footer.  It is not the same as
     * leaving the directive out, which inherits.
     */
    if (value[1].len == 0) {
        flcf->off = 1;
        return NGX_CONF_OK;
    }

    flcf->text = ngx_palloc(cf->pool, sizeof(ngx_http_complex_value_t));
    if (flcf->text == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = flcf->text;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    flcf->off = 0;

    return NGX_CONF_OK;
}


static void *
ngx_http_footer_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_footer_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_footer_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->text = NULL;
     *     conf->types = { NULL };
     *     conf->types_keys = NULL;
     */

    conf->off = NGX_CONF_UNSET;

    return conf;
}


static char *
ngx_http_footer_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_footer_loc_conf_t  *prev = parent;
    ngx_http_footer_loc_conf_t  *conf = child;

    if (ngx_http_merge_types(cf, &conf->types_keys, &conf->types,
                             &prev->types_keys, &prev->types,
                             ngx_http_html_default_types)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    if (conf->off == NGX_CONF_UNSET) {
        conf->off = (prev->off == NGX_CONF_UNSET) ? 0 : prev->off;
        if (conf->text == NULL) {
            conf->text = prev->text;
        }
    }

    if (conf->off) {
        conf->text = NULL;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_footer_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_footer_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_footer_body_filter;

    return NGX_OK;
}
