#pragma once

#include "bitreader.h"

struct HuffRevLut
{
  u_int8_t bits2len[2048];
  u_int8_t bits2sym[2048];
};

typedef struct HuffReader
{
  // Array to hold the output of the huffman read array operation
  u_int8_t *output, *output_end;
  // We decode three parallel streams, two forwards, |src| and |src_mid|
  // while |src_end| is decoded backwards.
  const u_int8_t *src, *src_mid, *src_end, *src_mid_org;
  int32_t src_bitpos, src_mid_bitpos, src_end_bitpos;
  u_int32_t src_bits, src_mid_bits, src_end_bits;
} HuffReader;

struct HuffRange
{
  u_int16_t symbol;
  u_int16_t num;
};

struct NewHuffLut
{
  // Mapping that maps a bit pattern to a code length.
  u_int8_t bits2len[2048 + 16];
  // Mapping that maps a bit pattern to a symbol.
  u_int8_t bits2sym[2048 + 16];
};

int32_t Huff_ReadCodeLengthsOld (BitReader *bits, u_int8_t *syms,
                                 u_int32_t *code_prefix);
int32_t Huff_ConvertToRanges (HuffRange *range, int32_t num_symbols, int32_t P,
                              const u_int8_t *symlen, BitReader *bits);
int32_t Huff_ReadCodeLengthsNew (BitReader *bits, u_int8_t *syms,
                                 u_int32_t *code_prefix);
bool Huff_MakeLut (const u_int32_t *prefix_org, const u_int32_t *prefix_cur,
                   NewHuffLut *hufflut, u_int8_t *syms);
