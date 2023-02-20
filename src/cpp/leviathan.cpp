#include "oozle/include/leviathan.h"

bool
Leviathan_ReadLzTable (int32_t chunk_type, const u_int8_t *src,
                       const u_int8_t *src_end, u_int8_t *dst,
                       int32_t dst_size, int32_t offset, u_int8_t *scratch,
                       u_int8_t *scratch_end, LeviathanLzTable *lztable)
{
  u_int8_t *packed_offs_stream, *packed_len_stream, *out;
  int32_t decode_count, n;

  if (chunk_type > 5)
    return false;

  if (src_end - src < 13)
    return false;

  if (offset == 0)
    {
      COPY_64 (dst, src);
      dst += 8;
      src += 8;
    }

  int32_t offs_scaling = 0;
  u_int8_t *packed_offs_stream_extra = NULL;

  int32_t offs_stream_limit = dst_size / 3;

  if (!(src[0] & 0x80))
    {
      // Decode packed offset stream, it's bounded by the command length.
      packed_offs_stream = scratch;
      n = Oozle_DecodeBytes (&packed_offs_stream, src, src_end,
                             &lztable->offs_stream_size,
                             MIN (scratch_end - scratch, offs_stream_limit),
                             false, scratch, scratch_end);
      if (n < 0)
        return false;
      src += n;
      scratch += lztable->offs_stream_size;
    }
  else
    {
      // uses the mode where distances are coded with 2 tables
      // and the transformation offs * scaling + low_bits
      offs_scaling = src[0] - 127;
      src++;

      packed_offs_stream = scratch;
      n = Oozle_DecodeBytes (&packed_offs_stream, src, src_end,
                             &lztable->offs_stream_size,
                             MIN (scratch_end - scratch, offs_stream_limit),
                             false, scratch, scratch_end);
      if (n < 0)
        return false;
      src += n;
      scratch += lztable->offs_stream_size;

      if (offs_scaling != 1)
        {
          packed_offs_stream_extra = scratch;
          n = Oozle_DecodeBytes (
              &packed_offs_stream_extra, src, src_end, &decode_count,
              MIN (scratch_end - scratch, offs_stream_limit), false, scratch,
              scratch_end);
          if (n < 0 || decode_count != lztable->offs_stream_size)
            return false;
          src += n;
          scratch += decode_count;
        }
    }

  // Decode packed litlen stream. It's bounded by 1/5 of dst_size.
  packed_len_stream = scratch;
  n = Oozle_DecodeBytes (
      &packed_len_stream, src, src_end, &lztable->len_stream_size,
      MIN (scratch_end - scratch, dst_size / 5), false, scratch, scratch_end);
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

  if (scratch > scratch_end)
    return false;

  if (chunk_type <= 1)
    {
      // Decode lit stream, bounded by dst_size
      out = scratch;
      n = Oozle_DecodeBytes (&out, src, src_end, &decode_count,
                             MIN (scratch_end - scratch, dst_size), true,
                             scratch, scratch_end);
      if (n < 0)
        return false;
      src += n;
      lztable->lit_stream[0] = out;
      lztable->lit_stream_size[0] = decode_count;
    }
  else
    {
      int32_t array_count = (chunk_type == 2) ? 2 : (chunk_type == 3) ? 4 : 16;
      n = Oozle_DecodeMultiArray (src, src_end, scratch, scratch_end,
                                  lztable->lit_stream,
                                  lztable->lit_stream_size, array_count,
                                  &decode_count, true, scratch, scratch_end);
      if (n < 0)
        return false;
      src += n;
    }
  scratch += decode_count;
  lztable->lit_stream_total = decode_count;

  if (src >= src_end)
    return false;

  if (!(src[0] & 0x80))
    {
      // Decode command stream, bounded by dst_size
      out = scratch;
      n = Oozle_DecodeBytes (&out, src, src_end, &decode_count,
                             MIN (scratch_end - scratch, dst_size), true,
                             scratch, scratch_end);
      if (n < 0)
        return false;
      src += n;
      lztable->cmd_stream = out;
      lztable->cmd_stream_size = decode_count;
      scratch += decode_count;
    }
  else
    {
      if (src[0] != 0x83)
        return false;
      src++;
      int32_t multi_cmd_lens[8];
      n = Oozle_DecodeMultiArray (src, src_end, scratch, scratch_end,
                                  lztable->multi_cmd_ptr, multi_cmd_lens, 8,
                                  &decode_count, true, scratch, scratch_end);
      if (n < 0)
        return false;
      src += n;
      for (size_t i = 0; i < 8; i++)
        lztable->multi_cmd_end[i]
            = lztable->multi_cmd_ptr[i] + multi_cmd_lens[i];

      lztable->cmd_stream = NULL;
      lztable->cmd_stream_size = decode_count;
      scratch += decode_count;
    }

  if (dst_size > scratch_end - scratch)
    return false;

  return Kraken_UnpackOffsets (
      src, src_end, packed_offs_stream, packed_offs_stream_extra,
      lztable->offs_stream_size, offs_scaling, packed_len_stream,
      lztable->len_stream_size, lztable->offs_stream, lztable->len_stream, 0,
      0);
}

