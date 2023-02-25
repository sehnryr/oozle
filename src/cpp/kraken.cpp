#include "oozle/include/kraken.h"

// Unpacks the packed 8 bit offset and lengths into 32 bit.
bool
Kraken_UnpackOffsets (const u_int8_t *src, const u_int8_t *src_end,
                      const u_int8_t *packed_offs_stream,
                      const u_int8_t *packed_offs_stream_extra,
                      int32_t packed_offs_stream_size,
                      int32_t multi_dist_scale,
                      const u_int8_t *packed_litlen_stream,
                      int32_t packed_litlen_stream_size, int32_t *offs_stream,
                      int32_t *len_stream, bool excess_flag,
                      int32_t excess_bytes)
{
  BitReader bits_a, bits_b;
  int32_t n, i;
  int32_t u32_len_stream_size = 0;

  bits_a.bitpos = 24;
  bits_a.bits = 0;
  bits_a.p = src;
  bits_a.p_end = src_end;
  BitReader_Refill (&bits_a);

  bits_b.bitpos = 24;
  bits_b.bits = 0;
  bits_b.p = src_end;
  bits_b.p_end = src;
  BitReader_RefillBackwards (&bits_b);

  if (!excess_flag)
    {
      if (bits_b.bits < 0x2000)
        return false;
      n = 31 - BSR (bits_b.bits);
      bits_b.bitpos += n;
      bits_b.bits <<= n;
      BitReader_RefillBackwards (&bits_b);
      n++;
      u32_len_stream_size = (bits_b.bits >> (32 - n)) - 1;
      bits_b.bitpos += n;
      bits_b.bits <<= n;
      BitReader_RefillBackwards (&bits_b);
    }

  if (multi_dist_scale == 0)
    {
      // Traditional way of coding offsets
      const u_int8_t *packed_offs_stream_end
          = packed_offs_stream + packed_offs_stream_size;
      while (packed_offs_stream != packed_offs_stream_end)
        {
          *offs_stream++ = -(int32_t)BitReader_ReadDistance (
              &bits_a, *packed_offs_stream++);
          if (packed_offs_stream == packed_offs_stream_end)
            break;
          *offs_stream++ = -(int32_t)BitReader_ReadDistanceB (
              &bits_b, *packed_offs_stream++);
        }
    }
  else
    {
      // New way of coding offsets
      int32_t *offs_stream_org = offs_stream;
      const u_int8_t *packed_offs_stream_end
          = packed_offs_stream + packed_offs_stream_size;
      u_int32_t cmd, offs;
      while (packed_offs_stream != packed_offs_stream_end)
        {
          cmd = *packed_offs_stream++;
          if ((cmd >> 3) > 26)
            return 0;
          offs = ((8 + (cmd & 7)) << (cmd >> 3))
                 | BitReader_ReadMoreThan24Bits (&bits_a, (cmd >> 3));
          *offs_stream++ = 8 - (int32_t)offs;
          if (packed_offs_stream == packed_offs_stream_end)
            break;
          cmd = *packed_offs_stream++;
          if ((cmd >> 3) > 26)
            return 0;
          offs = ((8 + (cmd & 7)) << (cmd >> 3))
                 | BitReader_ReadMoreThan24BitsB (&bits_b, (cmd >> 3));
          *offs_stream++ = 8 - (int32_t)offs;
        }
      if (multi_dist_scale != 1)
        {
          CombineScaledOffsetArrays (
              offs_stream_org, offs_stream - offs_stream_org, multi_dist_scale,
              packed_offs_stream_extra);
        }
    }
  u_int32_t u32_len_stream_buf[512]; // max count is 128kb / 256 = 512
  if (u32_len_stream_size > 512)
    return false;

  u_int32_t *u32_len_stream = u32_len_stream_buf,
            *u32_len_stream_end = u32_len_stream_buf + u32_len_stream_size;
  for (i = 0; i + 1 < u32_len_stream_size; i += 2)
    {
      if (!BitReader_ReadLength (&bits_a, &u32_len_stream[i + 0]))
        return false;
      if (!BitReader_ReadLengthB (&bits_b, &u32_len_stream[i + 1]))
        return false;
    }
  if (i < u32_len_stream_size)
    {
      if (!BitReader_ReadLength (&bits_a, &u32_len_stream[i + 0]))
        return false;
    }

  bits_a.p -= (24 - bits_a.bitpos) >> 3;
  bits_b.p += (24 - bits_b.bitpos) >> 3;

  if (bits_a.p != bits_b.p)
    return false;

  for (i = 0; i < packed_litlen_stream_size; i++)
    {
      u_int32_t v = packed_litlen_stream[i];
      if (v == 255)
        v = *u32_len_stream++ + 255;
      len_stream[i] = v + 3;
    }
  if (u32_len_stream != u32_len_stream_end)
    return false;

  return true;
}

