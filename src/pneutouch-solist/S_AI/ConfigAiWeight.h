/*****************************************************************************
 * File: ConfigAiWeight.h
 * Title: AI重み設定モジュール
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file ConfigAiWeight.h
 * @brief AI重み設定モジュール
 */

#ifndef CONFIG_AI_WEIGHT_H__
#define CONFIG_AI_WEIGHT_H__

#include <stdint.h>
#include "solistAi.h"	// bfloat16の定義があるため

/**
 * @brief 重み種類を示す列挙型
 */
typedef enum
{
	WEIGHT_KIND_BETA = 0,	/**< 重みBETA */
	WEIGHT_KIND_P = 1,		/**< 重みP */
} WEIGHT_KIND;

/**
 * @brief 重みを保存する
 * 
 * @param kind 保存対象の重み種類
 * @param index 0から始まる保存先インデックス値
 * @param data 保存値
 * @return int 成功時0、失敗時-1
 */
int ConfigAiWeightWrite(WEIGHT_KIND kind, uint32_t index, bfloat16 data);

/**
 * @brief 重みを取得する
 * 
 * @param kind 取得対象の重み種類
 * @param index 0から始まる読込先インデックス値
 * @param data 読込値
 * @return int 成功時0、失敗時-1
 */
int ConfigAiWeightRead(WEIGHT_KIND kind, uint32_t index, bfloat16* data);

/**
 * @brief 重みのindex値が有効か検証する
 * 
 * @param kind 検証するindex値の重み種類
 * @param index 検証するindex値
 * @return int 成功時0、失敗時-1
 */
int ConfigAiWeightValidateIndex(WEIGHT_KIND kind, uint32_t index);

/**
 * @brief 重みをクリアする。
 * 
 * @return int 成功時0、失敗時-1
 */
int ConfigAiWeightClear(void);

/**
 * @brief 単体テスト
 * 
 * @return int テスト結果。成功時0
 */
int ConfigAiWeightTest(void);

#endif // CONFIG_AI_WEIGHT_H__
