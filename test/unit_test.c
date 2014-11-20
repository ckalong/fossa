/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 * This software is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. For the terms of this
 * license, see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * Alternatively, you can license this software under a commercial
 * license, as set out in <http://cesanta.com/>.
 */

#include "../fossa.h"

#define FAIL(str, line) do {                    \
  printf("%s:%d:1 [%s]\n", __FILE__, line, str); \
  return str;                                   \
} while (0)

#define ASSERT(expr) do {             \
  static_num_tests++;                 \
  if (!(expr)) FAIL(#expr, __LINE__); \
} while (0)

#define RUN_TEST(test) do {                 \
  const char *msg = NULL;                   \
  if (strstr(# test, filter)) msg = test(); \
  if (msg) return msg;                      \
} while (0)

#define HTTP_PORT "45772"
#define LOOPBACK_IP  "127.0.0.1"
#define LISTENING_ADDR LOOPBACK_IP ":" HTTP_PORT

static int static_num_tests = 0;
static const char *s_argv_0 = NULL;

static char *read_file(const char *path, size_t *size) {
  FILE *fp;
  ns_stat_t st;
  char *data = NULL;
  if ((fp = ns_fopen(path, "rb")) != NULL && !fstat(fileno(fp), &st)) {
    *size = st.st_size;
    data = (char *) malloc(*size);
    fread(data, 1, *size, fp);
    fclose(fp);
  }
  return data;
}

static const char *test_iobuf(void) {
  struct iobuf io;
  const char *data = "TEST";
  const char *prefix = "MY";
  const char *big_prefix = "Some long prefix: ";

  iobuf_init(&io, 0);
  ASSERT(io.buf == NULL && io.len == 0 && io.size == 0);
  iobuf_free(&io);
  ASSERT(io.buf == NULL && io.len == 0 && io.size == 0);

  iobuf_init(&io, 10);
  ASSERT(io.buf != NULL && io.len == 0 && io.size == 10);
  iobuf_free(&io);
  ASSERT(io.buf == NULL && io.len == 0 && io.size == 0);

  iobuf_init(&io, 10);
  ASSERT(iobuf_append(&io, NULL, 0) == 0);
  /* test allocation failure */
  ASSERT(iobuf_append(&io, NULL, 1125899906842624) == 0);

  ASSERT(iobuf_append(&io, data, strlen(data)) == strlen(data));

  iobuf_resize(&io, 2);
  ASSERT(io.size == 10);
  ASSERT(io.len == strlen(data));

  ASSERT(iobuf_prepend(&io, prefix, strlen(prefix)) == strlen(prefix));
  ASSERT(io.size == 10);
  ASSERT(io.len == strlen(data) + strlen(prefix));

  ASSERT(iobuf_prepend(&io, big_prefix, strlen(big_prefix)) == strlen(big_prefix));
  ASSERT(io.size == strlen(big_prefix) + strlen(prefix) + strlen(data));

  /* test overflow */
  ASSERT(iobuf_prepend(&io, NULL, -1) == 0);
  /* test allocation failure */
  ASSERT(iobuf_prepend(&io, NULL, 1125899906842624) == 0);
  iobuf_free(&io);
  return NULL;
}

static void poll_mgr(struct ns_mgr *mgr, int num_iterations) {
  while (num_iterations-- > 0) {
    ns_mgr_poll(mgr, 1);
  }
}

static void eh1(struct ns_connection *nc, int ev, void *ev_data) {
  struct iobuf *io = &nc->recv_iobuf;

  switch (ev) {
    case NS_CONNECT:
      ns_printf(nc, "%d %s there", * (int *) ev_data, "hi");
      break;
    case NS_RECV:
      if (nc->listener != NULL) {
        ns_printf(nc, "%d", (int) io->len);
        iobuf_remove(io, io->len);
      } else if (io->len == 2 && memcmp(io->buf, "10", 2) == 0) {
        sprintf((char *) nc->user_data, "%s", "ok!");
        nc->flags |= NSF_CLOSE_IMMEDIATELY;
      }
      break;
    default:
      break;
  }
}

#define S_PEM  "server.pem"
#define C_PEM  "client.pem"
#define CA_PEM "ca.pem"

static const char *test_mgr_with_ssl(int use_ssl) {
  char addr[100] = "127.0.0.1:0", ip[sizeof(addr)], buf[100] = "";
  struct ns_mgr mgr;
  struct ns_connection *nc;
  int port, port2;
#ifndef NS_ENABLE_SSL
  (void)use_ssl;
#endif

  ns_mgr_init(&mgr, NULL);
  /* mgr.hexdump_file = "/dev/stdout"; */

  ASSERT((nc = ns_bind(&mgr, addr, eh1)) != NULL);
  port2 = htons(nc->sa.sin.sin_port);
  ASSERT(port2 > 0);
#ifdef NS_ENABLE_SSL
  if (use_ssl) {
    ASSERT(ns_set_ssl(nc, S_PEM, CA_PEM) == NULL);
  }
#endif

  ns_sock_to_str(nc->sock, addr, sizeof(addr), 3);
  ASSERT(sscanf(addr, "%[^:]:%d", ip, &port) == 2);
  ASSERT(strcmp(ip, "127.0.0.1") == 0);
  ASSERT(port == port2);

  ASSERT((nc = ns_connect(&mgr, addr, eh1)) != NULL);
#ifdef NS_ENABLE_SSL
  if (use_ssl) {
    ASSERT(ns_set_ssl(nc, C_PEM, CA_PEM) == NULL);
  }
#endif
  nc->user_data = buf;
  poll_mgr(&mgr, 50);

  ASSERT(strcmp(buf, "ok!") == 0);

  ns_mgr_free(&mgr);
  return NULL;
}

static const char *test_mgr(void) {
  return test_mgr_with_ssl(0);
}

#ifdef NS_ENABLE_SSL
static const char *test_ssl(void) {
  return test_mgr_with_ssl(1);
}
#endif

static const char *test_to64(void) {
  ASSERT(to64("0") == 0);
  ASSERT(to64("") == 0);
  ASSERT(to64("123") == 123);
  ASSERT(to64("-34") == -34);
  ASSERT(to64("3566626116") == 3566626116U);
  return NULL;
}

#if 0
static const char *test_parse_address(void) {
  static const char *valid[] = {
    "1", "1.2.3.4:1", "tcp://123", "udp://0.0.0.0:99", "ssl://17",
    "ssl://900:a.pem:b.pem", "ssl://1.2.3.4:9000:aa.pem",
#if defined(NS_ENABLE_IPV6)
    "udp://[::1]:123", "[3ffe:2a00:100:7031::1]:900",
#endif
    NULL
  };
  static const int protos[] = {SOCK_STREAM, SOCK_STREAM, SOCK_STREAM,
    SOCK_DGRAM, SOCK_STREAM, SOCK_STREAM, SOCK_STREAM, SOCK_DGRAM, SOCK_STREAM};
  static const int use_ssls[] = {0, 0, 0, 0, 1, 1, 1, 0, 0};
  static const char *invalid[] = {
    "99999", "1k", "1.2.3", "1.2.3.4:", "1.2.3.4:2p", "blah://12", NULL
  };
  union socket_address sa;
  char cert[100], ca[100];
  int i, proto, use_ssl;

  for (i = 0; valid[i] != NULL; i++) {
    ASSERT(ns_parse_address(valid[i], &sa, &proto, &use_ssl, cert, ca) != 0);
    ASSERT(proto == protos[i]);
    ASSERT(use_ssl == use_ssls[i]);
  }

  for (i = 0; invalid[i] != NULL; i++) {
    ASSERT(ns_parse_address(invalid[i], &sa, &proto, &use_ssl, cert, ca) == 0);
  }
  ASSERT(ns_parse_address("0", &sa, &proto, &use_ssl, cert, ca) != 0);

  return NULL;
}
#endif

static int avt(char **buf, size_t buf_size, const char *fmt, ...) {
  int result;
  va_list ap;
  va_start(ap, fmt);
  result = ns_avprintf(buf, buf_size, fmt, ap);
  va_end(ap);
  return result;
}

static const char *test_alloc_vprintf(void) {
  char buf[5], *p = buf;

  ASSERT(avt(&p, sizeof(buf), "%d", 123) == 3);
  ASSERT(p == buf);
  ASSERT(strcmp(p, "123") == 0);

  ASSERT(avt(&p, sizeof(buf), "%d", 123456789) == 9);
  ASSERT(p != buf);
  ASSERT(strcmp(p, "123456789") == 0);
  free(p);

  return NULL;
}

static const char *test_socketpair(void) {
  sock_t sp[2];
  static const char foo[] = "hi there";
  char buf[20];

  ASSERT(ns_socketpair2(sp, SOCK_DGRAM) == 1);
  ASSERT(sizeof(foo) < sizeof(buf));

  /* Send string in one direction */
  ASSERT(send(sp[0], foo, sizeof(foo), 0) == sizeof(foo));
  ASSERT(recv(sp[1], buf, sizeof(buf), 0) == sizeof(foo));
  ASSERT(strcmp(buf, "hi there") == 0);

  /* Now in opposite direction */
  ASSERT(send(sp[1], foo, sizeof(foo), 0) == sizeof(foo));
  ASSERT(recv(sp[0], buf, sizeof(buf), 0) == sizeof(foo));
  ASSERT(strcmp(buf, "hi there") == 0);

  closesocket(sp[0]);
  closesocket(sp[1]);

  return NULL;
}

static void eh2(struct ns_connection *nc, int ev, void *p) {
  (void) p;
  switch (ev) {
    case NS_RECV:
      strcpy((char *) nc->user_data, nc->recv_iobuf.buf);
      break;
    default:
      break;
  }
}

static void *thread_func(void *param) {
  sock_t sock = * (sock_t *) param;
  send(sock, ":-)", 4, 0);
  return NULL;
}

static const char *test_thread(void) {
  struct ns_mgr mgr;
  struct ns_connection *nc;
  sock_t sp[2];
  char buf[20];

  ASSERT(ns_socketpair(sp) == 1);
  ns_start_thread(thread_func, &sp[1]);

  ns_mgr_init(&mgr, NULL);
  ASSERT((nc = ns_add_sock(&mgr, sp[0], eh2)) != NULL);
  nc->user_data = buf;
  poll_mgr(&mgr, 50);
  ASSERT(strcmp(buf, ":-)") == 0);
  ns_mgr_free(&mgr);
  closesocket(sp[1]);

  return NULL;
}

static void eh3(struct ns_connection *nc, int ev, void *p) {
  struct iobuf *io = &nc->recv_iobuf;
  (void) p;

  if (ev == NS_RECV) {
    memcpy((char *) nc->mgr->user_data, io->buf, io->len);
  }
}

static const char *test_udp(void) {
  struct ns_mgr mgr;
  struct ns_connection *nc;
  const char *address = "udp://127.0.0.1:7878";
  char buf[20] = "";

  ns_mgr_init(&mgr, buf);
  ASSERT(ns_bind(&mgr, address, eh3) != NULL);
  ASSERT((nc = ns_connect(&mgr, address, eh3)) != NULL);
  ns_printf(nc, "%s", "boo!");

  { int i; for (i = 0; i < 50; i++) ns_mgr_poll(&mgr, 1); }
  ASSERT(memcmp(buf, "boo!", 4) == 0);
  ns_mgr_free(&mgr);

  return NULL;
}

static const char *test_parse_http_message(void) {
  static const char *a = "GET / HTTP/1.0\n\n";
  static const char *b = "GET /blah HTTP/1.0\r\nFoo:  bar  \r\n\r\n";
  static const char *c = "get b c\nz:  k \nb: t\nvvv\n\n xx";
  static const char *d = "a b c\nContent-Length: 21 \nb: t\nvvv\n\n";
  static const char *e = "GET /foo?a=b&c=d HTTP/1.0\n\n";
  static const char *f = "POST /x HTTP/1.0\n\n";
  static const char *g = "HTTP/1.0 200 OK\n\n";
  static const char *h = "WOHOO /x HTTP/1.0\n\n";
  struct ns_str *v;
  struct http_message req;

  ASSERT(ns_parse_http("\b23", 3, &req) == -1);
  ASSERT(ns_parse_http("get\n\n", 5, &req) == -1);
  ASSERT(ns_parse_http(a, strlen(a) - 1, &req) == 0);
  ASSERT(ns_parse_http(a, strlen(a), &req) == (int) strlen(a));

  ASSERT(ns_parse_http(b, strlen(b), &req) == (int) strlen(b));
  ASSERT(req.header_names[0].len == 3);
  ASSERT(req.header_values[0].len == 3);
  ASSERT(req.header_names[1].p == NULL);

  ASSERT(ns_parse_http(c, strlen(c), &req) == (int) strlen(c) - 3);
  ASSERT(req.header_names[2].p == NULL);
  ASSERT(req.header_names[0].p != NULL);
  ASSERT(req.header_names[1].p != NULL);
  ASSERT(memcmp(req.header_values[1].p, "t", 1) == 0);
  ASSERT(req.header_names[1].len == 1);
  ASSERT(req.body.len == 0);

  ASSERT(ns_parse_http(d, strlen(d), &req) == (int) strlen(d));
  ASSERT(req.body.len == 21);
  ASSERT(req.message.len == 21 + strlen(d));
  ASSERT(ns_get_http_header(&req, "foo") == NULL);
  ASSERT((v = ns_get_http_header(&req, "contENT-Length")) != NULL);
  ASSERT(v->len == 2 && memcmp(v->p, "21", 2) == 0);

  ASSERT(ns_parse_http(e, strlen(e), &req) == (int) strlen(e));
  ASSERT(ns_vcmp(&req.uri, "/foo") == 0);
  ASSERT(ns_vcmp(&req.query_string, "a=b&c=d") == 0);

  ASSERT(ns_parse_http(f, strlen(f), &req) == (int) strlen(f));
  ASSERT(req.body.len == (size_t) ~0);
  ASSERT(ns_parse_http(g, strlen(g), &req) == (int) strlen(g));
  ASSERT(req.body.len == (size_t) ~0);
  ASSERT(ns_parse_http(h, strlen(h), &req) == (int) strlen(h));
  ASSERT(req.body.len == 0);

  return NULL;
}

static const char *test_get_http_var(void) {
  char buf[256];
  struct ns_str body;
  body.p = "key1=value1&key2=value2&key3=value%203&key4=value+4";
  body.len = strlen(body.p);

  ASSERT(ns_get_http_var(&body, "key1", buf, sizeof(buf)) > 0);
  ASSERT(strcmp(buf, "value1") == 0);
  ASSERT(ns_get_http_var(&body, "KEY1", buf, sizeof(buf)) > 0);
  ASSERT(strcmp(buf, "value1") == 0);
  ASSERT(ns_get_http_var(&body, "key2", buf, sizeof(buf)) > 0);
  ASSERT(strcmp(buf, "value2") == 0);
  ASSERT(ns_get_http_var(&body, "key3", buf, sizeof(buf)) > 0);
  ASSERT(strcmp(buf, "value 3") == 0);
  ASSERT(ns_get_http_var(&body, "key4", buf, sizeof(buf)) > 0);
  ASSERT(strcmp(buf, "value 4") == 0);

  ASSERT(ns_get_http_var(&body, "key", NULL, sizeof(buf)) == -2);
  ASSERT(ns_get_http_var(&body, "key", buf, 0) == -2);
  ASSERT(ns_get_http_var(&body, NULL, buf, sizeof(buf)) == -1);

  body.p = "key=broken%2";
  body.len = strlen(body.p);
  ASSERT(ns_get_http_var(&body, "key", buf, sizeof(buf)) < 0);

  body.p = "key=broken%2x";
  body.len = strlen(body.p);
  ASSERT(ns_get_http_var(&body, "key", buf, sizeof(buf)) < 0);
  return NULL;
}

static void cb1(struct ns_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *) ev_data;

  if (ev == NS_HTTP_REQUEST) {
    if (ns_vcmp(&hm->uri, "/foo") == 0) {
      ns_printf(nc, "HTTP/1.0 200 OK\n\n[%.*s %d]",
                (int) hm->uri.len, hm->uri.p, (int) hm->body.len);
      nc->flags |= NSF_FINISHED_SENDING_DATA;
    } else {
      static struct ns_serve_http_opts opts;
      opts.document_root = ".";
      ns_serve_http(nc, hm, opts);
    }
  }
}

