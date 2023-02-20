#include "oozle/include/mermaid.h"

int32_t Mermaid_DecodeFarOffsets(const u_int8_t *src, const u_int8_t *src_end, u_int32_t *output, size_t output_size, int64_t offset)
{
    const u_int8_t *src_cur = src;
    size_t i;
    u_int32_t off;

    if (offset < (0xC00000 - 1))
    {
        for (i = 0; i != output_size; i++)
        {
            if (src_end - src_cur < 3)
                return -1;
            off = src_cur[0] | src_cur[1] << 8 | src_cur[2] << 16;
            src_cur += 3;
            output[i] = off;
            if (off > offset)
                return -1;
        }
        return src_cur - src;
    }

    for (i = 0; i != output_size; i++)
    {
        if (src_end - src_cur < 3)
            return -1;
        off = src_cur[0] | src_cur[1] << 8 | src_cur[2] << 16;
        src_cur += 3;

        if (off >= 0xc00000)
        {
            if (src_cur == src_end)
                return -1;
            off += *src_cur++ << 22;
        }
        output[i] = off;
        if (off > offset)
            return -1;
    }
    return src_cur - src;
}

void Mermaid_CombineOffs16(u_int16_t *dst, size_t size, const u_int8_t *lo, const u_int8_t *hi)
{
    for (size_t i = 0; i != size; i++)
        dst[i] = lo[i] + hi[i] * 256;
}

