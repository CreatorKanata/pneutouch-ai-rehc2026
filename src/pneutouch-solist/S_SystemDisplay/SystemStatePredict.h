/*****************************************************************************
 * File: SystemStatePredict.h
 * Title: 推論画面を表示する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file SystemStatePredict.h
 * @brief 推論画面を表示する。
 */
#ifndef SYSTEM_STATE_PREDICT_H__
#define SYSTEM_STATE_PREDICT_H__

/**
 * @brief AI推論画面を出力する。
 */
void SystemStatePredict(void);

/**
 * @brief 周期的に画面へ表示する。10msのタイマー割り込みハンドラで使用。
 */
void SystemStatePredictDrawTimer(void);
#endif //SYSTEM_STATE_PREDICT_H__
