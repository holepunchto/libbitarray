#include <intrusive.h>
#include <quickbit.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/bitarray.h"
#include "intrusive/set.h"

static inline size_t
bitarray__segment_offset (bitarray_segment_t *segment) {
  return segment->node.index * BITARRAY_BYTES_PER_SEGMENT;
}

static inline size_t
bitarray__page_offset (bitarray_page_t *page) {
  return page->node.index * BITARRAY_BYTES_PER_PAGE - bitarray__segment_offset(page->segment);
}

static inline bitarray_node_t *
bitarray__node (const intrusive_set_node_t *node) {
  return node == NULL ? NULL : intrusive_entry(node, bitarray_node_t, set);
}

static size_t
bitarray__on_hash (const void *key, void *data) {
  return (size_t) key;
}

static bool
bitarray__on_equal (const void *key, const intrusive_set_node_t *node, void *data) {
  return (size_t) key == bitarray__node(node)->index;
}

int
bitarray_init (bitarray_t *bitarray) {
  intrusive_set_init(&bitarray->segments, bitarray->segment_buckets, 16, (void *) bitarray, bitarray__on_hash, bitarray__on_equal);

  intrusive_set_init(&bitarray->pages, bitarray->page_buckets, 128, (void *) bitarray, bitarray__on_hash, bitarray__on_equal);

  return 0;
}

void
bitarray_destroy (bitarray_t *bitarray) {
  intrusive_set_for_each(cursor, i, &bitarray->segments) {
    free(bitarray__node(cursor));
  }

  intrusive_set_for_each(cursor, i, &bitarray->pages) {
    free(bitarray__node(cursor));
  }
}

static inline void
bitarray__bit_location (int64_t bit, size_t *offset, size_t *page, size_t *segment) {
  *offset = bit & (BITARRAY_BITS_PER_PAGE - 1);
  *page = (bit - *offset) / BITARRAY_BITS_PER_PAGE;
  if (segment) *segment = *page / BITARRAY_PAGES_PER_SEGMENT;
}

bool
bitarray_get (bitarray_t *bitarray, int64_t bit) {
  size_t i, j;
  bitarray__bit_location(bit, &i, &j, NULL);

  bitarray_page_t *page = (bitarray_page_t *) bitarray__node(intrusive_set_get(&bitarray->pages, (void *) j));

  if (page == NULL) return false;

  return quickbit_get(page->bitfield, BITARRAY_BITS_PER_PAGE, i);
}

bool
bitarray_set (bitarray_t *bitarray, int64_t bit, bool value) {
  size_t i, j, k;
  bitarray__bit_location(bit, &i, &j, &k);

  bitarray_page_t *page = (bitarray_page_t *) bitarray__node(intrusive_set_get(&bitarray->pages, (void *) j));

  if (page == NULL) {
    if (value == false) return false;

    bitarray_segment_t *segment = (bitarray_segment_t *) bitarray__node(intrusive_set_get(&bitarray->segments, (void *) k));

    if (segment == NULL) {
      segment = malloc(sizeof(bitarray_segment_t));
      segment->node.index = k;

      quickbit_index_init_sparse(segment->index, NULL, 0);

      intrusive_set_add(&bitarray->segments, (void *) k, &segment->node.set);
    }

    page = malloc(sizeof(bitarray_page_t));
    page->node.index = j;
    page->segment = segment;

    memset(page->bitfield, 0, BITARRAY_BYTES_PER_PAGE);

    intrusive_set_add(&bitarray->pages, (void *) j, &page->node.set);
  }

  if (quickbit_set(page->bitfield, BITARRAY_BITS_PER_PAGE, i, value)) {
    quickbit_chunk_t chunk = {
      .field = page->bitfield,
      .len = BITARRAY_BYTES_PER_PAGE,
      .offset = bitarray__page_offset(page)
    };

    quickbit_index_update_sparse(page->segment->index, &chunk, 1, bitarray__page_offset(page) * 8 + bit);

    return true;
  }

  return false;
}

void
bitarray_fill (bitarray_t *bitarray, bool value, int64_t start, int64_t end) {}
