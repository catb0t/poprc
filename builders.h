#ifndef __BUILDERS__
#define __BUILDERS__

#define WORD__ITEM(__name, __func, __in, __out) \
  BUILDER##__in##__out(__func, __func)
#define WORD_ALIAS__ITEM(__name, __func, __in, __out, __builder)  \
  BUILDER##__in##__out(__builder, __func)

#define BUILDER01(__name, __op)                 \
  static inline cell_t *build_##__name() {      \
    return build01(OP_##__op);                  \
  }
#define BUILDER11(__name, __op)                         \
  static inline cell_t *build_##__name(cell_t *i0) {    \
    return build11(OP_##__op, i0);                      \
  }
#define BUILDER21(__name, __op)                                         \
  static inline cell_t *build_##__name(cell_t *i0, cell_t *i1) {        \
    return build21(OP_##__op, i0, i1);                                  \
  }
#define BUILDER12(__name, __op)                                         \
  static inline cell_t *build_##__name(cell_t *i0, cell_t **o1) {       \
    return build12(OP_##__op, i0, o1);                                  \
  }
#define BUILDER22(__name, __op)                                         \
  static inline cell_t *build_##__name(cell_t *i0, cell_t *i1, cell_t **o1) { \
    return build22(OP_##__op, i0, i1, o1);                              \
  }

#include "word_list.h"

#undef WORD__ITEM

#endif