bool
Kraken_ReadLzTable (int32_t mode, const u_int8_t *src, const u_int8_t *src_end,
                    u_int8_t *dst, int32_t dst_size, int32_t offset,
                    u_int8_t *scratch, u_int8_t *scratch_end,
                    KrakenLzTable *lztable)
{
  u_int8_t *out;
  int32_t decode_count, n;
  u_int8_t *packed_offs_stream, *packed_len_stream;

  if (mode > 1)
    return false;

  if (src_end - src < 13)
    return false;

  if (offset == 0)
    {
      COPY_64 (dst, src);
      dst += 8;
      src += 8;
    }

  if (*src & 0x80)
    {
      u_int8_t flag = *src++;
      if ((flag & 0xc0) != 0x80)
        return false; // reserved flag set

      return false; // excess bytes not supported
    }

  // Disable no copy optimization if source and dest overlap
  bool force_copy = dst <= src_end && src <= dst + dst_size;

  // Decode lit stream, bounded by dst_size
  out = scratch;
  n = Oozle_DecodeBytes (&out, src, src_end, &decode_count,
                         MIN (scratch_end - scratch, dst_size), force_copy,
                         scratch, scratch_end);
  if (n < 0)
    return false;
  src += n;
  lztable->lit_stream = out;
  lztable->lit_stream_size = decode_count;
  scratch += decode_count;

  // Decode command stream, bounded by dst_size
  out = scratch;
  n = Oozle_DecodeBytes (&out, src, src_end, &decode_count,
                         MIN (scratch_end - scratch, dst_size), force_copy,
                         scratch, scratch_end);
  if (n < 0)
    return false;
  src += n;
  lztable->cmd_stream = out;
  lztable->cmd_stream_size = decode_count;
  scratch += decode_count;

  // Check if to decode the multistuff crap
  if (src_end - src < 3)
    return false;

  int32_t offs_scaling = 0;
  u_int8_t *packed_offs_stream_extra = NULL;

  if (src[0] & 0x80)
    {
      // uses the mode where distances are coded with 2 tables
      offs_scaling = src[0] - 127;
      src++;

      packed_offs_stream = scratch;
      n = Oozle_DecodeBytes (
          &packed_offs_stream, src, src_end, &lztable->offs_stream_size,
          MIN (scratch_end - scratch, lztable->cmd_stream_size), false,
          scratch, scratch_end);
      if (n < 0)
        return false;
      src += n;
      scratch += lztable->offs_stream_size;

      if (offs_scaling != 1)
        {
          packed_offs_stream_extra = scratch;
          n = Oozle_DecodeBytes (
              &packed_offs_stream_extra, src, src_end, &decode_count,
              MIN (scratch_end - scratch, lztable->offs_stream_size), false,
              scratch, scratch_end);
          if (n < 0 || decode_count != lztable->offs_stream_size)
            return false;
          src += n;
          scratch += decode_count;
        }
    }
  else
    {
      // Decode packed offset stream, it's bounded by the command length.
      packed_offs_stream = scratch;
      n = Oozle_DecodeBytes (
          &packed_offs_stream, src, src_end, &lztable->offs_stream_size,
          MIN (scratch_end - scratch, lztable->cmd_stream_size), false,
          scratch, scratch_end);
      if (n < 0)
        return false;
      src += n;
      scratch += lztable->offs_stream_size;
    }

  // Decode packed litlen stream. It's bounded by 1/4 of dst_size.
  packed_len_stream = scratch;
  n = Oozle_DecodeBytes (
      &packed_len_stream, src, src_end, &lztable->len_stream_size,
      MIN (scratch_end - scratch, dst_size >> 2), false, scratch, scratch_end);
  if (n < 0)
    return false;
  src += n;
  scratch += lztable->len_stream_size;

  // Reserve memory for final dist stream
  scratch = ALIGN_POINTER (scratch, 16);
  lztable->offs_stream = (int32_t *)scratch;
  scratch += lztable->offs_stream_size * 4;

  // Reserve memory for final len stream
  scratch = ALIGN_POINTER (scratch, 16);
  lztable->len_stream = (int32_t *)scratch;
  scratch += lztable->len_stream_size * 4;

  if (scratch + 64 > scratch_end)
    return false;

  return Kraken_UnpackOffsets (
      src, src_end, packed_offs_stream, packed_offs_stream_extra,
      lztable->offs_stream_size, offs_scaling, packed_len_stream,
      lztable->len_stream_size, lztable->offs_stream, lztable->len_stream, 0,
      0);
}

