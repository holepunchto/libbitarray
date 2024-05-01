#include <assert.h>
#include <stdbool.h>

#include "../include/bitarray.h"

int
main () {
  int e;

  bitarray_t b;
  e = bitarray_init(&b);
  assert(e == 0);

  bool changed = bitarray_set(&b, 126700, true);
  assert(changed);

  bool value = bitarray_get(&b, 126700);
  assert(value == true);

  bitarray_destroy(&b);
}
