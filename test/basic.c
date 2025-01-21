#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "../include/bitarray.h"

int
main() {
  int e;

  bitarray_t b;
  e = bitarray_init(&b, NULL, NULL);
  assert(e == 0);

  bool c;

  c = bitarray_set(&b, 126700, true);
  assert(c);

  c = bitarray_set(&b, 12670000, true);
  assert(c);

  bool v = bitarray_get(&b, 126700);
  assert(v == true);

  int64_t p;

  p = bitarray_find_first(&b, true, 0);
  assert(p == 126700);

  p = bitarray_find_first(&b, true, 126700);
  assert(p == 126700);

  p = bitarray_find_first(&b, true, 126701);
  assert(p == 12670000);

  p = bitarray_find_first(&b, true, 12670000);
  assert(p == 12670000);

  p = bitarray_find_first(&b, true, 12670001);
  assert(p == -1);

  p = bitarray_count(&b, true, 0, 12670001);
  assert(p == 2);

  bitarray_destroy(&b);
}
