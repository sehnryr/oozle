#include "oozle/include/huffman.h"
#include "oozle/include/decompress.h"

int32_t
Huff_ReadCodeLengthsOld (BitReader *bits, u_int8_t *syms,
                         u_int32_t *code_prefix)
{
  if (BitReader_ReadBitNoRefill (bits))
    {
      int32_t n, sym = 0, codelen, num_symbols = 0;
      int32_t avg_bits_x4 = 32;
      int32_t forced_bits = BitReader_ReadBitsNoRefill (bits, 2);

      u_int32_t thres_for_valid_gamma_bits = 1 << (31 - (20u >> forced_bits));
      if (BitReader_ReadBit (bits))
        goto SKIP_INITIAL_ZEROS;
      do
        {
          // Run of zeros
          if (!(bits->bits & 0xff000000))
            return -1;
          sym += BitReader_ReadBitsNoRefill (
                     bits, 2 * (CountLeadingZeros (bits->bits) + 1))
                 - 2 + 1;
          if (sym >= 256)
            break;
        SKIP_INITIAL_ZEROS:
          BitReader_Refill (bits);
          // Read out the gamma value for the # of symbols
          if (!(bits->bits & 0xff000000))
            return -1;
          n = BitReader_ReadBitsNoRefill (
                  bits, 2 * (CountLeadingZeros (bits->bits) + 1))
              - 2 + 1;
          // Overflow?
          if (sym + n > 256)
            return -1;
          BitReader_Refill (bits);
          num_symbols += n;
          do
            {
              if (bits->bits < thres_for_valid_gamma_bits)
                return -1; // too big gamma value?

              int32_t lz = CountLeadingZeros (bits->bits);
              int32_t v
                  = BitReader_ReadBitsNoRefill (bits, lz + forced_bits + 1)
                    + ((lz - 1) << forced_bits);
              codelen
                  = (-(int32_t)(v & 1) ^ (v >> 1)) + ((avg_bits_x4 + 2) >> 2);
              if (codelen < 1 || codelen > 11)
                return -1;
              avg_bits_x4 = codelen + ((3 * avg_bits_x4 + 2) >> 2);
              BitReader_Refill (bits);
              syms[code_prefix[codelen]++] = sym++;
            }
          while (--n);
        }
      while (sym != 256);
      return (sym == 256) && (num_symbols >= 2) ? num_symbols : -1;
    }
  else
    {
      // Sparse symbol encoding
      int32_t num_symbols = BitReader_ReadBitsNoRefill (bits, 8);
      if (num_symbols == 0)
        return -1;
      if (num_symbols == 1)
        {
          syms[0] = BitReader_ReadBitsNoRefill (bits, 8);
        }
      else
        {
          int32_t codelen_bits = BitReader_ReadBitsNoRefill (bits, 3);
          if (codelen_bits > 4)
            return -1;
          for (int32_t i = 0; i < num_symbols; i++)
            {
              BitReader_Refill (bits);
              int32_t sym = BitReader_ReadBitsNoRefill (bits, 8);
              int32_t codelen
                  = BitReader_ReadBitsNoRefillZero (bits, codelen_bits) + 1;
              if (codelen > 11)
                return -1;
              syms[code_prefix[codelen]++] = sym;
            }
        }
      return num_symbols;
    }
}

int32_t
Huff_ConvertToRanges (HuffRange *range, int32_t num_symbols, int32_t P,
                      const u_int8_t *symlen, BitReader *bits)
{
  int32_t num_ranges = P >> 1, v, sym_idx = 0;

  // Start with space?
  if (P & 1)
    {
      BitReader_Refill (bits);
      v = *symlen++;
      if (v >= 8)
        return -1;
      sym_idx = BitReader_ReadBitsNoRefill (bits, v + 1) + (1 << (v + 1)) - 1;
    }
  int32_t syms_used = 0;

  for (int32_t i = 0; i < num_ranges; i++)
    {
      BitReader_Refill (bits);
      v = symlen[0];
      if (v >= 9)
        return -1;
      int32_t num = BitReader_ReadBitsNoRefillZero (bits, v) + (1 << v);
      v = symlen[1];
      if (v >= 8)
        return -1;
      int32_t space
          = BitReader_ReadBitsNoRefill (bits, v + 1) + (1 << (v + 1)) - 1;
      range[i].symbol = sym_idx;
      range[i].num = num;
      syms_used += num;
      sym_idx += num + space;
      symlen += 2;
    }

  if (sym_idx >= 256 || syms_used >= num_symbols
      || sym_idx + num_symbols - syms_used > 256)
    return -1;

  range[num_ranges].symbol = sym_idx;
  range[num_ranges].num = num_symbols - syms_used;

  return num_ranges + 1;
}

