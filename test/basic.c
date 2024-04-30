#include <assert.h>
#include <stdbool.h>

#include "../include/bitarray.h"

int
main () {
  int e;

  bitarray_t b;
  e = bitarray_init(&b);
  assert(e == 0);

  bitarray_set(&b, 126700, true);

  bitarray_destroy(&b);
}
