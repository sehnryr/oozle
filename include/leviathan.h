#pragma once

#include "decompress.h"
#include "stdafx.h"

struct LeviathanLzTable
{
  int32_t *offs_stream;
  int32_t offs_stream_size;
  int32_t *len_stream;
  int32_t len_stream_size;
  u_int8_t *lit_stream[16];
  int32_t lit_stream_size[16];
  int32_t lit_stream_total;
  u_int8_t *multi_cmd_ptr[8];
  u_int8_t *multi_cmd_end[8];
  u_int8_t *cmd_stream;
  int32_t cmd_stream_size;
};

bool Leviathan_ReadLzTable (int32_t chunk_type, const u_int8_t *src,
                            const u_int8_t *src_end, u_int8_t *dst,
                            int32_t dst_size, int32_t offset,
                            u_int8_t *scratch, u_int8_t *scratch_end,
                            LeviathanLzTable *lztable);
template <typename Mode, bool MultiCmd>
bool Leviathan_ProcessLz (LeviathanLzTable *lzt, u_int8_t *dst,
                          u_int8_t *dst_start, u_int8_t *dst_end,
                          u_int8_t *window_base);
bool Leviathan_ProcessLzRuns (int32_t chunk_type, u_int8_t *dst,
                              int32_t dst_size, int32_t offset,
                              LeviathanLzTable *lzt);
int32_t Leviathan_DecodeQuantum (u_int8_t *dst, u_int8_t *dst_end,
                                 u_int8_t *dst_start, const u_int8_t *src,
                                 const u_int8_t *src_end, u_int8_t *scratch,
                                 u_int8_t *scratch_end);
