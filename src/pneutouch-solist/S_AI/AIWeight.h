/*****************************************************************************
 * File: AIWeight.h
 * Title: AIの重みデータを読み書きする。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file AIWeight.h
 * @brief AIの重みデータを読み書きする。
 */
#ifndef AI_WEIGHT_H__
#define	AI_WEIGHT_H__
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief AIの重みデータ(Beta、P)をAIから読み出してFRAMに保存する。
 *
 * @param instance AIモデル番号。0か1。このアプリでは同時には使用できない。
 * @param hiddenSize AIの隠れ層ノード数。1~64
 * @param outputSize AIの出力層ノード数。1~256
 * @return bool 引数が適切な範囲でないとfalseを返す。
 */
bool AIWeightExportWeightBetaAndPFromAIToFram(uint8_t instance, uint16_t hiddenSize,uint16_t outputSize);

/**
 * @brief AIの重みデータ(Beta、P)をFRAMから読み出してAIに書き込む。
 * 
 * @param instance AIモデル番号。0か1。このアプリでは同時には使用できない。
 * @param hiddenSize AIの隠れ層ノード数。1~64
 * @param outputSize AIの出力層ノード数。1~256
 * @return bool 引数が適切な範囲でないとfalseを返す。
 */
bool AIWeightImportWeightBetaAndPFromFramToAI(uint8_t instance, uint16_t hiddenSize,uint16_t outputSize);
#endif //AI_WEIGHT_H__
