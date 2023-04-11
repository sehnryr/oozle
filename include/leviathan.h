#pragma once

#include "decompress.h"
#include "stdafx.h"

struct LeviathanLzTable
{
  int32_t *offs_stream;
  int32_t offs_stream_size;
  int32_t *len_stream;
  int32_t len_stream_size;
  uint8_t *lit_stream[16];
  int32_t lit_stream_size[16];
  int32_t lit_stream_total;
  uint8_t *multi_cmd_ptr[8];
  uint8_t *multi_cmd_end[8];
  uint8_t *cmd_stream;
  int32_t cmd_stream_size;
};

bool Leviathan_ReadLzTable (int32_t chunk_type, const uint8_t *src,
                            const uint8_t *src_end, uint8_t *dst,
                            int32_t dst_size, int32_t offset,
                            uint8_t *scratch, uint8_t *scratch_end,
                            LeviathanLzTable *lztable);
template <typename Mode, bool MultiCmd>
bool Leviathan_ProcessLz (LeviathanLzTable *lzt, uint8_t *dst,
                          uint8_t *dst_start, uint8_t *dst_end,
                          uint8_t *window_base);
bool Leviathan_ProcessLzRuns (int32_t chunk_type, uint8_t *dst,
                              int32_t dst_size, int32_t offset,
                              LeviathanLzTable *lzt);
int32_t Leviathan_DecodeQuantum (uint8_t *dst, uint8_t *dst_end,
                                 uint8_t *dst_start, const uint8_t *src,
                                 const uint8_t *src_end, uint8_t *scratch,
                                 uint8_t *scratch_end);
