#include "oozle/include/decompress.h"

uint32_t
BSR (uint32_t x)
{
  uint64_t index;
  BitScanReverse (&index, x);
  return index;
}

uint32_t
BSF (uint32_t x)
{
  uint64_t index;
  BitScanForward (&index, x);
  return index;
}

int32_t
Log2RoundUp (uint32_t v)
{
  if (v > 1)
    {
      uint64_t idx;
      BitScanReverse (&idx, v - 1);
      return idx + 1;
    }
  else
    {
      return 0;
    }
}

// Rearranges elements in the input array so that bits in the index
// get flipped.
static void
ReverseBitsArray2048 (const uint8_t *input, uint8_t *output)
{
  static const uint8_t offsets[32]
      = { 0,    0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50,
          0xD0, 0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8,
          0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8 };
  __m128i t0, t1, t2, t3, s0, s1, s2, s3;
  int32_t i, j;
  for (i = 0; i != 32; i++)
    {
      j = offsets[i];
      t0 = _mm_unpacklo_epi8 (
          _mm_loadl_epi64 ((const __m128i *)&input[j]),
          _mm_loadl_epi64 ((const __m128i *)&input[j + 256]));
      t1 = _mm_unpacklo_epi8 (
          _mm_loadl_epi64 ((const __m128i *)&input[j + 512]),
          _mm_loadl_epi64 ((const __m128i *)&input[j + 768]));
      t2 = _mm_unpacklo_epi8 (
          _mm_loadl_epi64 ((const __m128i *)&input[j + 1024]),
          _mm_loadl_epi64 ((const __m128i *)&input[j + 1280]));
      t3 = _mm_unpacklo_epi8 (
          _mm_loadl_epi64 ((const __m128i *)&input[j + 1536]),
          _mm_loadl_epi64 ((const __m128i *)&input[j + 1792]));

      s0 = _mm_unpacklo_epi8 (t0, t1);
      s1 = _mm_unpacklo_epi8 (t2, t3);
      s2 = _mm_unpackhi_epi8 (t0, t1);
      s3 = _mm_unpackhi_epi8 (t2, t3);

      t0 = _mm_unpacklo_epi8 (s0, s1);
      t1 = _mm_unpacklo_epi8 (s2, s3);
      t2 = _mm_unpackhi_epi8 (s0, s1);
      t3 = _mm_unpackhi_epi8 (s2, s3);

      _mm_storel_epi64 ((__m128i *)&output[0], t0);
      _mm_storeh_pi ((__m64 *)&output[1024], _mm_castsi128_ps (t0));
      _mm_storel_epi64 ((__m128i *)&output[256], t1);
      _mm_storeh_pi ((__m64 *)&output[1280], _mm_castsi128_ps (t1));
      _mm_storel_epi64 ((__m128i *)&output[512], t2);
      _mm_storeh_pi ((__m64 *)&output[1536], _mm_castsi128_ps (t2));
      _mm_storel_epi64 ((__m128i *)&output[768], t3);
      _mm_storeh_pi ((__m64 *)&output[1792], _mm_castsi128_ps (t3));
      output += 8;
    }
}

