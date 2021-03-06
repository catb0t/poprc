/* Copyright 2012-2018 Dustin DeWeese
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

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "rt_types.h"

#include "startle/error.h"
#include "startle/support.h"
#include "startle/log.h"

#include "cells.h"
#include "rt.h"
#include "primitive.h"
#include "special.h"
#include "print.h"
#include "trace.h"
#include "list.h"
#include "parse.h"
#include "builders.h"

   /*-----------------------------------------------,
    |          VARIABLE NAME CONVENTIONS            |
    |-----------------------------------------------|
    |                                               |
    |  cell_t *c = closure being reduced            |
    |  cell_t *d = first dep                        |
    |    (second closure returned, lower on stack)  |
    |  cell_t *e = second dep                       |
    |  cell_t *f = ...                              |
    |  cell_t *p = expr.arg[0], leftmost arg        |
    |  cell_t *q = expr.arg[1]                      |
    |  bool s = success                             |
    |  cell_t *res = result to be stored in c       |
    |  cell_t *res_X = result to be stored in X     |
    |                                               |
    '-----------------------------------------------*/

cell_t *_op2(cell_t *c, uint8_t t, val_t (*op)(val_t, val_t), cell_t *x, cell_t *y) {
  if(is_var(x) || is_var(y)) {
    return var(t, c);
  } else {
    return val(t, op(x->value.integer,
                     y->value.integer));
  }
}

cell_t *_op1(cell_t *c, uint8_t t, val_t (*op)(val_t), cell_t *x) {
  if(is_var(x)) {
    return var(t, c);
  } else {
    return val(t, op(x->value.integer));
  }
}

response func_op2(cell_t **cp, context_t *ctx, int arg_type, int res_type, val_t (*op)(val_t, val_t), bool nonzero) {
  cell_t *res = 0;
  PRE(op2);

  CHECK_IF(!check_type(ctx->t, res_type), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(t, arg_type)));
  CHECK(reduce_arg(c, 1, &CTX(t, arg_type)));
  CHECK_IF(as_conflict(ctx->alt_set), FAIL);
  CHECK_DELAY();
  clear_flags(c);

  cell_t *p = c->expr.arg[0], *q = c->expr.arg[1];
  CHECK_IF(nonzero && !is_var(q) && q->value.integer == 0, FAIL); // TODO assert this for variables
  res = _op2(c, res_type, op, p, q);
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  add_conditions(res, p, q);
  if(nonzero) FLAG_SET(c->expr, EXPR_PARTIAL);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

response func_op1(cell_t **cp, context_t *ctx, int arg_type, int res_type, val_t (*op)(val_t), val_t (*inv_op)(val_t)) {
  cell_t *res = 0;
  PRE(op1);

  CHECK_IF(!check_type(ctx->t, res_type), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(t, arg_type, CTX_INV(inv_op))));
  CHECK_DELAY();
  clear_flags(c);

  cell_t *p = c->expr.arg[0];
  res = _op1(c, res_type, op, p);
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  add_conditions(res, p);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

cell_t *_op2_float(double (*op)(double, double), cell_t *x, cell_t *y) {
  return float_val(op(x->value.flt,
                      y->value.flt));
}

cell_t *_op1_float(double (*op)(double), cell_t *x) {
  return float_val(op(x->value.flt));
}

response func_op2_float(cell_t **cp, context_t *ctx, double (*op)(double, double), bool nonzero) {
  cell_t *res = 0;
  PRE(op2_float);

  CHECK_IF(!check_type(ctx->t, T_FLOAT), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(float)));
  CHECK(reduce_arg(c, 1, &CTX(float)));
  CHECK_IF(as_conflict(ctx->alt_set), FAIL);
  CHECK_DELAY();
  clear_flags(c);

  cell_t *p = c->expr.arg[0], *q = c->expr.arg[1];
  CHECK_IF(nonzero && !is_var(q) && q->value.flt == 0.0, FAIL); // TODO assert this for variables
  res = is_var(p) || is_var(q) ? var(ctx->t, c) : _op2_float(op, p, q);
  res->value.type = T_FLOAT;
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  add_conditions(res, p, q);
  if(nonzero) FLAG_SET(c->expr, EXPR_PARTIAL);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

