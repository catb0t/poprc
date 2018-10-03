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

#include "rt_types.h"
#include <stdio.h>

#include "startle/log.h"
#include "startle/map.h"
#include "startle/error.h"
#include "print.h"
#include "cells.h"
#include "rt.h"
#include "list.h"
#include "trace.h"
#include "special.h"
#include "log_tree.h"

static MAP(map, 255);

static
void fprint_arg(FILE *f, map_t map, cell_t *c) {
  if(c) {
    pair_t *x = map_find(map, (uintptr_t)clear_ptr(c));
    if(x) {
      fprintf(f, " %d", (int)x->second);
    } else {
      fprintf(f, " ?");
    }
  } else {
    fprintf(f, " X");
  }
}

static
void fprint_tree(cell_t *c, map_t map, FILE *f) {
  c = clear_ptr(c);
  if(!is_closure(c) || map_find(map, (uintptr_t)c)) return;

  // print subtree
  TRAVERSE(c, in, ptrs) {
    fprint_tree(*p, map, f);
  }

  int i = *map_cnt(map);
  map_insert(map, (pair_t) {(uintptr_t)c, i});

  if(is_dep(c)) return;

  fprintf(f, "%d", i);
  if(!is_value(c)) {
    if(closure_out(c)) {
      TRAVERSE(c, out) {
        if(*p) {
          int d = *map_cnt(map);
          map_insert(map, (pair_t) {(uintptr_t)clear_ptr(*p), d});
          fprintf(f, " %d", d);
        }
      }
    }

    const char *module_name, *word_name;
    get_name(c, &module_name, &word_name);
    if(c->op == OP_exec) {
      fprintf(f, " <- %s.%s", module_name, word_name);
    } else {
      fprintf(f, " <- %s", word_name);
    }
    TRAVERSE(c, in) {
      fprint_arg(f, map, *p);
    }
  } else {
    fprintf(f, " <- %s:", show_type_all(c));
    if(is_var(c)) {
      cell_t *var = c->value.var;
      cell_t *entry = var_entry(var);
      int index = var_index(var);
      fprintf(f, " %s.%s[%d]",
              entry->module_name,
              entry->word_name,
              index);
    } else if(is_list(c)) {
      TRAVERSE(c, ptrs) {
        fprint_arg(f, map, *p);
      }
    } else {
      fprintf(f, " %d", (int)c->value.integer);
    }
  }
  fprintf(f, "\n");
}

void print_tree(cell_t *c) {
  if(c) {
    map_clear(map);
    fprint_tree(c, map, stdout);
  }
}

void log_trees() {
  static FILE *f = NULL;
  if(!f) f = fopen("trees.log", "w");
  tag_t tag;
  get_tag(tag);
  fprintf(f, "# TAG: " FORMAT_TAG "\n", tag);
  map_clear(map);
  COUNTUP(i, rt_roots_n) {
    cell_t **p = rt_roots[i];
    if(p && *p) {
      fprint_tree(*p, map, f);
    }
  }
  fprintf(f, "\n");
  fflush(f);
}