#define finline __forceinline

struct LeviathanModeRaw
{
  const u_int8_t *lit_stream;

  finline
  LeviathanModeRaw (LeviathanLzTable *lzt, u_int8_t *dst_start)
      : lit_stream (lzt->lit_stream[0])
  {
  }

  finline bool
  CopyLiterals (u_int32_t cmd, u_int8_t *&dst, const int32_t *&len_stream,
                u_int8_t *match_zone_end, size_t last_offset)
  {
    u_int32_t litlen = (cmd >> 3) & 3;
    // use cmov
    u_int32_t len_stream_value = *len_stream & 0xffffff;
    const int32_t *next_len_stream = len_stream + 1;
    len_stream = (litlen == 3) ? next_len_stream : len_stream;
    litlen = (litlen == 3) ? len_stream_value : litlen;
    COPY_64 (dst, lit_stream);
    if (litlen > 8)
      {
        COPY_64 (dst + 8, lit_stream + 8);
        if (litlen > 16)
          {
            COPY_64 (dst + 16, lit_stream + 16);
            if (litlen > 24)
              {
                if (litlen > match_zone_end - dst)
                  return false; // out of bounds
                do
                  {
                    COPY_64 (dst + 24, lit_stream + 24);
                    litlen -= 8, dst += 8, lit_stream += 8;
                  }
                while (litlen > 24);
              }
          }
      }
    dst += litlen;
    lit_stream += litlen;
    return true;
  }

  finline void
  CopyFinalLiterals (u_int32_t final_len, u_int8_t *&dst, size_t last_offset)
  {
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
  }
};

struct LeviathanModeSub
{
  const u_int8_t *lit_stream;

  finline
  LeviathanModeSub (LeviathanLzTable *lzt, u_int8_t *dst_start)
      : lit_stream (lzt->lit_stream[0])
  {
  }

  finline bool
  CopyLiterals (u_int32_t cmd, u_int8_t *&dst, const int32_t *&len_stream,
                u_int8_t *match_zone_end, size_t last_offset)
  {
    u_int32_t litlen = (cmd >> 3) & 3;
    // use cmov
    u_int32_t len_stream_value = *len_stream & 0xffffff;
    const int32_t *next_len_stream = len_stream + 1;
    len_stream = (litlen == 3) ? next_len_stream : len_stream;
    litlen = (litlen == 3) ? len_stream_value : litlen;
    COPY_64_ADD (dst, lit_stream, &dst[last_offset]);
    if (litlen > 8)
      {
        COPY_64_ADD (dst + 8, lit_stream + 8, &dst[last_offset + 8]);
        if (litlen > 16)
          {
            COPY_64_ADD (dst + 16, lit_stream + 16, &dst[last_offset + 16]);
            if (litlen > 24)
              {
                if (litlen > match_zone_end - dst)
                  return false; // out of bounds
                do
                  {
                    COPY_64_ADD (dst + 24, lit_stream + 24,
                                 &dst[last_offset + 24]);
                    litlen -= 8, dst += 8, lit_stream += 8;
                  }
                while (litlen > 24);
              }
          }
      }
    dst += litlen;
    lit_stream += litlen;
    return true;
  }