int32_t
Huff_ReadCodeLengthsNew (BitReader *bits, u_int8_t *syms,
                         u_int32_t *code_prefix)
{
  int32_t forced_bits = BitReader_ReadBitsNoRefill (bits, 2);

  int32_t num_symbols = BitReader_ReadBitsNoRefill (bits, 8) + 1;

  int32_t fluff = BitReader_ReadFluff (bits, num_symbols);

  u_int8_t code_len[512];
  BitReader2 br2;
  br2.bitpos = (bits->bitpos - 24) & 7;
  br2.p_end = bits->p_end;
  br2.p = bits->p - (unsigned)((24 - bits->bitpos + 7) >> 3);

  if (!DecodeGolombRiceLengths (code_len, num_symbols + fluff, &br2))
    return -1;
  memset (code_len + (num_symbols + fluff), 0, 16);
  if (!DecodeGolombRiceBits (code_len, num_symbols, forced_bits, &br2))
    return -1;

  // Reset the bits decoder.
  bits->bitpos = 24;
  bits->p = br2.p;
  bits->bits = 0;
  BitReader_Refill (bits);
  bits->bits <<= br2.bitpos;
  bits->bitpos += br2.bitpos;

  if (1)
    {
      u_int32_t running_sum = 0x1e;
      int32_t maxlen = 11;
      for (int32_t i = 0; i < num_symbols; i++)
        {
          int32_t v = code_len[i];
          v = -(int32_t)(v & 1) ^ (v >> 1);
          code_len[i] = v + (running_sum >> 2) + 1;
          if (code_len[i] < 1 || code_len[i] > 11)
            return -1;
          running_sum += v;
        }
    }
  else
    {
      // Ensure we don't read unknown data that could contaminate
      // max_codeword_len.
      __m128i bak = _mm_loadu_si128 ((__m128i *)&code_len[num_symbols]);
      _mm_storeu_si128 ((__m128i *)&code_len[num_symbols], _mm_set1_epi32 (0));
      // apply a filter
      __m128i avg = _mm_set1_epi8 (0x1e);
      __m128i ones = _mm_set1_epi8 (1);
      __m128i max_codeword_len = _mm_set1_epi8 (10);
      for (uint i = 0; i < num_symbols; i += 16)
        {
          __m128i v = _mm_loadu_si128 ((__m128i *)&code_len[i]), t;
          // avg[0..15] = avg[15]
          avg = _mm_unpackhi_epi8 (avg, avg);
          avg = _mm_unpackhi_epi8 (avg, avg);
          avg = _mm_shuffle_epi32 (avg, 255);
          // v = -(int32_t)(v & 1) ^ (v >> 1)
          v = _mm_xor_si128 (
              _mm_sub_epi8 (_mm_set1_epi8 (0), _mm_and_si128 (v, ones)),
              _mm_and_si128 (_mm_srli_epi16 (v, 1), _mm_set1_epi8 (0x7f)));
          // create all the sums. v[n] = v[0] + ... + v[n]
          t = _mm_add_epi8 (_mm_slli_si128 (v, 1), v);
          t = _mm_add_epi8 (_mm_slli_si128 (t, 2), t);
          t = _mm_add_epi8 (_mm_slli_si128 (t, 4), t);
          t = _mm_add_epi8 (_mm_slli_si128 (t, 8), t);
          // u[x] = (avg + t[x-1]) >> 2
          __m128i u = _mm_and_si128 (
              _mm_srli_epi16 (_mm_add_epi8 (_mm_slli_si128 (t, 1), avg), 2u),
              _mm_set1_epi8 (0x3f));
          // v += u
          v = _mm_add_epi8 (v, u);
          // avg += t
          avg = _mm_add_epi8 (avg, t);
          // max_codeword_len = max(max_codeword_len, v)
          max_codeword_len = _mm_max_epu8 (max_codeword_len, v);
          // mem[] = v+1
          _mm_storeu_si128 ((__m128i *)&code_len[i],
                            _mm_add_epi8 (v, _mm_set1_epi8 (1)));
        }
      _mm_storeu_si128 ((__m128i *)&code_len[num_symbols], bak);
      if (_mm_movemask_epi8 (
              _mm_cmpeq_epi8 (max_codeword_len, _mm_set1_epi8 (10)))
          != 0xffff)
        return -1; // codeword too big?
    }

  HuffRange range[128];
  int32_t ranges = Huff_ConvertToRanges (range, num_symbols, fluff,
                                         &code_len[num_symbols], bits);
  if (ranges <= 0)
    return -1;

  u_int8_t *cp = code_len;
  for (int32_t i = 0; i < ranges; i++)
    {
      int32_t sym = range[i].symbol;
      int32_t n = range[i].num;
      do
        {
          syms[code_prefix[*cp++]++] = sym++;
        }
      while (--n);
    }

  return num_symbols;
}

bool
Huff_MakeLut (const u_int32_t *prefix_org, const u_int32_t *prefix_cur,
              NewHuffLut *hufflut, u_int8_t *syms)
{
  u_int32_t currslot = 0;
  for (u_int32_t i = 1; i < 11; i++)
    {
      u_int32_t start = prefix_org[i];
      u_int32_t count = prefix_cur[i] - start;
      if (count)
        {
          u_int32_t stepsize = 1 << (11 - i);
          u_int32_t num_to_set = count << (11 - i);
          if (currslot + num_to_set > 2048)
            return false;
          FillByteOverflow16 (&hufflut->bits2len[currslot], i, num_to_set);

          u_int8_t *p = &hufflut->bits2sym[currslot];
          for (u_int32_t j = 0; j != count; j++, p += stepsize)
            FillByteOverflow16 (p, syms[start + j], stepsize);
          currslot += num_to_set;
        }
    }
  if (prefix_cur[11] - prefix_org[11] != 0)
    {
      u_int32_t num_to_set = prefix_cur[11] - prefix_org[11];
      if (currslot + num_to_set > 2048)
        return false;
      FillByteOverflow16 (&hufflut->bits2len[currslot], 11, num_to_set);
      memcpy (&hufflut->bits2sym[currslot], &syms[prefix_org[11]], num_to_set);
      currslot += num_to_set;
    }
  return currslot == 2048;
}