static void cb2(struct ns_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *) ev_data;

  if (ev == NS_HTTP_REPLY) {
    memcpy(nc->user_data, hm->body.p, hm->body.len);
    nc->flags |= NSF_CLOSE_IMMEDIATELY;
  }
}

static void cb7(struct ns_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *) ev_data;
  struct ns_str *s;
  size_t size;
  char *data;

  if (ev == NS_HTTP_REPLY) {
    /* Make sure that we've downloaded this executable, byte-to-byte */
    data = read_file(s_argv_0, &size);
    strcpy((char *) nc->user_data, data == NULL || size != hm->body.len ||
           (s = ns_get_http_header(hm, "Content-Type")) == NULL ||
           (ns_vcmp(s, "text/plain")) != 0 ||
           memcmp(hm->body.p, data, size) != 0 ? "fail" : "success");
    free(data);
    nc->flags |= NSF_CLOSE_IMMEDIATELY;
  }
}

static void cb10(struct ns_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *) ev_data;
  struct ns_str *s;

  if (ev == NS_HTTP_REPLY &&
      (s = ns_get_http_header(hm, "Content-Type")) != NULL) {
    sprintf((char *) nc->user_data, "%.*s", (int) s->len, s->p);
  }
}

static const char *test_http(void) {
  struct ns_mgr mgr;
  struct ns_connection *nc;
  const char *local_addr = "127.0.0.1:7777";
  char buf[20] = "", status[20] = "", mime[20] = "";

  ns_mgr_init(&mgr, NULL);
  ASSERT((nc = ns_bind(&mgr, local_addr, cb1)) != NULL);
  ns_set_protocol_http_websocket(nc);

  /* Valid HTTP request. Pass test buffer to the callback. */
  ASSERT((nc = ns_connect(&mgr, local_addr, cb2)) != NULL);
  ns_set_protocol_http_websocket(nc);
  nc->user_data = buf;
  ns_printf(nc, "%s", "POST /foo HTTP/1.0\nContent-Length: 10\n\n"
            "0123456789");

  /* Invalid HTTP request */
  ASSERT((nc = ns_connect(&mgr, local_addr, cb2)) != NULL);
  ns_set_protocol_http_websocket(nc);
  ns_printf(nc, "%s", "bl\x03\n\n");

  /* Test static file download by downloading this executable, argv[0] */
  ASSERT((nc = ns_connect(&mgr, local_addr, cb7)) != NULL);
  ns_set_protocol_http_websocket(nc);
  nc->user_data = status;
  ns_printf(nc, "GET /%s HTTP/1.0\n\n", s_argv_0);

  /* Test mime type for static file */
  ASSERT((nc = ns_connect(&mgr, local_addr, cb10)) != NULL);
  ns_set_protocol_http_websocket(nc);
  nc->user_data = mime;
  ns_printf(nc, "%s", "GET /data/dummy.xml HTTP/1.0\n\n");

  /* Run event loop. Use more cycles to let file download complete. */
  poll_mgr(&mgr, 200);
  ns_mgr_free(&mgr);

  /* Check that test buffer has been filled by the callback properly. */
  ASSERT(strcmp(buf, "[/foo 10]") == 0);
  ASSERT(strcmp(status, "success") == 0);
  ASSERT(strcmp(mime, "text/xml") == 0);

  return NULL;
}