bool
Oozle_DecodeBytesCore (HuffReader *hr, HuffRevLut *lut)
{
  const uint8_t *src = hr->src;
  uint32_t src_bits = hr->src_bits;
  int32_t src_bitpos = hr->src_bitpos;

  const uint8_t *src_mid = hr->src_mid;
  uint32_t src_mid_bits = hr->src_mid_bits;
  int32_t src_mid_bitpos = hr->src_mid_bitpos;

  const uint8_t *src_end = hr->src_end;
  uint32_t src_end_bits = hr->src_end_bits;
  int32_t src_end_bitpos = hr->src_end_bitpos;

  int32_t k, n;

  uint8_t *dst = hr->output;
  uint8_t *dst_end = hr->output_end;

  if (src > src_mid)
    return false;

  if (hr->src_end - src_mid >= 4 && dst_end - dst >= 6)
    {
      dst_end -= 5;
      src_end -= 4;

      while (dst < dst_end && src <= src_mid && src_mid <= src_end)
        {
          src_bits |= *(uint32_t *)src << src_bitpos;
          src += (31 - src_bitpos) >> 3;

          src_end_bits |= byteswap_ulong (*(uint32_t *)src_end)
                          << src_end_bitpos;
          src_end -= (31 - src_end_bitpos) >> 3;

          src_mid_bits |= *(uint32_t *)src_mid << src_mid_bitpos;
          src_mid += (31 - src_mid_bitpos) >> 3;

          src_bitpos |= 0x18;
          src_end_bitpos |= 0x18;
          src_mid_bitpos |= 0x18;

          k = src_bits & 0x7FF;
          n = lut->bits2len[k];
          src_bits >>= n;
          src_bitpos -= n;
          dst[0] = lut->bits2sym[k];

          k = src_end_bits & 0x7FF;
          n = lut->bits2len[k];
          src_end_bits >>= n;
          src_end_bitpos -= n;
          dst[1] = lut->bits2sym[k];

          k = src_mid_bits & 0x7FF;
          n = lut->bits2len[k];
          src_mid_bits >>= n;
          src_mid_bitpos -= n;
          dst[2] = lut->bits2sym[k];

          k = src_bits & 0x7FF;
          n = lut->bits2len[k];
          src_bits >>= n;
          src_bitpos -= n;
          dst[3] = lut->bits2sym[k];

          k = src_end_bits & 0x7FF;
          n = lut->bits2len[k];
          src_end_bits >>= n;
          src_end_bitpos -= n;
          dst[4] = lut->bits2sym[k];

          k = src_mid_bits & 0x7FF;
          n = lut->bits2len[k];
          src_mid_bits >>= n;
          src_mid_bitpos -= n;
          dst[5] = lut->bits2sym[k];
          dst += 6;
        }
      dst_end += 5;

      src -= src_bitpos >> 3;
      src_bitpos &= 7;

      src_end += 4 + (src_end_bitpos >> 3);
      src_end_bitpos &= 7;

      src_mid -= src_mid_bitpos >> 3;
      src_mid_bitpos &= 7;
    }
  for (;;)
    {
      if (dst >= dst_end)
        break;

      if (src_mid - src <= 1)
        {
          if (src_mid - src == 1)
            src_bits |= *src << src_bitpos;
        }
      else
        {
          src_bits |= *(uint16_t *)src << src_bitpos;
        }
      k = src_bits & 0x7FF;
      n = lut->bits2len[k];
      src_bitpos -= n;
      src_bits >>= n;
      *dst++ = lut->bits2sym[k];
      src += (7 - src_bitpos) >> 3;
      src_bitpos &= 7;

      if (dst < dst_end)
        {
          if (src_end - src_mid <= 1)
            {
              if (src_end - src_mid == 1)
                {
                  src_end_bits |= *src_mid << src_end_bitpos;
                  src_mid_bits |= *src_mid << src_mid_bitpos;
                }
            }
          else
            {
              uint32_t v = *(uint16_t *)(src_end - 2);
              src_end_bits |= (((v >> 8) | (v << 8)) & 0xffff)
                              << src_end_bitpos;
              src_mid_bits |= *(uint16_t *)src_mid << src_mid_bitpos;
            }
          n = lut->bits2len[src_end_bits & 0x7FF];
          *dst++ = lut->bits2sym[src_end_bits & 0x7FF];
          src_end_bitpos -= n;
          src_end_bits >>= n;
          src_end -= (7 - src_end_bitpos) >> 3;
          src_end_bitpos &= 7;
          if (dst < dst_end)
            {
              n = lut->bits2len[src_mid_bits & 0x7FF];
              *dst++ = lut->bits2sym[src_mid_bits & 0x7FF];
              src_mid_bitpos -= n;
              src_mid_bits >>= n;
              src_mid += (7 - src_mid_bitpos) >> 3;
              src_mid_bitpos &= 7;
            }
        }
      if (src > src_mid || src_mid > src_end)
        return false;
    }
  if (src != hr->src_mid_org || src_end != src_mid)
    return false;
  return true;
}

bool
DecodeGolombRiceLengths (uint8_t *dst, size_t size, BitReader2 *br)
{
  const uint8_t *p = br->p, *p_end = br->p_end;
  uint8_t *dst_end = dst + size;
  if (p >= p_end)
    return false;

  int32_t count = -(int32_t)br->bitpos;
  uint32_t v = *p++ & (255 >> br->bitpos);
  for (;;)
    {
      if (v == 0)
        {
          count += 8;
        }
      else
        {
          uint32_t x = kRiceCodeBits2Value[v];
          *(uint32_t *)&dst[0] = count + (x & 0x0f0f0f0f);
          *(uint32_t *)&dst[4] = (x >> 4) & 0x0f0f0f0f;
          dst += kRiceCodeBits2Len[v];
          if (dst >= dst_end)
            break;
          count = x >> 28;
        }
      if (p >= p_end)
        return false;
      v = *p++;
    }
  // went too far, step back
  if (dst > dst_end)
    {
      int32_t n = dst - dst_end;
      do
        v &= (v - 1);
      while (--n);
    }
  // step back if uint8_t not finished
  int32_t bitpos = 0;
  if (!(v & 1))
    {
      p--;
      uint64_t q;
      BitScanForward (&q, v);
      bitpos = 8 - q;
    }
  br->p = p;
  br->bitpos = bitpos;
  return true;
}

