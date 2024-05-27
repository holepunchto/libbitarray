#ifndef BITARRAY_H
#define BITARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <intrusive.h>
#include <quickbit.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BITARRAY_BITS_PER_PAGE  32768
#define BITARRAY_BYTES_PER_PAGE (BITARRAY_BITS_PER_PAGE / 8)

#define BITARRAY_BITS_PER_SEGMENT          2097152
#define BITARRAY_BYTES_PER_SEGMENT         (BITARRAY_BITS_PER_SEGMENT / 8)
#define BITARRAY_INITIAL_BYTES_PER_SEGMENT 4096
#define BITARRAY_PAGES_PER_SEGMENT         (BITARRAY_BITS_PER_SEGMENT / BITARRAY_BITS_PER_PAGE)

#define BITARRAY_SEGMENT_GROWTH_FACTOR 4

typedef struct bitarray_s bitarray_t;
typedef struct bitarray_node_s bitarray_node_t;
typedef struct bitarray_page_s bitarray_page_t;
typedef struct bitarray_segment_s bitarray_segment_t;

typedef void *(*bitarray_alloc_cb)(size_t size, bitarray_t *bitarray);
typedef void (*bitarray_free_cb)(void *ptr, bitarray_t *bitarray);

struct bitarray_s {
  uint32_t last_segment;
  uint32_t last_page;

  intrusive_set_t segments;
  intrusive_set_node_t *segment_buckets[16];

  intrusive_set_t pages;
  intrusive_set_node_t *page_buckets[128];

  bitarray_alloc_cb alloc;
  bitarray_free_cb free;

  void *data;
};

struct bitarray_node_s {
  uint32_t index;

  intrusive_set_node_t set;
};

struct bitarray_page_s {
  bitarray_node_t node;

  bitarray_segment_t *segment;

  uint8_t bitfield[BITARRAY_BYTES_PER_PAGE];
};

struct bitarray_segment_s {
  bitarray_node_t node;

  quickbit_index_t index;

  bitarray_page_t *pages[BITARRAY_PAGES_PER_SEGMENT];
};

int
bitarray_init (bitarray_t *bitarray, bitarray_alloc_cb alloc, bitarray_free_cb free);

void
bitarray_destroy (bitarray_t *bitarray);

bitarray_page_t *
bitarray_page (bitarray_t *bitarray, uint32_t i);

int
bitarray_insert (bitarray_t *bitarray, const uint8_t *bitfield, size_t len, int64_t start);

int
bitarray_clear (bitarray_t *bitarray, const uint8_t *bitfield, size_t len, int64_t start);

bool
bitarray_get (bitarray_t *bitarray, int64_t bit);

bool
bitarray_set (bitarray_t *bitarray, int64_t bit, bool value);

void
bitarray_fill (bitarray_t *bitarray, bool value, int64_t start, int64_t end);

int64_t
bitarray_find_first (bitarray_t *bitarray, bool value, int64_t pos);

int64_t
bitarray_find_last (bitarray_t *bitarray, bool value, int64_t pos);

int64_t
bitarray_count (bitarray_t *bitarray, bool value, int64_t start, int64_t end);

#ifdef __cplusplus
}
#endif

#endif // BITARRAY_H
