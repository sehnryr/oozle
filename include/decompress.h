#pragma once

#include "stdafx.h"
#include "lzna.h"
#include "bitknit.h"
#include "mermaid.h"
#include "leviathan.h"

#define ALIGN_POINTER(p, align) ((u_int8_t *)(((uintptr_t)(p) + (align - 1)) & ~(align - 1)))
#define ALIGN_16(x) (((x) + 15) & ~15)
#define COPY_64(d, s)                          \
    {                                          \
        *(u_int64_t *)(d) = *(u_int64_t *)(s); \
    }
#define COPY_64_BYTES(d, s)                                                    \
    {                                                                          \
        _mm_storeu_si128((__m128i *)d + 0, _mm_loadu_si128((__m128i *)s + 0)); \
        _mm_storeu_si128((__m128i *)d + 1, _mm_loadu_si128((__m128i *)s + 1)); \
        _mm_storeu_si128((__m128i *)d + 2, _mm_loadu_si128((__m128i *)s + 2)); \
        _mm_storeu_si128((__m128i *)d + 3, _mm_loadu_si128((__m128i *)s + 3)); \
    }
#define COPY_64_ADD(d, s, t) _mm_storel_epi64((__m128i *)(d), _mm_add_epi8(_mm_loadl_epi64((__m128i *)(s)), _mm_loadl_epi64((__m128i *)(t))))

#define finline __forceinline

// Header in front of each 256k block
typedef struct KrakenHeader
{
    // Type of decoder used, 6 means kraken
    u_int32_t decoder_type;

    // Whether to restart the decoder
    bool restart_decoder;

    // Whether this block is uncompressed
    bool uncompressed;

    // Whether this block uses checksums.
    bool use_checksums;
} KrakenHeader;

// Additional header in front of each 256k block ("quantum").
typedef struct KrakenQuantumHeader
{
    // The compressed size of this quantum. If this value is 0 it means
    // the quantum is a special quantum such as memset.
    u_int32_t compressed_size;
    // If checksums are enabled, holds the checksum.
    u_int32_t checksum;
    // Two flags
    u_int8_t flag1;
    u_int8_t flag2;
    // Whether the whole block matched a previous block
    u_int32_t whole_match_distance;
} KrakenQuantumHeader;

// Kraken decompression happens in two phases, first one decodes
// all the literals and copy lengths using huffman and second
// phase runs the copy loop. This holds the tables needed by stage 2.
typedef struct KrakenLzTable
{
    // Stream of (literal, match) pairs. The flag u_int8_t contains
    // the length of the match, the length of the literal and whether
    // to use a recent offset.
    u_int8_t *cmd_stream;
    int32_t cmd_stream_size;

    // Holds the actual distances in case we're not using a recent
    // offset.
    int32_t *offs_stream;
    int32_t offs_stream_size;

    // Holds the sequence of literals. All literal copying happens from
    // here.
    u_int8_t *lit_stream;
    int32_t lit_stream_size;

    // Holds the lengths that do not fit in the flag stream. Both literal
    // lengths and match length are stored in the same array.
    int32_t *len_stream;
    int32_t len_stream_size;
} KrakenLzTable;

typedef struct KrakenDecoder
{
    // Updated after the |*_DecodeStep| function completes to hold
    // the number of bytes read and written.
    int32_t src_used, dst_used;

    // Pointer to a 256k buffer that holds the intermediate state
    // in between decode phase 1 and 2.
    u_int8_t *scratch;
    size_t scratch_size;

    KrakenHeader hdr;
} KrakenDecoder;

typedef struct BitReader
{
    // |p| holds the current u_int8_t and |p_end| the end of the buffer.
    const u_int8_t *p, *p_end;
    // Bits accumulated so far
    u_int32_t bits;
    // Next u_int8_t will end up in the |bitpos| position in |bits|.
    int32_t bitpos;
} BitReader;

struct HuffRevLut
{
    u_int8_t bits2len[2048];
    u_int8_t bits2sym[2048];
};

typedef struct HuffReader
{
    // Array to hold the output of the huffman read array operation
    u_int8_t *output, *output_end;
    // We decode three parallel streams, two forwards, |src| and |src_mid|
    // while |src_end| is decoded backwards.
    const u_int8_t *src, *src_mid, *src_end, *src_mid_org;
    int32_t src_bitpos, src_mid_bitpos, src_end_bitpos;
    u_int32_t src_bits, src_mid_bits, src_end_bits;
} HuffReader;

struct HuffRange
{
    u_int16_t symbol;
    u_int16_t num;
};