bool
DecodeGolombRiceBits (uint8_t *dst, uint32_t size, uint32_t bitcount,
                      BitReader2 *br)
{
  if (bitcount == 0)
    return true;
  uint8_t *dst_end = dst + size;
  const uint8_t *p = br->p;
  int32_t bitpos = br->bitpos;

  uint32_t bits_required = bitpos + bitcount * size;
  uint32_t bytes_required = (bits_required + 7) >> 3;
  if (bytes_required > br->p_end - p)
    return false;

  br->p = p + (bits_required >> 3);
  br->bitpos = bits_required & 7;

  // todo. handle r/w outside of range
  uint64_t bak = *(uint64_t *)dst_end;

  if (bitcount < 2)
    {
      assert (bitcount == 1);
      do
        {
          // Read the next byte
          uint64_t bits
              = (uint8_t)(byteswap_ulong (*(uint32_t *)p) >> (24 - bitpos));
          p += 1;
          // Expand each bit into each uint8_t of the uint64_t.
          bits = (bits | (bits << 28)) & 0xF0000000Full;
          bits = (bits | (bits << 14)) & 0x3000300030003ull;
          bits = (bits | (bits << 7)) & 0x0101010101010101ull;
          *(uint64_t *)dst
              = *(uint64_t *)dst * 2 + byteswap_uint64 (bits);
          dst += 8;
        }
      while (dst < dst_end);
    }
  else if (bitcount == 2)
    {
      do
        {
          // Read the next 2 bytes
          uint64_t bits = (uint16_t)(byteswap_ulong (*(uint32_t *)p)
                                       >> (16 - bitpos));
          p += 2;
          // Expand each bit into each uint8_t of the uint64_t.
          bits = (bits | (bits << 24)) & 0xFF000000FFull;
          bits = (bits | (bits << 12)) & 0xF000F000F000Full;
          bits = (bits | (bits << 6)) & 0x0303030303030303ull;
          *(uint64_t *)dst
              = *(uint64_t *)dst * 4 + byteswap_uint64 (bits);
          dst += 8;
        }
      while (dst < dst_end);
    }
  else
    {
      assert (bitcount == 3);
      do
        {
          // Read the next 3 bytes
          uint64_t bits
              = (byteswap_ulong (*(uint32_t *)p) >> (8 - bitpos)) & 0xffffff;
          p += 3;
          // Expand each bit into each uint8_t of the uint64_t.
          bits = (bits | (bits << 20)) & 0xFFF00000FFFull;
          bits = (bits | (bits << 10)) & 0x3F003F003F003Full;
          bits = (bits | (bits << 5)) & 0x0707070707070707ull;
          *(uint64_t *)dst
              = *(uint64_t *)dst * 8 + byteswap_uint64 (bits);
          dst += 8;
        }
      while (dst < dst_end);
    }
  *(uint64_t *)dst_end = bak;
  return true;
}

// May overflow 16 bytes past the end
void
FillByteOverflow16 (uint8_t *dst, uint8_t v, size_t n)
{
  memset (dst, v, n);
}