static void cb8(struct ns_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *) ev_data;

  if (ev == NS_HTTP_REPLY) {
    snprintf((char *) nc->user_data, 40, "%.*s", (int)hm->message.len, hm->message.p);
    nc->flags |= NSF_CLOSE_IMMEDIATELY;
  }
}

static const char *test_http_errors(void) {
  struct ns_mgr mgr;
  struct ns_connection *nc;
  const char *local_addr = "127.0.0.1:7777";
  char status[40] = "";

  ns_mgr_init(&mgr, NULL);
  ASSERT((nc = ns_bind(&mgr, local_addr, cb1)) != NULL);
  ns_set_protocol_http_websocket(nc);

#ifndef TEST_UNDER_VIRTUALBOX
  /* Test file which exists but cannot be opened */
  ASSERT((nc = ns_connect(&mgr, local_addr, cb8)) != NULL);
  ns_set_protocol_http_websocket(nc);
  nc->user_data = status;
  system("touch test_unreadable; chmod 000 test_unreadable");
  ns_printf(nc, "GET /%s HTTP/1.0\n\n", "../test_unreadable");

  /* Run event loop. Use more cycles to let file download complete. */
  poll_mgr(&mgr, 200);
  system("rm -f test_unreadable");

  /* Check that it failed */
  ASSERT(strncmp(status, "HTTP/1.1 500", strlen("HTTP/1.1 500")) == 0);
#endif

  /* Test non existing file */
  ASSERT((nc = ns_connect(&mgr, local_addr, cb8)) != NULL);
  ns_set_protocol_http_websocket(nc);
  nc->user_data = status;
  ns_printf(nc, "GET /%s HTTP/1.0\n\n", "/please_dont_create_this_file_srsly");

  /* Run event loop. Use more cycles to let file download complete. */
  poll_mgr(&mgr, 200);

  /* Check that it failed */
  ASSERT(strncmp(status, "HTTP/1.1 404", strlen("HTTP/1.1 404")) == 0);

  /* Test directory without index.html */
  ASSERT((nc = ns_connect(&mgr, local_addr, cb8)) != NULL);
  ns_set_protocol_http_websocket(nc);
  nc->user_data = status;
  ns_printf(nc, "GET /%s HTTP/1.0\n\n", "/");

  /* Run event loop. Use more cycles to let file download complete. */
  poll_mgr(&mgr, 200);

  /* Check that it failed */
  ASSERT(strncmp(status, "HTTP/1.1 403", strlen("HTTP/1.1 403")) == 0);

  /* Cleanup */
  ns_mgr_free(&mgr);

  return NULL;
}

