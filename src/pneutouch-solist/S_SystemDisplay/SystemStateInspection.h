/*****************************************************************************
 * File: SystemStateInspection.h
 * Title: 検査画面を制御する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file SystemStateInspection.h
 * @brief 検査画面を制御する。
 */
#ifndef SYSTEM_STATE_INSEPCTION_H__
#define SYSTEM_STATE_INSEPCTION_H__

/**
 * @brief 検査システムを制御する画面を出力する。
 */
void SystemStateInspection(void);

/**
 * @brief 周期的に画面へ表示する。10msのタイマー割り込みハンドラで使用。
 */
void SystemStateInspectionDrawTimer(void);

/**
 * @brief 単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int SystemStateInspectionTest(void);
#endif //SYSTEM_STATE_INSEPCTION_H__