int32_t
Oozle_DecodeBytes_Type12 (const uint8_t *src, size_t src_size,
                          uint8_t *output, int32_t output_size, int32_t type)
{
  BitReader bits;
  int32_t half_output_size;
  uint32_t split_left, split_mid, split_right;
  const uint8_t *src_mid;
  NewHuffLut huff_lut;
  HuffReader hr;
  HuffRevLut rev_lut;
  const uint8_t *src_end = src + src_size;

  bits.bitpos = 24;
  bits.bits = 0;
  bits.p = src;
  bits.p_end = src_end;
  BitReader_Refill (&bits);

  static const uint32_t code_prefix_org[12] = { 0x0,  0x0,   0x2,   0x6,
                                                 0xE,  0x1E,  0x3E,  0x7E,
                                                 0xFE, 0x1FE, 0x2FE, 0x3FE };
  uint32_t code_prefix[12] = { 0x0,  0x0,  0x2,  0x6,   0xE,   0x1E,
                                0x3E, 0x7E, 0xFE, 0x1FE, 0x2FE, 0x3FE };
  uint8_t syms[1280];
  int32_t num_syms;
  if (!BitReader_ReadBitNoRefill (&bits))
    {
      num_syms = Huff_ReadCodeLengthsOld (&bits, syms, code_prefix);
    }
  else if (!BitReader_ReadBitNoRefill (&bits))
    {
      num_syms = Huff_ReadCodeLengthsNew (&bits, syms, code_prefix);
    }
  else
    {
      return -1;
    }

  if (num_syms < 1)
    return -1;
  src = bits.p - ((24 - bits.bitpos) / 8);

  if (num_syms == 1)
    {
      memset (output, syms[0], output_size);
      return src - src_end;
    }

  if (!Huff_MakeLut (code_prefix_org, code_prefix, &huff_lut, syms))
    return -1;

  ReverseBitsArray2048 (huff_lut.bits2len, rev_lut.bits2len);
  ReverseBitsArray2048 (huff_lut.bits2sym, rev_lut.bits2sym);

  if (type == 1)
    {
      if (src + 3 > src_end)
        return -1;
      split_mid = *(uint16_t *)src;
      src += 2;
      hr.output = output;
      hr.output_end = output + output_size;
      hr.src = src;
      hr.src_end = src_end;
      hr.src_mid_org = hr.src_mid = src + split_mid;
      hr.src_bitpos = 0;
      hr.src_bits = 0;
      hr.src_mid_bitpos = 0;
      hr.src_mid_bits = 0;
      hr.src_end_bitpos = 0;
      hr.src_end_bits = 0;
      if (!Oozle_DecodeBytesCore (&hr, &rev_lut))
        return -1;
    }
  else
    {
      if (src + 6 > src_end)
        return -1;

      half_output_size = (output_size + 1) >> 1;
      split_mid = *(uint32_t *)src & 0xFFFFFF;
      src += 3;
      if (split_mid > (src_end - src))
        return -1;
      src_mid = src + split_mid;
      split_left = *(uint16_t *)src;
      src += 2;
      if (src_mid - src < split_left + 2 || src_end - src_mid < 3)
        return -1;
      split_right = *(uint16_t *)src_mid;
      if (src_end - (src_mid + 2) < split_right + 2)
        return -1;

      hr.output = output;
      hr.output_end = output + half_output_size;
      hr.src = src;
      hr.src_end = src_mid;
      hr.src_mid_org = hr.src_mid = src + split_left;
      hr.src_bitpos = 0;
      hr.src_bits = 0;
      hr.src_mid_bitpos = 0;
      hr.src_mid_bits = 0;
      hr.src_end_bitpos = 0;
      hr.src_end_bits = 0;
      if (!Oozle_DecodeBytesCore (&hr, &rev_lut))
        return -1;

      hr.output = output + half_output_size;
      hr.output_end = output + output_size;
      hr.src = src_mid + 2;
      hr.src_end = src_end;
      hr.src_mid_org = hr.src_mid = src_mid + 2 + split_right;
      hr.src_bitpos = 0;
      hr.src_bits = 0;
      hr.src_mid_bitpos = 0;
      hr.src_mid_bits = 0;
      hr.src_end_bitpos = 0;
      hr.src_end_bits = 0;
      if (!Oozle_DecodeBytesCore (&hr, &rev_lut))
        return -1;
    }
  return (int32_t)src_size;
}

