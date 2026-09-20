/*****************************************************************************
 * File: LogControlEnd.h
 * Title: End設定の際にログ機能を取り扱う。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file LogControlEnd.h
 * @brief End設定の際にログ機能を取り扱う。
 */
#ifndef LOG_CONTROL_END_H__
#define LOG_CONTROL_END_H__
#include "LogControl.h"

/**
 * @brief End設定で使用するログ機能の初期化を行う。
 *
 * @param func LOG_FUNC構造体のポインタを入れると本モジュールで使用する関数が設定される。
 */
void LogControlEndInit(LOG_FUNC* func);

/**
 * @brief End設定で使用するログ機能が正しく設定されているか確認する。
 *
 * @param func LOG_FUNC構造体のポインタ。LogControlCheckFuncで取得したデータを入れる。
 * @return int 成功時0。失敗時は0以外。
 */
int LogControlEndCheckFunc(LOG_FUNC* func);

/**
 * @brief 単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int LogControlEndTest(void);
#endif //LOG_CONTROL_END_H__
