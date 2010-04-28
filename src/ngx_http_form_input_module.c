#define DDEBUG 1
#include "ddebug.h"

#include <ndk.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_flag_t    enabled;
} ngx_http_form_input_loc_conf_t;


typedef struct {
    ngx_int_t         done;
} ngx_http_form_input_ctx_t;


static ngx_int_t ngx_http_set_form_input(ngx_http_request_t *r, ngx_str_t *res,
    ngx_http_variable_value_t *v);

static char *ngx_http_set_form_input_conf_handler(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static void *ngx_http_form_input_create_loc_conf(ngx_conf_t *cf);

static char *ngx_http_form_input_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);

static ngx_int_t ngx_http_form_input_init(ngx_conf_t *cf);

static ngx_int_t ngx_http_form_input_handler(ngx_http_request_t *r);

static void ngx_http_form_input_post_read(ngx_http_request_t *r);

static ngx_int_t ngx_http_form_input_arg(ngx_http_request_t *r, u_char *name,
    size_t len, ngx_str_t *value);

static ngx_command_t ngx_http_form_input_commands[] = {

    { ngx_string("set_form_input"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_http_set_form_input_conf_handler,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_form_input_loc_conf_t, enabled),
      NULL },

      ngx_null_command
};


static ngx_http_module_t ngx_http_form_input_module_ctx = {
    NULL,                                   /* preconfiguration */
    ngx_http_form_input_init,               /* postconfiguration */

    NULL,                                   /* create main configuration */
    NULL,                                   /* init main configuration */

    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */

    ngx_http_form_input_create_loc_conf,    /* create location configuration */
    ngx_http_form_input_merge_loc_conf      /* merge location configuration */
};


