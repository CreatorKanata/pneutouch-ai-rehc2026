/*****************************************************************************
 * File: TimeControl.h
 * Title: Timer1を制御して時間待ち、タイムアウト検出を行う。
 * LastUpdated: 2025.05.29
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file TimeControl.h
 * @brief 時間待ち、TimeOut検出を行う。同時使用は不可。Timer1を制御する。
 */
#ifndef TIME_CONTROL_H__
#define TIME_CONTROL_H__
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief TimeControlの初期化を行う。
 */
void TimeControlInit(void);

/**
 * @brief ms単位での同期待ちを行う。
 *
 * @param xms 1~1000
 * @return bool 引数が適切な範囲でないとfalseを返す。
 */
bool TimeControlDelayMs(uint16_t xms);

/**
 * @brief ms単位でタイムアウト検出。
 *
 * @param xms 1~1000
 * @return bool 引数が適切な範囲でないとfalseを返す。
 */
bool TimeControlSetTimeOutMs(uint16_t xms);

/**
 * @brief タイムアウトしているかどうかを確認する。 
 *
 * @return bool タイムアウトしているとtrueを返す。
 */
bool TimeControlIsTimeOut(void);

/**
 * @brief タイムアウト検出前に正常終了した時の後処理を行う。 
 */
void TimeControlTimeOutMsDispose(void);


/**
 * @brief 単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int TimeControlTest(void);
#endif //TIME_CONTROL_H__
