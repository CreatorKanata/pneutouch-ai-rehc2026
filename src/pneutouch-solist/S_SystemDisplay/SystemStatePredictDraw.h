/*****************************************************************************
 * File: SystemStatePredictDraw.h
 * Title: 推論画面の表示を制御する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file SystemStatePredictDraw.h
 * @brief 推論画面の表示を制御する。
 */
#ifndef SYSTEM_STATE_PREDICT_DRAW_H__
#define SYSTEM_STATE_PREDICT_DRAW_H__
#include "AI.h"


/**
 * @brief 描画担当モジュールの初期化、再初期化関数型。
 */
typedef void (*SystemStatePredictDrawFuncReset)(void);

/**
 * @brief 描画する関数型
 */
typedef void (*SystemStatePredictDrawFuncDraw)(float anomalyValue, ANOMALY anomalyResult);


/**
 * @brief ログ機能を取り扱うための関数型の構造体
 */
typedef struct
{
	SystemStatePredictDrawFuncReset Reset;	/**< モジュールの初期化、再初期化。 */
	SystemStatePredictDrawFuncDraw DrawAnomaly;	/**< 描画 */
}PREDICT_DRAW_FUNC;


/**
 * @brief 推論画面描画機能を初期化する関数型
 */
typedef void (*SystemStatePredictDrawFuncInit)(PREDICT_DRAW_FUNC* drawFunc);

/**
 * @brief 使用する描画インターフェースを設定するための初期化を行う。
 */
void SystemStatePredictDrawInit(PREDICT_DRAW_FUNC* drawFunc);

/**
 * @brief 描画関数を取得する。
 *
 * @return PREDICT_DRAW_FUNC* 描画関数の型
 */
PREDICT_DRAW_FUNC* SystemStatePredictDrawCheckFunc(void);

/**
 * @brief 描画する。
 *
 * @param anomalyValue 異常度
 * @param anomalyResult 異常判定結果
 */
void SystemStatePredictDrawDraw(float anomalyValue, ANOMALY anomalyResult);

/**
 * @brief 描画担当モジュールの初期化、再初期化を行う。
 */
void SystemStatePredictDrawReset(void);

/**
 * @brief 単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int SystemStatePredictDrawTest(void);
#endif //SYSTEM_STATE_PREDICT_DRAW_H__
