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
#include "startle/support.h"
#include "startle/map.h"

// Provides a way to assign string tags to pointers, which is useful
// to understand what things are when debugging.

static MAP(tags, 4096);

void set_ptr_tag(const void *ptr, const char *str) {
  pair_t p = { (uintptr_t)ptr, (uintptr_t)str };
  map_insert(tags, p);
}

// TODO: a way to remove tags

const char *get_ptr_tag(const void *ptr) {
  pair_t *p = map_find(tags, (uintptr_t)ptr);
  if(p) {
    return (const char *)p->second;
  } else {
    return NULL;
  }
}

void clear_ptr_tags() {
  map_clear(tags);
}