  finline void
  CopyFinalLiterals (u_int32_t final_len, u_int8_t *&dst, size_t last_offset)
  {
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
  }
};

struct LeviathanModeLamSub
{
  const u_int8_t *lit_stream, *lam_lit_stream;

  finline
  LeviathanModeLamSub (LeviathanLzTable *lzt, u_int8_t *dst_start)
      : lit_stream (lzt->lit_stream[0]), lam_lit_stream (lzt->lit_stream[1])
  {
  }

  finline bool
  CopyLiterals (u_int32_t cmd, u_int8_t *&dst, const int32_t *&len_stream,
                u_int8_t *match_zone_end, size_t last_offset)
  {
    u_int32_t lit_cmd = cmd & 0x18;
    if (!lit_cmd)
      return true;

    u_int32_t litlen = lit_cmd >> 3;
    // use cmov
    u_int32_t len_stream_value = *len_stream & 0xffffff;
    const int32_t *next_len_stream = len_stream + 1;
    len_stream = (litlen == 3) ? next_len_stream : len_stream;
    litlen = (litlen == 3) ? len_stream_value : litlen;

    if (litlen-- == 0)
      return false; // lamsub mode requires one literal

    dst[0] = *lam_lit_stream++ + dst[last_offset], dst++;

    COPY_64_ADD (dst, lit_stream, &dst[last_offset]);
    if (litlen > 8)
      {
        COPY_64_ADD (dst + 8, lit_stream + 8, &dst[last_offset + 8]);
        if (litlen > 16)
          {
            COPY_64_ADD (dst + 16, lit_stream + 16, &dst[last_offset + 16]);
            if (litlen > 24)
              {
                if (litlen > match_zone_end - dst)
                  return false; // out of bounds
                do
                  {
                    COPY_64_ADD (dst + 24, lit_stream + 24,
                                 &dst[last_offset + 24]);
                    litlen -= 8, dst += 8, lit_stream += 8;
                  }
                while (litlen > 24);
              }
          }
      }
    dst += litlen;
    lit_stream += litlen;
    return true;
  }

  finline void
  CopyFinalLiterals (u_int32_t final_len, u_int8_t *&dst, size_t last_offset)
  {
    dst[0] = *lam_lit_stream++ + dst[last_offset], dst++;
    final_len -= 1;

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
  }
};

struct LeviathanModeSubAnd3
{
  enum
  {
    NUM = 4,
    MASK = NUM - 1
  };
  const u_int8_t *lit_stream[NUM];

  finline
  LeviathanModeSubAnd3 (LeviathanLzTable *lzt, u_int8_t *dst_start)
  {
    for (size_t i = 0; i != NUM; i++)
      lit_stream[i] = lzt->lit_stream[(-(intptr_t)dst_start + i) & MASK];
  }
  finline bool
  CopyLiterals (u_int32_t cmd, u_int8_t *&dst, const int32_t *&len_stream,
                u_int8_t *match_zone_end, size_t last_offset)
  {
    u_int32_t lit_cmd = cmd & 0x18;

    if (lit_cmd == 0x18)
      {
        u_int32_t litlen = *len_stream++ & 0xffffff;
        if (litlen > match_zone_end - dst)
          return false;
        while (litlen)
          {
            *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
            dst++, litlen--;
          }
      }
    else if (lit_cmd)
      {
        *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
        dst++;
        if (lit_cmd == 0x10)
          {
            *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
            dst++;
          }
      }
    return true;
  }

