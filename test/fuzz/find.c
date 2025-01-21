#include <assert.h>
#include <bitarray.h>
#include <quickbit.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  int err;

  bitarray_t b;
  err = bitarray_init(&b, NULL, NULL);
  assert(err == 0);

  err = bitarray_insert(&b, data, size, 0);
  assert(err == 0);

  int64_t len = size * 8, pos = 0;

  while (pos < len) {
    int64_t start = pos;

    pos = bitarray_find_first(&b, true, start);

    if (pos == len) pos = -1;

    assert(quickbit_find_first((const quickbit_t) data, size, true, start) == pos);

    if (pos == -1) break;

    pos += 1;
  }

  pos = 0;

  while (pos < len) {
    int64_t start = pos;

    pos = bitarray_find_first(&b, false, start);

    if (pos == len) pos = -1;

    assert(quickbit_find_first((const quickbit_t) data, size, false, start) == pos);

    if (pos == -1) break;

    pos += 1;
  }

  pos = len - 1;

  while (pos >= 0) {
    int64_t start = pos;

    pos = bitarray_find_last(&b, true, start);

    assert(quickbit_find_last((const quickbit_t) data, size, true, start) == pos);

    if (pos == -1) break;

    pos -= 1;
  }

  pos = len - 1;

  while (pos >= 0) {
    int64_t start = pos;

    pos = bitarray_find_last(&b, false, start);

    assert(quickbit_find_last((const quickbit_t) data, size, false, start) == pos);

    if (pos == -1) break;

    pos -= 1;
  }

  bitarray_destroy(&b);

  return 0;
}
