#ifndef BITARRAY_H
#define BITARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <intrusive.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BITARRAY_BITS_PER_PAGE  32768
#define BITARRAY_BYTES_PER_PAGE BITARRAY_BITS_PER_PAGE / 8

#define BITARRAY_BITS_PER_SEGMENT          2097152
#define BITARRAY_BYTES_PER_SEGMENT         BITARRAY_BITS_PER_SEGMENT / 8
#define BITARRAY_INITIAL_BYTES_PER_SEGMENT 4096
#define BITARRAY_PAGES_PER_SEGMENT         BITARRAY_BITS_PER_SEGMENT / BITARRAY_BITS_PER_PAGE

#define BITARRAY_SEGMENT_GROWTH_FACTOR 4

typedef struct bitarray_s bitarray_t;
typedef struct bitarray_node_s bitarray_node_t;
typedef struct bitarray_page_s bitarray_page_t;
typedef struct bitarray_segment_s bitarray_segment_t;

struct bitarray_s {
  intrusive_set_t segments;
  intrusive_set_node_t *segment_buckets[16];

  intrusive_set_t pages;
  intrusive_set_node_t *page_buckets[128];
};

struct bitarray_node_s {
  size_t index;

  intrusive_set_node_t set;
};

struct bitarray_page_s {
  bitarray_node_t node;

  bitarray_segment_t *segment;
};

struct bitarray_segment_s {
  bitarray_node_t node;
};

int
bitarray_init (bitarray_t *bitarray);

void
bitarray_destroy (bitarray_t *bitarray);

bool
bitarray_get (bitarray_t *bitarray, int64_t bit);

void
bitarray_set (bitarray_t *bitarray, int64_t bit, bool value);

void
bitarray_fill (bitarray_t *bitarray, bool value, int64_t start, int64_t end);

#ifdef __cplusplus
}
#endif

#endif // BITARRAY_H
