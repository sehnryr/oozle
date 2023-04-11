#include "oozle/include/tans.h"

template <typename T>
void
SimpleSort (T *p, T *pend)
{
  if (p != pend)
    {
      for (T *lp = p + 1, *rp; lp != pend; lp++)
        {
          T t = lp[0];
          for (rp = lp; rp > p && t < rp[-1]; rp--)
            rp[0] = rp[-1];
          rp[0] = t;
        }
    }
}

bool
Tans_DecodeTable (BitReader *bits, int32_t L_bits, TansData *tans_data)
{
  BitReader_Refill (bits);
  if (BitReader_ReadBitNoRefill (bits))
    {
      int32_t Q = BitReader_ReadBitsNoRefill (bits, 3);
      int32_t num_symbols = BitReader_ReadBitsNoRefill (bits, 8) + 1;
      if (num_symbols < 2)
        return false;
      int32_t fluff = BitReader_ReadFluff (bits, num_symbols);
      int32_t total_rice_values = fluff + num_symbols;
      uint8_t rice[512 + 16];
      BitReader2 br2;

      // another bit reader...
      br2.p = bits->p - ((uint)(24 - bits->bitpos + 7) >> 3);
      br2.p_end = bits->p_end;
      br2.bitpos = (bits->bitpos - 24) & 7;

      if (!DecodeGolombRiceLengths (rice, total_rice_values, &br2))
        return false;
      memset (rice + total_rice_values, 0, 16);

      // Switch back to other bitreader impl
      bits->bitpos = 24;
      bits->p = br2.p;
      bits->bits = 0;
      BitReader_Refill (bits);
      bits->bits <<= br2.bitpos;
      bits->bitpos += br2.bitpos;

      HuffRange range[133];
      fluff = Huff_ConvertToRanges (range, num_symbols, fluff,
                                    &rice[num_symbols], bits);
      if (fluff < 0)
        return false;

      BitReader_Refill (bits);

      uint32_t L = 1 << L_bits;
      uint8_t *cur_rice_ptr = rice;
      int32_t average = 6;
      int32_t somesum = 0;
      uint8_t *tanstable_A = tans_data->A;
      uint32_t *tanstable_B = tans_data->B;

      for (int32_t ri = 0; ri < fluff; ri++)
        {
          int32_t symbol = range[ri].symbol;
          int32_t num = range[ri].num;
          do
            {
              BitReader_Refill (bits);

              int32_t nextra = Q + *cur_rice_ptr++;
              if (nextra > 15)
                return false;
              int32_t v = BitReader_ReadBitsNoRefillZero (bits, nextra)
                          + (1 << nextra) - (1 << Q);

              int32_t average_div4 = average >> 2;
              int32_t limit = 2 * average_div4;
              if (v <= limit)
                v = average_div4 + (-(v & 1) ^ ((uint32_t)v >> 1));
              if (limit > v)
                limit = v;
              v += 1;
              average += limit - average_div4;
              *tanstable_A = symbol;
              *tanstable_B = (symbol << 16) + v;
              tanstable_A += (v == 1);
              tanstable_B += v >= 2;
              somesum += v;
              symbol += 1;
            }
          while (--num);
        }
      tans_data->A_used = tanstable_A - tans_data->A;
      tans_data->B_used = tanstable_B - tans_data->B;
      if (somesum != L)
        return false;

      return true;
    }
  else
    {
      bool seen[256];
      memset (seen, 0, sizeof (seen));
      uint32_t L = 1 << L_bits;

      int32_t count = BitReader_ReadBitsNoRefill (bits, 3) + 1;

      int32_t bits_per_sym = BSR (L_bits) + 1;
      int32_t max_delta_bits = BitReader_ReadBitsNoRefill (bits, bits_per_sym);

      if (max_delta_bits == 0 || max_delta_bits > L_bits)
        return false;

      uint8_t *tanstable_A = tans_data->A;
      uint32_t *tanstable_B = tans_data->B;

      int32_t weight = 0;
      int32_t total_weights = 0;

      do
        {
          BitReader_Refill (bits);

          int32_t sym = BitReader_ReadBitsNoRefill (bits, 8);
          if (seen[sym])
            return false;

          int32_t delta = BitReader_ReadBitsNoRefill (bits, max_delta_bits);

          weight += delta;

          if (weight == 0)
            return false;

          seen[sym] = true;
          if (weight == 1)
            {
              *tanstable_A++ = sym;
            }
          else
            {
              *tanstable_B++ = (sym << 16) + weight;
            }

          total_weights += weight;
        }
      while (--count);

      BitReader_Refill (bits);

      int32_t sym = BitReader_ReadBitsNoRefill (bits, 8);
      if (seen[sym])
        return false;

      if (L - total_weights < weight || L - total_weights <= 1)
        return false;

      *tanstable_B++ = (sym << 16) + (L - total_weights);

      tans_data->A_used = tanstable_A - tans_data->A;
      tans_data->B_used = tanstable_B - tans_data->B;

      SimpleSort (tans_data->A, tanstable_A);
      SimpleSort (tans_data->B, tanstable_B);
      return true;
    }
}

