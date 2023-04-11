#pragma once

#include "stdafx.h"

struct BitknitLiteral
{
  uint16_t lookup[512 + 4];
  uint16_t a[300 + 1];
  uint16_t freq[300];
  uint32_t adapt_interval;
};

struct BitknitDistanceLsb
{
  uint16_t lookup[64 + 4];
  uint16_t a[40 + 1];
  uint16_t freq[40];
  uint32_t adapt_interval;
};

struct BitknitDistanceBits
{
  uint16_t lookup[64 + 4];
  uint16_t a[21 + 1];
  uint16_t freq[21];
  uint32_t adapt_interval;
};

struct BitknitState
{
  uint32_t recent_dist[8];
  uint32_t last_match_dist;
  uint32_t recent_dist_mask;
  uint32_t bits, bits2;

  BitknitLiteral literals[4];
  BitknitDistanceLsb distance_lsb[4];
  BitknitDistanceBits distance_bits;
};

void BitknitLiteral_Init (BitknitLiteral *model);
void BitknitState_Init (BitknitState *bk);
void BitknitDistanceLsb_Init (BitknitDistanceLsb *model);
void BitknitDistanceBits_Init (BitknitDistanceBits *model);

void BitknitLiteral_Adaptive (BitknitLiteral *model, uint32_t sym);
void BitknitDistanceLsb_Adaptive (BitknitDistanceLsb *model, uint32_t sym);
void BitknitDistanceBits_Adaptive (BitknitDistanceBits *model, uint32_t sym);

uint32_t BitknitLiteral_Lookup (BitknitLiteral *model, uint32_t *bits);
uint32_t BitknitDistanceLsb_Lookup (BitknitDistanceLsb *model,
                                     uint32_t *bits);
uint32_t BitknitDistanceBits_Lookup (BitknitDistanceBits *model,
                                      uint32_t *bits);

size_t Bitknit_Decode (const uint8_t *src, const uint8_t *src_end,
                       uint8_t *dst, uint8_t *dst_end, uint8_t *dst_start,
                       BitknitState *bk);
