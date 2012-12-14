#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

typedef struct cell cell_t;
typedef void (*cell_func_t)(cell_t *, void *);
struct cell {
  union {
    cell_func_t func;
    cell_t *prev;
  };
  union {
    cell_t *arg[1];
    cell_t *next;
  };
};

// make sure &cells > 255
cell_t cells[1024];
cell_t *cells_ptr;

cell_t *nil = &cells[0];

// #define CHECK_CYCLE

#define show(x)\
printf(#x " = %d\n", x)

#define LENGTH(_a) (sizeof(_a) / sizeof((_a)[0]))

bool is_cell(void *p);
bool is_closure(void *p);
bool closure_is_ready(cell_t *c);
void closure_set_ready(cell_t *c, bool r);
void reduce(cell_t *c, void *r);
cell_t *cells_next();
bool check_cycle();
void cells_init();
void cell_alloc(cell_t *c);
cell_t *closure_alloc(int size);
int calculate_cells(int args);
int closure_cells(cell_t *c);
void closure_free(cell_t *c);
void func_nil(cell_t *c, void *r);
void func_val(cell_t *c, void *r);
void func_ref_reduced(cell_t *c, void *r);
void func_ref(cell_t *c, void *r);
cell_t *val(intptr_t x);
bool is_val(cell_t *c);
bool is_ref(cell_t *c);
bool is_offset(cell_t *c);
cell_t *func(cell_func_t f, int args);
void func_add(cell_t *c, void *r);
int closure_args(cell_t *c);
int closure_next_child(cell_t *c);
void arg(cell_t *c, cell_t *a);
cell_t *copy(cell_t *c);
cell_t *ref(cell_t *c);
cell_t *dup(cell_t *c);
void func_cons(cell_t *c, void *r);
bool is_nil(cell_t *c);
bool is_cons(cell_t *c);
cell_t *cons(cell_t *h, cell_t *t);
void drop(cell_t *c);
void func_head(cell_t *c, void *r);
void func_tail(cell_t *c, void *r);
void func_reduce_head(cell_t *c, void *r);
cell_t *pushl(cell_t *a, cell_t *b);
cell_t *compose(cell_t *a, cell_t *b);
void print_sexpr(cell_t *c);

bool is_cell(void *p) {
  return p >= (void *)&cells && p < (void *)(&cells+1);
}

bool is_closure(void *p) {
  return is_cell(p) && !is_cell(((cell_t *)p)->func);
}

bool closure_is_ready(cell_t *c) {
  assert(is_closure(c));
  return !((intptr_t)c->func & 0x1);
}

void closure_set_ready(cell_t *c, bool r) {
  assert(is_closure(c));
  c->func = (cell_func_t)(((intptr_t)c->func & ~0x1) | !r);
}

void reduce(cell_t *c, void *r) {
  assert(closure_is_ready(c));
  print_sexpr(c);
  c->func(c, r);
}

cell_t *cells_next() {
  cell_t *p = cells_ptr;
  assert(is_cell(p) && !is_closure(p) && is_cell(cells_ptr->next));
  cells_ptr = cells_ptr->next;
  return p;
}

#ifdef CHECK_CYCLE
bool check_cycle() {
  int i = 0;
  cell_t *start = cells_ptr, *ptr = start;
  while(ptr->next != start) {
    if(i > LENGTH(cells)) return false;
    i++;
    assert(is_cell(ptr->next->next));
    ptr = ptr->next;
  }
  return true;
}
#else
bool check_cycle() {
  return true;
}
#endif

void cells_init() {
  int i;

  // cells[0] is the nil cell
  cells[0].func = func_nil;
  cells[0].arg[0] = 0;

  // set up doubly-linked pointer ring
  for(i = 1; i < LENGTH(cells); i++) {
    cells[i].prev = &cells[i-1];
    cells[i].next = &cells[i+1];
  }
  cells[1].prev = &cells[LENGTH(cells)-1];
  cells[LENGTH(cells)-1].next = &cells[0];

  cells_ptr = &cells[1];
  assert(check_cycle());
}

void cell_alloc(cell_t *c) {
  assert(is_cell(c) && !is_closure(c));
  cell_t *prev = c->prev;
  assert(is_cell(prev) && !is_closure(prev));
  cell_t *next = c->next;
  assert(is_cell(next) && !is_closure(next));
  if(cells_ptr == c) cells_next();
  prev->next = next;
  next->prev = prev;
  assert(check_cycle());
}

cell_t *closure_alloc(int size) {
  cell_t *ptr = cells_next(), *c = ptr;
  cell_t *mark = ptr;
  int cnt = 0;

  // search for contiguous chunk
  while(cnt < size) {
    if(is_cell(ptr) && !is_closure(ptr)) {
      cnt++;
      ptr++;
    } else {
      cnt = 0;
      c = ptr = cells_next();
      assert(c != mark);
    }
  }
  /*
  while(cells_ptr >= &c[0] && cells_ptr < &c[size])
    cells_next();
  */
  // remove the found chunk
  int i;
  for(i = 0; i < size; i++) {
    cell_alloc(&c[i]);
  }

  bzero(c, sizeof(cell_t)*size);
  assert(check_cycle());
  return c;
}

int calculate_cells(int args) {
  return 1 + ((sizeof(cell_t *) * (args - 1) + sizeof(cell_t) - 1) / sizeof(cell_t));
}

int closure_cells(cell_t *c) {
  return calculate_cells(closure_args(c));
}

void closure_free(cell_t *c) {
  int i, size = closure_cells(c);
  assert(is_closure(c));
  for(i = 0; i < size; i++) {
    c[i].prev = &c[i-1];
    c[i].next = &c[i+1];
  }
  c[0].prev = cells_ptr->prev;
  cells_ptr->prev->next = &c[0];
  c[size-1].next = cells_ptr;
  cells_ptr->prev = &c[size-1];
  assert(check_cycle());
}

void func_nil(cell_t *c, void *r) {
  assert(false);
}

void func_val(cell_t *c, void *r) {
  assert(is_closure(c));
  intptr_t val = (intptr_t)c->arg[0];
  closure_free(c);
  *(intptr_t *)r = val;
}

void func_ref_reduced(cell_t *c, void *r) {
  assert(is_closure(c));
  intptr_t val = (intptr_t)c->arg[1];
  if(!--*(intptr_t *)&c->arg[0]) closure_free(c);
  *(intptr_t *)r = val;
}

void func_ref(cell_t *c, void *r) {
  intptr_t val;
  reduce(c->arg[1], &val);
  c->arg[1] = (cell_t *)val;
  c->func = func_ref_reduced;
  if(!--*(intptr_t *)&c->arg[0]) closure_free(c);
  *(intptr_t *)r = val;
}

cell_t *val(intptr_t x) {
  cell_t *c = closure_alloc(1);
  c->func = func_val;
  c->arg[0] = (cell_t *)x;
  return c;
}

bool is_val(cell_t *c) {
  return c->func == func_val;
}

bool is_ref(cell_t *c) {
  return c->func == func_ref || c->func == func_ref_reduced;
}

// max offset is 255
bool is_offset(cell_t *c) {
  return !((intptr_t)c & ~0xff);
}

// args must be >= 1
cell_t *func(cell_func_t f, int args) {
  assert(args >= 0);
  int size = calculate_cells(args);
  cell_t *c = closure_alloc(size);
  assert(c->func == 0);
  c->func = f;
  c->arg[0] = (cell_t *)(intptr_t)(args - 1);
  closure_set_ready(c, false);
  return c;
}

void func_add(cell_t *c, void *r) {
  intptr_t x;
  reduce(c->arg[0], &x);
  intptr_t y;
  reduce(c->arg[1], &y);
  closure_free(c);
  *(intptr_t *)r = x + y;
}

int closure_args(cell_t *c) {
  assert(is_closure(c));
  if(is_val(c)) return 1;
  if(is_ref(c)) return 2;
  cell_t **p = c->arg;
  int n = 0;
  if(is_offset(*p)) {
    intptr_t o = (intptr_t)(*p) + 1;
    p += o;
    n += o;
  }
  while(is_closure(*p)) {
    p++;
    n++;
  }
  return n;
}

int closure_next_child(cell_t *c) {
  assert(is_closure(c));
  return is_offset(c->arg[0]) ? (intptr_t)c->arg[0] : 0;
}

void arg(cell_t *c, cell_t *a) {
  assert(is_closure(c) && is_closure(a));
  assert(!closure_is_ready(c));
  int i = closure_next_child(c);
  if(!is_cell(c->arg[i])) {
    c->arg[0] = (cell_t *)(intptr_t)(i - (closure_is_ready(a) ? 1 : 0));
    c->arg[i] = a;
    if(i == 0) closure_set_ready(c, closure_is_ready(a));
  } else {
    arg(c->arg[i], a);
    if(closure_is_ready(c->arg[i])) {
      if(i == 0) closure_set_ready(c, true);
      else --*(intptr_t *)c->arg; // decrement offset
    }
  }
}

cell_t *copy(cell_t *c) {
  int size = closure_cells(c);
  cell_t *new = closure_alloc(size);
  memcpy(new, c, size * sizeof(cell_t));
  return new;
}

cell_t *ref(cell_t *c) {
  assert(is_closure(c));
  if(is_ref(c)) {
    ++*(intptr_t *)&c->arg[0];
    return c;
  } else {
    cell_t *new = copy(c);
    cell_t *ref_cell = closure_alloc(2);
    ref_cell->func = func_ref;
    ref_cell->arg[0] = (cell_t *)2;
    ref_cell->arg[1] = new;
    return ref_cell;
  }
}

cell_t *dup(cell_t *c) {
  assert(is_closure(c));
  if(closure_is_ready(c)) return ref(c);
  int args = closure_args(c);
  cell_t *tmp = copy(c);
  // c->arg[<i] are empty and c->arg[>i] are ready, so no need to copy
  // c->arg[i] only needs copied if filled
  int i = closure_next_child(c);
  if(is_closure(c->arg[i])) tmp->arg[i] = dup(c->arg[i]);
  while(++i < args) tmp->arg[i] = ref(c->arg[i]);
  return tmp;
}

void func_cons(cell_t *c, void *r) {
  *(cell_t **)r = c;
}

bool is_nil(cell_t *c) {
  return c == nil;
}

bool is_cons(cell_t *c) {
  return c->func == func_cons;
}

cell_t *cons(cell_t *h, cell_t *t) {
  assert(is_cons(t) || is_nil(t));
  cell_t *c = closure_alloc(2);
  c->func = func_cons;
  c->arg[0] = h;
  c->arg[1] = t;
  return c;
}

void drop(cell_t *c) {
  if(!is_closure(c)) return;
  if(is_ref(c)) {
    if(--*(intptr_t *)&c->arg[0]) {
      if(c->func == func_ref)
	drop(c->arg[1]);
      closure_free(c);
    }
  } else if(is_val(c)) {
    closure_free(c);
  } else {
    int i = closure_args(c) - 1;
    while(i--) drop(c->arg[i]);
    closure_free(c);
  }
}

void func_head(cell_t *c, void *r) {
  cell_t *x = c->arg[0];
  assert(is_cons(x));
  drop(x->arg[1]);
  *(cell_t **)r = x->arg[0];
}

void func_tail(cell_t *c, void *r) {
  cell_t *x = c->arg[0];
  assert(is_cons(x));
  drop(x->arg[0]);
  *(cell_t **)r = x->arg[1];
}

cell_t *pushl(cell_t *a, cell_t *b) {
  assert(is_closure(a) && is_closure(b));
  assert(is_cons(b) || is_nil(b));
  if(is_nil(b)) {
    return cons(a, b);
  }
  cell_t *h = b->arg[0];
  if(closure_is_ready(h)) {
    return cons(a, b);
  } else {
    arg(h, a);
    return b;
  }
}

void func_pushl(cell_t *c, void *r) {
  *(cell_t **)r = pushl(c->arg[0], c->arg[1]);
}

void func_quot(cell_t *c, void *r) {
  *(cell_t **)r = cons(c->arg[0], nil);
}

void func_reduce_head(cell_t *c, void *r) {
  cell_t *x;
  reduce(c->arg[0], &x);
  assert(is_cons(x));
  drop(x->arg[1]);
  reduce(x->arg[0], r);
}

cell_t *compose(cell_t *a, cell_t *b) {
  assert(is_closure(a) && is_closure(b));
  assert((is_cons(a) || is_nil(a)) &&
	 (is_cons(b) || is_nil(b)));
  if(is_nil(a)) return b;
  if(is_nil(b)) return a;
  cell_t *h = a->arg[0];
  cell_t *bp = compose(a->arg[1], b);
  return pushl(h, bp);
}

void func_compose(cell_t *c, void *r) {
  *(cell_t **)r = compose(c->arg[0], c->arg[1]);
}

void alloc_test() {
  int i, j;
  cell_t *a[30];
  for(j = 0; j < 50; j++) {
    for(i = 0; i < LENGTH(a); i++) {
      a[i] = func(func_add, 9);
    }
    for(i = 0; i < LENGTH(a); i++) {
      closure_free(a[i]);
    }
  }
}

void test() {
  cell_t *a, *b, *c, *d, *e, *f, *g, *h, *i, *j;
  g = func(func_add, 2);
  e = func(func_add, 2);
  c = func(func_add, 2);

  h = val(10);
  b = val(2);

  a = val(1);

  arg(e, c);
  arg(e, a);
  arg(e, b);
  show(closure_args(e));
  f = dup(e);
  d = val(4);
  arg(e, d);
  arg(f, dup(a));
  arg(g, h);
  arg(g, f);
  i = func(func_quot, 1);
  arg(i, g);
  j = func(func_reduce_head, 1);
  arg(j, i);

  intptr_t ret;
  reduce(j, &ret);
  show((int)ret);
}

void print_sexpr_help(cell_t *);

void print_list_help(cell_t *c) {
  if(c == nil) {
    printf(" ] ");
    return;
  }
  assert(is_cons(c));
  print_sexpr_help(c->arg[0]);
  print_list_help(c->arg[1]); 
}

void print_list(cell_t *c) {
  printf(" [");
  print_list_help(c);
}

void print_sexpr_help(cell_t *r) {
  if(!is_closure(r)) {
    printf(" ?");
    return;
  } else if(is_cons(r) || is_nil(r)) {
    print_list(r);
    return;
  } else if(is_ref(r)) {
    //print_sexpr_help(r->arg[1]);
    printf(" ref");
    return;
  }

  intptr_t f = ((intptr_t)r->func) & ~1;
  char *n;
  int i;
  int args = closure_args(r);

  if(is_val(r)) {
    printf(" %d", (int)(intptr_t)r->arg[0]);
    return;
  }

# define CASE(x) if(f == (intptr_t)func_##x) n = #x

  CASE(add);
  CASE(pushl);
  CASE(reduce_head);


# undef CASE

  printf(" (%s", n);

  for(i = 0; i < args; i++) {
    print_sexpr_help(r->arg[i]);
  }

  printf(")");
}

void print_sexpr(cell_t *r) {
  print_sexpr_help(r);
  printf("\n");
}

void test2() {
  cell_t *a, *b, *c, *d;
  // 1 [2 +] pushl popr
  a = func(func_add, 2);
  arg(a, val(2));
  b = cons(a, nil);
  c = func(func_pushl, 2);
  arg(c, b);
  arg(c, val(1));
  d = func(func_reduce_head, 1);
  arg(d, c);
  //print_sexpr(d);

  intptr_t ret;
  reduce(d, &ret);
  show((int)ret);
}

int main() {
  cells_init();
  //alloc_test();
  test2();
  return 0;
}