void
Tans_InitLut (TansData *tans_data, int32_t L_bits, TansLutEnt *lut)
{
  TansLutEnt *pointers[4];

  int32_t L = 1 << L_bits;
  int32_t a_used = tans_data->A_used;

  uint32_t slots_left_to_alloc = L - a_used;

  uint32_t sa = slots_left_to_alloc >> 2;
  pointers[0] = lut;
  uint32_t sb = sa + ((slots_left_to_alloc & 3) > 0);
  pointers[1] = lut + sb;
  sb += sa + ((slots_left_to_alloc & 3) > 1);
  pointers[2] = lut + sb;
  sb += sa + ((slots_left_to_alloc & 3) > 2);
  pointers[3] = lut + sb;

  // Setup the single entrys with weight=1
  {
    TansLutEnt *lut_singles = lut + slots_left_to_alloc, le;
    le.w = 0;
    le.bits_x = L_bits;
    le.x = (1 << L_bits) - 1;
    for (int32_t i = 0; i < a_used; i++)
      {
        lut_singles[i] = le;
        lut_singles[i].symbol = tans_data->A[i];
      }
  }

  // Setup the entrys with weight >= 2
  int32_t weights_sum = 0;
  for (int32_t i = 0; i < tans_data->B_used; i++)
    {
      int32_t weight = tans_data->B[i] & 0xffff;
      int32_t symbol = tans_data->B[i] >> 16;
      if (weight > 4)
        {
          uint32_t sym_bits = BSR (weight);
          int32_t Z = L_bits - sym_bits;
          TansLutEnt le;
          le.symbol = symbol;
          le.bits_x = Z;
          le.x = (1 << Z) - 1;
          le.w = (L - 1) & (weight << Z);
          int32_t what_to_add = 1 << Z;
          int32_t X = (1 << (sym_bits + 1)) - weight;

          for (int32_t j = 0; j < 4; j++)
            {
              TansLutEnt *dst = pointers[j];

              int32_t Y = (weight + ((weights_sum - j - 1) & 3)) >> 2;
              if (X >= Y)
                {
                  for (int32_t n = Y; n; n--)
                    {
                      *dst++ = le;
                      le.w += what_to_add;
                    }
                  X -= Y;
                }
              else
                {
                  for (int32_t n = X; n; n--)
                    {
                      *dst++ = le;
                      le.w += what_to_add;
                    }
                  Z--;

                  what_to_add >>= 1;
                  le.bits_x = Z;
                  le.w = 0;
                  le.x >>= 1;
                  for (int32_t n = Y - X; n; n--)
                    {
                      *dst++ = le;
                      le.w += what_to_add;
                    }
                  X = weight;
                }
              pointers[j] = dst;
            }
        }
      else
        {
          assert (weight > 0);
          uint32_t bits = ((1 << weight) - 1) << (weights_sum & 3);
          bits |= (bits >> 4);
          int32_t n = weight, ww = weight;
          do
            {
              uint32_t idx = BSF (bits);
              bits &= bits - 1;
              TansLutEnt *dst = pointers[idx]++;
              dst->symbol = symbol;
              uint32_t weight_bits = BSR (ww);
              dst->bits_x = L_bits - weight_bits;
              dst->x = (1 << (L_bits - weight_bits)) - 1;
              dst->w = (L - 1) & (ww++ << (L_bits - weight_bits));
            }
          while (--n);
        }
      weights_sum += weight;
    }
}