// Note: may access memory out of bounds on invalid input.
bool
Kraken_ProcessLzRuns_Type0 (KrakenLzTable *lzt, u_int8_t *dst,
                            u_int8_t *dst_end, u_int8_t *dst_start)
{
  const u_int8_t *cmd_stream = lzt->cmd_stream,
                 *cmd_stream_end = cmd_stream + lzt->cmd_stream_size;
  const int32_t *len_stream = lzt->len_stream;
  const int32_t *len_stream_end = lzt->len_stream + lzt->len_stream_size;
  const u_int8_t *lit_stream = lzt->lit_stream;
  const u_int8_t *lit_stream_end = lzt->lit_stream + lzt->lit_stream_size;
  const int32_t *offs_stream = lzt->offs_stream;
  const int32_t *offs_stream_end = lzt->offs_stream + lzt->offs_stream_size;
  const u_int8_t *copyfrom;
  u_int32_t final_len;
  int32_t offset;
  int32_t recent_offs[7];
  int32_t last_offset;

  recent_offs[3] = -8;
  recent_offs[4] = -8;
  recent_offs[5] = -8;
  last_offset = -8;

  while (cmd_stream < cmd_stream_end)
    {
      u_int32_t f = *cmd_stream++;
      u_int32_t litlen = f & 3;
      u_int32_t offs_index = f >> 6;
      u_int32_t matchlen = (f >> 2) & 0xF;

      // use cmov
      u_int32_t next_long_length = *len_stream;
      const int32_t *next_len_stream = len_stream + 1;

      len_stream = (litlen == 3) ? next_len_stream : len_stream;
      litlen = (litlen == 3) ? next_long_length : litlen;
      recent_offs[6] = *offs_stream;

      COPY_64_ADD (dst, lit_stream, &dst[last_offset]);
      if (litlen > 8)
        {
          COPY_64_ADD (dst + 8, lit_stream + 8, &dst[last_offset + 8]);
          if (litlen > 16)
            {
              COPY_64_ADD (dst + 16, lit_stream + 16, &dst[last_offset + 16]);
              if (litlen > 24)
                {
                  do
                    {
                      COPY_64_ADD (dst + 24, lit_stream + 24,
                                   &dst[last_offset + 24]);
                      litlen -= 8;
                      dst += 8;
                      lit_stream += 8;
                    }
                  while (litlen > 24);
                }
            }
        }
      dst += litlen;
      lit_stream += litlen;

      offset = recent_offs[offs_index + 3];
      recent_offs[offs_index + 3] = recent_offs[offs_index + 2];
      recent_offs[offs_index + 2] = recent_offs[offs_index + 1];
      recent_offs[offs_index + 1] = recent_offs[offs_index + 0];
      recent_offs[3] = offset;
      last_offset = offset;

      offs_stream
          = (int32_t *)((intptr_t)offs_stream + ((offs_index + 1) & 4));

      if ((uintptr_t)offset < (uintptr_t)(dst_start - dst))
        return false; // offset out of bounds

      copyfrom = dst + offset;
      if (matchlen != 15)
        {
          COPY_64 (dst, copyfrom);
          COPY_64 (dst + 8, copyfrom + 8);
          dst += matchlen + 2;
        }
      else
        {
          matchlen = 14 + *len_stream++; // why is the value not 16 here, the
                                         // above case copies up to 16 bytes.
          if ((uintptr_t)matchlen > (uintptr_t)(dst_end - dst))
            return false; // copy length out of bounds
          COPY_64 (dst, copyfrom);
          COPY_64 (dst + 8, copyfrom + 8);
          COPY_64 (dst + 16, copyfrom + 16);
          do
            {
              COPY_64 (dst + 24, copyfrom + 24);
              matchlen -= 8;
              dst += 8;
              copyfrom += 8;
            }
          while (matchlen > 24);
          dst += matchlen;
        }
    }

  // check for incorrect input
  if (offs_stream != offs_stream_end || len_stream != len_stream_end)
    return false;

  final_len = dst_end - dst;
  if (final_len != lit_stream_end - lit_stream)
    return false;

  if (final_len >= 8)
    {
      do
        {
          COPY_64_ADD (dst, lit_stream, &dst[last_offset]);
          dst += 8, lit_stream += 8, final_len -= 8;
        }
      while (final_len >= 8);
    }
  if (final_len > 0)
    {
      do
        {
          *dst = *lit_stream++ + dst[last_offset];
        }
      while (dst++, --final_len);
    }
  return true;
}

