#pragma once

#include "stdafx.h"

struct BitReader
{
  // |p| holds the current uint8_t and |p_end| the end of the buffer.
  const uint8_t *p, *p_end;
  // Bits accumulated so far
  uint32_t bits;
  // Next uint8_t will end up in the |bitpos| position in |bits|.
  int32_t bitpos;
};

struct BitReader2
{
  const uint8_t *p, *p_end;
  uint32_t bitpos;
};

void BitReader_Refill (BitReader *bits);
void BitReader_RefillBackwards (BitReader *bits);
int32_t BitReader_ReadBit (BitReader *bits);
int32_t BitReader_ReadBitNoRefill (BitReader *bits);
int32_t BitReader_ReadBitsNoRefill (BitReader *bits, int32_t n);
int32_t BitReader_ReadBitsNoRefillZero (BitReader *bits, int32_t n);
uint32_t BitReader_ReadMoreThan24Bits (BitReader *bits, int32_t n);
uint32_t BitReader_ReadMoreThan24BitsB (BitReader *bits, int32_t n);
int32_t BitReader_ReadGamma (BitReader *bits);
int32_t BitReader_ReadGammaX (BitReader *bits, int32_t forced);
uint32_t BitReader_ReadDistance (BitReader *bits, uint32_t v);
uint32_t BitReader_ReadDistanceB (BitReader *bits, uint32_t v);
bool BitReader_ReadLength (BitReader *bits, uint32_t *v);
bool BitReader_ReadLengthB (BitReader *bits, uint32_t *v);
int32_t BitReader_ReadFluff (BitReader *bits, int32_t num_symbols);
