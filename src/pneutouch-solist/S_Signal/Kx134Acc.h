/*****************************************************************************
 * File: Kx134Acc.h
 * Title: MEMS加速度センサーKx134-1211を制御するモジュール
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file Kx134Acc.h
 * @brief MEMS加速度センサーKx134-1211を制御するモジュール
 */
#ifndef KX134_ACC_H__
#define KX134_ACC_H__

#include <stdint.h>
#include <stdbool.h>
#include "AIContext.h"
#include "SensorCommon.h"


/**
 * @brief モジュールの初期化。
 */
void Kx134AccInitialize(void);

/**
 * @brief このモジュールでデータ入力バッファとして使用するAI_CONTEXTを設定する。
 * ダブルバッファリングするため2つ必要。
 * 
 * @param con1 1つ目のAI_CONTEXT
 * @param con2 2つ目のAI_CONTEXT
 */
void Kx134AccSetAiContextForStoring(AI_CONTEXT* con1, AI_CONTEXT* con2);

/**
 * @brief センサーによってデータが格納されたAiContextを取得する。
 * Kx134AccSetBufferFullCallback によって通知された後に呼ぶこと。
 * 
 * @return AI_CONTEXT* データが格納されたAI_CONTEXTへのポインタ
 */
AI_CONTEXT* Kx134AccGetAiContextDataStored(void);

/**
 * @brief Kx134AccGetAiContextDataStoredによって取得したAiContextを使い終わった事をこのモジュールに知らせる。
 */
void Kx134AccCompletedUsingAiContext(void);

/**
 * @brief バッファサイズ分のデータが貯まった時に呼ばれるコールバック関数を設定する。
 * 
 * @param func コールバック関数
 */
void Kx134AccSetBufferFullCallback(SensorBufferFullCallbackFunc func);

/**
 * @brief センサー入力を開始する。
 */
void Kx134AccStart(void);

/**
 * @brief センサー入力を停止する。
 */
void Kx134AccStop(void);

/**
 * @brief センサー入力中かを確認する。
 * 
 * @return true センサー入力中
 * @return false センサー入力停止中
 */
bool Kx134AccIsRunning(void);
#endif //KX134_ACC_H__