struct NewHuffLut
{
    // Mapping that maps a bit pattern to a code length.
    u_int8_t bits2len[2048 + 16];
    // Mapping that maps a bit pattern to a symbol.
    u_int8_t bits2sym[2048 + 16];
};

struct BitReader2
{
    const u_int8_t *p, *p_end;
    u_int32_t bitpos;
};

struct TansData
{
    u_int32_t A_used;
    u_int32_t B_used;
    u_int8_t A[256];
    u_int32_t B[256];
};

struct TansLutEnt
{
    u_int32_t x;
    u_int8_t bits_x;
    u_int8_t symbol;
    u_int16_t w;
};

struct TansDecoderParams
{
    TansLutEnt *lut;
    u_int8_t *dst, *dst_end;
    const u_int8_t *ptr_f, *ptr_b;
    u_int32_t bits_f, bits_b;
    int32_t bitpos_f, bitpos_b;
    u_int32_t state_0, state_1, state_2, state_3, state_4;
};

static const u_int32_t kRiceCodeBits2Value[256] = {
    0x80000000, 0x00000007, 0x10000006, 0x00000006, 0x20000005, 0x00000105, 0x10000005, 0x00000005,
    0x30000004, 0x00000204, 0x10000104, 0x00000104, 0x20000004, 0x00010004, 0x10000004, 0x00000004,
    0x40000003, 0x00000303, 0x10000203, 0x00000203, 0x20000103, 0x00010103, 0x10000103, 0x00000103,
    0x30000003, 0x00020003, 0x10010003, 0x00010003, 0x20000003, 0x01000003, 0x10000003, 0x00000003,
    0x50000002, 0x00000402, 0x10000302, 0x00000302, 0x20000202, 0x00010202, 0x10000202, 0x00000202,
    0x30000102, 0x00020102, 0x10010102, 0x00010102, 0x20000102, 0x01000102, 0x10000102, 0x00000102,
    0x40000002, 0x00030002, 0x10020002, 0x00020002, 0x20010002, 0x01010002, 0x10010002, 0x00010002,
    0x30000002, 0x02000002, 0x11000002, 0x01000002, 0x20000002, 0x00000012, 0x10000002, 0x00000002,
    0x60000001, 0x00000501, 0x10000401, 0x00000401, 0x20000301, 0x00010301, 0x10000301, 0x00000301,
    0x30000201, 0x00020201, 0x10010201, 0x00010201, 0x20000201, 0x01000201, 0x10000201, 0x00000201,
    0x40000101, 0x00030101, 0x10020101, 0x00020101, 0x20010101, 0x01010101, 0x10010101, 0x00010101,
    0x30000101, 0x02000101, 0x11000101, 0x01000101, 0x20000101, 0x00000111, 0x10000101, 0x00000101,
    0x50000001, 0x00040001, 0x10030001, 0x00030001, 0x20020001, 0x01020001, 0x10020001, 0x00020001,
    0x30010001, 0x02010001, 0x11010001, 0x01010001, 0x20010001, 0x00010011, 0x10010001, 0x00010001,
    0x40000001, 0x03000001, 0x12000001, 0x02000001, 0x21000001, 0x01000011, 0x11000001, 0x01000001,
    0x30000001, 0x00000021, 0x10000011, 0x00000011, 0x20000001, 0x00001001, 0x10000001, 0x00000001,
    0x70000000, 0x00000600, 0x10000500, 0x00000500, 0x20000400, 0x00010400, 0x10000400, 0x00000400,
    0x30000300, 0x00020300, 0x10010300, 0x00010300, 0x20000300, 0x01000300, 0x10000300, 0x00000300,
    0x40000200, 0x00030200, 0x10020200, 0x00020200, 0x20010200, 0x01010200, 0x10010200, 0x00010200,
    0x30000200, 0x02000200, 0x11000200, 0x01000200, 0x20000200, 0x00000210, 0x10000200, 0x00000200,
    0x50000100, 0x00040100, 0x10030100, 0x00030100, 0x20020100, 0x01020100, 0x10020100, 0x00020100,
    0x30010100, 0x02010100, 0x11010100, 0x01010100, 0x20010100, 0x00010110, 0x10010100, 0x00010100,
    0x40000100, 0x03000100, 0x12000100, 0x02000100, 0x21000100, 0x01000110, 0x11000100, 0x01000100,
    0x30000100, 0x00000120, 0x10000110, 0x00000110, 0x20000100, 0x00001100, 0x10000100, 0x00000100,
    0x60000000, 0x00050000, 0x10040000, 0x00040000, 0x20030000, 0x01030000, 0x10030000, 0x00030000,
    0x30020000, 0x02020000, 0x11020000, 0x01020000, 0x20020000, 0x00020010, 0x10020000, 0x00020000,
    0x40010000, 0x03010000, 0x12010000, 0x02010000, 0x21010000, 0x01010010, 0x11010000, 0x01010000,
    0x30010000, 0x00010020, 0x10010010, 0x00010010, 0x20010000, 0x00011000, 0x10010000, 0x00010000,
    0x50000000, 0x04000000, 0x13000000, 0x03000000, 0x22000000, 0x02000010, 0x12000000, 0x02000000,
    0x31000000, 0x01000020, 0x11000010, 0x01000010, 0x21000000, 0x01001000, 0x11000000, 0x01000000,
    0x40000000, 0x00000030, 0x10000020, 0x00000020, 0x20000010, 0x00001010, 0x10000010, 0x00000010,
    0x30000000, 0x00002000, 0x10001000, 0x00001000, 0x20000000, 0x00100000, 0x10000000, 0x00000000,
};

