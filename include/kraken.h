#pragma once

#include "decompress.h"
#include "stdafx.h"

// Kraken decompression happens in two phases, first one decodes
// all the literals and copy lengths using huffman and second
// phase runs the copy loop. This holds the tables needed by stage 2.
struct KrakenLzTable
{
  // Stream of (literal, match) pairs. The flag uint8_t contains
  // the length of the match, the length of the literal and whether
  // to use a recent offset.
  uint8_t *cmd_stream;
  int32_t cmd_stream_size;

  // Holds the actual distances in case we're not using a recent
  // offset.
  int32_t *offs_stream;
  int32_t offs_stream_size;

  // Holds the sequence of literals. All literal copying happens from
  // here.
  uint8_t *lit_stream;
  int32_t lit_stream_size;

  // Holds the lengths that do not fit in the flag stream. Both literal
  // lengths and match length are stored in the same array.
  int32_t *len_stream;
  int32_t len_stream_size;
};

bool Kraken_UnpackOffsets (const uint8_t *src, const uint8_t *src_end,
                           const uint8_t *packed_offs_stream,
                           const uint8_t *packed_offs_stream_extra,
                           int32_t packed_offs_stream_size,
                           int32_t multi_dist_scale,
                           const uint8_t *packed_litlen_stream,
                           int32_t packed_litlen_stream_size,
                           int32_t *offs_stream, int32_t *len_stream,
                           bool excess_flag, int32_t excess_bytes);
bool Kraken_ReadLzTable (int32_t mode, const uint8_t *src,
                         const uint8_t *src_end, uint8_t *dst,
                         int32_t dst_size, int32_t offset, uint8_t *scratch,
                         uint8_t *scratch_end, KrakenLzTable *lztable);
bool Kraken_ProcessLzRuns_Type0 (KrakenLzTable *lzt, uint8_t *dst,
                                 uint8_t *dst_end, uint8_t *dst_start);
bool Kraken_ProcessLzRuns_Type1 (KrakenLzTable *lzt, uint8_t *dst,
                                 uint8_t *dst_end, uint8_t *dst_start);
bool Kraken_ProcessLzRuns (int32_t mode, uint8_t *dst, int32_t dst_size,
                           int32_t offset, KrakenLzTable *lztable);
int32_t Kraken_DecodeQuantum (uint8_t *dst, uint8_t *dst_end,
                              uint8_t *dst_start, const uint8_t *src,
                              const uint8_t *src_end, uint8_t *scratch,
                              uint8_t *scratch_end);
