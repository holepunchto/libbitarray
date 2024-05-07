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

static inline int64_t
bitarray__max (int64_t a, int64_t b) {
  return a > b ? a : b;
}

static inline int64_t
bitarray__min (int64_t a, int64_t b) {
  return a < b ? a : b;
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
  bitarray->last_segment = (size_t) -1;
  bitarray->last_page = (size_t) -1;

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

bitarray_page_t *
bitarray_page (bitarray_t *bitarray, size_t i) {
  if (i >= bitarray->last_page) return NULL;

  return (bitarray_page_t *) bitarray__node(intrusive_set_get(&bitarray->pages, (void *) i));
}

static inline void
bitarray__bit_offset_in_segment (int64_t bit, size_t *offset, size_t *segment) {
  *offset = bit & (BITARRAY_BITS_PER_SEGMENT - 1);
  *segment = (bit - *offset) / BITARRAY_BITS_PER_SEGMENT;
}

static inline void
bitarray__bit_offset_in_page (int64_t bit, size_t *offset, size_t *page, size_t *segment) {
  *offset = bit & (BITARRAY_BITS_PER_PAGE - 1);
  *page = (bit - *offset) / BITARRAY_BITS_PER_PAGE;
  if (segment) *segment = *page / BITARRAY_PAGES_PER_SEGMENT;
}

static inline size_t
bitarray__segment_byte_offset (bitarray_segment_t *segment) {
  return segment->node.index * BITARRAY_BYTES_PER_SEGMENT;
}

static inline size_t
bitarray__page_byte_offset (bitarray_page_t *page) {
  return page->node.index * BITARRAY_BYTES_PER_PAGE - bitarray__segment_byte_offset(page->segment);
}

static inline size_t
bitarray__page_bit_offset (bitarray_page_t *page) {
  return bitarray__page_byte_offset(page) * 8;
}

static inline bitarray_segment_t *
bitarray__create_segment (bitarray_t *bitarray, size_t index) {
  bitarray_segment_t *segment = malloc(sizeof(bitarray_segment_t));

  segment->node.index = index;

  quickbit_index_init_sparse(segment->index, NULL, 0);

  memset(segment->pages, 0, sizeof(segment->pages));

  intrusive_set_add(&bitarray->segments, (void *) index, &segment->node.set);

  if (bitarray->last_segment == (size_t) -1 || index > bitarray->last_segment) {
    bitarray->last_segment = index;
  }

  return segment;
}

static inline bitarray_page_t *
bitarray__create_page (bitarray_t *bitarray, bitarray_segment_t *segment, size_t index) {
  bitarray_page_t *page = malloc(sizeof(bitarray_page_t));

  page->node.index = index;

  page->segment = segment;

  memset(page->bitfield, 0, sizeof(page->bitfield));

  intrusive_set_add(&bitarray->pages, (void *) index, &page->node.set);

  if (bitarray->last_page == (size_t) -1 || index > bitarray->last_page) {
    bitarray->last_page = index;
  }

  segment->pages[index - segment->node.index * BITARRAY_PAGES_PER_SEGMENT] = page;

  return page;
}

static inline void
bitarray__reindex_segment (bitarray_t *bitarray, bitarray_segment_t *segment) {
  quickbit_chunk_t chunks[BITARRAY_PAGES_PER_SEGMENT];

  size_t len = 0;

  for (size_t i = 0; i < BITARRAY_PAGES_PER_SEGMENT; i++) {
    bitarray_page_t *page = segment->pages[i];

    if (page == NULL) continue;

    quickbit_chunk_t chunk = {
      .field = page->bitfield,
      .len = BITARRAY_BYTES_PER_PAGE,
      .offset = bitarray__page_byte_offset(page)
    };

    chunks[len++] = chunk;
  }

  quickbit_index_init_sparse(segment->index, chunks, len);
}

static inline void
bitarray_insert__in_page (bitarray_t *bitarray, bitarray_page_t *page, const uint8_t *bitfield, size_t len, int64_t start) {
  memcpy(&page->bitfield[start / 8], bitfield, len);
}

static inline void
bitarray_insert__in_segment (bitarray_t *bitarray, bitarray_segment_t *segment, const uint8_t *bitfield, size_t len, int64_t start) {
  int64_t remaining = len * 8;

  size_t i, j;
  bitarray__bit_offset_in_page(start, &i, &j, NULL);

  while (remaining > 0) {
    int64_t end = bitarray__min(i + remaining, BITARRAY_BITS_PER_PAGE);
    int64_t range = end - i;

    bitarray_page_t *page = segment->pages[j];

    if (page == NULL) page = bitarray__create_page(bitarray, segment, segment->node.index * BITARRAY_PAGES_PER_SEGMENT + j);

    bitarray_insert__in_page(bitarray, page, bitfield, range / 8, i);

    bitfield = &bitfield[range / 8];

    i = 0;
    j++;
    remaining -= range;
  }

  bitarray__reindex_segment(bitarray, segment);
}

int
bitarray_insert (bitarray_t *bitarray, const uint8_t *bitfield, size_t len, int64_t start) {
  if (start % 8 != 0) return -1;

  int64_t remaining = len * 8;

  size_t i, j;
  bitarray__bit_offset_in_segment(start, &i, &j);

  while (remaining > 0) {
    int64_t end = bitarray__min(i + remaining, BITARRAY_BITS_PER_SEGMENT);
    int64_t range = end - i;

    bitarray_segment_t *segment = (bitarray_segment_t *) bitarray__node(intrusive_set_get(&bitarray->segments, (void *) j));

    if (segment == NULL) segment = bitarray__create_segment(bitarray, j);

    bitarray_insert__in_segment(bitarray, segment, bitfield, range / 8, i);

    bitfield = &bitfield[range / 8];

    i = 0;
    j++;
    remaining -= range;
  }

  return 0;
}

static inline void
bitarray_clear__in_page (bitarray_t *bitarray, bitarray_page_t *page, const uint8_t *bitfield, size_t len, int64_t start) {
  quickbit_chunk_t chunk = {
    .field = (uint8_t *) bitfield,
    .len = len,
    .offset = start / 8,
  };

  quickbit_clear(page->bitfield, BITARRAY_BYTES_PER_PAGE, &chunk);
}

static inline void
bitarray_clear__in_segment (bitarray_t *bitarray, bitarray_segment_t *segment, const uint8_t *bitfield, size_t len, int64_t start) {
  int64_t remaining = len * 8;

  size_t i, j;
  bitarray__bit_offset_in_page(start, &i, &j, NULL);

  while (remaining > 0) {
    int64_t end = bitarray__min(i + remaining, BITARRAY_BITS_PER_PAGE);
    int64_t range = end - i;

    bitarray_page_t *page = segment->pages[j];

    if (page == NULL) page = bitarray__create_page(bitarray, segment, segment->node.index * BITARRAY_PAGES_PER_SEGMENT + j);

    bitarray_clear__in_page(bitarray, page, bitfield, range / 8, i);

    bitfield = &bitfield[range / 8];

    i = 0;
    j++;
    remaining -= range;
  }

  bitarray__reindex_segment(bitarray, segment);
}

int
bitarray_clear (bitarray_t *bitarray, const uint8_t *bitfield, size_t len, int64_t start) {
  if (start % 8 != 0) return -1;

  int64_t remaining = len * 8;

  size_t i, j;
  bitarray__bit_offset_in_segment(start, &i, &j);

  while (remaining > 0) {
    int64_t end = bitarray__min(i + remaining, BITARRAY_BITS_PER_SEGMENT);
    int64_t range = end - i;

    bitarray_segment_t *segment = (bitarray_segment_t *) bitarray__node(intrusive_set_get(&bitarray->segments, (void *) j));

    if (segment == NULL) segment = bitarray__create_segment(bitarray, j);

    bitarray_clear__in_segment(bitarray, segment, bitfield, range / 8, i);

    bitfield = &bitfield[range / 8];

    i = 0;
    j++;
    remaining -= range;
  }

  return 0;
}

bool
bitarray_get (bitarray_t *bitarray, int64_t bit) {
  size_t i, j;
  bitarray__bit_offset_in_page(bit, &i, &j, NULL);

  bitarray_page_t *page = (bitarray_page_t *) bitarray__node(intrusive_set_get(&bitarray->pages, (void *) j));

  if (page == NULL) return false;

  return quickbit_get(page->bitfield, BITARRAY_BITS_PER_PAGE, i);
}

bool
bitarray_set (bitarray_t *bitarray, int64_t bit, bool value) {
  size_t i, j, k;
  bitarray__bit_offset_in_page(bit, &i, &j, &k);

  bitarray_page_t *page = (bitarray_page_t *) bitarray__node(intrusive_set_get(&bitarray->pages, (void *) j));

  if (page == NULL) {
    if (value == false) return false;

    bitarray_segment_t *segment = (bitarray_segment_t *) bitarray__node(intrusive_set_get(&bitarray->segments, (void *) k));

    if (segment == NULL) segment = bitarray__create_segment(bitarray, k);

    page = bitarray__create_page(bitarray, segment, j);
  }

  if (quickbit_set(page->bitfield, BITARRAY_BITS_PER_PAGE, i, value)) {
    quickbit_chunk_t chunk = {
      .field = page->bitfield,
      .len = BITARRAY_BYTES_PER_PAGE,
      .offset = bitarray__page_byte_offset(page)
    };

    quickbit_index_update_sparse(page->segment->index, &chunk, 1, bitarray__page_bit_offset(page) + i);

    return true;
  }

  return false;
}

static inline void
bitarray_fill__in_page (bitarray_t *bitarray, bitarray_page_t *page, bool value, int64_t start, int64_t end) {
  int64_t remaining = end - start;

  quickbit_fill(page->bitfield, BITARRAY_BYTES_PER_PAGE, value, start, end);

  quickbit_chunk_t chunk = {
    .field = page->bitfield,
    .len = BITARRAY_BYTES_PER_PAGE,
    .offset = bitarray__page_byte_offset(page)
  };

  int64_t i = start / 128;
  int64_t n = i + (1 + ((remaining - 1) / 128));

  while (i <= n) {
    quickbit_index_update_sparse(page->segment->index, &chunk, 1, bitarray__page_bit_offset(page) + i++ * 128);
  }
}

static inline void
bitarray_fill__in_segment (bitarray_t *bitarray, bitarray_segment_t *segment, bool value, int64_t start, int64_t end) {
  int64_t remaining = end - start;

  size_t i, j;
  bitarray__bit_offset_in_page(start, &i, &j, NULL);

  while (remaining > 0) {
    int64_t end = bitarray__min(i + remaining, BITARRAY_BITS_PER_PAGE);
    int64_t range = end - i;

    bitarray_page_t *page = segment->pages[j];

    if (page == NULL && value) page = bitarray__create_page(bitarray, segment, segment->node.index * BITARRAY_PAGES_PER_SEGMENT + j);

    if (page) bitarray_fill__in_page(bitarray, page, value, i, end);

    i = 0;
    j++;
    remaining -= range;
  }
}

void
bitarray_fill (bitarray_t *bitarray, bool value, int64_t start, int64_t end) {
  size_t len = bitarray->last_segment + 1;

  size_t n = len * BITARRAY_BITS_PER_SEGMENT;

  if (start < 0) start += n;
  if (end < 0) end += n;
  if (start < 0 || start >= end) return;

  int64_t remaining = end - start;

  size_t i, j;
  bitarray__bit_offset_in_segment(start, &i, &j);

  while (remaining > 0) {
    int64_t end = bitarray__min(i + remaining, BITARRAY_BITS_PER_SEGMENT);
    int64_t range = end - i;

    bitarray_segment_t *segment = (bitarray_segment_t *) bitarray__node(intrusive_set_get(&bitarray->segments, (void *) j));

    if (segment == NULL && value) segment = bitarray__create_segment(bitarray, j);

    if (segment) bitarray_fill__in_segment(bitarray, segment, value, i, end);

    i = 0;
    j++;
    remaining -= range;
  }
}

static inline int64_t
bitarray_find_first__in_page (bitarray_t *bitarray, bitarray_page_t *page, bool value, int64_t pos) {
  return quickbit_find_first(page->bitfield, BITARRAY_BYTES_PER_PAGE, value, pos);
}

static inline int64_t
bitarray_find_first__in_segment (bitarray_t *bitarray, bitarray_segment_t *segment, bool value, int64_t pos) {
  pos = quickbit_skip_first(segment->index, BITARRAY_BYTES_PER_SEGMENT, !value, pos);

  size_t i, j;
  bitarray__bit_offset_in_page(pos, &i, &j, NULL);

  while (j < BITARRAY_PAGES_PER_SEGMENT) {
    bitarray_page_t *page = segment->pages[j];

    int64_t offset = -1;

    if (page) offset = bitarray_find_first__in_page(bitarray, page, value, i);
    else if (!value) offset = i;

    if (offset != -1) return j * BITARRAY_BITS_PER_PAGE + offset;

    i = 0;
    j++;
  }

  return -1;
}

int64_t
bitarray_find_first (bitarray_t *bitarray, bool value, int64_t pos) {
  size_t len = bitarray->last_segment + 1;

  size_t n = len * BITARRAY_BITS_PER_SEGMENT;

  if (pos < 0) pos += n;
  if (pos < 0) pos = 0;
  if (pos >= n) return value ? -1 : n;

  size_t i, j;
  bitarray__bit_offset_in_segment(pos, &i, &j);

  while (j < len) {
    bitarray_segment_t *segment = (bitarray_segment_t *) bitarray__node(intrusive_set_get(&bitarray->segments, (void *) j));

    int64_t offset = -1;

    if (segment) offset = bitarray_find_first__in_segment(bitarray, segment, value, i);
    else if (!value) offset = i;

    if (offset != -1) return j * BITARRAY_BITS_PER_SEGMENT + offset;

    i = 0;
    j++;
  }

  return value ? -1 : bitarray__max(pos, n);
}

static inline int64_t
bitarray_find_last__in_page (bitarray_t *bitarray, bitarray_page_t *page, bool value, int64_t pos) {
  return quickbit_find_last(page->bitfield, BITARRAY_BYTES_PER_PAGE, value, pos);
}

static inline int64_t
bitarray_find_last__in_segment (bitarray_t *bitarray, bitarray_segment_t *segment, bool value, int64_t pos) {
  pos = quickbit_skip_last(segment->index, BITARRAY_BYTES_PER_SEGMENT, !value, pos);

  size_t i, j;
  bitarray__bit_offset_in_page(pos, &i, &j, NULL);

  if (j >= BITARRAY_PAGES_PER_SEGMENT) return -1;

  while (j >= 0) {
    bitarray_page_t *page = segment->pages[j];

    int64_t offset = -1;

    if (page) offset = bitarray_find_last__in_page(bitarray, page, value, i);
    else if (!value) offset = i;

    if (offset != -1) return j * BITARRAY_BITS_PER_PAGE + offset;

    i = BITARRAY_BITS_PER_PAGE - 1;
    j++;
  }

  return -1;
}

int64_t
bitarray_find_last (bitarray_t *bitarray, bool value, int64_t pos) {
  size_t len = bitarray->last_segment + 1;

  size_t n = len * BITARRAY_BITS_PER_SEGMENT;

  if (pos < 0) pos += n;
  if (pos < 0) return -1;
  if (pos >= n) pos = value ? n - 1 : pos;

  size_t i, j;
  bitarray__bit_offset_in_segment(pos, &i, &j);

  while (j >= 0) {
    bitarray_segment_t *segment = (bitarray_segment_t *) bitarray__node(intrusive_set_get(&bitarray->segments, (void *) j));

    int64_t offset = -1;

    if (segment) offset = bitarray_find_last__in_segment(bitarray, segment, value, i);
    else if (!value) offset = i;

    if (offset != -1) return j * BITARRAY_BITS_PER_SEGMENT + offset;

    i = BITARRAY_BITS_PER_SEGMENT - 1;
    j--;
  }

  return -1;
}

int64_t
bitarray_count__in_segment (bitarray_t *bitarray, bitarray_segment_t *segment, bool value, int64_t start, int64_t end) {
  int64_t remaining = end - start;
  int64_t c = 0;

  while (remaining > 0) {
    int64_t l = bitarray_find_first__in_segment(bitarray, segment, value, start);
    if (l == -1 || l >= end) return c;

    int64_t h = bitarray_find_first__in_segment(bitarray, segment, !value, l + 1);
    if (h == -1 || h >= end) return c + end - l;

    c += h - l;
    remaining -= h - start;
    start = h;
  }

  return c;
}

int64_t
bitarray_count (bitarray_t *bitarray, bool value, int64_t start, int64_t end) {
  size_t len = bitarray->last_segment + 1;

  size_t n = len * BITARRAY_BITS_PER_SEGMENT;

  if (start < 0) start += n;
  if (end < 0) end += n;
  if (start < 0 || start >= end) return 0;

  int64_t remaining = end - start;

  if (start >= n) return value ? 0 : remaining;

  size_t i, j;
  bitarray__bit_offset_in_segment(start, &i, &j);

  int64_t c = 0;

  while (remaining > 0) {
    int64_t end = bitarray__min(i + remaining, BITARRAY_BITS_PER_SEGMENT);
    int64_t range = end - i;

    bitarray_segment_t *segment = (bitarray_segment_t *) bitarray__node(intrusive_set_get(&bitarray->segments, (void *) j));

    if (segment) c += bitarray_count__in_segment(bitarray, segment, value, i, end);
    else if (!value) c += range;

    i = 0;
    j++;
    remaining -= range;
  }

  return c;
}