int32_t
Oozle_DecodeMultiArray (const uint8_t *src, const uint8_t *src_end,
                        uint8_t *dst, uint8_t *dst_end,
                        uint8_t **array_data, int32_t *array_lens,
                        int32_t array_count, int32_t *total_size_out,
                        bool force_memmove, uint8_t *scratch,
                        uint8_t *scratch_end)
{
  const uint8_t *src_org = src;

  if (src_end - src < 4)
    return -1;

  int32_t decoded_size;
  int32_t num_arrays_in_file = *src++;
  if (!(num_arrays_in_file & 0x80))
    return -1;
  num_arrays_in_file &= 0x3f;

  if (dst == scratch)
    {
      // todo: ensure scratch space first?
      scratch += (scratch_end - scratch - 0xc000) >> 1;
      dst_end = scratch;
    }

  int32_t total_size = 0;

  if (num_arrays_in_file == 0)
    {
      for (int32_t i = 0; i < array_count; i++)
        {
          uint8_t *chunk_dst = dst;
          int32_t dec = Oozle_DecodeBytes (
              &chunk_dst, src, src_end, &decoded_size, dst_end - dst,
              force_memmove, scratch, scratch_end);
          if (dec < 0)
            return -1;
          dst += decoded_size;
          array_lens[i] = decoded_size;
          array_data[i] = chunk_dst;
          src += dec;
          total_size += decoded_size;
        }
      *total_size_out = total_size;
      return src - src_org; // not supported yet
    }

  uint8_t *entropy_array_data[32];
  uint32_t entropy_array_size[32];

  // First loop just decodes everything to scratch
  uint8_t *scratch_cur = scratch;

  for (int32_t i = 0; i < num_arrays_in_file; i++)
    {
      uint8_t *chunk_dst = scratch_cur;
      int32_t dec = Oozle_DecodeBytes (
          &chunk_dst, src, src_end, &decoded_size, scratch_end - scratch_cur,
          force_memmove, scratch_cur, scratch_end);
      if (dec < 0)
        return -1;
      entropy_array_data[i] = chunk_dst;
      entropy_array_size[i] = decoded_size;
      scratch_cur += decoded_size;
      total_size += decoded_size;
      src += dec;
    }
  *total_size_out = total_size;

  if (src_end - src < 3)
    return -1;

  int32_t Q = *(uint16_t *)src;
  src += 2;

  int32_t out_size;
  if (Oozle_GetBlockSize (src, src_end, &out_size, total_size) < 0)
    return -1;
  int32_t num_indexes = out_size;

  int32_t num_lens = num_indexes - array_count;
  if (num_lens < 1)
    return -1;

  if (scratch_end - scratch_cur < num_indexes)
    return -1;
  uint8_t *interval_lenlog2 = scratch_cur;
  scratch_cur += num_indexes;

  if (scratch_end - scratch_cur < num_indexes)
    return -1;
  uint8_t *interval_indexes = scratch_cur;
  scratch_cur += num_indexes;

  if (Q & 0x8000)
    {
      int32_t size_out;
      int32_t n
          = Oozle_DecodeBytes (&interval_indexes, src, src_end, &size_out,
                               num_indexes, false, scratch_cur, scratch_end);
      if (n < 0 || size_out != num_indexes)
        return -1;
      src += n;

      for (int32_t i = 0; i < num_indexes; i++)
        {
          int32_t t = interval_indexes[i];
          interval_lenlog2[i] = t >> 4;
          interval_indexes[i] = t & 0xF;
        }

      num_lens = num_indexes;
    }
  else
    {
      int32_t lenlog2_chunksize = num_indexes - array_count;

      int32_t size_out;
      int32_t n
          = Oozle_DecodeBytes (&interval_indexes, src, src_end, &size_out,
                               num_indexes, false, scratch_cur, scratch_end);
      if (n < 0 || size_out != num_indexes)
        return -1;
      src += n;

      n = Oozle_DecodeBytes (&interval_lenlog2, src, src_end, &size_out,
                             lenlog2_chunksize, false, scratch_cur,
                             scratch_end);
      if (n < 0 || size_out != lenlog2_chunksize)
        return -1;
      src += n;

      for (int32_t i = 0; i < lenlog2_chunksize; i++)
        if (interval_lenlog2[i] > 16)
          return -1;
    }

  if (scratch_end - scratch_cur < 4)
    return -1;

  scratch_cur = ALIGN_POINTER (scratch_cur, 4);
  if (scratch_end - scratch_cur < num_lens * 4)
    return -1;
  uint32_t *decoded_intervals = (uint32_t *)scratch_cur;

  int32_t varbits_complen = Q & 0x3FFF;
  if (src_end - src < varbits_complen)
    return -1;

  const uint8_t *f = src;
  uint32_t bits_f = 0;
  int32_t bitpos_f = 24;

  const uint8_t *src_end_actual = src + varbits_complen;

  const uint8_t *b = src_end_actual;
  uint32_t bits_b = 0;
  int32_t bitpos_b = 24;

  int32_t i;
  for (i = 0; i + 2 <= num_lens; i += 2)
    {
      bits_f |= byteswap_ulong (*(uint32_t *)f) >> (24 - bitpos_f);
      f += (bitpos_f + 7) >> 3;

      bits_b |= ((uint32_t *)b)[-1] >> (24 - bitpos_b);
      b -= (bitpos_b + 7) >> 3;

      int32_t numbits_f = interval_lenlog2[i + 0];
      int32_t numbits_b = interval_lenlog2[i + 1];

      bits_f = rotl (bits_f | 1, numbits_f);
      bitpos_f += numbits_f - 8 * ((bitpos_f + 7) >> 3);

      bits_b = rotl (bits_b | 1, numbits_b);
      bitpos_b += numbits_b - 8 * ((bitpos_b + 7) >> 3);

      int32_t value_f = bits_f & bitmasks[numbits_f];
      bits_f &= ~bitmasks[numbits_f];

      int32_t value_b = bits_b & bitmasks[numbits_b];
      bits_b &= ~bitmasks[numbits_b];

      decoded_intervals[i + 0] = value_f;
      decoded_intervals[i + 1] = value_b;
    }

  // read final one since above loop reads 2
  if (i < num_lens)
    {
      bits_f |= byteswap_ulong (*(uint32_t *)f) >> (24 - bitpos_f);
      int32_t numbits_f = interval_lenlog2[i];
      bits_f = rotl (bits_f | 1, numbits_f);
      int32_t value_f = bits_f & bitmasks[numbits_f];
      decoded_intervals[i + 0] = value_f;
    }

  if (interval_indexes[num_indexes - 1])
    return -1;

  int32_t indi = 0, leni = 0, source;
  int32_t increment_leni = (Q & 0x8000) != 0;

  for (int32_t arri = 0; arri < array_count; arri++)
    {
      array_data[arri] = dst;
      if (indi >= num_indexes)
        return -1;

      while ((source = interval_indexes[indi++]) != 0)
        {
          if (source > num_arrays_in_file)
            return -1;
          if (leni >= num_lens)
            return -1;
          int32_t cur_len = decoded_intervals[leni++];
          int32_t bytes_left = entropy_array_size[source - 1];
          if (cur_len > bytes_left || cur_len > dst_end - dst)
            return -1;
          uint8_t *blksrc = entropy_array_data[source - 1];
          entropy_array_size[source - 1] -= cur_len;
          entropy_array_data[source - 1] += cur_len;
          uint8_t *dstx = dst;
          dst += cur_len;
          memcpy (dstx, blksrc, cur_len);
        }
      leni += increment_leni;
      array_lens[arri] = dst - array_data[arri];
    }

  if (indi != num_indexes || leni != num_lens)
    return -1;

  for (int32_t i = 0; i < num_arrays_in_file; i++)
    {
      if (entropy_array_size[i])
        return -1;
    }
  return src_end_actual - src_org;
}

