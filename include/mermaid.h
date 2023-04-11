#pragma once

#include "decompress.h"
#include "stdafx.h"

// Mermaid/Selkie decompression also happens in two phases, just like in
// Kraken, but the match copier works differently. Both Mermaid and Selkie use
// the same on-disk format, only the compressor differs.
struct MermaidLzTable
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
  //    Read uint8_t L from |length_stream|
  //    If L > 251: L += 4 * Read word from |length_stream|
  //    L += 64
  //    Copy L bytes from |lit_stream|.
  //
  //  If flagbyte == 1 :
  //    Read uint8_t L from |length_stream|
  //    If L > 251: L += 4 * Read word from |length_stream|
  //    L += 91
  //    Copy L bytes from match pointed by next offset from |off16_stream|
  //
  //  If flagbyte == 2 :
  //    Read uint8_t L from |length_stream|
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
  const uint8_t *cmd_stream, *cmd_stream_end;

  // Length stream
  const uint8_t *length_stream;

  // Literal stream
  const uint8_t *lit_stream, *lit_stream_end;

  // Near offsets
  const uint16_t *off16_stream, *off16_stream_end;

  // Far offsets for current chunk
  uint32_t *off32_stream, *off32_stream_end;

  // Holds the offsets for the two chunks
  uint32_t *off32_stream_1, *off32_stream_2;
  uint32_t off32_size_1, off32_size_2;

  // Flag offsets for next 64k chunk.
  uint32_t cmd_stream_2_offs, cmd_stream_2_offs_end;
};

int32_t Mermaid_DecodeFarOffsets (const uint8_t *src, const uint8_t *src_end,
                                  uint32_t *output, size_t output_size,
                                  int64_t offset);
void Mermaid_CombineOffs16 (uint16_t *dst, size_t size, const uint8_t *lo,
                            const uint8_t *hi);
bool Mermaid_ReadLzTable (int32_t mode, const uint8_t *src,
                          const uint8_t *src_end, uint8_t *dst,
                          int32_t dst_size, int64_t offset, uint8_t *scratch,
                          uint8_t *scratch_end, MermaidLzTable *lz);
const uint8_t *Mermaid_Mode0 (uint8_t *dst, size_t dst_size,
                               uint8_t *dst_ptr_end, uint8_t *dst_start,
                               const uint8_t *src_end, MermaidLzTable *lz,
                               int32_t *saved_dist, size_t startoff);
const uint8_t *Mermaid_Mode1 (uint8_t *dst, size_t dst_size,
                               uint8_t *dst_ptr_end, uint8_t *dst_start,
                               const uint8_t *src_end, MermaidLzTable *lz,
                               int32_t *saved_dist, size_t startoff);
bool Mermaid_ProcessLzRuns (int32_t mode, const uint8_t *src,
                            const uint8_t *src_end, uint8_t *dst,
                            size_t dst_size, uint64_t offset,
                            uint8_t *dst_end, MermaidLzTable *lz);
int32_t Mermaid_DecodeQuantum (uint8_t *dst, uint8_t *dst_end,
                               uint8_t *dst_start, const uint8_t *src,
                               const uint8_t *src_end, uint8_t *temp,
                               uint8_t *temp_end);