bool Mermaid_ReadLzTable(int32_t mode,
                         const u_int8_t *src, const u_int8_t *src_end,
                         u_int8_t *dst, int32_t dst_size, int64_t offset,
                         u_int8_t *scratch, u_int8_t *scratch_end, MermaidLzTable *lz)
{
    u_int8_t *out;
    int32_t decode_count, n;
    u_int32_t tmp, off32_size_2, off32_size_1;

    if (mode > 1)
        return false;

    if (src_end - src < 10)
        return false;

    if (offset == 0)
    {
        COPY_64(dst, src);
        dst += 8;
        src += 8;
    }

    // Decode lit stream
    out = scratch;
    n = Oozle_DecodeBytes(&out, src, src_end, &decode_count, MIN(scratch_end - scratch, dst_size), false, scratch, scratch_end);
    if (n < 0)
        return false;
    src += n;
    lz->lit_stream = out;
    lz->lit_stream_end = out + decode_count;
    scratch += decode_count;

    // Decode flag stream
    out = scratch;
    n = Oozle_DecodeBytes(&out, src, src_end, &decode_count, MIN(scratch_end - scratch, dst_size), false, scratch, scratch_end);
    if (n < 0)
        return false;
    src += n;
    lz->cmd_stream = out;
    lz->cmd_stream_end = out + decode_count;
    scratch += decode_count;

    lz->cmd_stream_2_offs_end = decode_count;
    if (dst_size <= 0x10000)
    {
        lz->cmd_stream_2_offs = decode_count;
    }
    else
    {
        if (src_end - src < 2)
            return false;
        lz->cmd_stream_2_offs = *(u_int16_t *)src;
        src += 2;
        if (lz->cmd_stream_2_offs > lz->cmd_stream_2_offs_end)
            return false;
    }

    if (src_end - src < 2)
        return false;

    int32_t off16_count = *(u_int16_t *)src;
    if (off16_count == 0xffff)
    {
        // off16 is entropy coded
        u_int8_t *off16_lo, *off16_hi;
        int32_t off16_lo_count, off16_hi_count;
        src += 2;
        off16_hi = scratch;
        n = Oozle_DecodeBytes(&off16_hi, src, src_end, &off16_hi_count, MIN(scratch_end - scratch, dst_size >> 1), false, scratch, scratch_end);
        if (n < 0)
            return false;
        src += n;
        scratch += off16_hi_count;

        off16_lo = scratch;
        n = Oozle_DecodeBytes(&off16_lo, src, src_end, &off16_lo_count, MIN(scratch_end - scratch, dst_size >> 1), false, scratch, scratch_end);
        if (n < 0)
            return false;
        src += n;
        scratch += off16_lo_count;

        if (off16_lo_count != off16_hi_count)
            return false;
        scratch = ALIGN_POINTER(scratch, 2);
        lz->off16_stream = (u_int16_t *)scratch;
        if (scratch + off16_lo_count * 2 > scratch_end)
            return false;
        scratch += off16_lo_count * 2;
        lz->off16_stream_end = (u_int16_t *)scratch;
        Mermaid_CombineOffs16((u_int16_t *)lz->off16_stream, off16_lo_count, off16_lo, off16_hi);
    }
    else
    {
        lz->off16_stream = (u_int16_t *)(src + 2);
        src += 2 + off16_count * 2;
        lz->off16_stream_end = (u_int16_t *)src;
    }

    if (src_end - src < 3)
        return false;
    tmp = src[0] | src[1] << 8 | src[2] << 16;
    src += 3;

    if (tmp != 0)
    {
        off32_size_1 = tmp >> 12;
        off32_size_2 = tmp & 0xFFF;
        if (off32_size_1 == 4095)
        {
            if (src_end - src < 2)
                return false;
            off32_size_1 = *(u_int16_t *)src;
            src += 2;
        }
        if (off32_size_2 == 4095)
        {
            if (src_end - src < 2)
                return false;
            off32_size_2 = *(u_int16_t *)src;
            src += 2;
        }
        lz->off32_size_1 = off32_size_1;
        lz->off32_size_2 = off32_size_2;

        if (scratch + 4 * (off32_size_2 + off32_size_1) + 64 > scratch_end)
            return false;

        scratch = ALIGN_POINTER(scratch, 4);

        lz->off32_stream_1 = (u_int32_t *)scratch;
        scratch += off32_size_1 * 4;
        // store dummy bytes after for prefetcher.
        ((u_int64_t *)scratch)[0] = 0;
        ((u_int64_t *)scratch)[1] = 0;
        ((u_int64_t *)scratch)[2] = 0;
        ((u_int64_t *)scratch)[3] = 0;
        scratch += 32;

        lz->off32_stream_2 = (u_int32_t *)scratch;
        scratch += off32_size_2 * 4;
        // store dummy bytes after for prefetcher.
        ((u_int64_t *)scratch)[0] = 0;
        ((u_int64_t *)scratch)[1] = 0;
        ((u_int64_t *)scratch)[2] = 0;
        ((u_int64_t *)scratch)[3] = 0;
        scratch += 32;

        n = Mermaid_DecodeFarOffsets(src, src_end, lz->off32_stream_1, lz->off32_size_1, offset);
        if (n < 0)
            return false;
        src += n;

        n = Mermaid_DecodeFarOffsets(src, src_end, lz->off32_stream_2, lz->off32_size_2, offset + 0x10000);
        if (n < 0)
            return false;
        src += n;
    }
    else
    {
        if (scratch_end - scratch < 32)
            return false;
        lz->off32_size_1 = 0;
        lz->off32_size_2 = 0;
        lz->off32_stream_1 = (u_int32_t *)scratch;
        lz->off32_stream_2 = (u_int32_t *)scratch;
        // store dummy bytes after for prefetcher.
        ((u_int64_t *)scratch)[0] = 0;
        ((u_int64_t *)scratch)[1] = 0;
        ((u_int64_t *)scratch)[2] = 0;
        ((u_int64_t *)scratch)[3] = 0;
    }
    lz->length_stream = src;
    return true;
}