static void cb9(struct ns_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *) ev_data;

  if (ev == NS_HTTP_REPLY) {
    snprintf((char *) nc->user_data, 20, "%.*s", (int)hm->body.len, hm->body.p);
    nc->flags |= NSF_CLOSE_IMMEDIATELY;
  }
}

static const char *test_http_index(void) {
  struct ns_mgr mgr;
  struct ns_connection *nc;
  const char *local_addr = "127.0.0.1:7777";
  char buf[20] = "";

  ns_mgr_init(&mgr, NULL);
  ASSERT((nc = ns_bind(&mgr, local_addr, cb1)) != NULL);
  ns_set_protocol_http_websocket(nc);

  /* Test directory. */
  ASSERT((nc = ns_connect(&mgr, local_addr, cb9)) != NULL);
  ns_set_protocol_http_websocket(nc);
  nc->user_data = buf;
  ns_printf(nc, "GET /%s HTTP/1.0\n\n", "/");

  system("echo testdata >index.html");

  /* Run event loop. Use more cycles to let file download complete. */
  poll_mgr(&mgr, 200);
  ns_mgr_free(&mgr);
  system("rm index.html");

  /* Check that test buffer has been filled by the callback properly. */
  ASSERT(strcmp(buf, "testdata\n") == 0);

  return NULL;
}

static void cb3(struct ns_connection *nc, int ev, void *ev_data) {
  struct websocket_message *wm = (struct websocket_message *) ev_data;

  if (ev == NS_WEBSOCKET_FRAME) {
    const char *reply = wm->size == 2 && !memcmp(wm->data, "hi", 2) ? "A": "B";
    ns_printf_websocket_frame(nc, WEBSOCKET_OP_TEXT, "%s", reply);
  }
}

static void cb4(struct ns_connection *nc, int ev, void *ev_data) {
  struct websocket_message *wm = (struct websocket_message *) ev_data;

  if (ev == NS_WEBSOCKET_FRAME) {
    memcpy(nc->user_data, wm->data, wm->size);
    ns_send_websocket_frame(nc, WEBSOCKET_OP_CLOSE, NULL, 0);
  } else if (ev == NS_WEBSOCKET_HANDSHAKE_DONE) {
    /* Send "hi" to server. server must reply "A". */
    struct ns_str h[2];
    h[0].p = "h";
    h[0].len = 1;
    h[1].p = "i";
    h[1].len = 1;
    ns_send_websocket_framev(nc, WEBSOCKET_OP_TEXT, h, 2);
  }
}