// Note: may access memory out of bounds on invalid input.
bool
Kraken_ProcessLzRuns_Type1 (KrakenLzTable *lzt, u_int8_t *dst,
                            u_int8_t *dst_end, u_int8_t *dst_start)
{
  const u_int8_t *cmd_stream = lzt->cmd_stream,
                 *cmd_stream_end = cmd_stream + lzt->cmd_stream_size;
  const int32_t *len_stream = lzt->len_stream;
  const int32_t *len_stream_end = lzt->len_stream + lzt->len_stream_size;
  const u_int8_t *lit_stream = lzt->lit_stream;
  const u_int8_t *lit_stream_end = lzt->lit_stream + lzt->lit_stream_size;
  const int32_t *offs_stream = lzt->offs_stream;
  const int32_t *offs_stream_end = lzt->offs_stream + lzt->offs_stream_size;
  const u_int8_t *copyfrom;
  u_int32_t final_len;
  int32_t offset;
  int32_t recent_offs[7];

  recent_offs[3] = -8;
  recent_offs[4] = -8;
  recent_offs[5] = -8;

  while (cmd_stream < cmd_stream_end)
    {
      u_int32_t f = *cmd_stream++;
      u_int32_t litlen = f & 3;
      u_int32_t offs_index = f >> 6;
      u_int32_t matchlen = (f >> 2) & 0xF;

      // use cmov
      u_int32_t next_long_length = *len_stream;
      const int32_t *next_len_stream = len_stream + 1;

      len_stream = (litlen == 3) ? next_len_stream : len_stream;
      litlen = (litlen == 3) ? next_long_length : litlen;
      recent_offs[6] = *offs_stream;

      COPY_64 (dst, lit_stream);
      if (litlen > 8)
        {
          COPY_64 (dst + 8, lit_stream + 8);
          if (litlen > 16)
            {
              COPY_64 (dst + 16, lit_stream + 16);
              if (litlen > 24)
                {
                  do
                    {
                      COPY_64 (dst + 24, lit_stream + 24);
                      litlen -= 8;
                      dst += 8;
                      lit_stream += 8;
                    }
                  while (litlen > 24);
                }
            }
        }
      dst += litlen;
      lit_stream += litlen;

      offset = recent_offs[offs_index + 3];
      recent_offs[offs_index + 3] = recent_offs[offs_index + 2];
      recent_offs[offs_index + 2] = recent_offs[offs_index + 1];
      recent_offs[offs_index + 1] = recent_offs[offs_index + 0];
      recent_offs[3] = offset;

      offs_stream
          = (int32_t *)((intptr_t)offs_stream + ((offs_index + 1) & 4));

      if ((uintptr_t)offset < (uintptr_t)(dst_start - dst))
        return false; // offset out of bounds

      copyfrom = dst + offset;
      if (matchlen != 15)
        {
          COPY_64 (dst, copyfrom);
          COPY_64 (dst + 8, copyfrom + 8);
          dst += matchlen + 2;
        }
      else
        {
          matchlen = 14 + *len_stream++; // why is the value not 16 here, the
                                         // above case copies up to 16 bytes.
          if ((uintptr_t)matchlen > (uintptr_t)(dst_end - dst))
            return false; // copy length out of bounds
          COPY_64 (dst, copyfrom);
          COPY_64 (dst + 8, copyfrom + 8);
          COPY_64 (dst + 16, copyfrom + 16);
          while (matchlen > 24)
            {
              COPY_64 (dst + 24, copyfrom + 24);
              matchlen -= 8;
              dst += 8;
              copyfrom += 8;
            }
          dst += matchlen;
        }
    }

  // check for incorrect input
  if (offs_stream != offs_stream_end || len_stream != len_stream_end)
    return false;

  final_len = dst_end - dst;
  if (final_len != lit_stream_end - lit_stream)
    return false;

  if (final_len >= 64)
    {
      do
        {
          COPY_64_BYTES (dst, lit_stream);
          dst += 64, lit_stream += 64, final_len -= 64;
        }
      while (final_len >= 64);
    }
  if (final_len >= 8)
    {
      do
        {
          COPY_64 (dst, lit_stream);
          dst += 8, lit_stream += 8, final_len -= 8;
        }
      while (final_len >= 8);
    }
  if (final_len > 0)
    {
      do
        {
          *dst++ = *lit_stream++;
        }
      while (--final_len);
    }
  return true;
}