const u_int8_t *Mermaid_Mode0(u_int8_t *dst, size_t dst_size, u_int8_t *dst_ptr_end, u_int8_t *dst_start,
                              const u_int8_t *src_end, MermaidLzTable *lz, int32_t *saved_dist, size_t startoff)
{
    const u_int8_t *dst_end = dst + dst_size;
    const u_int8_t *cmd_stream = lz->cmd_stream;
    const u_int8_t *cmd_stream_end = lz->cmd_stream_end;
    const u_int8_t *length_stream = lz->length_stream;
    const u_int8_t *lit_stream = lz->lit_stream;
    const u_int8_t *lit_stream_end = lz->lit_stream_end;
    const u_int16_t *off16_stream = lz->off16_stream;
    const u_int16_t *off16_stream_end = lz->off16_stream_end;
    const u_int32_t *off32_stream = lz->off32_stream;
    const u_int32_t *off32_stream_end = lz->off32_stream_end;
    intptr_t recent_offs = *saved_dist;
    const u_int8_t *match;
    intptr_t length;
    const u_int8_t *dst_begin = dst;

    dst += startoff;

    while (cmd_stream < cmd_stream_end)
    {
        uintptr_t cmd = *cmd_stream++;
        if (cmd >= 24)
        {
            intptr_t new_dist = *off16_stream;
            uintptr_t use_distance = (uintptr_t)(cmd >> 7) - 1;
            uintptr_t litlen = (cmd & 7);
            COPY_64_ADD(dst, lit_stream, &dst[recent_offs]);
            dst += litlen;
            lit_stream += litlen;
            recent_offs ^= use_distance & (recent_offs ^ -new_dist);
            off16_stream = (u_int16_t *)((uintptr_t)off16_stream + (use_distance & 2));
            match = dst + recent_offs;
            COPY_64(dst, match);
            COPY_64(dst + 8, match + 8);
            dst += (cmd >> 3) & 0xF;
        }
        else if (cmd > 2)
        {
            length = cmd + 5;

            if (off32_stream == off32_stream_end)
                return NULL;
            match = dst_begin - *off32_stream++;
            recent_offs = (match - dst);

            if (dst_end - dst < length)
                return NULL;
            COPY_64(dst, match);
            COPY_64(dst + 8, match + 8);
            COPY_64(dst + 16, match + 16);
            COPY_64(dst + 24, match + 24);
            dst += length;
            _mm_prefetch((char *)dst_begin - off32_stream[3], _MM_HINT_T0);
        }
        else if (cmd == 0)
        {
            if (src_end - length_stream == 0)
                return NULL;
            length = *length_stream;
            if (length > 251)
            {
                if (src_end - length_stream < 3)
                    return NULL;
                length += (size_t) * (u_int16_t *)(length_stream + 1) * 4;
                length_stream += 2;
            }
            length_stream += 1;

            length += 64;
            if (dst_end - dst < length ||
                lit_stream_end - lit_stream < length)
                return NULL;

            do
            {
                COPY_64_ADD(dst, lit_stream, &dst[recent_offs]);
                COPY_64_ADD(dst + 8, lit_stream + 8, &dst[recent_offs + 8]);
                dst += 16;
                lit_stream += 16;
                length -= 16;
            } while (length > 0);
            dst += length;
            lit_stream += length;
        }
        else if (cmd == 1)
        {
            if (src_end - length_stream == 0)
                return NULL;
            length = *length_stream;
            if (length > 251)
            {
                if (src_end - length_stream < 3)
                    return NULL;
                length += (size_t) * (u_int16_t *)(length_stream + 1) * 4;
                length_stream += 2;
            }
            length_stream += 1;
            length += 91;

            if (off16_stream == off16_stream_end)
                return NULL;
            match = dst - *off16_stream++;
            recent_offs = (match - dst);
            do
            {
                COPY_64(dst, match);
                COPY_64(dst + 8, match + 8);
                dst += 16;
                match += 16;
                length -= 16;
            } while (length > 0);
            dst += length;
        }
        else /* flag == 2 */
        {
            if (src_end - length_stream == 0)
                return NULL;
            length = *length_stream;
            if (length > 251)
            {
                if (src_end - length_stream < 3)
                    return NULL;
                length += (size_t) * (u_int16_t *)(length_stream + 1) * 4;
                length_stream += 2;
            }
            length_stream += 1;
            length += 29;
            if (off32_stream == off32_stream_end)
                return NULL;
            match = dst_begin - *off32_stream++;
            recent_offs = (match - dst);
            do
            {
                COPY_64(dst, match);
                COPY_64(dst + 8, match + 8);
                dst += 16;
                match += 16;
                length -= 16;
            } while (length > 0);
            dst += length;
            _mm_prefetch((char *)dst_begin - off32_stream[3], _MM_HINT_T0);
        }
    }

    length = dst_end - dst;
    if (length >= 8)
    {
        do
        {
            COPY_64_ADD(dst, lit_stream, &dst[recent_offs]);
            dst += 8;
            lit_stream += 8;
            length -= 8;
        } while (length >= 8);
    }
    if (length > 0)
    {
        do
        {
            *dst = *lit_stream++ + dst[recent_offs];
            dst++;
        } while (--length);
    }

    *saved_dist = (int32_t)recent_offs;
    lz->length_stream = length_stream;
    lz->off16_stream = off16_stream;
    lz->lit_stream = lit_stream;
    return length_stream;
}