static const char *test_websocket(void) {
  struct ns_mgr mgr;
  struct ns_connection *nc;
  const char *local_addr = "127.0.0.1:7778";
  char buf[20] = "";

  ns_mgr_init(&mgr, NULL);
  /* mgr.hexdump_file = "/dev/stdout"; */
  ASSERT((nc = ns_bind(&mgr, local_addr, cb3)) != NULL);
  ns_set_protocol_http_websocket(nc);

  /* Websocket request */
  ASSERT((nc = ns_connect(&mgr, local_addr, cb4)) != NULL);
  ns_set_protocol_http_websocket(nc);
  nc->user_data = buf;
  ns_send_websocket_handshake(nc, "/ws", NULL);
  poll_mgr(&mgr, 50);
  ns_mgr_free(&mgr);

  /* Check that test buffer has been filled by the callback properly. */
  ASSERT(strcmp(buf, "A") == 0);

  return NULL;
}

struct big_payload_params {
  size_t size;
  char *buf;
};

static void cb3_big(struct ns_connection *nc, int ev, void *ev_data) {
  struct websocket_message *wm = (struct websocket_message *) ev_data;

  if (ev == NS_WEBSOCKET_FRAME) {
    int success = 1;
    size_t i;
    for (i = 0; i < wm->size; i++) {
      if (wm->data[i] != 'x') {
        success = 0;
        break;
      }
    }
    ns_printf_websocket_frame(nc, WEBSOCKET_OP_TEXT, "%s", success ? "success": "fail");
  }
}

static void cb4_big(struct ns_connection *nc, int ev, void *ev_data) {
  struct websocket_message *wm = (struct websocket_message *) ev_data;
  struct big_payload_params *params = (struct big_payload_params *)nc->user_data;

  if (ev == NS_WEBSOCKET_FRAME) {
    memcpy(params->buf, wm->data, wm->size);
    ns_send_websocket_frame(nc, WEBSOCKET_OP_CLOSE, NULL, 0);
  } else if (ev == NS_WEBSOCKET_HANDSHAKE_DONE) {
    /* Send large payload to server. server must reply "success". */
    char *payload = (char *)malloc(params->size);
    memset(payload, 'x', params->size);
    ns_printf_websocket_frame(nc, WEBSOCKET_OP_TEXT, "%.*s", params->size, payload);
    free(payload);
  }
}

/* Big payloads follow a different code path because it will use the extended
 * length field and possibly ns_avprintf will need to reallocate the buffer. */
static const char *test_websocket_big(void) {
  struct ns_mgr mgr;
  struct ns_connection *nc;
  const char *local_addr = "127.0.0.1:7778";
  char buf[20] = "";
  struct big_payload_params params;
  params.buf = buf;

  ns_mgr_init(&mgr, NULL);
  /* mgr.hexdump_file = "/dev/stdout"; */
  ASSERT((nc = ns_bind(&mgr, local_addr, cb3_big)) != NULL);
  ns_set_protocol_http_websocket(nc);

  /* Websocket request */
  ASSERT((nc = ns_connect(&mgr, local_addr, cb4_big)) != NULL);
  ns_set_protocol_http_websocket(nc);
  params.size = 8192;
  nc->user_data = &params;
  ns_send_websocket_handshake(nc, "/ws", NULL);
  poll_mgr(&mgr, 50);

  /* Check that test buffer has been filled by the callback properly. */
  ASSERT(strcmp(buf, "success") == 0);

  /* Websocket request */
  ASSERT((nc = ns_connect(&mgr, local_addr, cb4_big)) != NULL);
  ns_set_protocol_http_websocket(nc);
  params.size = 65535;
  nc->user_data = &params;
  ns_send_websocket_handshake(nc, "/ws", NULL);
  poll_mgr(&mgr, 50);
  ns_mgr_free(&mgr);

  /* Check that test buffer has been filled by the callback properly. */
  ASSERT(strcmp(buf, "success") == 0);

  return NULL;
}

static const char *test_mqtt_publish(void) {
  struct ns_connection *nc = (struct ns_connection *) calloc(1, sizeof(*nc));
  char data[] = "dummy";
  const char *got;

  ns_mqtt_publish(nc, "/test", 42, NS_MQTT_QOS(1) | NS_MQTT_RETAIN, data, sizeof(data));
  got = nc->send_iobuf.buf;
  ASSERT(nc->send_iobuf.len == 17);

  ASSERT(got[0] & NS_MQTT_RETAIN);
  ASSERT((got[0] & 0xf0) == (NS_MQTT_CMD_PUBLISH << 4));
  ASSERT(NS_MQTT_GET_QOS(got[0]) == 1);
  ASSERT((size_t)got[1] == (nc->send_iobuf.len - 2));

  ASSERT(got[2] == 0);
  ASSERT(got[3] == 5);
  ASSERT(strncmp(&got[4], "/test", 5) == 0);

  ASSERT(got[9] == 0);
  ASSERT(got[10] == 42);

  ASSERT(strncmp(&got[11], data, sizeof(data)) == 0);

  iobuf_free(&nc->send_iobuf);
  free(nc);
  return NULL;
}

static const char *test_mqtt_subscribe(void) {
  struct ns_connection *nc = (struct ns_connection *) calloc(1, sizeof(*nc));
  const char *got;
  const int qos = 1;
  struct ns_mqtt_topic_expression topic_expressions[] = {
    {"/stuff", qos}
  };

  ns_mqtt_subscribe(nc, topic_expressions, 1, 42);
  got = nc->send_iobuf.buf;
  ASSERT(nc->send_iobuf.len == 13);
  ASSERT((got[0] & 0xf0) == (NS_MQTT_CMD_SUBSCRIBE << 4));
  ASSERT((size_t)got[1] == (nc->send_iobuf.len - 2));
  ASSERT(got[2] == 0);
  ASSERT(got[3] == 42);

  ASSERT(got[4] == 0);
  ASSERT(got[5] == 6);
  ASSERT(strncmp(&got[6], "/stuff", 6) == 0);
  ASSERT(got[12] == qos);

  iobuf_free(&nc->send_iobuf);
  free(nc);
  return NULL;
}

