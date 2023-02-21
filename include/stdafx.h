#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

#define assert(x) (void)0

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

inline u_int16_t
_byteswap_ushort (u_int16_t i)
{
  u_int16_t j;
  j = (i << 8);
  j += (i >> 8);
  return j;
}

inline u_int32_t
_byteswap_ulong (u_int32_t i)
{
  u_int32_t j;
  j = (i << 24);
  j += (i << 8) & 0x00FF0000;
  j += (i >> 8) & 0x0000FF00;
  j += (i >> 24);
  return j;
}

inline u_int64_t
_byteswap_u_int64_t (u_int64_t i)
{
  u_int64_t j;
  j = (i << 56);
  j += (i << 40) & 0x00FF000000000000;
  j += (i << 24) & 0x0000FF0000000000;
  j += (i << 8) & 0x000000FF00000000;
  j += (i >> 8) & 0x00000000FF000000;
  j += (i >> 24) & 0x0000000000FF0000;
  j += (i >> 40) & 0x000000000000FF00;
  j += (i >> 56);
  return j;
}

// GCC __forceinline macro
#define __forceinline inline __attribute__ ((always_inline))

__forceinline u_int8_t
_BitScanReverse (u_int64_t *const Index, const u_int64_t Mask)
{
  *Index = 31 - __builtin_clz (Mask);
  return Mask ? 1 : 0;
}

__forceinline u_int8_t
_BitScanForward (u_int64_t *const Index, const u_int64_t Mask)
{
  *Index = __builtin_ctz (Mask);
  return Mask ? 1 : 0;
}

__forceinline u_int32_t
_rotl (u_int32_t value, int32_t shift)
{
  return (((value) << ((int32_t)(shift)))
          | ((value) >> (32 - (int32_t)(shift))));
}