const u_int8_t *Mermaid_Mode1(u_int8_t *dst, size_t dst_size, u_int8_t *dst_ptr_end, u_int8_t *dst_start,
                              const u_int8_t *src_end, MermaidLzTable *lz, int32_t *saved_dist, size_t startoff)
{
    const u_int8_t *dst_end = dst + dst_size;
    const u_int8_t *cmd_stream = lz->cmd_stream;
    const u_int8_t *cmd_stream_end = lz->cmd_stream_end;
    const u_int8_t *length_stream = lz->length_stream;
    const u_int8_t *lit_stream = lz->lit_stream;
    const u_int8_t *lit_stream_end = lz->lit_stream_end;
    const u_int16_t *off16_stream = lz->off16_stream;
    const u_int16_t *off16_stream_end = lz->off16_stream_end;
    const u_int32_t *off32_stream = lz->off32_stream;
    const u_int32_t *off32_stream_end = lz->off32_stream_end;
    intptr_t recent_offs = *saved_dist;
    const u_int8_t *match;
    intptr_t length;
    const u_int8_t *dst_begin = dst;

    dst += startoff;

    while (cmd_stream < cmd_stream_end)
    {
        uintptr_t flag = *cmd_stream++;
        if (flag >= 24)
        {
            intptr_t new_dist = *off16_stream;
            uintptr_t use_distance = (uintptr_t)(flag >> 7) - 1;
            uintptr_t litlen = (flag & 7);
            COPY_64(dst, lit_stream);
            dst += litlen;
            lit_stream += litlen;
            recent_offs ^= use_distance & (recent_offs ^ -new_dist);
            off16_stream = (u_int16_t *)((uintptr_t)off16_stream + (use_distance & 2));
            match = dst + recent_offs;
            COPY_64(dst, match);
            COPY_64(dst + 8, match + 8);
            dst += (flag >> 3) & 0xF;
        }
        else if (flag > 2)
        {
            length = flag + 5;

            if (off32_stream == off32_stream_end)
                return NULL;
            match = dst_begin - *off32_stream++;
            recent_offs = (match - dst);

            if (dst_end - dst < length)
                return NULL;
            COPY_64(dst, match);
            COPY_64(dst + 8, match + 8);
            COPY_64(dst + 16, match + 16);
            COPY_64(dst + 24, match + 24);
            dst += length;
            _mm_prefetch((char *)dst_begin - off32_stream[3], _MM_HINT_T0);
        }
        else if (flag == 0)
        {
            if (src_end - length_stream == 0)
                return NULL;
            length = *length_stream;
            if (length > 251)
            {
                if (src_end - length_stream < 3)
                    return NULL;
                length += (size_t) * (u_int16_t *)(length_stream + 1) * 4;
                length_stream += 2;
            }
            length_stream += 1;

            length += 64;
            if (dst_end - dst < length ||
                lit_stream_end - lit_stream < length)
                return NULL;

            do
            {
                COPY_64(dst, lit_stream);
                COPY_64(dst + 8, lit_stream + 8);
                dst += 16;
                lit_stream += 16;
                length -= 16;
            } while (length > 0);
            dst += length;
            lit_stream += length;
        }
        else if (flag == 1)
        {
            if (src_end - length_stream == 0)
                return NULL;
            length = *length_stream;
            if (length > 251)
            {
                if (src_end - length_stream < 3)
                    return NULL;
                length += (size_t) * (u_int16_t *)(length_stream + 1) * 4;
                length_stream += 2;
            }
            length_stream += 1;
            length += 91;

            if (off16_stream == off16_stream_end)
                return NULL;
            match = dst - *off16_stream++;
            recent_offs = (match - dst);
            do
            {
                COPY_64(dst, match);
                COPY_64(dst + 8, match + 8);
                dst += 16;
                match += 16;
                length -= 16;
            } while (length > 0);
            dst += length;
        }
        else /* flag == 2 */
        {
            if (src_end - length_stream == 0)
                return NULL;
            length = *length_stream;
            if (length > 251)
            {
                if (src_end - length_stream < 3)
                    return NULL;
                length += (size_t) * (u_int16_t *)(length_stream + 1) * 4;
                length_stream += 2;
            }
            length_stream += 1;
            length += 29;

            if (off32_stream == off32_stream_end)
                return NULL;
            match = dst_begin - *off32_stream++;
            recent_offs = (match - dst);

            do
            {
                COPY_64(dst, match);
                COPY_64(dst + 8, match + 8);
                dst += 16;
                match += 16;
                length -= 16;
            } while (length > 0);
            dst += length;

            _mm_prefetch((char *)dst_begin - off32_stream[3], _MM_HINT_T0);
        }
    }

    length = dst_end - dst;
    if (length >= 8)
    {
        do
        {
            COPY_64(dst, lit_stream);
            dst += 8;
            lit_stream += 8;
            length -= 8;
        } while (length >= 8);
    }
    if (length > 0)
    {
        do
        {
            *dst++ = *lit_stream++;
        } while (--length);
    }

    *saved_dist = (int32_t)recent_offs;
    lz->length_stream = length_stream;
    lz->off16_stream = off16_stream;
    lz->lit_stream = lit_stream;
    return length_stream;
}

