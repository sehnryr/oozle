#pragma once

#include "decompress.h"

struct TansData
{
  u_int32_t A_used;
  u_int32_t B_used;
  u_int8_t A[256];
  u_int32_t B[256];
};

struct TansLutEnt
{
  u_int32_t x;
  u_int8_t bits_x;
  u_int8_t symbol;
  u_int16_t w;
};

struct TansDecoderParams
{
  TansLutEnt *lut;
  u_int8_t *dst, *dst_end;
  const u_int8_t *ptr_f, *ptr_b;
  u_int32_t bits_f, bits_b;
  int32_t bitpos_f, bitpos_b;
  u_int32_t state_0, state_1, state_2, state_3, state_4;
};

template <typename T> void SimpleSort (T *p, T *pend);

bool Tans_DecodeTable (BitReader *bits, int32_t L_bits, TansData *tans_data);
void Tans_InitLut (TansData *tans_data, int32_t L_bits, TansLutEnt *lut);
bool Tans_Decode (TansDecoderParams *params);
