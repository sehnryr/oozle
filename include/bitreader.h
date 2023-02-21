#pragma once

#include "stdafx.h"

typedef struct BitReader
{
  // |p| holds the current u_int8_t and |p_end| the end of the buffer.
  const u_int8_t *p, *p_end;
  // Bits accumulated so far
  u_int32_t bits;
  // Next u_int8_t will end up in the |bitpos| position in |bits|.
  int32_t bitpos;
} BitReader;

struct BitReader2
{
  const u_int8_t *p, *p_end;
  u_int32_t bitpos;
};

void BitReader_Refill (BitReader *bits);
void BitReader_RefillBackwards (BitReader *bits);
int32_t BitReader_ReadBit (BitReader *bits);
int32_t BitReader_ReadBitNoRefill (BitReader *bits);
int32_t BitReader_ReadBitsNoRefill (BitReader *bits, int32_t n);
int32_t BitReader_ReadBitsNoRefillZero (BitReader *bits, int32_t n);
u_int32_t BitReader_ReadMoreThan24Bits (BitReader *bits, int32_t n);
u_int32_t BitReader_ReadMoreThan24BitsB (BitReader *bits, int32_t n);
int32_t BitReader_ReadGamma (BitReader *bits);
int32_t BitReader_ReadGammaX (BitReader *bits, int32_t forced);
u_int32_t BitReader_ReadDistance (BitReader *bits, u_int32_t v);
u_int32_t BitReader_ReadDistanceB (BitReader *bits, u_int32_t v);
bool BitReader_ReadLength (BitReader *bits, u_int32_t *v);
bool BitReader_ReadLengthB (BitReader *bits, u_int32_t *v);
int32_t BitReader_ReadFluff (BitReader *bits, int32_t num_symbols);
