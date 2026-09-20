/*****************************************************************************
 * File: PeriodicHandler10ms.h
 * Title: 10msタイマー割込みを使用する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file PeriodicHandler10ms.h
 * @brief 10msタイマー割込みを使用する。
 */
#ifndef PERIODIC_HANDLER_10MS__
#define PERIODIC_HANDLER_10MS__

/**
 * @brief 10msタイマー割り込みで実行する関数型
 */
typedef void (*PeriodicHandler10msExe) (void);

/**
 * @brief 10msタイマー割り込みの初期化。
 */
void PeriodicHandler10msInit(void);

/**
 * @brief 10msタイマー割り込みで使用する関数を設定する。
 *
 * @param exe タイマー割り込みで実行する関数
 */
void PeriodicHandler10msSetCallBack(PeriodicHandler10msExe exe);
#endif //PERIODIC_HANDLER_10MS__