  finline void
  CopyFinalLiterals (u_int32_t final_len, u_int8_t *&dst, size_t last_offset)
  {
    if (final_len > 0)
      {
        do
          {
            *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
          }
        while (dst++, --final_len);
      }
  }
};

struct LeviathanModeSubAndF
{
  enum
  {
    NUM = 16,
    MASK = NUM - 1
  };
  const u_int8_t *lit_stream[NUM];

  finline
  LeviathanModeSubAndF (LeviathanLzTable *lzt, u_int8_t *dst_start)
  {
    for (size_t i = 0; i != NUM; i++)
      lit_stream[i] = lzt->lit_stream[(-(intptr_t)dst_start + i) & MASK];
  }
  finline bool
  CopyLiterals (u_int32_t cmd, u_int8_t *&dst, const int32_t *&len_stream,
                u_int8_t *match_zone_end, size_t last_offset)
  {
    u_int32_t lit_cmd = cmd & 0x18;

    if (lit_cmd == 0x18)
      {
        u_int32_t litlen = *len_stream++ & 0xffffff;
        if (litlen > match_zone_end - dst)
          return false;
        while (litlen)
          {
            *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
            dst++, litlen--;
          }
      }
    else if (lit_cmd)
      {
        *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
        dst++;
        if (lit_cmd == 0x10)
          {
            *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
            dst++;
          }
      }
    return true;
  }

  finline void
  CopyFinalLiterals (u_int32_t final_len, u_int8_t *&dst, size_t last_offset)
  {
    if (final_len > 0)
      {
        do
          {
            *dst = *lit_stream[(uintptr_t)dst & MASK]++ + dst[last_offset];
          }
        while (dst++, --final_len);
      }
  }
};

struct LeviathanModeO1
{
  const u_int8_t *lit_streams[16];
  u_int8_t next_lit[16];

  finline
  LeviathanModeO1 (LeviathanLzTable *lzt, u_int8_t *dst_start)
  {
    for (size_t i = 0; i != 16; i++)
      {
        u_int8_t *p = lzt->lit_stream[i];
        next_lit[i] = *p;
        lit_streams[i] = p + 1;
      }
  }

  finline bool
  CopyLiterals (u_int32_t cmd, u_int8_t *&dst, const int32_t *&len_stream,
                u_int8_t *match_zone_end, size_t last_offset)
  {
    u_int32_t lit_cmd = cmd & 0x18;

    if (lit_cmd == 0x18)
      {
        u_int32_t litlen = *len_stream++;
        if ((int32_t)litlen <= 0)
          return false;
        u_int32_t context = dst[-1];
        do
          {
            size_t slot = context >> 4;
            *dst++ = (context = next_lit[slot]);
            next_lit[slot] = *lit_streams[slot]++;
          }
        while (--litlen);
      }
    else if (lit_cmd)
      {
        // either 1 or 2
        u_int32_t context = dst[-1];
        size_t slot = context >> 4;
        *dst++ = (context = next_lit[slot]);
        next_lit[slot] = *lit_streams[slot]++;
        if (lit_cmd == 0x10)
          {
            slot = context >> 4;
            *dst++ = (context = next_lit[slot]);
            next_lit[slot] = *lit_streams[slot]++;
          }
      }
    return true;
  }

  finline void
  CopyFinalLiterals (u_int32_t final_len, u_int8_t *&dst, size_t last_offset)
  {
    u_int32_t context = dst[-1];
    while (final_len)
      {
        size_t slot = context >> 4;
        *dst++ = (context = next_lit[slot]);
        next_lit[slot] = *lit_streams[slot]++;
        final_len--;
      }
  }
};

