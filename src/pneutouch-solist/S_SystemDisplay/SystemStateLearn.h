/*****************************************************************************
 * File: SystemStateLearn.h
 * Title: 学習画面を表示する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file SystemStateLearn.h
 * @brief 学習画面を表示する。
 */
#ifndef SYSTEM_STATE_LEARN_H__
#define SYSTEM_STATE_LEARN_H__
#include <stdint.h>

/**
 * @brief AI学習画面を出力する。
 */
void SystemStateLearn(void);

/**
 * @brief 周期的に画面へ表示する。10msのタイマー割り込みハンドラで使用。
 */
void SystemStateLearnDrawTimer(void);
#endif //SYSTEM_STATE_LEARN_H__