static const char *test_mqtt_unsubscribe(void) {
  struct ns_connection *nc = (struct ns_connection *) calloc(1, sizeof(*nc));
  const char *got;
  char *topics[] = {(char *) "/stuff"};

  ns_mqtt_unsubscribe(nc, topics, 1, 42);
  got = nc->send_iobuf.buf;
  ASSERT(nc->send_iobuf.len == 12);
  ASSERT((got[0] & 0xf0) == (NS_MQTT_CMD_UNSUBSCRIBE << 4));
  ASSERT((size_t)got[1] == (nc->send_iobuf.len - 2));
  ASSERT(got[2] == 0);
  ASSERT(got[3] == 42);

  ASSERT(got[4] == 0);
  ASSERT(got[5] == 6);
  ASSERT(strncmp(&got[6], "/stuff", 6) == 0);

  iobuf_free(&nc->send_iobuf);
  free(nc);
  return NULL;
}

static const char *test_mqtt_connack(void) {
  struct ns_connection *nc = (struct ns_connection *) calloc(1, sizeof(*nc));
  const char *got;
  ns_mqtt_connack(nc, 42);
  got = nc->send_iobuf.buf;
  ASSERT(nc->send_iobuf.len > 0);
  ASSERT((got[0] & 0xf0) == (NS_MQTT_CMD_CONNACK << 4));
  ASSERT((size_t)got[1] == (nc->send_iobuf.len - 2));
  ASSERT(got[3] == 42);

  iobuf_free(&nc->send_iobuf);
  free(nc);
  return NULL;
}

static const char *test_mqtt_suback(void) {
  struct ns_connection *nc = (struct ns_connection *) calloc(1, sizeof(*nc));
  const char *got;

  uint8_t qoss[] = {1};

  ns_mqtt_suback(nc, qoss, 1, 42);

  got = nc->send_iobuf.buf;
  ASSERT(nc->send_iobuf.len == 5);
  ASSERT((got[0] & 0xf0) == (NS_MQTT_CMD_SUBACK << 4));
  ASSERT(NS_MQTT_GET_QOS(got[0]) == 1);
  ASSERT((size_t)got[1] == (nc->send_iobuf.len - 2));
  ASSERT(got[2] == 0);
  ASSERT(got[3] == 42);
  ASSERT(got[4] == 1);

  iobuf_free(&nc->send_iobuf);
  free(nc);
  return NULL;
}

static const char *test_mqtt_simple_acks(void) {
  unsigned long i;
  struct {
    uint8_t cmd;
    void (*f)(struct ns_connection *, uint16_t);
  } cases[] = {
    {NS_MQTT_CMD_PUBACK, ns_mqtt_puback},
    {NS_MQTT_CMD_PUBREC, ns_mqtt_pubrec},
    {NS_MQTT_CMD_PUBREL, ns_mqtt_pubrel},
    {NS_MQTT_CMD_PUBCOMP, ns_mqtt_pubcomp},
    {NS_MQTT_CMD_UNSUBACK, ns_mqtt_unsuback},
  };

  for (i = 0; i < ARRAY_SIZE(cases); i++) {
    struct ns_connection *nc = (struct ns_connection *) calloc(1, sizeof(*nc));
    const char *got;

    cases[i].f(nc, 42);

    got = nc->send_iobuf.buf;
    ASSERT(nc->send_iobuf.len == 4);
    ASSERT((got[0] & 0xf0) == (cases[i].cmd << 4));
    ASSERT(NS_MQTT_GET_QOS(got[0]) == 1);
    ASSERT((size_t)got[1] == (nc->send_iobuf.len - 2));
    ASSERT(got[2] == 0);
    ASSERT(got[3] == 42);

    iobuf_free(&nc->send_iobuf);
    free(nc);
  }
  return NULL;
}

static const char *test_mqtt_ping(void) {
  unsigned long i;
  struct {
    uint8_t cmd;
    void (*f)(struct ns_connection *);
  } cases[] = {
    {NS_MQTT_CMD_PINGREQ, ns_mqtt_ping},
    {NS_MQTT_CMD_PINGRESP, ns_mqtt_pong},
  };

  for (i = 0; i < ARRAY_SIZE(cases); i++) {
    struct ns_connection *nc = (struct ns_connection *) calloc(1, sizeof(*nc));
    const char *got;

    cases[i].f(nc);

    got = nc->send_iobuf.buf;
    ASSERT(nc->send_iobuf.len == 2);
    ASSERT((got[0] & 0xf0) == (cases[i].cmd << 4));
    ASSERT((size_t)got[1] == (nc->send_iobuf.len - 2));
    free(nc);
  }
  return NULL;
}

static const int mqtt_long_payload_len = 200;
static const int mqtt_very_long_payload_len = 20000;

static void mqtt_eh(struct ns_connection *nc, int ev, void *ev_data) {
  struct ns_mqtt_message *mm = (struct ns_mqtt_message *) ev_data;
  int i;
  (void) nc;
  (void) ev_data;

  switch (ev) {
    case NS_MQTT_SUBACK:
      *((int*)nc->user_data) = 1;
      break;
    case NS_MQTT_PUBLISH:
      if (strncmp(mm->topic, "/topic", 6)) break;

      for (i=0; i < mm->payload_len; i++) {
        if (nc->recv_iobuf.buf[10 + i] != 'A') break;
      }

      if (mm->payload_len == mqtt_long_payload_len) {
        *((int*)nc->user_data) = 2;
      } else if (mm->payload_len == mqtt_very_long_payload_len) {
        *((int*)nc->user_data) = 3;
      }
      break;
  }
}