template <typename Mode, bool MultiCmd>
bool
Leviathan_ProcessLz (LeviathanLzTable *lzt, u_int8_t *dst, u_int8_t *dst_start,
                     u_int8_t *dst_end, u_int8_t *window_base)
{
  const u_int8_t *cmd_stream = lzt->cmd_stream,
                 *cmd_stream_end = cmd_stream + lzt->cmd_stream_size;
  const int32_t *len_stream = lzt->len_stream;
  const int32_t *len_stream_end = len_stream + lzt->len_stream_size;

  const int32_t *offs_stream = lzt->offs_stream;
  const int32_t *offs_stream_end = offs_stream + lzt->offs_stream_size;
  const u_int8_t *copyfrom;
  u_int8_t *match_zone_end
      = (dst_end - dst_start >= 16) ? dst_end - 16 : dst_start;

  int32_t recent_offs[16];
  recent_offs[8] = recent_offs[9] = recent_offs[10] = recent_offs[11] = -8;
  recent_offs[12] = recent_offs[13] = recent_offs[14] = -8;

  size_t offset = -8;

  Mode mode (lzt, dst_start);

  u_int32_t cmd_stream_left;
  const u_int8_t *multi_cmd_stream[8], **cmd_stream_ptr;
  if (MultiCmd)
    {
      for (size_t i = 0; i != 8; i++)
        multi_cmd_stream[i]
            = lzt->multi_cmd_ptr[(i - (uintptr_t)dst_start) & 7];
      cmd_stream_left = lzt->cmd_stream_size;
      cmd_stream_ptr = &multi_cmd_stream[(uintptr_t)dst & 7];
      cmd_stream = *cmd_stream_ptr;
    }

  for (;;)
    {
      u_int32_t cmd;

      if (!MultiCmd)
        {
          if (cmd_stream >= cmd_stream_end)
            break;
          cmd = *cmd_stream++;
        }
      else
        {
          if (cmd_stream_left == 0)
            break;
          cmd_stream_left--;
          cmd = *cmd_stream;
          *cmd_stream_ptr = cmd_stream + 1;
        }

      u_int32_t offs_index = cmd >> 5;
      u_int32_t matchlen = (cmd & 7) + 2;

      recent_offs[15] = *offs_stream;

      if (!mode.CopyLiterals (cmd, dst, len_stream, match_zone_end, offset))
        return false;

      offset = recent_offs[(size_t)offs_index + 8];

      // Permute the recent offsets table
      __m128i temp = _mm_loadu_si128 (
          (const __m128i *)&recent_offs[(size_t)offs_index + 4]);
      _mm_storeu_si128 (
          (__m128i *)&recent_offs[(size_t)offs_index + 1],
          _mm_loadu_si128 ((const __m128i *)&recent_offs[offs_index]));
      _mm_storeu_si128 ((__m128i *)&recent_offs[(size_t)offs_index + 5], temp);
      recent_offs[8] = (int32_t)offset;
      offs_stream += offs_index == 7;

      if ((uintptr_t)offset < (uintptr_t)(window_base - dst))
        return false; // offset out of bounds
      copyfrom = dst + offset;

      if (matchlen == 9)
        {
          if (len_stream >= len_stream_end)
            return false; // len stream empty
          matchlen = *--len_stream_end + 6;
          COPY_64 (dst, copyfrom);
          COPY_64 (dst + 8, copyfrom + 8);
          u_int8_t *next_dst = dst + matchlen;
          if (MultiCmd)
            cmd_stream = *(cmd_stream_ptr
                           = &multi_cmd_stream[(uintptr_t)next_dst & 7]);
          if (matchlen > 16)
            {
              if (matchlen > (uintptr_t)(dst_end - 8 - dst))
                return false; // no space in buf
              COPY_64 (dst + 16, copyfrom + 16);
              do
                {
                  COPY_64 (dst + 24, copyfrom + 24);
                  matchlen -= 8;
                  dst += 8;
                  copyfrom += 8;
                }
              while (matchlen > 24);
            }
          dst = next_dst;
        }
      else
        {
          COPY_64 (dst, copyfrom);
          dst += matchlen;
          if (MultiCmd)
            cmd_stream
                = *(cmd_stream_ptr = &multi_cmd_stream[(uintptr_t)dst & 7]);
        }
    }

  // check for incorrect input
  if (offs_stream != offs_stream_end || len_stream != len_stream_end)
    return false;

  // copy final literals
  if (dst < dst_end)
    {
      mode.CopyFinalLiterals (dst_end - dst, dst, offset);
    }
  else if (dst != dst_end)
    {
      return false;
    }
  return true;
}

