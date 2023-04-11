#pragma once

#include "bitreader.h"

struct HuffRevLut
{
  uint8_t bits2len[2048];
  uint8_t bits2sym[2048];
};

struct HuffReader
{
  // Array to hold the output of the huffman read array operation
  uint8_t *output, *output_end;
  // We decode three parallel streams, two forwards, |src| and |src_mid|
  // while |src_end| is decoded backwards.
  const uint8_t *src, *src_mid, *src_end, *src_mid_org;
  int32_t src_bitpos, src_mid_bitpos, src_end_bitpos;
  uint32_t src_bits, src_mid_bits, src_end_bits;
};

struct HuffRange
{
  uint16_t symbol;
  uint16_t num;
};

struct NewHuffLut
{
  // Mapping that maps a bit pattern to a code length.
  uint8_t bits2len[2048 + 16];
  // Mapping that maps a bit pattern to a symbol.
  uint8_t bits2sym[2048 + 16];
};

int32_t Huff_ReadCodeLengthsOld (BitReader *bits, uint8_t *syms,
                                 uint32_t *code_prefix);
int32_t Huff_ConvertToRanges (HuffRange *range, int32_t num_symbols, int32_t P,
                              const uint8_t *symlen, BitReader *bits);
int32_t Huff_ReadCodeLengthsNew (BitReader *bits, uint8_t *syms,
                                 uint32_t *code_prefix);
bool Huff_MakeLut (const uint32_t *prefix_org, const uint32_t *prefix_cur,
                   NewHuffLut *hufflut, uint8_t *syms);