static const u_int8_t kRiceCodeBits2Len[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

static u_int32_t bitmasks[32] = {
    0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff,
    0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff, 0xffff,
    0x1ffff, 0x3ffff, 0x7ffff, 0xfffff, 0x1fffff, 0x3fffff, 0x7fffff,
    0xffffff, 0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff};

void *MallocAligned(size_t size, size_t alignment);
void FreeAligned(void *p);

u_int32_t BSR(u_int32_t x); // Bit scan reverse
u_int32_t BSF(u_int32_t x); // Bit scan forward

void BitReader_Refill(BitReader *bits);
void BitReader_RefillBackwards(BitReader *bits);
int32_t BitReader_ReadBit(BitReader *bits);
int32_t BitReader_ReadBitNoRefill(BitReader *bits);
int32_t BitReader_ReadBitsNoRefill(BitReader *bits, int32_t n);
int32_t BitReader_ReadBitsNoRefillZero(BitReader *bits, int32_t n);
u_int32_t BitReader_ReadMoreThan24Bits(BitReader *bits, int32_t n);
u_int32_t BitReader_ReadMoreThan24BitsB(BitReader *bits, int32_t n);
int32_t BitReader_ReadGamma(BitReader *bits);
int32_t BitReader_ReadGammaX(BitReader *bits, int32_t forced);
u_int32_t BitReader_ReadDistance(BitReader *bits, u_int32_t v);
u_int32_t BitReader_ReadDistanceB(BitReader *bits, u_int32_t v);
bool BitReader_ReadLength(BitReader *bits, u_int32_t *v);
bool BitReader_ReadLengthB(BitReader *bits, u_int32_t *v);

int32_t CountLeadingZeros(u_int32_t bits);
int32_t Log2RoundUp(u_int32_t v);

KrakenDecoder *Kraken_Create();
void Kraken_Destroy(KrakenDecoder *kraken);

const u_int8_t *Kraken_ParseHeader(
    KrakenHeader *hdr,
    const u_int8_t *p);
const u_int8_t *Kraken_ParseQuantumHeader(
    KrakenQuantumHeader *hdr,
    const u_int8_t *p,
    bool use_checksum);

const u_int8_t *LZNA_ParseWholeMatchInfo(
    const u_int8_t *p,
    u_int32_t *dist);
const u_int8_t *LZNA_ParseQuantumHeader(
    KrakenQuantumHeader *hdr,
    const u_int8_t *p,
    bool use_checksum,
    int32_t raw_len);

u_int32_t Kraken_GetCrc(const u_int8_t *p, size_t p_size);

bool Kraken_DecodeBytesCore(HuffReader *hr, HuffRevLut *lut);

int32_t Huff_ReadCodeLengthsOld(
    BitReader *bits,
    u_int8_t *syms,
    u_int32_t *code_prefix);

int32_t BitReader_ReadFluff(BitReader *bits, int32_t num_symbols);

bool DecodeGolombRiceLengths(
    u_int8_t *dst,
    size_t size,
    BitReader2 *br);
bool DecodeGolombRiceBits(
    u_int8_t *dst,
    u_int32_t size,
    u_int32_t bitcount,
    BitReader2 *br);

int32_t Huff_ConvertToRanges(
    HuffRange *range,
    int32_t num_symbols,
    int32_t P,
    const u_int8_t *symlen,
    BitReader *bits);
int32_t Huff_ReadCodeLengthsNew(
    BitReader *bits,
    u_int8_t *syms,
    u_int32_t *code_prefix);

void FillByteOverflow16(u_int8_t *dst, u_int8_t v, size_t n);

bool Huff_MakeLut(
    const u_int32_t *prefix_org,
    const u_int32_t *prefix_cur,
    NewHuffLut *hufflut,
    u_int8_t *syms);

int32_t Kraken_DecodeBytes_Type12(
    const u_int8_t *src,
    size_t src_size,
    u_int8_t *output,
    int32_t output_size,
    int32_t type);
int32_t Kraken_DecodeMultiArray(
    const u_int8_t *src,
    const u_int8_t *src_end,
    u_int8_t *dst,
    u_int8_t *dst_end,
    u_int8_t **array_data,
    int32_t *array_lens,
    int32_t array_count,
    int32_t *total_size_out,
    bool force_memmove,
    u_int8_t *scratch,
    u_int8_t *scratch_end);
int32_t Krak_DecodeRecursive(
    const u_int8_t *src,
    size_t src_size,
    u_int8_t *output,
    int32_t output_size,
    u_int8_t *scratch,
    u_int8_t *scratch_end);
int32_t Krak_DecodeRLE(
    const u_int8_t *src,
    size_t src_size,
    u_int8_t *dst,
    int32_t dst_size,
    u_int8_t *scratch,
    u_int8_t *scratch_end);

template <typename T>
void SimpleSort(T *p, T *pend);

bool Tans_DecodeTable(BitReader *bits, int32_t L_bits, TansData *tans_data);
void Tans_InitLut(TansData *tans_data, int32_t L_bits, TansLutEnt *lut);
bool Tans_Decode(TansDecoderParams *params);

int32_t Krak_DecodeTans(
    const u_int8_t *src,
    size_t src_size,
    u_int8_t *dst,
    int32_t dst_size,
    u_int8_t *scratch,
    u_int8_t *scratch_end);
int32_t Kraken_GetBlockSize(
    const u_int8_t *src,
    const u_int8_t *src_end,
    int32_t *dest_size,
    int32_t dest_capacity);

int32_t Kraken_DecodeBytes(
    u_int8_t **output,
    const u_int8_t *src,
    const u_int8_t *src_end,
    int32_t *decoded_size,
    size_t output_size,
    bool force_memmove,
    u_int8_t *scratch,
    u_int8_t *scratch_end);

void CombineScaledOffsetArrays(
    int32_t *offs_stream,
    size_t offs_stream_size,
    int32_t scale,
    const u_int8_t *low_bits);

bool Kraken_UnpackOffsets(
    const u_int8_t *src,
    const u_int8_t *src_end,
    const u_int8_t *packed_offs_stream,
    const u_int8_t *packed_offs_stream_extra,
    int32_t packed_offs_stream_size,
    int32_t multi_dist_scale,
    const u_int8_t *packed_litlen_stream,
    int32_t packed_litlen_stream_size,
    int32_t *offs_stream,
    int32_t *len_stream,
    bool excess_flag,
    int32_t excess_bytes);
bool Kraken_ReadLzTable(
    int32_t mode,
    const u_int8_t *src,
    const u_int8_t *src_end,
    u_int8_t *dst,
    int32_t dst_size,
    int32_t offset,
    u_int8_t *scratch,
    u_int8_t *scratch_end,
    KrakenLzTable *lztable);
bool Kraken_ProcessLzRuns_Type0(
    KrakenLzTable *lzt,
    u_int8_t *dst,
    u_int8_t *dst_end,
    u_int8_t *dst_start);
bool Kraken_ProcessLzRuns_Type1(
    KrakenLzTable *lzt,
    u_int8_t *dst,
    u_int8_t *dst_end,
    u_int8_t *dst_start);
bool Kraken_ProcessLzRuns(
    int32_t mode,
    u_int8_t *dst,
    int32_t dst_size,
    int32_t offset,
    KrakenLzTable *lztable);
int32_t Kraken_DecodeQuantum(
    u_int8_t *dst,
    u_int8_t *dst_end,
    u_int8_t *dst_start,
    const u_int8_t *src,
    const u_int8_t *src_end,
    u_int8_t *scratch,
    u_int8_t *scratch_end);

void Kraken_CopyWholeMatch(
    u_int8_t *dst,
    u_int32_t offset,
    size_t length);
bool Kraken_DecodeStep(
    struct KrakenDecoder *dec,
    u_int8_t *dst_start,
    int32_t offset,
    size_t dst_bytes_left_in,
    const u_int8_t *src,
    size_t src_bytes_left);
int32_t Kraken_Decompress(
    const u_int8_t *src,
    size_t src_len,
    u_int8_t *dst,
    size_t dst_len);