#pragma once

#include "stdafx.h"

struct BitknitLiteral
{
    u_int16_t lookup[512 + 4];
    u_int16_t a[300 + 1];
    u_int16_t freq[300];
    u_int32_t adapt_interval;
};

struct BitknitDistanceLsb
{
    u_int16_t lookup[64 + 4];
    u_int16_t a[40 + 1];
    u_int16_t freq[40];
    u_int32_t adapt_interval;
};

struct BitknitDistanceBits
{
    u_int16_t lookup[64 + 4];
    u_int16_t a[21 + 1];
    u_int16_t freq[21];
    u_int32_t adapt_interval;
};

struct BitknitState
{
    u_int32_t recent_dist[8];
    u_int32_t last_match_dist;
    u_int32_t recent_dist_mask;
    u_int32_t bits, bits2;

    BitknitLiteral literals[4];
    BitknitDistanceLsb distance_lsb[4];
    BitknitDistanceBits distance_bits;
};

void BitknitLiteral_Init(BitknitLiteral *model);
void BitknitState_Init(BitknitState *bk);
void BitknitDistanceLsb_Init(BitknitDistanceLsb *model);
void BitknitDistanceBits_Init(BitknitDistanceBits *model);

void BitknitLiteral_Adaptive(BitknitLiteral *model, u_int32_t sym);
void BitknitDistanceLsb_Adaptive(BitknitDistanceLsb *model, u_int32_t sym);
void BitknitDistanceBits_Adaptive(BitknitDistanceBits *model, u_int32_t sym);

u_int32_t BitknitLiteral_Lookup(BitknitLiteral *model, u_int32_t *bits);
u_int32_t BitknitDistanceLsb_Lookup(BitknitDistanceLsb *model, u_int32_t *bits);
u_int32_t BitknitDistanceBits_Lookup(BitknitDistanceBits *model, u_int32_t *bits);

static void BitknitCopyLongDist(u_int8_t *dst, size_t dist, size_t length);
static void BitknitCopyShortDist(u_int8_t *dst, size_t dist, size_t length);

size_t Bitknit_Decode(
    const u_int8_t *src,
    const u_int8_t *src_end,
    u_int8_t *dst,
    u_int8_t *dst_end,
    u_int8_t *dst_start,
    BitknitState *bk);