static const char *test_mqtt_parse_mqtt(void) {
  struct ns_connection *nc = (struct ns_connection *) calloc(1, sizeof(*nc));
  char msg[] = {(char)(NS_MQTT_CMD_SUBACK << 4), 2};
  char *long_msg;
  int check = 0;
  int num_bytes = sizeof(msg);
  int rest_len;

  nc->user_data = &check;
  nc->handler = mqtt_eh;
  ns_set_protocol_mqtt(nc);

  iobuf_append(&nc->recv_iobuf, msg, num_bytes);
  nc->proto_handler(nc, NS_RECV, &num_bytes);

  ASSERT(check == 1);
  iobuf_free(&nc->recv_iobuf);

  /* test a payload whose length encodes as two bytes */
  rest_len = 8 + mqtt_long_payload_len;
  long_msg = (char *) malloc(512);
  long_msg[0] = (char)(NS_MQTT_CMD_PUBLISH << 4);
  long_msg[1] = (rest_len & 127) | 0x80;
  long_msg[2] = rest_len >> 7;
  memcpy(&long_msg[3], "\0\006/topic", 8);
  memset(&long_msg[11], 'A', mqtt_long_payload_len);

  num_bytes = 2 + rest_len;
  iobuf_append(&nc->recv_iobuf, long_msg, num_bytes);
  nc->proto_handler(nc, NS_RECV, &num_bytes);

  ASSERT(check == 2);
  iobuf_free(&nc->recv_iobuf);
  free(long_msg);

  /* test a payload whose length encodes as two bytes */
  rest_len = 8 + mqtt_very_long_payload_len;
  long_msg = (char *) malloc(20100);
  long_msg[0] = (char)(NS_MQTT_CMD_PUBLISH << 4);
  long_msg[1] = (rest_len & 127) | 0x80;
  long_msg[2] = ((rest_len >> 7) & 127) | 0x80;
  long_msg[3] = (rest_len >> 14);
  memcpy(&long_msg[4], "\0\006/topic", 8);
  memset(&long_msg[12], 'A', mqtt_very_long_payload_len);

  num_bytes = 2 + rest_len;
  iobuf_append(&nc->recv_iobuf, long_msg, num_bytes);
  nc->proto_handler(nc, NS_RECV, &num_bytes);

  ASSERT(check == 3);
  iobuf_free(&nc->recv_iobuf);
  free(long_msg);

  free(nc);
  return NULL;
}

static int rpc_sum(char *buf, int len, struct ns_rpc_request *req) {
  double sum = 0;
  int i;

  if (req->params[0].type != JSON_TYPE_ARRAY) {
    return ns_rpc_create_std_error(buf, len, req,
                                   JSON_RPC_INVALID_PARAMS_ERROR);
  }

  for (i = 0; i < req->params[0].num_desc; i++) {
    if (req->params[i + 1].type != JSON_TYPE_NUMBER) {
      return ns_rpc_create_std_error(buf, len, req,
                                     JSON_RPC_INVALID_PARAMS_ERROR);
    }
    sum += strtod(req->params[i + 1].ptr, NULL);
  }
  return ns_rpc_create_reply(buf, len, req, "f", sum);
}

static void rpc_server(struct ns_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *) ev_data;
  static const char *methods[] = { "sum", NULL };
  static ns_rpc_handler_t handlers[] = { rpc_sum, NULL };
  char buf[100];

  switch (ev) {
    case NS_HTTP_REQUEST:
      ns_rpc_dispatch(hm->body.p, hm->body.len, buf, sizeof(buf),
                      methods, handlers);
      ns_printf(nc, "HTTP/1.0 200 OK\r\nContent-Length: %d\r\n"
                "Content-Type: application/json\r\n\r\n%s",
                (int) strlen(buf), buf);
      nc->flags |= NSF_FINISHED_SENDING_DATA;
      break;
    default:
      break;
  }
}

static void rpc_client(struct ns_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *) ev_data;
  struct ns_rpc_reply rpc_reply;
  struct ns_rpc_error rpc_error;
  struct json_token toks[20];
  char buf[100];

  switch (ev) {
    case NS_CONNECT:
      ns_rpc_create_request(buf, sizeof(buf), "sum", "1", "[f,f,f]",
                            1.0, 2.0, 13.0);
      ns_printf(nc, "POST / HTTP/1.0\r\nContent-Type: application/json\r\n"
                "Content-Length: %d\r\n\r\n%s", (int) strlen(buf), buf);
      break;
    case NS_HTTP_REPLY:
      ns_rpc_parse_reply(hm->body.p, hm->body.len,
                         toks, sizeof(toks) / sizeof(toks[0]),
                         &rpc_reply, &rpc_error);
      if (rpc_reply.result != NULL) {
        sprintf((char *) nc->user_data, "%d %.*s %.*s",
                rpc_reply.id->type, (int) rpc_reply.id->len, rpc_reply.id->ptr,
                (int) rpc_reply.result->len, rpc_reply.result->ptr);
      }
      break;
    default:
      break;
  }
}

static const char *test_rpc(void) {
  struct ns_mgr mgr;
  struct ns_connection *nc;
  const char *local_addr = "127.0.0.1:7779";
  char buf[100] = "";

  ns_mgr_init(&mgr, NULL);

  ASSERT((nc = ns_bind(&mgr, local_addr, rpc_server)) != NULL);
  ns_set_protocol_http_websocket(nc);

  ASSERT((nc = ns_connect(&mgr, local_addr, rpc_client)) != NULL);
  ns_set_protocol_http_websocket(nc);
  nc->user_data = buf;

  poll_mgr(&mgr, 50);
  ns_mgr_free(&mgr);

  ASSERT(strcmp(buf, "1 1 16") == 0);

  return NULL;
}

static void cb5(struct ns_connection *nc, int ev, void *ev_data) {
  switch (ev) {
    case NS_CONNECT:
      sprintf((char *) nc->user_data, "%d", * (int *) ev_data);
      break;
    default:
      break;
  }
}

static const char *test_connect_fail(void) {
  struct ns_mgr mgr;
  struct ns_connection *nc;
  char buf[100] = "0";

  ns_mgr_init(&mgr, NULL);
  ASSERT((nc = ns_connect(&mgr, "127.0.0.1:33211", cb5)) != NULL);
  nc->user_data = buf;
  poll_mgr(&mgr, 50);
  ns_mgr_free(&mgr);

  /* printf("failed connect status: [%s]\n", buf); */
  ASSERT(strcmp(buf, "0") != 0);

  return NULL;
}

static void cb6(struct ns_connection *nc, int ev, void *ev_data) {
  (void)nc;
  (void)ev;
  (void)ev_data;
}