ngx_module_t ngx_http_form_input_module = {
    NGX_MODULE_V1,
    &ngx_http_form_input_module_ctx,        /* module context */
    ngx_http_form_input_commands,           /* module directives */
    NGX_HTTP_MODULE,                        /* module type */
    NULL,                                   /* init master */
    NULL,                                   /* init module */
    NULL,                                   /* init process */
    NULL,                                   /* init thread */
    NULL,                                   /* exit thread */
    NULL,                                   /* exit precess */
    NULL,                                   /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_set_form_input(ngx_http_request_t *r, ngx_str_t *res,
    ngx_http_variable_value_t *v)
{
    ngx_http_form_input_ctx_t           *ctx;
    ngx_int_t                            rc;
    dd("ndk handler:set form input handler");

    dd("set default return value");
    res->data = NULL;
    res->len = 0;

    ctx = ngx_http_get_module_ctx(r, ngx_http_form_input_module);

    if (ctx == NULL) {
        dd("ndk handler:null ctx");
        return NGX_OK;
    }

    if (ctx->done != 1) {
        dd("ctx not done");
        return NGX_AGAIN;
    }

    rc = ngx_http_form_input_arg(r, v->data, v->len, res);

    return rc;
}


static ngx_int_t
ngx_http_form_input_arg(ngx_http_request_t *r, u_char *arg_name, size_t arg_len,
    ngx_str_t *value)
{
    /* fork from ngx_http_arg */
    u_char              *p;
    u_char              *last;
    u_char              *buf;
    ngx_chain_t         *cl;
    ngx_int_t            len;

    value->data = NULL;
    value->len = 0;

    if (r->request_body == NULL) {
        return NGX_OK;
    } else if (r->request_body->bufs == NULL) {
        return NGX_OK;
    }

    len = 0;

    for (cl = r->request_body->bufs; cl != NULL; cl = cl->next) {
        len += (ngx_int_t)(cl->buf->last - cl->buf->pos);
    }

    dd("len=%d", (int)len);

    if (len == 0) {
        dd("post body len 0");
        return NGX_OK;
    }

    buf = ngx_palloc(r->pool, len);
    if (buf == NULL) {
        return NGX_ERROR;
    }
    p = buf;
    last = p + len;

    for (cl = r->request_body->bufs; cl != NULL; cl = cl->next) {
        p = ngx_copy(p, cl->buf->pos, (cl->buf->last - cl->buf->pos));
    }

    dd("buf=%p", buf);
    dd("last=%p", last);
    for (p = buf; p < last; p++) {
        /* we need '=' after name, so drop one char from last */

        p = ngx_strlcasestrn(p, last - 1, arg_name, arg_len - 1);
        if (p == NULL) {
            return NGX_OK;
        }

        if ((p == buf || *(p - 1) == '&') && *(p + arg_len) == '=') {
            value->data = p + arg_len + 1;

            p = ngx_strlchr(p, last, '&');

            if (p == NULL) {
                p = last;
            }

            value->len = p - value->data;

            return NGX_OK;
        }
    }
    return NGX_OK;
}


static char *
ngx_http_set_form_input_conf_handler(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf)
{
    ndk_set_var_t                            filter;
    ngx_str_t                               *var_name, s;
    u_char                                  *p;
    ngx_http_form_input_loc_conf_t          *flcf = conf;
    dd("set form input conf handler");

    /*if (flcf == NULL) {
        dd("flcf is null");
        return NGX_CONF_OK;
    }
    dd("flcf->enabled is 1");
    */
    flcf->enabled = 1;

    filter.type = NDK_SET_VAR_MULTI_VALUE;
    filter.func = ngx_http_set_form_input;
    filter.size = 1;

    var_name = cf->args->elts;
    var_name++;
    if (cf->args->nelts == 2)
    {
        dd("if starts");
        p = var_name->data;
        p++;
        s.len = var_name->len - 1;
        s.data = p;
        dd("if ends");
    } else if (cf->args->nelts == 3) {
        dd("else starts");
        s.len = (var_name + 1)->len;
        s.data = (var_name + 1)->data;
        dd("else ends");
    }

    return ndk_set_var_multi_value_core (cf, var_name,  &s, &filter);
}


static void *
ngx_http_form_input_create_loc_conf(ngx_conf_t *cf)
{
    dd("form input create loc conf");
    ngx_http_form_input_loc_conf_t *conf;
    conf = ngx_palloc(cf->pool, sizeof(ngx_http_form_input_loc_conf_t));

    if (conf == NULL) {
        return NULL;
    }

    conf->enabled = NGX_CONF_UNSET;

    return conf;
}


static char *
ngx_http_form_input_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    dd("form input merge loc conf");
    ngx_http_form_input_loc_conf_t *prev = parent;
    ngx_http_form_input_loc_conf_t *conf = child;

    ngx_conf_merge_value(prev->enabled, conf->enabled, 0);

    return NGX_CONF_OK;
}


/* register a new rewrite phase handler */
static ngx_int_t
ngx_http_form_input_init(ngx_conf_t *cf)
{
    dd("post conf: form_input_init");

    ngx_http_handler_pt             *h;
    ngx_http_core_main_conf_t       *cmcf;
    /* ngx_http_form_input_loc_conf_t  *lcf; */
/*
    lcf = ngx_http_get_module_loc_conf(cf, ngx_http_form_input);
    if (lcf->enabled != 1) {
        return NGX_OK;
    }
*/
    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers);

    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_form_input_handler;

    return NGX_OK;
}


