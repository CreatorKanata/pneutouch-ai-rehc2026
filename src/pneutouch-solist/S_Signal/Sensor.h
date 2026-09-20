/*****************************************************************************
 * File: Sensor.h
 * Title: 複数あるセンサーのどれを使うかを、設定によって切り替えるためのラッパーモジュール
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file Sensor.h
 * @brief 複数あるセンサーのどれを使うかを、設定によって切り替えるためのラッパーモジュール
 * 
 * システムで使用するセンサーは全てこのモジュールを介してアクセスする
 */

#ifndef SENSOR_H__
#define SENSOR_H__

#include <stdint.h>
#include <stdbool.h>
#include "AIContext.h"
#include "SensorCommon.h"

/**
 * @brief モジュールの初期化
 */
void SensorInitialize(void);

/**
 * @brief このモジュールでデータ入力バッファとして使用するAI_CONTEXTを設定する
 * 
 * ダブルバッファリングするため2つ必要。
 * 
 * @param con1 1つ目のAI_CONTEXT
 * @param con2 2つ目のAI_CONTEXT
 */
void SensorSetAiContextForStoring(AI_CONTEXT* con1, AI_CONTEXT* con2);

/**
 * @brief センサーによってデータが格納されたAiContextを取得する
 * 
 * SensorSetBufferFullCallback によって通知された後に呼ぶこと。
 * 
 * @return AI_CONTEXT* データが格納されたAI_CONTEXTへのポインタ
 */
AI_CONTEXT* SensorGetAiContextDataStored(void);

/**
 * @brief SensorGetAiContextDataStoredによって取得したAiContextを使い終わった事をこのモジュールに知らせる
 */
void SensorCompletedUsingAiContext(void);

/**
 * @brief バッファサイズ分のデータが貯まった時に呼ばれるコールバック関数を設定する
 * 
 * @param func コールバック関数
 */
void SensorSetBufferFullCallback(SensorBufferFullCallbackFunc func);

/**
 * @brief センサー入力を開始する
 */
void SensorStart(void);

/**
 * @brief センサー入力を停止する
 */
void SensorStop(void);

/**
 * @brief センサー入力中かを確認する
 * 
 * @return true センサー入力中
 * @return false センサー入力停止中
 */
bool SensorIsRunning(void);

/**
 * @brief センサーテスト
 * 
 * @return int テスト結果。成功時0
 */
int SensorTest(void);

#endif // SENSOR_H__
