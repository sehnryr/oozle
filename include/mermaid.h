#pragma once

#include "stdafx.h"
#include "kraken.h"

// Mermaid/Selkie decompression also happens in two phases, just like in Kraken,
// but the match copier works differently.
// Both Mermaid and Selkie use the same on-disk format, only the compressor
// differs.
typedef struct MermaidLzTable
{
    // Flag stream. Format of flags:
    // Read flagbyte from |cmd_stream|
    // If flagbyte >= 24:
    //   flagbyte & 0x80 == 0 : Read from |off16_stream| into |recent_offs|.
    //                   != 0 : Don't read offset.
    //   flagbyte & 7 = Number of literals to copy first from |lit_stream|.
    //   (flagbyte >> 3) & 0xF = Number of bytes to copy from |recent_offs|.
    //
    //  If flagbyte == 0 :
    //    Read u_int8_t L from |length_stream|
    //    If L > 251: L += 4 * Read word from |length_stream|
    //    L += 64
    //    Copy L bytes from |lit_stream|.
    //
    //  If flagbyte == 1 :
    //    Read u_int8_t L from |length_stream|
    //    If L > 251: L += 4 * Read word from |length_stream|
    //    L += 91
    //    Copy L bytes from match pointed by next offset from |off16_stream|
    //
    //  If flagbyte == 2 :
    //    Read u_int8_t L from |length_stream|
    //    If L > 251: L += 4 * Read word from |length_stream|
    //    L += 29
    //    Copy L bytes from match pointed by next offset from |off32_stream|,
    //    relative to start of block.
    //    Then prefetch |off32_stream[3]|
    //
    //  If flagbyte > 2:
    //    L = flagbyte + 5
    //    Copy L bytes from match pointed by next offset from |off32_stream|,
    //    relative to start of block.
    //    Then prefetch |off32_stream[3]|
    const u_int8_t *cmd_stream, *cmd_stream_end;

    // Length stream
    const u_int8_t *length_stream;

    // Literal stream
    const u_int8_t *lit_stream, *lit_stream_end;

    // Near offsets
    const u_int16_t *off16_stream, *off16_stream_end;

    // Far offsets for current chunk
    u_int32_t *off32_stream, *off32_stream_end;

    // Holds the offsets for the two chunks
    u_int32_t *off32_stream_1, *off32_stream_2;
    u_int32_t off32_size_1, off32_size_2;

    // Flag offsets for next 64k chunk.
    u_int32_t cmd_stream_2_offs, cmd_stream_2_offs_end;
} MermaidLzTable;

int32_t Mermaid_DecodeFarOffsets(
    const u_int8_t *src,
    const u_int8_t *src_end,
    u_int32_t *output,
    size_t output_size,
    int64_t offset);
void Mermaid_CombineOffs16(
    u_int16_t *dst,
    size_t size,
    const u_int8_t *lo,
    const u_int8_t *hi);
bool Mermaid_ReadLzTable(
    int32_t mode,
    const u_int8_t *src,
    const u_int8_t *src_end,
    u_int8_t *dst,
    int32_t dst_size,
    int64_t offset,
    u_int8_t *scratch,
    u_int8_t *scratch_end, MermaidLzTable *lz);
const u_int8_t *Mermaid_Mode0(
    u_int8_t *dst,
    size_t dst_size,
    u_int8_t *dst_ptr_end,
    u_int8_t *dst_start,
    const u_int8_t *src_end,
    MermaidLzTable *lz,
    int32_t *saved_dist,
    size_t startoff);
const u_int8_t *Mermaid_Mode1(
    u_int8_t *dst,
    size_t dst_size,
    u_int8_t *dst_ptr_end,
    u_int8_t *dst_start,
    const u_int8_t *src_end,
    MermaidLzTable *lz,
    int32_t *saved_dist,
    size_t startoff);
bool Mermaid_ProcessLzRuns(
    int32_t mode,
    const u_int8_t *src,
    const u_int8_t *src_end,
    u_int8_t *dst,
    size_t dst_size,
    u_int64_t offset,
    u_int8_t *dst_end,
    MermaidLzTable *lz);
int32_t Mermaid_DecodeQuantum(
    u_int8_t *dst,
    u_int8_t *dst_end,
    u_int8_t *dst_start,
    const u_int8_t *src,
    const u_int8_t *src_end,
    u_int8_t *temp,
    u_int8_t *temp_end);