int32_t
Oozle_DecodeRecursive (const uint8_t *src, size_t src_size, uint8_t *output,
                       int32_t output_size, uint8_t *scratch,
                       uint8_t *scratch_end)
{
  const uint8_t *src_org = src;
  uint8_t *output_end = output + output_size;
  const uint8_t *src_end = src + src_size;

  if (src_size < 6)
    return -1;

  int32_t n = src[0] & 0x7f;
  if (n < 2)
    return -1;

  if (!(src[0] & 0x80))
    {
      src++;
      do
        {
          int32_t decoded_size;
          int32_t dec = Oozle_DecodeBytes (&output, src, src_end,
                                           &decoded_size, output_end - output,
                                           true, scratch, scratch_end);
          if (dec < 0)
            return -1;
          output += decoded_size;
          src += dec;
        }
      while (--n);
      if (output != output_end)
        return -1;
      return src - src_org;
    }
  else
    {
      uint8_t *array_data;
      int32_t array_len, decoded_size;
      int32_t dec = Oozle_DecodeMultiArray (
          src, src_end, output, output_end, &array_data, &array_len, 1,
          &decoded_size, true, scratch, scratch_end);
      if (dec < 0)
        return -1;
      output += decoded_size;
      if (output != output_end)
        return -1;
      return dec;
    }
}

int32_t
Oozle_DecodeRLE (const uint8_t *src, size_t src_size, uint8_t *dst,
                 int32_t dst_size, uint8_t *scratch, uint8_t *scratch_end)
{
  if (src_size <= 1)
    {
      if (src_size != 1)
        return -1;
      memset (dst, src[0], dst_size);
      return 1;
    }
  uint8_t *dst_end = dst + dst_size;
  const uint8_t *cmd_ptr = src + 1, *cmd_ptr_end = src + src_size;
  // Unpack the first X bytes of the command buffer?
  if (src[0])
    {
      uint8_t *dst_ptr = scratch;
      int32_t dec_size;
      int32_t n = Oozle_DecodeBytes (&dst_ptr, src, src + src_size, &dec_size,
                                     scratch_end - scratch, true, scratch,
                                     scratch_end);
      if (n <= 0)
        return -1;
      int32_t cmd_len = src_size - n + dec_size;
      if (cmd_len > scratch_end - scratch)
        return -1;
      memcpy (dst_ptr + dec_size, src + n, src_size - n);
      cmd_ptr = dst_ptr;
      cmd_ptr_end = &dst_ptr[cmd_len];
    }

  int32_t rle_byte = 0;

  while (cmd_ptr < cmd_ptr_end)
    {
      uint32_t cmd = cmd_ptr_end[-1];
      if (cmd - 1 >= 0x2f)
        {
          cmd_ptr_end--;
          uint32_t bytes_to_copy = (-1 - cmd) & 0xF;
          uint32_t bytes_to_rle = cmd >> 4;
          if (dst_end - dst < bytes_to_copy + bytes_to_rle
              || cmd_ptr_end - cmd_ptr < bytes_to_copy)
            return -1;
          memcpy (dst, cmd_ptr, bytes_to_copy);
          cmd_ptr += bytes_to_copy;
          dst += bytes_to_copy;
          memset (dst, rle_byte, bytes_to_rle);
          dst += bytes_to_rle;
        }
      else if (cmd >= 0x10)
        {
          uint32_t data = *(uint16_t *)(cmd_ptr_end - 2) - 4096;
          cmd_ptr_end -= 2;
          uint32_t bytes_to_copy = data & 0x3F;
          uint32_t bytes_to_rle = data >> 6;
          if (dst_end - dst < bytes_to_copy + bytes_to_rle
              || cmd_ptr_end - cmd_ptr < bytes_to_copy)
            return -1;
          memcpy (dst, cmd_ptr, bytes_to_copy);
          cmd_ptr += bytes_to_copy;
          dst += bytes_to_copy;
          memset (dst, rle_byte, bytes_to_rle);
          dst += bytes_to_rle;
        }
      else if (cmd == 1)
        {
          rle_byte = *cmd_ptr++;
          cmd_ptr_end--;
        }
      else if (cmd >= 9)
        {
          uint32_t bytes_to_rle
              = (*(uint16_t *)(cmd_ptr_end - 2) - 0x8ff) * 128;
          cmd_ptr_end -= 2;
          if (dst_end - dst < bytes_to_rle)
            return -1;
          memset (dst, rle_byte, bytes_to_rle);
          dst += bytes_to_rle;
        }
      else
        {
          uint32_t bytes_to_copy
              = (*(uint16_t *)(cmd_ptr_end - 2) - 511) * 64;
          cmd_ptr_end -= 2;
          if (cmd_ptr_end - cmd_ptr < bytes_to_copy
              || dst_end - dst < bytes_to_copy)
            return -1;
          memcpy (dst, cmd_ptr, bytes_to_copy);
          dst += bytes_to_copy;
          cmd_ptr += bytes_to_copy;
        }
    }
  if (cmd_ptr_end != cmd_ptr)
    return -1;

  if (dst != dst_end)
    return -1;

  return src_size;
}