bool
Kraken_ProcessLzRuns (int32_t mode, u_int8_t *dst, int32_t dst_size,
                      int32_t offset, KrakenLzTable *lztable)
{
  u_int8_t *dst_end = dst + dst_size;

  if (mode == 1)
    return Kraken_ProcessLzRuns_Type1 (lztable, dst + (offset == 0 ? 8 : 0),
                                       dst_end, dst - offset);

  if (mode == 0)
    return Kraken_ProcessLzRuns_Type0 (lztable, dst + (offset == 0 ? 8 : 0),
                                       dst_end, dst - offset);

  return false;
}

// Decode one 256kb big quantum block. It's divided into two 128k blocks
// internally that are compressed separately but with a shared history.
int32_t
Kraken_DecodeQuantum (u_int8_t *dst, u_int8_t *dst_end, u_int8_t *dst_start,
                      const u_int8_t *src, const u_int8_t *src_end,
                      u_int8_t *scratch, u_int8_t *scratch_end)
{
  const u_int8_t *src_in = src;
  int32_t mode, chunkhdr, dst_count, src_used, written_bytes;

  while (dst_end - dst != 0)
    {
      dst_count = dst_end - dst;
      if (dst_count > 0x20000)
        dst_count = 0x20000;
      if (src_end - src < 4)
        return -1;
      chunkhdr = src[2] | src[1] << 8 | src[0] << 16;
      if (!(chunkhdr & 0x800000))
        {
          // Stored as entropy without any match copying.
          u_int8_t *out = dst;
          src_used
              = Oozle_DecodeBytes (&out, src, src_end, &written_bytes,
                                   dst_count, false, scratch, scratch_end);
          if (src_used < 0 || written_bytes != dst_count)
            return -1;
        }
      else
        {
          src += 3;
          src_used = chunkhdr & 0x7FFFF;
          mode = (chunkhdr >> 19) & 0xF;
          if (src_end - src < src_used)
            return -1;
          if (src_used < dst_count)
            {
              size_t scratch_usage
                  = MIN (MIN (3 * dst_count + 32 + 0xd000, 0x6C000),
                         scratch_end - scratch);
              if (scratch_usage < sizeof (KrakenLzTable))
                return -1;
              if (!Kraken_ReadLzTable (
                      mode, src, src + src_used, dst, dst_count,
                      dst - dst_start, scratch + sizeof (KrakenLzTable),
                      scratch + scratch_usage, (KrakenLzTable *)scratch))
                return -1;
              if (!Kraken_ProcessLzRuns (mode, dst, dst_count, dst - dst_start,
                                         (KrakenLzTable *)scratch))
                return -1;
            }
          else if (src_used > dst_count || mode != 0)
            {
              return -1;
            }
          else
            {
              memmove (dst, src, dst_count);
            }
        }
      src += src_used;
      dst += dst_count;
    }
  return src - src_in;
}