bool
Leviathan_ProcessLzRuns (int32_t chunk_type, u_int8_t *dst, int32_t dst_size,
                         int32_t offset, LeviathanLzTable *lzt)
{
  u_int8_t *dst_cur = dst + (offset == 0 ? 8 : 0);
  u_int8_t *dst_end = dst + dst_size;
  u_int8_t *dst_start = dst - offset;

  if (lzt->cmd_stream != NULL)
    {
      // single cmd mode
      switch (chunk_type)
        {
        case 0:
          return Leviathan_ProcessLz<LeviathanModeSub, false> (
              lzt, dst_cur, dst, dst_end, dst_start);
        case 1:
          return Leviathan_ProcessLz<LeviathanModeRaw, false> (
              lzt, dst_cur, dst, dst_end, dst_start);
        case 2:
          return Leviathan_ProcessLz<LeviathanModeLamSub, false> (
              lzt, dst_cur, dst, dst_end, dst_start);
        case 3:
          return Leviathan_ProcessLz<LeviathanModeSubAnd3, false> (
              lzt, dst_cur, dst, dst_end, dst_start);
        case 4:
          return Leviathan_ProcessLz<LeviathanModeO1, false> (
              lzt, dst_cur, dst, dst_end, dst_start);
        case 5:
          return Leviathan_ProcessLz<LeviathanModeSubAndF, false> (
              lzt, dst_cur, dst, dst_end, dst_start);
        }
    }
  else
    {
      // multi cmd mode
      switch (chunk_type)
        {
        case 0:
          return Leviathan_ProcessLz<LeviathanModeSub, true> (
              lzt, dst_cur, dst, dst_end, dst_start);
        case 1:
          return Leviathan_ProcessLz<LeviathanModeRaw, true> (
              lzt, dst_cur, dst, dst_end, dst_start);
        case 2:
          return Leviathan_ProcessLz<LeviathanModeLamSub, true> (
              lzt, dst_cur, dst, dst_end, dst_start);
        case 3:
          return Leviathan_ProcessLz<LeviathanModeSubAnd3, true> (
              lzt, dst_cur, dst, dst_end, dst_start);
        case 4:
          return Leviathan_ProcessLz<LeviathanModeO1, true> (
              lzt, dst_cur, dst, dst_end, dst_start);
        case 5:
          return Leviathan_ProcessLz<LeviathanModeSubAndF, true> (
              lzt, dst_cur, dst, dst_end, dst_start);
        }
    }
  return false;
}

// Decode one 256kb big quantum block. It's divided into two 128k blocks
// internally that are compressed separately but with a shared history.
int32_t
Leviathan_DecodeQuantum (u_int8_t *dst, u_int8_t *dst_end, u_int8_t *dst_start,
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
              if (scratch_usage < sizeof (LeviathanLzTable))
                return -1;
              if (!Leviathan_ReadLzTable (
                      mode, src, src + src_used, dst, dst_count,
                      dst - dst_start, scratch + sizeof (LeviathanLzTable),
                      scratch + scratch_usage, (LeviathanLzTable *)scratch))
                return -1;
              if (!Leviathan_ProcessLzRuns (mode, dst, dst_count,
                                            dst - dst_start,
                                            (LeviathanLzTable *)scratch))
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
