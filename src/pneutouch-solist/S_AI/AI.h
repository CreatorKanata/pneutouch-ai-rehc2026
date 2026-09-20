/*****************************************************************************
 * File: AI.h
 * Title: AIを制御する。
 * LastUpdated: 2025.05.23
******************************************************************************/
/**
 * @file AI.h
 * @brief AIを制御する。
 */

#ifndef AI_H__
#define	AI_H__

#include "solistAi.h"
#include "AIContext.h"
#include "Shutdown.h"

/**
 * @brief 使用するAIのモデル番号
 */
#define AI_MODEL									(0)

/**
 * @brief 学習時の異常度と回数を格納するバッファサイズ
 */
#define NUMBER_OF_CHARACTERS_OF_AI_LEARN_STRING		(17)

/**
 * @brief 推論後に行うコールバックの関数型
 */
typedef void (*AIPredictCallBack)(void);

/**
 * @brief AIの異常判定結果を示す列挙型
 */
typedef enum
{
	ANOMALY_NORMAL = 0,								/**< 正常 */
	ANOMALY_YELLOW,									/**< 黄色警告異常 */
	ANOMALY_RED,									/**< 赤色警告異常 */
}ANOMALY;

/**
 * @brief AI機能の初期化をする。
 *
 * @note 初期化でFRAMに保存している重みデータをAIに読み込む。
 */
void AIInit(void);

/**
 * @brief 学習を開始する。
 */
void AILearnStart(void);

/**
 * @brief 学習を終了する。
 */
void AILearnStop(void);

/**
 * @brief 推論を開始する。
 */
void AIPredictStart(void);

/**
 * @brief 推論を終了する。
 */
void AIPredictStop(void);

/**
 * @brief AIで学習・推論した時の異常値を取得する。
 *
 * @return float
 * @note AI処理で異常値は更新される。AI処理中に呼び出さないなら割り込み禁止して呼び出す。
 */
float AIGetFloatAnomalyValue(void);

/**
 * @brief 現在の判定結果を取得する。
 *
 * @return ANOMALY
 * @note AI処理で判定結果は更新される。AI処理中に呼び出さないなら割り込み禁止して呼び出す。
 */
ANOMALY AIGetCurrentAnomalyResult(void);

/**
 * @brief 一つ前の異常値判定結果を取得する。
 *
 * @return ANOMALY
 */
ANOMALY AIGetOldAnomalyResult(void);

/**
 * @brief AIモデルの重みデータのリセット。 
 */
void AIModelWeightReset(void);

/**
 * @brief AIの学習回数のリセット。 
 */
void AILearningCounterReset(void);

/**
 * @brief  学習時の異常度と回数の情報が入った文字列を取得する。
 *
 * @return char* 必ずバッファサイズ分返す。
 * @note AI処理で文字列は更新される。AI処理中に呼び出さないなら割り込み禁止して呼び出す。
 */
char* AIGetFloatAnomalyValueString(void);

/**
 * @brief AIのチャンク番号のリセット。 
 */
void AIChunkNoReset(void);

/**
 * @brief AI推論後に行う関数を設定する。
 *
 * @param predictCallBack コールバック 
 */
void AISetAIPredictCallBack(AIPredictCallBack predictCallBack);

/**
 * @brief 最新のAIContextのポインタを取得する。
 *
 * @return AI_CONTEXT*
 */
AI_CONTEXT* AIGetLatestAIContext(void);

/**
 * @brief 一番目のAIContextのポインタを取得する。
 *
 * @return AI_CONTEXT*
 */
AI_CONTEXT* AIGetFirstAIContext(void);

/**
 * @brief 二番目のAIContextのポインタを取得する。
 *
 * @return AI_CONTEXT*
 */
AI_CONTEXT* AIGetSecondAIContext(void);
#endif //AI_H__
