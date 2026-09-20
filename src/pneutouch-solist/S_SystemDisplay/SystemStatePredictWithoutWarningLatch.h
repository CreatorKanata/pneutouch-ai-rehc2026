/*****************************************************************************
 * File: SystemStatePredictWithoutWarningLatch.h
 * Title: 通常時に表示する推論画面を作成する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file SystemStatePredictWithoutWarningLatch.h
 * @brief 通常時に表示する推論画面を作成する。
 */
#ifndef SYSTEM_STATE_PREDICT_WITHOUT_WARNING_LATCH_H__
#define SYSTEM_STATE_PREDICT_WITHOUT_WARNING_LATCH_H__
#include "SystemStatePredictDraw.h"

/**
 * @brief 通常時の推論画面の表示を制御する関数を設定する。
 * 
 * @param drawFunc PREDICT_DRAW_FUNCの型
 */
void SystemStatePredictWithoutWarningLatchInit(PREDICT_DRAW_FUNC* drawFunc);

/**
 * @brief 警告ラッチなしで使用する描画機能が正しく設定されているか確認する。
 *
 * @param drawFunc PREDICT_DRAW_FUNCのポインタ。SystemStatePredictDrawCheckFuncの戻り値を入れる。
 * @return int 成功時0。失敗時は0以外。
 */
int SystemStatePredictWithoutWarningLatchCheckFunc(PREDICT_DRAW_FUNC* drawFunc);

/**
 * @brief 単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int SystemStatePredictWithoutWarningLatchTest(void);
#endif //SYSTEM_STATE_PREDICT_WITHOUT_WARNING_LATCH_H__