static const char *test_connect_opts(void) {
  struct ns_mgr mgr;
  struct ns_connection *nc;
  struct ns_connect_opts opts;

  opts.user_data = (void*)0xdeadbeef;
  opts.flags = NSF_USER_6;

  ns_mgr_init(&mgr, NULL);
  ASSERT((nc = ns_connect_opt(&mgr, "127.0.0.1:33211", cb6, opts)) != NULL);
  ASSERT(nc->user_data == (void*)0xdeadbeef);
  ASSERT(nc->flags & NSF_USER_6);
  poll_mgr(&mgr, 50);
  ns_mgr_free(&mgr);
  return NULL;
}

static const char *test_connect_opts_error_string(void) {
  struct ns_mgr mgr;
  struct ns_connection *nc;
  struct ns_connect_opts opts;
  char *error_string = NULL;

  opts.error_string = &error_string;

  ns_mgr_init(&mgr, NULL);
  ASSERT((nc = ns_connect_opt(&mgr, "127.0.0.1:65537", cb6, opts)) == NULL);
  ASSERT(error_string != NULL);
  ASSERT(strcmp(error_string, "cannot parse address") == 0);
  free(error_string);
  return NULL;
}

#ifndef NO_DNS_TEST
static const char *test_resolve(void) {
  char buf[20];

  ASSERT(ns_resolve("localhost", buf, sizeof(buf)) > 0);
  ASSERT(strcmp(buf, "127.0.0.1") == 0);

  ASSERT(ns_resolve("please_dont_name_a_host_like_ths", buf, sizeof(buf)) == 0);
  return NULL;
}
#endif

static const char *test_base64(void) {
  const char *cases[] = {"test", "longer string"};
  unsigned long i;
  char enc[8192];
  char dec[8192];

  for (i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
    ns_base64_encode((unsigned char *)cases[i], strlen(cases[i]), enc);
    ns_base64_decode((unsigned char *)enc, strlen(enc), dec);

    ASSERT(strcmp(cases[i], dec) == 0);
  }
  return NULL;
}

static const char *test_hexdump(void) {
  const char *src = "\1\2\3\4abcd";
  char got[256];

  const char *want ="0000  01 02 03 04 61 62 63 64"
                    "                          ....abcd\n\n";
  ASSERT(ns_hexdump(src, strlen(src), got, sizeof(got)) == (int)strlen(want));
  ASSERT(strcmp(got, want) == 0);
  return NULL;
}

static const char *test_hexdump_file(void) {
  const char *path = "test_hexdump";
  const char *want =  "0xbeef :0 -> :0 3\n"
                      "0000  66 6f 6f   "
                      "                                      foo\n\n";
  char *data, *got;
  size_t size;
  struct ns_connection *nc = (struct ns_connection *) calloc(1, sizeof(*nc));

  /* "In the GNU system, non-null pointers are printed as unsigned integers,
   * as if a `%#x' conversion were used. Null pointers print as `(nil)'.
   * (Pointers might print differently in other systems.)"
   * indeed it prints 0x0 on apple.
   */
  nc->user_data = (void *)0xbeef;
  truncate(path, 0);

  iobuf_append(&nc->send_iobuf, "foo", 3);
  iobuf_append(&nc->recv_iobuf, "bar", 3);
  ns_hexdump_connection(nc, path, 3, NS_SEND);

  iobuf_free(&nc->send_iobuf);
  iobuf_free(&nc->recv_iobuf);
  free(nc);

  ASSERT((data = read_file(path, &size)) != NULL);
  unlink(path);

  got = data;
  while(got-data < (int)size && *got++ != ' ');
  size -= got-data;
  ASSERT(strncmp(got, want, size) == 0);

  free(data);
  return NULL;
}

static const char *test_http_chunk(void) {
  struct ns_connection nc;

  memset(&nc, 0, sizeof(nc));

  ns_printf_http_chunk(&nc, "%d %s", 123, ":-)");
  ASSERT(nc.send_iobuf.len == 12);
  ASSERT(memcmp(nc.send_iobuf.buf, "7\r\n123 :-)\r\n", 12) == 0);
  iobuf_free(&nc.send_iobuf);

  ns_send_http_chunk(&nc, "", 0);
  ASSERT(nc.send_iobuf.len == 5);
  ASSERT(memcmp(nc.send_iobuf.buf, "0\r\n\r\n", 3) == 0);
  iobuf_free(&nc.send_iobuf);

  return NULL;
}

static const char *run_tests(const char *filter) {
  RUN_TEST(test_iobuf);
#if 0
  RUN_TEST(test_parse_address);
#endif
  RUN_TEST(test_connect_fail);
  RUN_TEST(test_connect_opts);
  RUN_TEST(test_connect_opts_error_string);
  RUN_TEST(test_to64);
  RUN_TEST(test_alloc_vprintf);
  RUN_TEST(test_socketpair);
  RUN_TEST(test_thread);
  RUN_TEST(test_mgr);
  RUN_TEST(test_parse_http_message);
  RUN_TEST(test_get_http_var);
  RUN_TEST(test_http);
  RUN_TEST(test_http_errors);
  RUN_TEST(test_http_index);
  RUN_TEST(test_websocket);
  RUN_TEST(test_websocket_big);
  RUN_TEST(test_rpc);
  RUN_TEST(test_http_chunk);
  RUN_TEST(test_mqtt_publish);
  RUN_TEST(test_mqtt_subscribe);
  RUN_TEST(test_mqtt_unsubscribe);
  RUN_TEST(test_mqtt_connack);
  RUN_TEST(test_mqtt_suback);
  RUN_TEST(test_mqtt_simple_acks);
  RUN_TEST(test_mqtt_ping);
  RUN_TEST(test_mqtt_parse_mqtt);
#ifndef NO_DNS_TEST
  RUN_TEST(test_resolve);
#endif
  RUN_TEST(test_base64);
  RUN_TEST(test_hexdump);
  RUN_TEST(test_hexdump_file);
#ifdef NS_ENABLE_SSL
  RUN_TEST(test_ssl);
#endif
  RUN_TEST(test_udp);
  return NULL;
}

int __cdecl main(int argc, char *argv[]) {
  const char *fail_msg;
  const char *filter = argc > 1 ? argv[1] : "";

  s_argv_0 = argv[0];
  fail_msg = run_tests(filter);
  printf("%s, tests run: %d\n", fail_msg ? "FAIL" : "PASS", static_num_tests);

  return fail_msg == NULL ? EXIT_SUCCESS : EXIT_FAILURE;
}
