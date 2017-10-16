/* Copyright 2012-2017 Dustin DeWeese
   This file is part of PoprC.

    PoprC is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PoprC is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PoprC.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "rt_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "gen/error.h"
#include "gen/log.h"

#define REVERSE 0x80
#define INDENT  0x40
#define MASK (REVERSE | INDENT)

#define LOG_SIZE (1 << 12)
static intptr_t log[LOG_SIZE];
static unsigned int log_head = 0;
static unsigned int log_tail = 0;
static unsigned int log_watch = ~0;
static intptr_t log_watch_fmt = 0;
static bool set_log_watch_fmt = false;
static unsigned int msg_head = 0;

context_t *__log_context = NULL;

void log_init() {
  log[0] = 0;
  log_head = 0;
  log_tail = 0;
  __log_context = NULL;
}

void set_log_watch(const tag_t tag, bool after) {
  log_watch = read_tag(tag);
  set_log_watch_fmt = after;
}

void log_soft_init() {
  if(log_head != log_tail) {
    log_add((intptr_t)"\xff\xff"); // reset indentation
  }
  __log_context = NULL;
}

static
int log_entry_len(unsigned int idx) {
  const char *fmt = (const char *)log[idx];
  if(!fmt) return 0;
  char len = fmt[0];
  if(len == '\xff') return 0;
  return (uint8_t)(len & ~MASK);
}

static
unsigned int log_printf(unsigned int idx, unsigned int *depth, bool event) {
  const char *fmt = (const char *)log[idx++];
  tag_t tag;
  //printf("%d %d %x %s\n", idx, *depth, fmt[0], fmt + 1);
  uint8_t len = fmt[0] & ~MASK;
  const char
    *p = fmt + 1,
    *n = strchr(p, '%');
  LOOP(*depth * 2) putchar(' ');
  if(fmt[0] & INDENT) (*depth)++;
  while(n) {
    printf("%.*s", (int)(n-p), p); // print the text
    if(!n[1]) break;
    switch(n[1]) {
#define CASE(c, type, fmt)                                      \
      case c:                                                   \
        if(len) {                                               \
          idx = idx % LOG_SIZE;                                 \
          printf(fmt, (type)log[idx++]);                        \
          len--;                                                \
        } else {                                                \
          printf("X");                                          \
        }                                                       \
        break;
      CASE('d', int, "%d");
      CASE('u', unsigned int, "%u");
      CASE('x', int, "%x");
      CASE('s', const char *, "%s");
      CASE('p', void *, "%p");
 #undef CASE
    case '.':
      if(n[2] == '*' && n[3] == 's') {
        if(len > 1) {
          idx = idx % LOG_SIZE;
          int size = log[idx++];
          idx = idx % LOG_SIZE;
          printf("%.*s", size, (const char *)log[idx++]);
          len -= 2;
        } else {
          printf("X");
        }
        n += 2;
        break;
      }
    case '%':
      printf("%%");
      break;
    default:
      printf("!?");
      break;
    }
    p = n + 2;
    n = strchr(p, '%');
  }
  idx = idx % LOG_SIZE;
  if(event) {
    write_tag(tag, idx);
    printf("%s " FADE(FORMAT_TAG) "\n", p, tag);
  } else {
    printf("%s\n", p);
  }
  return idx;
}

void log_add(intptr_t x) {
  log[log_head] = x;
  log_head = (log_head + 1) % LOG_SIZE;
  if(log_head == log_tail) {
    unsigned int len = log_entry_len(log_tail);
    log_tail = (log_tail + 1 + len) % LOG_SIZE;
  }
}

void log_add_first(intptr_t x) {
  msg_head = log_head;
  log_add(x);
}

void log_add_last(intptr_t x) {
  log_add(x);
  if(log_head == log_watch ||
     (log_watch_fmt && log[msg_head] == log_watch_fmt)) {
    if(set_log_watch_fmt) {
      log_watch_fmt = log[msg_head];
    }
    breakpoint();
  }
}

void log_add_only(intptr_t x) {
  msg_head = log_head;
  log_add_last(x);
}

void print_last_log_msg() {
  unsigned int depth = 0;
  log_printf(msg_head, &depth, true);
}

static
bool end_context(unsigned int idx, unsigned int *depth) {
  const char *fmt = (const char *)log[idx];
  // TODO match the ends so that dropping entries doesn't break indentation
  if(fmt[0] != '\xff') return false;
  if(fmt[1] == '\xff') {
    *depth = 0;
    putchar('\n');
  } else if(*depth > 0) {
    (*depth)--;
  }
  return true;
}

static
unsigned int print_contexts(unsigned int idx, unsigned int *depth) {
  if(idx == log_head) return idx;
  const char *fmt = (const char *)log[idx];
  if(fmt[0] == '\xff' ||
     (fmt[0] & REVERSE) == 0) return idx;
  uint8_t len = (fmt[0] & ~MASK) + 1;
  unsigned int ret = print_contexts((idx + len) % LOG_SIZE, depth);
  log_printf(idx, depth, false);
  return ret;
}

void log_print_all() {
  unsigned int
    depth = 0,
    i = log_tail;
  while(i != log_head) {
    i = print_contexts(i, &depth);
    if(i == log_head) break;
    if(end_context(i, &depth)) {
      i = (i + 1) % LOG_SIZE;
    } else {
      i = log_printf(i, &depth, true);
    }
  }
}

#if INTERFACE
#define LOG_0(fmt, x0, x1, x2, x3, x4, x5, x6, x7, ...) \
  do {                                                  \
    log_add_context();                                  \
    log_add_first((intptr_t)("\x08" fmt));              \
    log_add((intptr_t)(x0));                            \
    log_add((intptr_t)(x1));                            \
    log_add((intptr_t)(x2));                            \
    log_add((intptr_t)(x3));                            \
    log_add((intptr_t)(x4));                            \
    log_add((intptr_t)(x5));                            \
    log_add((intptr_t)(x6));                            \
    log_add_last((intptr_t)(x7));                       \
  } while(0)
#define LOG_1(fmt, x0, x1, x2, x3, x4, x5, x6, ...)     \
  do {                                                  \
    log_add_context();                                  \
    log_add_first((intptr_t)("\x07" fmt));              \
    log_add((intptr_t)(x0));                            \
    log_add((intptr_t)(x1));                            \
    log_add((intptr_t)(x2));                            \
    log_add((intptr_t)(x3));                            \
    log_add((intptr_t)(x4));                            \
    log_add((intptr_t)(x5));                            \
    log_add_last((intptr_t)(x6));                       \
  } while(0)
#define LOG_2(fmt, x0, x1, x2, x3, x4, x5, ...) \
  do {                                          \
    log_add_context();                          \
    log_add_first((intptr_t)("\x06" fmt));      \
    log_add((intptr_t)(x0));                    \
    log_add((intptr_t)(x1));                    \
    log_add((intptr_t)(x2));                    \
    log_add((intptr_t)(x3));                    \
    log_add((intptr_t)(x4));                    \
    log_add_last((intptr_t)(x5));               \
  } while(0)
#define LOG_3(fmt, x0, x1, x2, x3, x4, ...)     \
  do {                                          \
    log_add_context();                          \
    log_add_first((intptr_t)("\x05" fmt));      \
    log_add((intptr_t)(x0));                    \
    log_add((intptr_t)(x1));                    \
    log_add((intptr_t)(x2));                    \
    log_add((intptr_t)(x3));                    \
    log_add_last((intptr_t)(x4));               \
  } while(0)
#define LOG_4(fmt, x0, x1, x2, x3, ...)         \
  do {                                          \
    log_add_context();                          \
    log_add_first((intptr_t)("\x04" fmt));      \
    log_add((intptr_t)(x0));                    \
    log_add((intptr_t)(x1));                    \
    log_add((intptr_t)(x2));                    \
    log_add_last((intptr_t)(x3));               \
  } while(0)
#define LOG_5(fmt, x0, x1, x2, ...)             \
  do {                                          \
    log_add_context();                          \
    log_add_first((intptr_t)("\x03" fmt));      \
    log_add((intptr_t)(x0));                    \
    log_add((intptr_t)(x1));                    \
    log_add_last((intptr_t)(x2));               \
  } while(0)
#define LOG_6(fmt, x0, x1, ...)                 \
  do {                                          \
    log_add_context();                          \
    log_add_first((intptr_t)("\x02" fmt));      \
    log_add((intptr_t)(x0));                    \
    log_add_last((intptr_t)(x1));               \
  } while(0)
#define LOG_7(fmt, x0, ...)                     \
  do {                                          \
    log_add_context();                          \
    log_add_first((intptr_t)("\x01" fmt));      \
    log_add_last((intptr_t)(x0));               \
  } while(0)
#define LOG_8(fmt, ...)                         \
  do {                                          \
    log_add_context();                          \
    log_add_only((intptr_t)("\x00" fmt));       \
  } while(0)
#define LOG(fmt, ...) DISPATCH(LOG, 9, __FILE__ ":" STRINGIFY(__LINE__) ": " fmt, ##__VA_ARGS__)
#define LOG_NO_POS(fmt, ...) DISPATCH(LOG, 9, fmt, ##__VA_ARGS__)
#define LOG_WHEN(test, fmt, ...) ((test) && (({ LOG(fmt, ##__VA_ARGS__); }), true))
#define LOG_UNLESS(test, fmt, ...) ((test) || (({ LOG(fmt, ##__VA_ARGS__); }), false))
#endif

int test_log() {
  log_init();
  LOG("test %d + %d = %d", 1, 2, 3);
  LOG("WAZZUP %s", "d00d");
  LOG("[%.*s]", 3, "12345");
  log_print_all();
  return 0;
}

// print the log
void command_log(UNUSED cell_t *rest) {
  log_print_all();
}

#if INTERFACE
typedef struct context_s context_t;
struct context_s {
  struct context_s *next;
  char const *fmt;
  intptr_t arg[0];
};

#define END_OF_CONTEXT_MACRO                    \
  __log_context = (context_t *)__context;
#define CONTEXT_0(fmt, x0, x1, x2, x3, x4, x5, x6, x7, ...)     \
  intptr_t __context[] = {                                      \
    (intptr_t)__log_context,                                    \
    (intptr_t)("\xff\xC8" fmt) + 1,                             \
    (intptr_t)(x0),                                             \
    (intptr_t)(x1),                                             \
    (intptr_t)(x2),                                             \
    (intptr_t)(x3),                                             \
    (intptr_t)(x4),                                             \
    (intptr_t)(x5),                                             \
    (intptr_t)(x6),                                             \
    (intptr_t)(x7)};                                            \
  END_OF_CONTEXT_MACRO
#define CONTEXT_1(fmt, x0, x1, x2, x3, x4, x5, x6, ...) \
  intptr_t __context[] = {                              \
    (intptr_t)__log_context,                            \
    (intptr_t)("\xff\xC7" fmt) + 1,                     \
    (intptr_t)(x0),                                     \
    (intptr_t)(x1),                                     \
    (intptr_t)(x2),                                     \
    (intptr_t)(x3),                                     \
    (intptr_t)(x4),                                     \
    (intptr_t)(x5),                                     \
    (intptr_t)(x6)};                                    \
  END_OF_CONTEXT_MACRO
#define CONTEXT_2(fmt, x0, x1, x2, x3, x4, x5, ...)     \
  intptr_t __context[] = {                              \
    (intptr_t)__log_context,                            \
    (intptr_t)("\xff\xC6" fmt) + 1,                     \
    (intptr_t)(x0),                                     \
    (intptr_t)(x1),                                     \
    (intptr_t)(x2),                                     \
    (intptr_t)(x3),                                     \
    (intptr_t)(x4),                                     \
    (intptr_t)(x5)};                                    \
  END_OF_CONTEXT_MACRO
#define CONTEXT_3(fmt, x0, x1, x2, x3, x4, ...) \
  intptr_t __context[] = {                      \
    (intptr_t)__log_context,                    \
    (intptr_t)("\xff\xC5" fmt) + 1,             \
    (intptr_t)(x0),                             \
    (intptr_t)(x1),                             \
    (intptr_t)(x2),                             \
    (intptr_t)(x3),                             \
    (intptr_t)(x4)};                            \
  END_OF_CONTEXT_MACRO
#define CONTEXT_4(fmt, x0, x1, x2, x3, ...)     \
  intptr_t __context[] = {                      \
    (intptr_t)__log_context,                    \
    (intptr_t)("\xff\xC4" fmt) + 1,             \
    (intptr_t)(x0),                             \
    (intptr_t)(x1),                             \
    (intptr_t)(x2),                             \
    (intptr_t)(x3)};                            \
  END_OF_CONTEXT_MACRO
#define CONTEXT_5(fmt, x0, x1, x2, ...)         \
  intptr_t __context[] = {                      \
    (intptr_t)__log_context,                    \
    (intptr_t)("\xff\xC3" fmt) + 1,             \
    (intptr_t)(x0),                             \
    (intptr_t)(x1),                             \
    (intptr_t)(x2)};                            \
  END_OF_CONTEXT_MACRO
#define CONTEXT_6(fmt, x0, x1, ...)             \
  intptr_t __context[] = {                      \
    (intptr_t)__log_context,                    \
    (intptr_t)("\xff\xC2" fmt) + 1,             \
    (intptr_t)(x0),                             \
    (intptr_t)(x1)};                            \
  END_OF_CONTEXT_MACRO
#define CONTEXT_7(fmt, x0, ...)                 \
  intptr_t __context[] = {                      \
    (intptr_t)__log_context,                    \
    (intptr_t)("\xff\xC1" fmt) + 1,             \
    (intptr_t)(x0)};                            \
  END_OF_CONTEXT_MACRO
#define CONTEXT_8(fmt, ...)                     \
  intptr_t __context[] = {                      \
    (intptr_t)__log_context,                    \
    (intptr_t)("\xff\xC0" fmt) + 1};            \
  END_OF_CONTEXT_MACRO
#define CONTEXT(fmt, ...) __attribute__((cleanup(log_cleanup_context))) DISPATCH(CONTEXT, 9, __FILE__ ":" STRINGIFY(__LINE__) ": " fmt, ##__VA_ARGS__)
#endif

#if INTERFACE
#define CONTEXT_LOG_0(fmt, x0, x1, x2, x3, x4, x5, x6, x7, ...) \
  do {                                                          \
    log_add_context();                                          \
    __context = "\xff\x48" fmt;                                 \
    log_add_first((intptr_t)(__context + 1));                   \
    log_add((intptr_t)(x0));                                    \
    log_add((intptr_t)(x1));                                    \
    log_add((intptr_t)(x2));                                    \
    log_add((intptr_t)(x3));                                    \
    log_add((intptr_t)(x4));                                    \
    log_add((intptr_t)(x5));                                    \
    log_add((intptr_t)(x6));                                    \
    log_add_last((intptr_t)(x7));                               \
  } while(0)
#define CONTEXT_LOG_1(fmt, x0, x1, x2, x3, x4, x5, x6, ...)     \
  do {                                                          \
    log_add_context();                                          \
    __context = "\xff\x47" fmt;                                 \
    log_add_first((intptr_t)(__context + 1));                   \
    log_add((intptr_t)(x0));                                    \
    log_add((intptr_t)(x1));                                    \
    log_add((intptr_t)(x2));                                    \
    log_add((intptr_t)(x3));                                    \
    log_add((intptr_t)(x4));                                    \
    log_add((intptr_t)(x5));                                    \
    log_add_last((intptr_t)(x6));                               \
  } while(0)
#define CONTEXT_LOG_2(fmt, x0, x1, x2, x3, x4, x5, ...) \
  do {                                                  \
    log_add_context();                                  \
    __context = "\xff\x46" fmt;                         \
    log_add_first((intptr_t)(__context + 1));           \
    log_add((intptr_t)(x0));                            \
    log_add((intptr_t)(x1));                            \
    log_add((intptr_t)(x2));                            \
    log_add((intptr_t)(x3));                            \
    log_add((intptr_t)(x4));                            \
    log_add_last((intptr_t)(x5));                       \
  } while(0)
#define CONTEXT_LOG_3(fmt, x0, x1, x2, x3, x4, ...)     \
  do {                                                  \
    log_add_context();                                  \
    __context = "\xff\x45" fmt;                         \
    log_add_first((intptr_t)(__context + 1));           \
    log_add((intptr_t)(x0));                            \
    log_add((intptr_t)(x1));                            \
    log_add((intptr_t)(x2));                            \
    log_add((intptr_t)(x3));                            \
    log_add_last((intptr_t)(x4));                       \
  } while(0)
#define CONTEXT_LOG_4(fmt, x0, x1, x2, x3, ...) \
  do {                                          \
    log_add_context();                          \
    __context = "\xff\x44" fmt;                 \
    log_add_first((intptr_t)(__context + 1));   \
    log_add((intptr_t)(x0));                    \
    log_add((intptr_t)(x1));                    \
    log_add((intptr_t)(x2));                    \
    log_add_last((intptr_t)(x3));               \
  } while(0)
#define CONTEXT_LOG_5(fmt, x0, x1, x2, ...)     \
  do {                                          \
    log_add_context();                          \
    __context = "\xff\x43" fmt;                 \
    log_add_first((intptr_t)(__context + 1));   \
    log_add((intptr_t)(x0));                    \
    log_add((intptr_t)(x1));                    \
    log_add_last((intptr_t)(x2));               \
  } while(0)
#define CONTEXT_LOG_6(fmt, x0, x1, ...)         \
  do {                                          \
    log_add_context();                          \
    __context = "\xff\x42" fmt;                 \
    log_add_first((intptr_t)(__context + 1));   \
    log_add((intptr_t)(x0));                    \
    log_add_last((intptr_t)(x1));               \
  } while(0)
#define CONTEXT_LOG_7(fmt, x0, ...)             \
  do {                                          \
    log_add_context();                          \
    __context = "\xff\x41" fmt;                 \
    log_add_first((intptr_t)(__context + 1));   \
    log_add_last((intptr_t)(x0));               \
  } while(0)
#define CONTEXT_LOG_8(fmt, ...)                 \
  do {                                          \
    log_add_context();                          \
    __context = "\xff\x40" fmt;                 \
    log_add_only((intptr_t)(__context + 1));    \
  } while(0)
#define CONTEXT_LOG(fmt, ...)                                           \
  const char *__context __attribute__((cleanup(log_cleanup_context_log))); \
  DISPATCH(CONTEXT_LOG, 9, __FILE__ ":" STRINGIFY(__LINE__) ": " fmt, ##__VA_ARGS__)
#endif

void log_cleanup_context(void *p) {
  context_t *ctx = p;
  if(ctx->fmt[0] == '\xff') {
    // add end marker
    log_add((intptr_t)ctx->fmt);
  }
  __log_context = ctx->next;
}

void log_cleanup_context_log(const char **fmt) {
  log_add((intptr_t)*fmt);
}

void log_add_context() {
  context_t *p = __log_context;
  while(p &&
        p->fmt[0] != '\xff') {
    log_add((intptr_t)p->fmt);
    uint8_t len = p->fmt[0] & ~MASK;
    COUNTUP(i, len) {
      log_add(p->arg[i]);
    }
    p->fmt--;
    p = p->next;
  }
}

static
void __test_context_c(int x) {
  CONTEXT_LOG("C %d", x);
}

static
void __test_context_b(int x) {
  CONTEXT("B %d", x);
  LOG_WHEN(x == 0, "(b) zero x");
}

static
void __test_context_a(int x) {
  CONTEXT("A %d", x);
  __test_context_b(x - 1);
  LOG_WHEN(x > 0, "(a) nonzero x");
}

static
void __test_context_e(int x) {
  CONTEXT_LOG("E %d", x);
}

static
void __test_context_d(int x) {
  CONTEXT_LOG("D %d", x);
  __test_context_e(x);
  LOG("exiting d");
}

int test_context() {
  log_init();
  __test_context_a(2);
  __test_context_a(1);
  __test_context_a(0);
  __test_context_c(3);
  __test_context_d(42);
  log_print_all();
  return 0;
}

#if INTERFACE
typedef char tag_t[4];
#define FORMAT_TAG "%.4s"
#endif

char to_tag_char(int x) {
  x &= 31;
  if(x < 24) {
    return 'a' + x;
  } else {
    return '2' + x - 24;
  }
}

int from_tag_char(char c) {
  if(c >= 'a') {
    if(c <= 'x') {
      return c - 'a';
    } else {
      return -1;
    }
  } else if(c >= '0') {
    return c - '2' + 24;
  } else {
    return -1;
  }
}

int spread_bits(int x) {
  int y = 0;
  COUNTUP(i, 4) {
    int t = 0;
    COUNTUP(j, 5) {
      t <<= 4;
      t |= x & 1;
      x >>= 1;
    }
    t <<= i;
    y |= t;
  }
  return y;
}

int gather_bits(int y) {
  int x = 0;
  COUNTDOWN(i, 4) {
    int t = y >> i;
    COUNTUP(j, 5) {
      x <<= 1;
      x |= t & 1;
      t >>= 4;
    }
  }
  return x;
}

int test_spread_gather_bits() {
  int x = 0x9AC35;
  int spread = spread_bits(x);
  int gather = gather_bits(spread);
  return x == gather ? 0 : -1;
}

// modular multiplicative inverses
const unsigned int tag_factor = 510199;
const unsigned int tag_factor_inverse = 96455;
const unsigned int tag_mask = 0x7ffff;

void write_tag(tag_t tag, unsigned int val) {
  val *= tag_factor;
  val &= tag_mask;
  COUNTDOWN(i, sizeof(tag_t)) {
    tag[i] = to_tag_char(val);
    val >>= 5;
  }
}

int read_tag(const tag_t tag) {
  unsigned int val = 0;
  COUNTUP(i, sizeof(tag_t)) {
    int x = from_tag_char(tag[i]);
    if(x < 0) return x;
    val = (val << 5) | x;
  }
  val *= tag_factor_inverse;
  val &= tag_mask;
  return val;
}

int test_tag() {
  tag_t tag = "good";
  int x = read_tag(tag);
  write_tag(tag, x);
  printf("tag: %d = " FORMAT_TAG "\n", x, tag);
  return strcmp("good", tag) == 0 ? 0 : -1;
}