int32_t
Oozle_DecodeTans (const uint8_t *src, size_t src_size, uint8_t *dst,
                  int32_t dst_size, uint8_t *scratch, uint8_t *scratch_end)
{
  if (src_size < 8 || dst_size < 5)
    return -1;

  const uint8_t *src_end = src + src_size;

  BitReader br;
  TansData tans_data;

  br.bitpos = 24;
  br.bits = 0;
  br.p = src;
  br.p_end = src_end;
  BitReader_Refill (&br);

  // reserved bit
  if (BitReader_ReadBitNoRefill (&br))
    return -1;

  int32_t L_bits = BitReader_ReadBitsNoRefill (&br, 2) + 8;

  if (!Tans_DecodeTable (&br, L_bits, &tans_data))
    return -1;

  src = br.p - (24 - br.bitpos) / 8;

  if (src >= src_end)
    return -1;

  uint32_t lut_space_required = ((sizeof (TansLutEnt) << L_bits) + 15) & ~15;
  if (lut_space_required > (scratch_end - scratch))
    return -1;

  TansDecoderParams params;
  params.dst = dst;
  params.dst_end = dst + dst_size - 5;

  params.lut = (TansLutEnt *)ALIGN_POINTER (scratch, 16);
  Tans_InitLut (&tans_data, L_bits, params.lut);

  // Read out the initial state
  uint32_t L_mask = (1 << L_bits) - 1;
  uint32_t bits_f = *(uint32_t *)src;
  src += 4;
  uint32_t bits_b = byteswap_ulong (*(uint32_t *)(src_end - 4));
  src_end -= 4;
  uint32_t bitpos_f = 32, bitpos_b = 32;

  // Read first two.
  params.state_0 = bits_f & L_mask;
  params.state_1 = bits_b & L_mask;
  bits_f >>= L_bits, bitpos_f -= L_bits;
  bits_b >>= L_bits, bitpos_b -= L_bits;

  // Read next two.
  params.state_2 = bits_f & L_mask;
  params.state_3 = bits_b & L_mask;
  bits_f >>= L_bits, bitpos_f -= L_bits;
  bits_b >>= L_bits, bitpos_b -= L_bits;

  // Refill more bits
  bits_f |= *(uint32_t *)src << bitpos_f;
  src += (31 - bitpos_f) >> 3;
  bitpos_f |= 24;

  // Read final state variable
  params.state_4 = bits_f & L_mask;
  bits_f >>= L_bits, bitpos_f -= L_bits;

  params.bits_f = bits_f;
  params.ptr_f = src - (bitpos_f >> 3);
  params.bitpos_f = bitpos_f & 7;

  params.bits_b = bits_b;
  params.ptr_b = src_end + (bitpos_b >> 3);
  params.bitpos_b = bitpos_b & 7;

  if (!Tans_Decode (&params))
    return -1;

  return src_size;
}