response func_op1_float(cell_t **cp, context_t *ctx, double (*op)(double)) {
  cell_t *res = 0;
  PRE(op1_float);

  CHECK_IF(!check_type(ctx->t, T_FLOAT), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(float)));
  CHECK_DELAY();
  clear_flags(c);

  cell_t *p = c->expr.arg[0];
  res = is_var(p) ? var(ctx->t, c) : _op1_float(op, p);
  res->value.type = T_FLOAT;
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  add_conditions(res, p);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

WORD("+", add, 2, 1)
val_t add_op(val_t x, val_t y) { return x + y; }
OP(add) {
  return func_op2(cp, ctx, T_INT, T_INT, add_op, false);
}

WORD("*", mul, 2, 1)
val_t mul_op(val_t x, val_t y) { return x * y; }
OP(mul) {
  return func_op2(cp, ctx, T_INT, T_INT, mul_op, false);
}

WORD("-", sub, 2, 1)
val_t sub_op(val_t x, val_t y) { return x - y; }
OP(sub) {
  return func_op2(cp, ctx, T_INT, T_INT, sub_op, false);
}

WORD("/", div, 2, 1)
val_t div_op(val_t x, val_t y) { return x / y; }
OP(div) {
  return func_op2(cp, ctx, T_INT, T_INT, div_op, true);
}

WORD("%", mod, 2, 1)
val_t mod_op(val_t x, val_t y) { return x % y; }
OP(mod) {
  return func_op2(cp, ctx, T_INT, T_INT, mod_op, true);
}

WORD("+f", add_float, 2, 1)
double add_float_op(double x, double y) { return x + y; }
OP(add_float) {
  return func_op2_float(cp, ctx, add_float_op, false);
}

WORD("*f", mul_float, 2, 1)
double mul_float_op(double x, double y) { return x * y; }
OP(mul_float) {
  return func_op2_float(cp, ctx, mul_float_op, false);
}

WORD("-f", sub_float, 2, 1)
double sub_float_op(double x, double y) { return x - y; }
OP(sub_float) {
  return func_op2_float(cp, ctx, sub_float_op, false);
}

WORD("/f", div_float, 2, 1)
double div_float_op(double x, double y) { return x / y; }
OP(div_float) {
  return func_op2_float(cp, ctx, div_float_op, true);
}

WORD("log", log, 1, 1)
OP(log) {
  return func_op1_float(cp, ctx, log);
}

WORD("exp", exp, 1, 1)
OP(exp) {
  return func_op1_float(cp, ctx, exp);
}

WORD("cos", cos, 1, 1)
OP(cos) {
  return func_op1_float(cp, ctx, cos);
}

WORD("sin", sin, 1, 1)
OP(sin) {
  return func_op1_float(cp, ctx, sin);
}

WORD("tan", tan, 1, 1)
OP(tan) {
  return func_op1_float(cp, ctx, tan);
}

WORD("atan2", atan2, 2, 1)
OP(atan2) {
  return func_op2_float(cp, ctx, atan2, false);
}

WORD("&b", bitand, 2, 1)
val_t bitand_op(val_t x, val_t y) { return x & y; }
OP(bitand) {
  return func_op2(cp, ctx, T_INT, T_INT, bitand_op, false);
}

WORD("|b", bitor, 2, 1)
val_t bitor_op(val_t x, val_t y) { return x | y; }
OP(bitor) {
  return func_op2(cp, ctx, T_INT, T_INT, bitor_op, false);
}

WORD("^b", bitxor, 2, 1)
val_t bitxor_op(val_t x, val_t y) { return x ^ y; }
OP(bitxor) {
  return func_op2(cp, ctx, T_INT, T_INT, bitxor_op, false);
}

