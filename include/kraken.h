#pragma once

#include "stdafx.h"
#include "decompress.h"

// Kraken decompression happens in two phases, first one decodes
// all the literals and copy lengths using huffman and second
// phase runs the copy loop. This holds the tables needed by stage 2.
typedef struct KrakenLzTable
{
    // Stream of (literal, match) pairs. The flag u_int8_t contains
    // the length of the match, the length of the literal and whether
    // to use a recent offset.
    u_int8_t *cmd_stream;
    int32_t cmd_stream_size;

    // Holds the actual distances in case we're not using a recent
    // offset.
    int32_t *offs_stream;
    int32_t offs_stream_size;

    // Holds the sequence of literals. All literal copying happens from
    // here.
    u_int8_t *lit_stream;
    int32_t lit_stream_size;

    // Holds the lengths that do not fit in the flag stream. Both literal
    // lengths and match length are stored in the same array.
    int32_t *len_stream;
    int32_t len_stream_size;
} KrakenLzTable;

bool Kraken_UnpackOffsets(
    const u_int8_t *src,
    const u_int8_t *src_end,
    const u_int8_t *packed_offs_stream,
    const u_int8_t *packed_offs_stream_extra,
    int32_t packed_offs_stream_size,
    int32_t multi_dist_scale,
    const u_int8_t *packed_litlen_stream,
    int32_t packed_litlen_stream_size,
    int32_t *offs_stream,
    int32_t *len_stream,
    bool excess_flag,
    int32_t excess_bytes);
bool Kraken_ReadLzTable(
    int32_t mode,
    const u_int8_t *src,
    const u_int8_t *src_end,
    u_int8_t *dst,
    int32_t dst_size,
    int32_t offset,
    u_int8_t *scratch,
    u_int8_t *scratch_end,
    KrakenLzTable *lztable);
bool Kraken_ProcessLzRuns_Type0(
    KrakenLzTable *lzt,
    u_int8_t *dst,
    u_int8_t *dst_end,
    u_int8_t *dst_start);
bool Kraken_ProcessLzRuns_Type1(
    KrakenLzTable *lzt,
    u_int8_t *dst,
    u_int8_t *dst_end,
    u_int8_t *dst_start);
bool Kraken_ProcessLzRuns(
    int32_t mode,
    u_int8_t *dst,
    int32_t dst_size,
    int32_t offset,
    KrakenLzTable *lztable);
int32_t Kraken_DecodeQuantum(
    u_int8_t *dst,
    u_int8_t *dst_end,
    u_int8_t *dst_start,
    const u_int8_t *src,
    const u_int8_t *src_end,
    u_int8_t *scratch,
    u_int8_t *scratch_end);
