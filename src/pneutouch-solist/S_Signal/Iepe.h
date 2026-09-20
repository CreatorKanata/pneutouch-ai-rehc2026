/*****************************************************************************
 * File: Iepe.h
 * Title: IEPEセンサーモジュール
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file Iepe.h
 * @brief IEPEセンサーモジュール
 * @details 
 * IEPEセンサーからの入力を行うモジュールです。
 */

#ifndef IEPE_H__
#define IEPE_H__

#include <stdint.h>
#include <stdbool.h>
#include "AIContext.h"
#include "SensorCommon.h"

/**
 * @brief モジュールの初期化。
 */
void IepeInitialize(void);

/**
 * @brief このモジュールでデータ入力バッファとして使用するAI_CONTEXTを設定する。
 * ダブルバッファリングするため2つ必要。
 * 
 * @param con1 1つ目のAI_CONTEXT
 * @param con2 2つ目のAI_CONTEXT
 */
void IepeSetAiContextForStoring(AI_CONTEXT* con1, AI_CONTEXT* con2);

/**
 * @brief センサーによってデータが格納されたAiContextを取得する。
 * IepeSetBufferFullCallback によって通知された後に呼ぶこと。
 * 
 * @return AI_CONTEXT* データが格納されたAI_CONTEXTへのポインタ
 */
AI_CONTEXT* IepeGetAiContextDataStored(void);

/**
 * @brief IepeGetAiContextDataStoredによって取得したAiContextを使い終わった事をこのモジュールに知らせる
 */
void IepeCompletedUsingAiContext(void);

/**
 * @brief バッファサイズ分のデータが貯まった時に呼ばれるコールバック関数を設定する。
 * 
 * @param func コールバック関数
 */
void IepeSetBufferFullCallback(SensorBufferFullCallbackFunc func);

/**
 * @brief センサー入力を開始する。
 */
void IepeStart(void);

/**
 * @brief センサー入力を停止する。
 */
void IepeStop(void);

/**
 * @brief センサー入力中かを確認する。
 * 
 * @return true センサー入力中
 * @return false センサー入力停止中
 */
bool IepeIsRunning(void);

/**
 * @brief 単体テスト
 * 
 * @return int テスト結果。成功時0。
 */
int IepeTest(void);

#endif // IEPE_H__