bool Mermaid_ProcessLzRuns(int32_t mode,
                           const u_int8_t *src, const u_int8_t *src_end,
                           u_int8_t *dst, size_t dst_size, u_int64_t offset, u_int8_t *dst_end,
                           MermaidLzTable *lz)
{

    int32_t iteration = 0;
    u_int8_t *dst_start = dst - offset;
    int32_t saved_dist = -8;
    const u_int8_t *src_cur;

    for (iteration = 0; iteration != 2; iteration++)
    {
        size_t dst_size_cur = dst_size;
        if (dst_size_cur > 0x10000)
            dst_size_cur = 0x10000;

        if (iteration == 0)
        {
            lz->off32_stream = lz->off32_stream_1;
            lz->off32_stream_end = lz->off32_stream_1 + lz->off32_size_1 * 4;
            lz->cmd_stream_end = lz->cmd_stream + lz->cmd_stream_2_offs;
        }
        else
        {
            lz->off32_stream = lz->off32_stream_2;
            lz->off32_stream_end = lz->off32_stream_2 + lz->off32_size_2 * 4;
            lz->cmd_stream_end = lz->cmd_stream + lz->cmd_stream_2_offs_end;
            lz->cmd_stream += lz->cmd_stream_2_offs;
        }

        if (mode == 0)
        {
            src_cur = Mermaid_Mode0(dst, dst_size_cur, dst_end, dst_start, src_end, lz, &saved_dist,
                                    (offset == 0) && (iteration == 0) ? 8 : 0);
        }
        else
        {
            src_cur = Mermaid_Mode1(dst, dst_size_cur, dst_end, dst_start, src_end, lz, &saved_dist,
                                    (offset == 0) && (iteration == 0) ? 8 : 0);
        }
        if (src_cur == NULL)
            return false;

        dst += dst_size_cur;
        dst_size -= dst_size_cur;
        if (dst_size == 0)
            break;
    }

    if (src_cur != src_end)
        return false;

    return true;
}

int32_t Mermaid_DecodeQuantum(u_int8_t *dst, u_int8_t *dst_end, u_int8_t *dst_start,
                              const u_int8_t *src, const u_int8_t *src_end,
                              u_int8_t *temp, u_int8_t *temp_end)
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
            // Stored without any match copying.
            u_int8_t *out = dst;
            src_used = Oozle_DecodeBytes(&out, src, src_end, &written_bytes, dst_count, false, temp, temp_end);
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
                int32_t temp_usage = 2 * dst_count + 32;
                if (temp_usage > 0x40000)
                    temp_usage = 0x40000;
                if (!Mermaid_ReadLzTable(mode,
                                         src, src + src_used,
                                         dst, dst_count,
                                         dst - dst_start,
                                         temp + sizeof(MermaidLzTable), temp + temp_usage,
                                         (MermaidLzTable *)temp))
                    return -1;
                if (!Mermaid_ProcessLzRuns(mode,
                                           src, src + src_used,
                                           dst, dst_count,
                                           dst - dst_start, dst_end,
                                           (MermaidLzTable *)temp))
                    return -1;
            }
            else if (src_used > dst_count || mode != 0)
            {
                return -1;
            }
            else
            {
                memmove(dst, src, dst_count);
            }
        }
        src += src_used;
        dst += dst_count;
    }
    return src - src_in;
}