WORD("<<b", shiftl, 2, 1)
val_t shiftl_op(val_t x, val_t y) { return x << y; }
OP(shiftl) {
  return func_op2(cp, ctx, T_INT, T_INT, shiftl_op, false);
}

WORD(">>b", shiftr, 2, 1)
val_t shiftr_op(val_t x, val_t y) { return x >> y; }
OP(shiftr) {
  return func_op2(cp, ctx, T_INT, T_INT, shiftr_op, false);
}

WORD("~b", complement, 1, 1)
val_t complement_op(val_t x) { return ~x; }
OP(complement) {
  return func_op1(cp, ctx, T_INT, T_INT, complement_op, complement_op);
}

WORD("not", not, 1, 1)
val_t not_op(val_t x) { return !x; }
OP(not) {
  return func_op1(cp, ctx, T_SYMBOL, T_SYMBOL, not_op, not_op);
}

WORD(">", gt, 2, 1)
val_t gt_op(val_t x, val_t y) { return x > y; }
OP(gt) {
  return func_op2(cp, ctx, T_INT, T_SYMBOL, gt_op, false);
}

WORD(">=", gte, 2, 1)
val_t gte_op(val_t x, val_t y) { return x >= y; }
OP(gte) {
  return func_op2(cp, ctx, T_INT, T_SYMBOL, gte_op, false);
}

WORD("<", lt, 2, 1)
val_t lt_op(val_t x, val_t y) { return x < y; }
OP(lt) {
  return func_op2(cp, ctx, T_INT, T_SYMBOL, lt_op, false);
}

WORD("<=", lte, 2, 1)
val_t lte_op(val_t x, val_t y) { return x <= y; }
OP(lte) {
  return func_op2(cp, ctx, T_INT, T_SYMBOL, lte_op, false);
}

WORD("==", eq, 2, 1)
WORD("=:=", eq_s, 2, 1)
val_t eq_op(val_t x, val_t y) { return x == y; }
OP(eq) {
  return func_op2(cp, ctx, T_INT, T_SYMBOL, eq_op, false);
}
OP(eq_s) {
  return func_op2(cp, ctx, T_SYMBOL, T_SYMBOL, eq_op, false);
}

WORD("!=", neq, 2, 1)
WORD("!:=", neq_s, 2, 1)
val_t neq_op(val_t x, val_t y) { return x != y; }
OP(neq) {
  return func_op2(cp, ctx, T_INT, T_SYMBOL, neq_op, false);
}
OP(neq_s) {
  return func_op2(cp, ctx, T_SYMBOL, T_SYMBOL, neq_op, false);
}

WORD("->f", to_float, 1, 1)
OP(to_float) {
  cell_t *res = 0;
  PRE(to_float);

  CHECK_IF(!check_type(ctx->t, T_FLOAT), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(int)));
  CHECK_DELAY();
  clear_flags(c);

  cell_t *p = c->expr.arg[0];
  if(is_var(p)) {
    res = var(T_FLOAT, c);
  } else {
    res = float_val(p->value.integer);
  }
  res->value.type = T_FLOAT;
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  add_conditions(res, p);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

WORD("trunc", trunc, 1, 1)
OP(trunc) {
  cell_t *res = 0;
  PRE(to_float);

  CHECK_IF(!check_type(ctx->t, T_INT), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(float)));
  CHECK_DELAY();
  clear_flags(c);

  cell_t *p = c->expr.arg[0];
  if(is_var(p)) {
    res = var(T_INT, c);
  } else {
    res = int_val(p->value.flt);
  }
  res->value.type = T_INT;
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  add_conditions(res, p);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

cell_t *set_alt(cell_t *c, alt_set_t as, cell_t *alt) {
  if(!c) return NULL;
  if(!as && !alt) return c;
  if(!c->n) {
    if(!as) {
      c->alt = conc_alt(alt, c->alt);
      return c;
    } else if(is_value(c)) {
      c->alt = conc_alt(alt, c->alt);
      c->value.alt_set = as;
      return c;
    }
  }
  cell_t *i = build_id(c);
  i->expr.alt_set = as;
  i->alt = alt;
  return i;
}

