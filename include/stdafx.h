#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

#define assert(x) (void)0

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

inline uint16_t
_byteswap_ushort (uint16_t i)
{
  uint16_t j;
  j = (i << 8);
  j += (i >> 8);
  return j;
}

inline uint32_t
_byteswap_ulong (uint32_t i)
{
  uint32_t j;
  j = (i << 24);
  j += (i << 8) & 0x00FF0000;
  j += (i >> 8) & 0x0000FF00;
  j += (i >> 24);
  return j;
}

inline uint64_t
_byteswap_uint64_t (uint64_t i)
{
  uint64_t j;
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

#ifdef __linux__
// GCC __forceinline macro
#define __forceinline inline __attribute__ ((always_inline))
#endif

__forceinline uint8_t
_BitScanReverse (uint64_t *const Index, const uint64_t Mask)
{
  *Index = 31 - __builtin_clz (Mask);
  return Mask ? 1 : 0;
}

__forceinline uint8_t
_BitScanForward (uint64_t *const Index, const uint64_t Mask)
{
  *Index = __builtin_ctz (Mask);
  return Mask ? 1 : 0;
}

__forceinline uint32_t
_rotl (uint32_t value, int32_t shift)
{
  return (((value) << ((int32_t)(shift)))
          | ((value) >> (32 - (int32_t)(shift))));
}