bool
Tans_Decode (TansDecoderParams *params)
{
  TansLutEnt *lut = params->lut, *e;
  uint8_t *dst = params->dst, *dst_end = params->dst_end;
  const uint8_t *ptr_f = params->ptr_f, *ptr_b = params->ptr_b;
  uint32_t bits_f = params->bits_f, bits_b = params->bits_b;
  int32_t bitpos_f = params->bitpos_f, bitpos_b = params->bitpos_b;
  uint32_t state_0 = params->state_0, state_1 = params->state_1;
  uint32_t state_2 = params->state_2, state_3 = params->state_3;
  uint32_t state_4 = params->state_4;

  if (ptr_f > ptr_b)
    return false;

#define TANS_FORWARD_BITS()                                                   \
  bits_f |= *(uint32_t *)ptr_f << bitpos_f;                                  \
  ptr_f += (31 - bitpos_f) >> 3;                                              \
  bitpos_f |= 24;

#define TANS_FORWARD_ROUND(state)                                             \
  e = &lut[state];                                                            \
  *dst++ = e->symbol;                                                         \
  bitpos_f -= e->bits_x;                                                      \
  state = (bits_f & e->x) + e->w;                                             \
  bits_f >>= e->bits_x;                                                       \
  if (dst >= dst_end)                                                         \
    break;

#define TANS_BACKWARD_BITS()                                                  \
  bits_b |= _byteswap_ulong (((uint32_t *)ptr_b)[-1]) << bitpos_b;           \
  ptr_b -= (31 - bitpos_b) >> 3;                                              \
  bitpos_b |= 24;

#define TANS_BACKWARD_ROUND(state)                                            \
  e = &lut[state];                                                            \
  *dst++ = e->symbol;                                                         \
  bitpos_b -= e->bits_x;                                                      \
  state = (bits_b & e->x) + e->w;                                             \
  bits_b >>= e->bits_x;                                                       \
  if (dst >= dst_end)                                                         \
    break;

  if (dst < dst_end)
    {
      for (;;)
        {
          TANS_FORWARD_BITS ();
          TANS_FORWARD_ROUND (state_0);
          TANS_FORWARD_ROUND (state_1);
          TANS_FORWARD_BITS ();
          TANS_FORWARD_ROUND (state_2);
          TANS_FORWARD_ROUND (state_3);
          TANS_FORWARD_BITS ();
          TANS_FORWARD_ROUND (state_4);
          TANS_BACKWARD_BITS ();
          TANS_BACKWARD_ROUND (state_0);
          TANS_BACKWARD_ROUND (state_1);
          TANS_BACKWARD_BITS ();
          TANS_BACKWARD_ROUND (state_2);
          TANS_BACKWARD_ROUND (state_3);
          TANS_BACKWARD_BITS ();
          TANS_BACKWARD_ROUND (state_4);
        }
    }

  if (ptr_b - ptr_f + (bitpos_f >> 3) + (bitpos_b >> 3) != 0)
    return false;

  uint32_t states_or = state_0 | state_1 | state_2 | state_3 | state_4;
  if (states_or & ~0xFF)
    return false;

  dst_end[0] = (uint8_t)state_0;
  dst_end[1] = (uint8_t)state_1;
  dst_end[2] = (uint8_t)state_2;
  dst_end[3] = (uint8_t)state_3;
  dst_end[4] = (uint8_t)state_4;
  return true;
}
