/*****************************************************************************
 * File: BfloatUtility.h
 * Title: bfloat16へのユーティリティ
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file BfloatUtility.h
 * @brief bfloat16へのユーティリティ
 */

#ifndef BFLOAT_UTILITY_H__
#define BFLOAT_UTILITY_H__

#include "solistAi.h"	// bfloat16の定義があるため

/**
 * @brief float型をbfloat16型に変換する
 * 
 * @param data 変換するfloat型データ
 * @return bfloat16 変換されたbfloat16型データ
 */
bfloat16 BfloatUtilityFloatToBfloat16(float data);

/**
 * @brief bfloat16型をfloat型に変換する
 * 
 * @param data 変換するbfloat16型データ
 * @return float 変換されたfloat型データ
 */
float BfloatUtilityBfloat16ToFloat(bfloat16 data);

#endif // BFLOAT_UTILITY_H__
