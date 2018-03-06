#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <string.h>
#include "hiredis/hiredis.h"

#include "dtm.pb-c.h"
#include "settings.pb-c.h"

typedef struct {
  ngx_str_t redis_host;
  ngx_int_t redis_port;
  ngx_str_t cookie_name;
  ngx_str_t redirect_location;
} auth_token_main_conf_t;

static ngx_command_t 
ngx_http_auth_token_commands[] = {
  {
    ngx_string("auth_token_redis_host"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    NGX_HTTP_MAIN_CONF_OFFSET,
    offsetof(auth_token_main_conf_t, redis_host),
    NULL
  },
  {
    ngx_string("auth_token_redis_port"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_num_slot,
    NGX_HTTP_MAIN_CONF_OFFSET,
    offsetof(auth_token_main_conf_t, redis_port),
    NULL
  },
  {
    ngx_string("auth_token_cookie_name"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    NGX_HTTP_MAIN_CONF_OFFSET,
    offsetof(auth_token_main_conf_t, cookie_name),
    NULL
  },
  {
    ngx_string("auth_token_redirect_location"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    NGX_HTTP_MAIN_CONF_OFFSET,
    offsetof(auth_token_main_conf_t, redirect_location),
    NULL
  },

  ngx_null_command
};


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
lookup_user(auth_token_main_conf_t *conf, ngx_str_t *auth_token, ngx_str_t *user_id)
{
  redisContext *context = redisConnect((const char*)conf->redis_host.data, conf->redis_port);
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

  auth_token_main_conf_t *conf = ngx_http_get_module_main_conf(r, ngx_http_auth_token_module);

  ngx_int_t cookie_location;
  ngx_str_t auth_token;
  cookie_location = ngx_http_parse_multi_header_lines(&r->headers_in.cookies, &conf->cookie_name, &auth_token);

  if(cookie_location == NGX_DECLINED) {
    return redirect(r, &conf->redirect_location);
  }

  ngx_str_t user_id;
  ngx_int_t lookup_result = lookup_user(conf, &auth_token, &user_id);

  if(lookup_result == NGX_DECLINED) {
    return redirect(r, &conf->redirect_location);
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

static void*
ngx_http_auth_token_create_main_conf(ngx_conf_t *cf)
{
  auth_token_main_conf_t *conf;

  conf = ngx_pcalloc(cf->pool, sizeof(auth_token_main_conf_t));
  if (conf == NULL) {
    return NULL;
  }

  conf->redis_port = NGX_CONF_UNSET_UINT;

  /* TODO: how to set default values: expansion of ngx_string macro doesn't work
  conf->redis_port = 6379;
  conf->redis_host = &ngx_string("localhost");
  conf->cookie_name = &ngx_string("auth_token");
  conf->redirect_location = &ngx_string("http://yahoo.com");
  */
  return conf;
}

static ngx_http_module_t
ngx_http_auth_token_module_ctx = {
  NULL,                                 /* preconfiguration */
  ngx_http_auth_token_init,             /* postconfiguration */
  ngx_http_auth_token_create_main_conf, /* create main configuration */
  NULL,                                 /* init main configuration */
  NULL,                                 /* create server configuration */
  NULL,                                 /* merge server configuration */
  NULL,                                 /* create location configuration */
  NULL                                  /* merge location configuration */
};

ngx_module_t
ngx_http_auth_token_module = {
  NGX_MODULE_V1,
  &ngx_http_auth_token_module_ctx,  /* module context */
  ngx_http_auth_token_commands,     /* module directives */
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