int32_t
Oozle_GetBlockSize (const uint8_t *src, const uint8_t *src_end,
                    int32_t *dest_size, int32_t dest_capacity)
{
  const uint8_t *src_org = src;
  int32_t src_size, dst_size;

  if (src_end - src < 2)
    return -1; // too few bytes

  int32_t chunk_type = (src[0] >> 4) & 0x7;
  if (chunk_type == 0)
    {
      if (src[0] >= 0x80)
        {
          // In this mode, memcopy stores the length in the bottom 12 bits.
          src_size = ((src[0] << 8) | src[1]) & 0xFFF;
          src += 2;
        }
      else
        {
          if (src_end - src < 3)
            return -1; // too few bytes
          src_size = ((src[0] << 16) | (src[1] << 8) | src[2]);
          if (src_size & ~0x3ffff)
            return -1; // reserved bits must not be set
          src += 3;
        }
      if (src_size > dest_capacity || src_end - src < src_size)
        return -1;
      *dest_size = src_size;
      return src + src_size - src_org;
    }

  if (chunk_type >= 6)
    return -1;

  // In all the other modes, the initial bytes encode
  // the src_size and the dst_size
  if (src[0] >= 0x80)
    {
      if (src_end - src < 3)
        return -1; // too few bytes

      // short mode, 10 bit sizes
      uint32_t bits = ((src[0] << 16) | (src[1] << 8) | src[2]);
      src_size = bits & 0x3ff;
      dst_size = src_size + ((bits >> 10) & 0x3ff) + 1;
      src += 3;
    }
  else
    {
      // long mode, 18 bit sizes
      if (src_end - src < 5)
        return -1; // too few bytes
      uint32_t bits
          = ((src[1] << 24) | (src[2] << 16) | (src[3] << 8) | src[4]);
      src_size = bits & 0x3ffff;
      dst_size = (((bits >> 18) | (src[0] << 14)) & 0x3FFFF) + 1;
      if (src_size >= dst_size)
        return -1;
      src += 5;
    }
  if (src_end - src < src_size || dst_size > dest_capacity)
    return -1;
  *dest_size = dst_size;
  return src_size;
}

int32_t
Oozle_DecodeBytes (uint8_t **output, const uint8_t *src,
                   const uint8_t *src_end, int32_t *decoded_size,
                   size_t output_size, bool force_memmove, uint8_t *scratch,
                   uint8_t *scratch_end)
{
  const uint8_t *src_org = src;
  int32_t src_size, dst_size;

  if (src_end - src < 2)
    return -1; // too few bytes

  int32_t chunk_type = (src[0] >> 4) & 0x7;
  if (chunk_type == 0)
    {
      if (src[0] >= 0x80)
        {
          // In this mode, memcopy stores the length in the bottom 12 bits.
          src_size = ((src[0] << 8) | src[1]) & 0xFFF;
          src += 2;
        }
      else
        {
          if (src_end - src < 3)
            return -1; // too few bytes
          src_size = ((src[0] << 16) | (src[1] << 8) | src[2]);
          if (src_size & ~0x3ffff)
            return -1; // reserved bits must not be set
          src += 3;
        }
      if (src_size > output_size || src_end - src < src_size)
        return -1;
      *decoded_size = src_size;
      if (force_memmove)
        memmove (*output, src, src_size);
      else
        *output = (uint8_t *)src;
      return src + src_size - src_org;
    }

  // In all the other modes, the initial bytes encode
  // the src_size and the dst_size
  if (src[0] >= 0x80)
    {
      if (src_end - src < 3)
        return -1; // too few bytes

      // short mode, 10 bit sizes
      uint32_t bits = ((src[0] << 16) | (src[1] << 8) | src[2]);
      src_size = bits & 0x3ff;
      dst_size = src_size + ((bits >> 10) & 0x3ff) + 1;
      src += 3;
    }
  else
    {
      // long mode, 18 bit sizes
      if (src_end - src < 5)
        return -1; // too few bytes
      uint32_t bits
          = ((src[1] << 24) | (src[2] << 16) | (src[3] << 8) | src[4]);
      src_size = bits & 0x3ffff;
      dst_size = (((bits >> 18) | (src[0] << 14)) & 0x3FFFF) + 1;
      if (src_size >= dst_size)
        return -1;
      src += 5;
    }
  if (src_end - src < src_size || dst_size > output_size)
    return -1;

  uint8_t *dst = *output;
  if (dst == scratch)
    {
      if (scratch_end - scratch < dst_size)
        return -1;
      scratch += dst_size;
    }

  //  printf("%d -> %d (%d)\n", src_size, dst_size, chunk_type);

  int32_t src_used = -1;
  switch (chunk_type)
    {
    case 2:
    case 4:
      src_used = Oozle_DecodeBytes_Type12 (src, src_size, dst, dst_size,
                                           chunk_type >> 1);
      break;
    case 5:
      src_used = Oozle_DecodeRecursive (src, src_size, dst, dst_size, scratch,
                                        scratch_end);
      break;
    case 3:
      src_used = Oozle_DecodeRLE (src, src_size, dst, dst_size, scratch,
                                  scratch_end);
      break;
    case 1:
      src_used = Oozle_DecodeTans (src, src_size, dst, dst_size, scratch,
                                   scratch_end);
      break;
    }
  if (src_used != src_size)
    return -1;
  *decoded_size = dst_size;
  return src + src_size - src_org;
}

void
CombineScaledOffsetArrays (int32_t *offs_stream, size_t offs_stream_size,
                           int32_t scale, const uint8_t *low_bits)
{
  for (size_t i = 0; i != offs_stream_size; i++)
    offs_stream[i] = scale * offs_stream[i] - low_bits[i];
}
