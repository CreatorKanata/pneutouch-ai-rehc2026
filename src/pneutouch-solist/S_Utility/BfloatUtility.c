/*****************************************************************************
 * File: BfloatUtility.c
 * Title: bfloat16へのユーティリティ
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file BfloatUtility.c
 * @brief bfloat16へのユーティリティ
 */
#include "BfloatUtility.h"

bfloat16 BfloatUtilityFloatToBfloat16(float data)
{
	uint32_t u32 = *((uint32_t*)&data);
	bfloat16 ret = (bfloat16)(u32 >> 16);
	return ret;
}

float BfloatUtilityBfloat16ToFloat(bfloat16 data)
{
	uint32_t u32 = ((uint32_t)data << 16);
	return *((float*)&u32);
}
