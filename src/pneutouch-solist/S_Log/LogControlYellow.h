/*****************************************************************************
 * File: LogControlYellow.h
 * Title: WarningYellowの設定の際にログ機能を取り扱う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file LogControlYellow.h
 * @brief WarningYellowの設定の際にログ機能を取り扱う。
 */

#ifndef LOG_CONTROL_YELLOW_H__
#define LOG_CONTROL_YELLOW_H__
#include "LogControl.h"

/**
 * @brief WarningYellowで使用するログ機能の初期化を行う。
 *
 * @param func LOG_FUNC構造体のポインタを入れると本モジュールで使用する関数が設定される。
 */
void LogControlYellowInit(LOG_FUNC* func);

/**
 * @brief WarningYellow設定で使用するログ機能が正しく設定されているか確認する。
 *
 * @param func LOG_FUNC構造体のポインタ。LogControlCheckFuncで取得したデータを入れる。
 * @return int 成功時0。失敗時は0以外。
 */
int LogControlYellowCheckFunc(LOG_FUNC* func);

#endif //LOG_CONTROL_YELLOW_H__
