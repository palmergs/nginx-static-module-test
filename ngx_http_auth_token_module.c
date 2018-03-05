#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <string.h>
#include "hiredis/hiredis.h"

ngx_module_t ngx_http_auth_token_module;

static ngx_int_t
redirect(ngx_http_request_t *r, ngx_str_t *location)
{
  ngx_table_elt_t *h;
  h = ngx_list_push(&r->headers_out.headers);
  h->hash = 1;
  ngx_str_set(&h->key, "Location");
  h->value = *location;

  return NGX_HTTP_MOVED_TEMPORARILY;
}

static ngx_int_t
lookup_user(ngx_str_t *auth_token, ngx_str_t *user_id)
{
  redisContext *context = redisConnect("localhost", 6379);
  redisReply *reply = redisCommand(context, "GET %s", auth_token->data);
  if(reply->type == REDIS_REPLY_NIL) {
    return NGX_DECLINED;
  } else {
    ngx_str_set(user_id, reply->str);
    return NGX_OK;
  }
}

static ngx_int_t
append_user_id(ngx_http_request_t *r, ngx_str_t *user_id)
{
  ngx_table_elt_t *h;
  h = ngx_list_push(&r->headers_out.headers);
  h->hash = 1;
  ngx_str_set(&h->key, "X-User-Id");
  h->value = *user_id;
}

static ngx_int_t 
ngx_http_auth_token_handler(ngx_http_request_t *r)
{
  if (r->main->internal) {
    return NGX_DECLINED;
  }

  r->main->internal = 1;

  ngx_str_t cookie = (ngx_str_t)ngx_string("auth_token");
  ngx_str_t location = (ngx_str_t)ngx_string("http://google.com");
  ngx_int_t lookup;
  ngx_str_t auth_token;
  lookup = ngx_http_parse_multi_header_lines(&r->headers_in.cookies, &cookie, &auth_token);

  if(lookup == NGX_DECLINED) {
    return redirect(r, &location);
  }

  ngx_str_t user_id;
  ngx_int_t lookup_result = lookup_user(&auth_token, &user_id);
  if(lookup_result == NGX_DECLINED) {
    return redirect(r, &location);
  }

  append_user_id(r, &user_id);
  return NGX_DECLINED;
}

static ngx_int_t ngx_http_auth_token_init(ngx_conf_t *cf)
{
  ngx_http_handler_pt *h;
  ngx_http_core_main_conf_t *cmcf;

  cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

  h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
  if (h == NULL) {
    return NGX_ERROR;
  }

  *h = ngx_http_auth_token_handler;

  return NGX_OK;
}

static ngx_http_module_t
ngx_http_auth_token_module_ctx = {
  NULL,                     /* preconfiguration */
  ngx_http_auth_token_init, /* postconfiguration */
  NULL,                     /* create main configuration */
  NULL,                     /* init main configuration */
  NULL,                     /* create server configuration */
  NULL,                     /* merge server configuration */
  NULL,                     /* create location configuration */
  NULL                      /* merge location configuration */
};


ngx_module_t
ngx_http_auth_token_module = {
  NGX_MODULE_V1,
  &ngx_http_auth_token_module_ctx,  /* module context */
  NULL,                             /* module directives */
  NGX_HTTP_MODULE,                  /* module type */
  NULL,                             /* init master */
  NULL,                             /* init module */
  NULL,                             /* init process */
  NULL,                             /* init thread */
  NULL,                             /* exit thread */
  NULL,                             /* exit process */
  NULL,                             /* exit master */
  NGX_MODULE_V1_PADDING
};
