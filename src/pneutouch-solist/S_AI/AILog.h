/*****************************************************************************
 * File: AILog.h
 * Title: 推論結果ログモジュール
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file AILog.h
 * @brief 推論結果ログモジュール
 */

#ifndef AI_LOG_H__
#define AI_LOG_H__

#include "AIContext.h"

/**
 * @brief 推論結果ログモジュールを初期化する。
 * 
 * @return int 成功時0、失敗時-1
 */
int AILogInitialize(void);

/**
 * @brief 推論結果ログを保存する。
 * 
 * @param logInfo 保存値
 * @return int 成功時0、失敗時-1
 */
int AILogSave(const LOG_INFO* logInfo);

/**
 * @brief 推論結果ログを取得する。
 * 
 * @param index 0から始まる読込先インデックス値
 * @param logInfo 取得先
 * @return int 成功時0、失敗時-1
 */
int AILogLoad(uint16_t index, LOG_INFO* logInfo);

/**
 * @brief 全ての推論ログを消去する。
 * 
 * @return int 成功時0、失敗時-1
 */
int AILogAllElase(void);

/**
 * @brief 単体テスト
 * 
 * @return int テスト結果。成功時0
 */
int AILogTest(void);

/**
 * @brief 単体テスト用。ログ書き込み先を0番目にする。
 */
void AILogResetNextWriteIndex(void);
#endif // AI_LOG_H__