static ngx_int_t
ngx_http_form_input_handler(ngx_http_request_t *r)
{
    dd("rewrite phase:form_input_handler");
    ngx_http_form_input_loc_conf_t  *lcf;
    ngx_http_form_input_ctx_t       *ctx;
    ngx_str_t                        value;
    ngx_int_t                        rc;

    lcf = ngx_http_get_module_loc_conf(r, ngx_http_form_input_module);

    if (lcf->enabled == 0 || lcf->enabled == NGX_CONF_UNSET) {
        dd("rewrite phase:lcf->enabled==0");
        return NGX_DECLINED;
    }

    if (r->method != NGX_HTTP_POST)
    {
        return NGX_DECLINED;
    }

    if (r->headers_in.content_type == NULL || r->headers_in.content_type->
            value.data == NULL) {
        dd("content_type is %s", r->headers_in.content_type == NULL?"NULL":
                "NOT NULL");
        return NGX_DECLINED;
    }

    value = r->headers_in.content_type->value;

    /* just focus on x-www-form-urlencoded if (r-> */
    if (value.len != (sizeof("application/x-www-form-urlencoded") - 1) ||
            ngx_strncmp(value.data, "application/x-www-form-urlencoded",
            sizeof("application/x-www-form-urlencoded")) != 0) {
        dd("not application/x-www-form-urlencoded");
        //lcf->enabled = 0;
        /* dd("content-type:%s", r->headers_in.content_type->value.data); */
        return NGX_DECLINED;
    }

    dd("content type is application/x-www-form-urlencoded");
    ctx = ngx_http_get_module_ctx(r, ngx_http_form_input_module);

    if (ctx == NULL) {
        dd("create new ctx");
        ctx = ngx_palloc(r->pool, sizeof(ngx_http_form_input_ctx_t));
        if (ctx == NULL) {
            return NGX_ERROR;
        }
        ctx->done = 0;
        ngx_http_set_ctx(r, ctx, ngx_http_form_input_module);
    } else {
        if (ctx->done) {
            return NGX_DECLINED;
        } else {
            return NGX_AGAIN;
        }
        dd("already have ctx");
        if (ctx->done) {
            return NGX_DECLINED;
        } else {
            return NGX_AGAIN;
        }
    }
    dd("start to read request_body");

    rc = ngx_http_read_client_request_body(r, ngx_http_form_input_post_read);

    if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rc;
    }

    if (rc == NGX_AGAIN) {
        return NGX_AGAIN;
    }

    return NGX_DECLINED;
}


static void ngx_http_form_input_post_read(ngx_http_request_t *r)
{
    ngx_http_request_body_t        *rb;
    ngx_http_form_input_ctx_t     *ctx;

    dd("ngx_http_post_read");
    rb = r->request_body;

    ctx = ngx_http_get_module_ctx(r, ngx_http_form_input_module);

    dd("post read done");

    ctx->done = 1;

    /* reschedule my ndk rewrite phase handler */
    ngx_http_core_run_phases(r);
}


/*
static ngx_int_t ngx_http_form_input_process_body(ngx_http_request_t *r,
    ngx_http_form_input_ctx_t *ctx)
{
    dd("process body:parse post body");
    ngx_chain_t     *chain;

    chain = r->request_body->bufs;

    if (chain == NULL) {
        dd("chain == NULL");
        return NGX_ERROR;
    }
    dd("chain != NULL");


    start = chain->buf->start;
    for (; chain != NULL; chain = chain->next) {
        buf = chain->buf;
        for (; buf->pos != buf->last; buf->pos++) {
            ch = *(buf->pos);
            if (ch == '=') {
                end = buf->pos;
                param = ngx_array_push(ctx->params);
                param->name.len = (ngx_int_t)(end - start);
                ngx_memcpy(param->name.data, start, param->name.len);
                start = end;
            } else if (ch == '&') {
                end = buf->pos;
                param->value.len = (ngx_int_t)(end - start);
                ngx_memcpy(param->value.data, start, param->value.len);
                start = end;
            }
        }
    }
    dd("finished loop");
    end = buf->pos;
    param->value.len = (ngx_int_t)(end - start);
    ngx_memcpy(param->value.data, start, param->value.len);


    dd("process body:finished");
    return NGX_DONE;
}
*/


/*
static ngx_int_t ngx_http_form_input_header_filter(ngx_http_request_t *r)
{
    dd("input header filter");
    ngx_http_form_input_loc_conf_t *lcf;
    lcf = ngx_http_get_module_loc_conf(r, ngx_http_form_input_module);

    ngx_http_form_input_ctx_t *ctx;

    if (lcf->enabled == 1) {
        ctx = ngx_http_get_module_ctx(r, ngx_http_form_input_module);
        if (ctx == NULL) {
            ctx = ngx_palloc(r->pool, sizeof(ngx_http_form_input_ctx_t));
            ngx_http_set_ctx(r, ctx, ngx_http_form_input_module);
            dd("create ngx_http_form_input_ctx");
        }
    }

    return ngx_http_next_header_filter(r);
}
*/