WORD("|", alt, 2, 1)
OP(alt) {
  PRE(alt);
  alt_set_t as0 = 0, as1 = 0;
  if(!is_linear(ctx)) {
    uint8_t a = new_alt_id(1);
    as0 = as_single(a, 0);
    as1 = as_single(a, 1);
  }
  cell_t *r1 = set_alt(c->expr.arg[1], as1, c->alt);
  cell_t *r0 = set_alt(c->expr.arg[0], as0, r1);
  store_lazy(cp, r0, 0);
  return RETRY;
}

WORD("!", assert, 2, 1)
OP(assert) {
  PRE(assert);
  CHECK_PRIORITY(PRIORITY_ASSERT);

  cell_t *res = NULL;
  cell_t *tc = NULL;
  cell_t *p = NULL;
  bool p_var = false;
  CHECK(reduce_arg(c, 1, &CTX(symbol, SYM_True)));
  if(rsp == SUCCESS) {
    p = clear_ptr(c->expr.arg[1]);
    p_var = is_var(p);

    CHECK_IF(!p_var &&
             p->value.integer != SYM_True, FAIL);

    if(p_var) {
      tc = trace_partial(OP_assert, 1, p);
    }
  }
  CHECK(reduce_arg(c, 0, &CTX_UP));
  CHECK_IF(as_conflict(ctx->alt_set), FAIL);
  CHECK_DELAY();
  clear_flags(c);
  cell_t **q = &c->expr.arg[0];

  if(p_var && is_var(*q)) {
    res = var_create((*q)->value.type, tc, 0, 0);
    trace_arg(tc, 0, *q);
  } else {
    // handle is_var(*q)?
    res = take(q);
    unique(&res);
    drop(res->alt);
    add_conditions_var(res, tc, p);
  }
  res->value.alt_set = ctx->alt_set;
  res->alt = c->alt;

  FLAG_SET(c->expr, EXPR_PARTIAL);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

// for internal use
// very similar to assert
WORD("seq", seq, 2, 1)
OP(seq) {
  PRE(seq);

  cell_t *res = NULL;
  cell_t *tc = NULL;
  cell_t *p = NULL;
  bool p_var = false;
  CHECK(reduce_arg(c, 1, &CTX(any))); // don't split arg here?
  if(rsp == SUCCESS) {
    p = clear_ptr(c->expr.arg[1]);
    p_var = is_var(p);

    if(p_var) {
      tc = trace_partial(OP_seq, 1, p);
    }
  }
  CHECK(reduce_arg(c, 0, &CTX_UP));
  CHECK_IF(as_conflict(ctx->alt_set), FAIL);
  CHECK_DELAY();
  clear_flags(c);
  cell_t **q = &c->expr.arg[0];

  if(p_var && is_var(*q)) {
    res = var_create((*q)->value.type, tc, 0, 0);
    trace_arg(tc, 0, *q);
  } else {
    res = take(q);
    unique(&res);
    drop(res->alt);
    add_conditions_var(res, tc, p);
  }
  res->value.alt_set = ctx->alt_set;
  res->alt = c->alt;

  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

// merge two otherwise's into a seq
bool rule_merge_otherwise(context_t *ctx) {
  if(ctx->inv) {
    cell_t *c = ctx->src;
    cell_t *p = ctx->up->src;
    if(p->op == OP_otherwise && clear_ptr(p->expr.arg[0]) == c) {
      LOG("rule match: merge_otherwise %C %C", p, c);
      p->op = OP_seq;
      p->expr.arg[0] = p->expr.arg[1];
      p->expr.arg[1] = ref(c->expr.arg[0]);
      drop(c);
      ctx->retry = true;
      return true;
    }
  }
  return false;
}

// very similar to assert
// TODO merge common code
WORD("otherwise", otherwise, 2, 1)
OP(otherwise) {
  PRE(otherwise);
  RULE(merge_otherwise);
  CHECK_PRIORITY(PRIORITY_OTHERWISE);

  cell_t *res = NULL;
  cell_t *tc = NULL, *tp = NULL;

  // reduce each alt
  cell_t **p = &c->expr.arg[0];
  cell_t **q = &c->expr.arg[1];
  while(*p) {
    response rsp0 = reduce(p, WITH(&CTX(any), inv, !ctx->inv));
    CHECK_IF(rsp0 == RETRY, RETRY);
    if(rsp0 == DELAY) {
      rsp = DELAY;
    } else {
      CHECK_IF(!is_var(*p) &&
               !(*p)->value.var &&
               rsp0 != FAIL, FAIL);
      if(rsp0 != FAIL) {
        tc = concatenate_conditions(trace_partial(OP_otherwise, 0, *p), tc);
        if(!tp) tp = tc;
      }
    }

    p = &(*p)->alt;
  }

  CHECK(reduce_arg(c, 1, &CTX_UP));
  CHECK_DELAY();
  clear_flags(c);

  if(tc && is_var(*q)) {
    // propagate type
    cell_t *entry = var_entry(tc);
    for(cell_t *p = tc; p != tp;
        p = &entry[trace_decode(p->expr.arg[1])]) {
      trace_set_type(p, (*q)->value.type);
    }

    res = var_create((*q)->value.type, tc, 0, 0);
    trace_arg(tp, 1, *q);
  } else {
    // handle is_var(*q)?
    res = take(q);
    unique(&res);
    drop(res->alt);
    add_conditions_var(res, tc);
  }
  res->value.alt_set = ctx->alt_set;
  res->alt = c->alt;

  FLAG_SET(c->expr, EXPR_PARTIAL);
  store_reduced(cp, res);
  return SUCCESS;

abort:
  return abort_op(rsp, cp, ctx);
}

WORD("id", id, 1, 1)
OP(id) {
  PRE(id);
  ctx->alt_set = c->expr.alt_set;
  int pos = c->pos;

  if(ctx->alt_set || c->alt) {
    CHECK(reduce_arg(c, 0, &CTX_UP));
    CHECK_IF(as_conflict(ctx->alt_set), FAIL);
    CHECK_DELAY();
    clear_flags(c);
    cell_t *res = mod_alt(ref(c->expr.arg[0]), c->alt, ctx->alt_set);
    mark_pos(res, pos);
    store_reduced(cp, res);
    return SUCCESS;
  } else {
    *cp = CUT(c, expr.arg[0]);
    mark_pos(*cp, pos);
    return RETRY;
  }

 abort:
  return abort_op(rsp, cp, ctx);
}

WORD("drop", drop, 2, 1)
OP(drop) {
  PRE(drop);
  *cp = CUT(c, expr.arg[0]);
  return RETRY;
}

WORD("swap", swap, 2, 2)
OP(swap) {
  PRE(swap);
  int pos = c->pos;
  store_lazy_dep(c->expr.arg[2],
                 c->expr.arg[0], 0);
  store_lazy(cp, c->expr.arg[1], 0);
  mark_pos(*cp, pos);
  return RETRY;
}

// for testing
WORD("delay", delay, 1, 1)
OP(delay) {
  PRE(delay);
  CHECK_PRIORITY(PRIORITY_DELAY);
  store_lazy(cp, c->expr.arg[0], 0);
  return RETRY;

abort:
  return abort_op(rsp, cp, ctx);
}

cell_t *id(cell_t *c, alt_set_t as) {
  if(!c) return NULL;
  cell_t *i = build_id(c);
  i->expr.alt_set = as;
  return i;
}

WORD("dup", dup, 1, 2)
OP(dup) {
  PRE(dup);
  cell_t *d = c->expr.arg[1];
  store_lazy_dep(d, ref(c->expr.arg[0]), 0);
  store_lazy(cp, c->expr.arg[0], 0);
  return RETRY;
}

// outputs required from the left operand given the rignt operand
csize_t function_compose_out(cell_t *c, csize_t arg_in, csize_t out) {
  c = clear_ptr(c);
  return csub(function_in(c) + csub(out, function_out(c, true)), arg_in);
}

// inputs required from the right operand given the left operand
csize_t function_compose_in(cell_t *c, csize_t req_in, csize_t arg_in, bool row) {
  c = clear_ptr(c);
  return csub(req_in, function_in(c)) + function_out(c, row) + arg_in;
}

static
response func_compose_ap(cell_t **cp, context_t *ctx, bool row) {
  CONTEXT("%s: %C", row ? "compose" : "ap", *cp);
  PRE_NO_CONTEXT(compose_ap);

  const csize_t
    in = closure_in(c) - 1,
    arg_in = in - row,
    n = closure_args(c),
    out = closure_out(c);

  cell_t *p = NULL;
  cell_t *res = NULL;
  int pos = c->pos ? c->pos : ((cell_t *)clear_ptr(c->expr.arg[in]))->pos;

  if(row) {
    CHECK(reduce_arg(c, 0, &CTX(list, ctx->in, 0)));
    CHECK_DELAY();
    p = clear_ptr(c->expr.arg[0]);
  }
  CHECK(reduce_arg(c, in,
                   &CTX(list,
                        function_compose_in(p, out ? 0 : ctx->in, arg_in, false /*_1_*/),
                        ctx->out + out)));
  CHECK_IF(as_conflict(ctx->alt_set), FAIL);
  CHECK_DELAY();
  clear_flags(c);
  if(row) {
    placeholder_extend(&c->expr.arg[0], ctx->in, function_compose_out(c->expr.arg[in], arg_in, ctx->out + out));
    p = clear_ptr(c->expr.arg[0]);
  }
  cell_t **q = &c->expr.arg[in];
  placeholder_extend(q, function_compose_in(p, ctx->in, arg_in, true /*_1_*/), ctx->out + out);
  // *** _1_ don't know if/why this works

  reverse_ptrs((void **)c->expr.arg, in);
  list_iterator_t it = {
    .array = c->expr.arg,
    .size = in - row,
    .row = row
  };

  // Maybe remove these?
  // *** prevent leaking outside variables into lists
  if(row && !pos) pos = p->pos;
  if(!pos) pos = (*q)->pos;

  cell_t *l = compose(it, ref(*q)); // TODO prevent leaking outside variables
  reverse_ptrs((void **)c->expr.arg, in);

  insert_root(q);
  it = list_begin(l);
  {
    list_iterator_t end = it;
    LOOP(out) list_next(&end, false);
    res = list_rest(end);
  }
  unique(&res);
  drop(res->alt);
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  //res->pos = pos; // ***
  add_conditions(res, p, *q);

  COUNTUP(i, out) {
    cell_t **x = list_next(&it, false);
    cell_t *d = c->expr.arg[n-1-i];
    if(d) {
      if(!x) {
        drop(l);
        LOG("null quote output");
        ABORT(FAIL);
      }
      mark_pos(*x, pos);
      cell_t *seq_x = build_seq(ref(*x), ref(res));
      store_lazy_dep(d, seq_x, ctx->alt_set);
      LOG_WHEN(res->alt, MARK("WARN") " alt seq dep %C <- %C #condition", d, seq_x);
      d->pos = pos; // ***
    }
  }
  remove_root(q);

  drop(l);
  if(out) FLAG_SET(c->expr, EXPR_PARTIAL);
  store_reduced(cp, res);
  ASSERT_REF();
  return SUCCESS;

 abort:
  drop(res);
  return abort_op(rsp, cp, ctx);
}

WORD_ALIAS("pushl", ap, 2, 1, pushl)
WORD_ALIAS("popr", ap, 1, 2, popr)
OP(ap) {
  return func_compose_ap(cp, ctx, false);
}

WORD(".", compose, 2, 1)
OP(compose) {
  return func_compose_ap(cp, ctx, true);
}

static
cell_t *expand_nil(cell_t *c) {
  int n = c->n;
  c->n = 0;
  c = expand(c, 1);
  c->n = n;
  c->expr.arg[closure_args(c) - 1] = empty_list();
  return c;
}

WORD("quote", quote, 1, 1)
OP(quote) {
  (*cp)->op = OP_ap;
  *cp = expand_nil(*cp);
  return RETRY;
}

WORD("pushr", pushr, 2, 1)
OP(pushr) {
  (*cp)->op = OP_compose;
  *cp = expand_nil(*cp);
  return RETRY;
}

WORD("print", print, 2, 1)
OP(print) {
  cell_t *res = 0;
  PRE(print);

  CHECK_IF(!check_type(ctx->t, T_SYMBOL), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(symbol)));
  CHECK(reduce_arg(c, 1, &CTX(string)));
  CHECK_IF(as_conflict(ctx->alt_set), FAIL);
  CHECK_DELAY();
  clear_flags(c);

  if(c->alt) {
    drop(c->alt);
    c->alt = 0;
  }

  cell_t *p = c->expr.arg[0], *q = c->expr.arg[1];
  if(is_var(p) || is_var(q)) {
    res = var(T_SYMBOL, c);
  } else if(p->value.integer == SYM_IO) {
    io->write(string_seg(q->value.str));
    res = ref(p);
  } else {
    ABORT(FAIL);
  }
  add_conditions(res, p, q);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

WORD("input", input, 1, 2)
OP(input) {
  cell_t *res = 0;
  PRE(input);

  CHECK_IF(!check_type(ctx->t, T_SYMBOL), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(symbol)));
  CHECK_IF(as_conflict(ctx->alt_set), FAIL);
  CHECK_DELAY();
  clear_flags(c);

  if(c->alt) {
    drop(c->alt);
    c->alt = 0;
  }

  cell_t *p = c->expr.arg[0];
  if(is_var(p)) {
    res = var(T_SYMBOL, c);
    store_dep_var(c, res, 1, T_STRING, ctx->alt_set);
  } else if(p->value.integer == SYM_IO) {
    seg_t s = io->read();
    store_lazy_dep(c->expr.arg[1], make_string(s), 0);
    res = ref(p);
  } else {
    ABORT(FAIL);
  }
  add_conditions(res, p);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

bool is_list_var(cell_t *c) {
  return is_row_list(c) && is_placeholder(c->value.ptr[0]);
}

response func_type(cell_t **cp, context_t *ctx, uint8_t type) {
  PRE(type);

  CHECK_IF(!check_type(ctx->t, type), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(t, type)));
  CHECK_DELAY();
  clear_flags(c);

  *cp = mod_alt(ref(c->expr.arg[0]), ref(c->alt), 0);
  drop(c);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

// annotations to work around inference failures

WORD("int_t", int_t, 1, 1)
OP(int_t) {
  return func_type(cp, ctx, T_INT);
}

WORD("symbol_t", symbol_t, 1, 1)
OP(symbol_t) {
  return func_type(cp, ctx, T_SYMBOL);
}

WORD("++", strcat, 2, 1)
OP(strcat) {
  cell_t *res = 0;
  PRE(strcat);

  CHECK_IF(!check_type(ctx->t, T_STRING), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(string)));
  CHECK(reduce_arg(c, 1, &CTX(string)));
  CHECK_IF(as_conflict(ctx->alt_set), FAIL);
  CHECK_DELAY();
  clear_flags(c);

  cell_t *p = c->expr.arg[0], *q = c->expr.arg[1];
  if(is_var(p) || is_var(q)) {
    res = var(T_STRING, c);
  } else {
    res = make_strcat(string_seg(p->value.str),
                      string_seg(q->value.str));
  }
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  add_conditions(res, p, q);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

WORD("=s", eq_str, 2, 1)
OP(eq_str) {
  cell_t *res = 0;
  PRE(eq_str);

  CHECK_IF(!check_type(ctx->t, T_SYMBOL), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(string)));
  CHECK(reduce_arg(c, 1, &CTX(string)));
  CHECK_IF(as_conflict(ctx->alt_set), FAIL);
  CHECK_DELAY();
  clear_flags(c);

  cell_t *p = c->expr.arg[0], *q = c->expr.arg[1];
  if(is_var(p) || is_var(q)) {
    res = var(T_SYMBOL, c);
  } else {
    res = val(T_SYMBOL,
              strcmp(p->value.str,
                     q->value.str) == 0);
  }
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  add_conditions(res, p, q);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

static char tmp_string_buf[1024];

seg_t int_to_string(uintptr_t x) {
  int n = snprintf(tmp_string_buf, sizeof(tmp_string_buf), "%lld", (long long)x);
  assert_error(n < (int)sizeof(tmp_string_buf));
  return (seg_t) { .s = tmp_string_buf, .n = n };
}

WORD("->str", to_string, 1, 1)
OP(to_string) {
  cell_t *res = 0;
  PRE(to_string);

  CHECK_IF(!check_type(ctx->t, T_STRING), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(any)));
  CHECK_DELAY();
  clear_flags(c);

  cell_t *p = c->expr.arg[0];
  if(is_var(p)) {
    res = var(T_STRING, c);
  } else if(p->value.type == T_INT) {
    res = make_string(int_to_string(p->value.integer));
  } else if(p->value.type == T_SYMBOL) {
    res = make_string(string_seg(symbol_string(p->value.integer)));
  } else {
    ABORT(FAIL);
  }
  res->value.type = T_STRING;
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  add_conditions(res, p);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

WORD("<-str", from_string, 1, 1)
OP(from_string) {
  cell_t *res = 0;
  PRE(from_string);

  CHECK_IF(!check_type(ctx->t, T_INT), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(string)));
  CHECK_DELAY();
  clear_flags(c);

  cell_t *p = c->expr.arg[0];
  if(is_var(p)) {
    res = var(T_INT, c);
  } else {
    char *end = NULL;
    val_t x = strtoll(p->value.str, &end, 0);
    CHECK_IF(!end || *end != '\0', FAIL);
    res = val(T_INT, x);
  }
  res->value.type = T_INT;
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  add_conditions(res, p);
  FLAG_SET(c->expr, EXPR_PARTIAL);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

WORD("strsplit", strsplit, 2, 2)
OP(strsplit) {
  cell_t *res = 0;
  PRE(strsplit);

  CHECK_IF(!check_type(ctx->t, T_STRING), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(string)));
  CHECK(reduce_arg(c, 1, &CTX(string)));
  CHECK_DELAY();
  clear_flags(c);

  cell_t *p = c->expr.arg[0];
  cell_t *q = c->expr.arg[1];
  if(is_var(p)) {
    res = var(T_STRING, c);
    store_dep_var(c, res, 2, T_STRING, ctx->alt_set);
  } else {
    char *s = strstr(p->value.str, q->value.str);
    CHECK_IF(!s, FAIL);
    int needle_len = strlen(q->value.str);
    res = make_string((seg_t) {.s = p->value.str, .n = s - p->value.str});
    store_lazy_dep(c->expr.arg[2],
                   make_string((seg_t) {.s = s + needle_len,
                                        .n = strlen(s + needle_len)}),
                   0);
  }
  res->value.type = T_STRING;
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  add_conditions(res, p, q);
  FLAG_SET(c->expr, EXPR_PARTIAL);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}

WORD("strtrim", strtrim, 1, 1)
OP(strtrim) {
  cell_t *res = 0;
  PRE(strtrim);

  CHECK_IF(!check_type(ctx->t, T_STRING), FAIL);

  CHECK(reduce_arg(c, 0, &CTX(string)));
  CHECK_DELAY();
  clear_flags(c);

  cell_t *p = c->expr.arg[0];
  if(is_var(p)) {
    res = var(T_STRING, c);
  } else {
    res = make_string(seg_trim(string_seg(p->value.str)));
  }
  res->value.type = T_STRING;
  res->alt = c->alt;
  res->value.alt_set = ctx->alt_set;
  add_conditions(res, p);
  store_reduced(cp, res);
  return SUCCESS;

 abort:
  return abort_op(rsp, cp, ctx);
}
