/*****************************************************************************
 * File: LogControl.c
 * Title: ログ機能を取り扱う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file LogControl.c
 * @brief ログ機能を取り扱う。
 */

#include "LogControl.h"
#include <stdio.h>
#include "AI.h"
#include "RX4111.h"
#include "AILog.h"
#include "BfloatUtility.h"
#include "ConfigData.h"

static LOG_FUNC func;

void LogControlInit(LOG_FUNC* instance)
{
	func = *instance;
}

LOG_FUNC* LogControlCheckFunc(void)
{
	return &func;
}

//モジュール初期化
void LogControlReset(void)
{
	if(func.Reset != NULL) func.Reset();
}


//モジュール終了
void LogControlFin(void)
{
	if(func.Fin != NULL) func.Fin();
}

//学習推論終了時にログ
void LogControlSaveEndLog(void)
{
	if(func.SaveEndLog != NULL) func.SaveEndLog();
}

//推論ログの保存が要求されているか
bool LogControlIsPredictLogRequiredSave(void)
{
	if(func.IsPredictLogRequiredSave != NULL) return func.IsPredictLogRequiredSave();
	return true;
}

//推論中にログ
void LogControlSavePredictLog(void)
{
	if(func.SavePredictLog != NULL) func.SavePredictLog();
}

void LogControlCommonSaveLog(LOG_FACTOR factor,LOG_SAVE save)
{
	AI_CONTEXT* aiContext = AIGetLatestAIContext();
	RX4111_DATE_TIME dateTime;
	
	//ログ要因
	aiContext->LogInfo.Factor = factor;
	//時刻
	RX4111GetTime(&dateTime);
	aiContext->LogInfo.Year = dateTime.Year;
	aiContext->LogInfo.MonthDayHour.Month = dateTime.Month;
	aiContext->LogInfo.MonthDayHour.Day = dateTime.Day;
 	aiContext->LogInfo.MonthDayHour.Hour = dateTime.Hour;
	aiContext->LogInfo.MinuteSecond.Minute = dateTime.Minute;
	aiContext->LogInfo.MinuteSecond.Second = dateTime.Sec;
	//ログ保存状態
	aiContext->LogSave = save;
	//ログ保存
	AILogSave(&aiContext->LogInfo);
}


int LogControlTest(void)
{
	LOG_FUNC writeTestFunc = {.Reset = NULL, .Fin = NULL, .SaveEndLog = NULL, .IsPredictLogRequiredSave = NULL, .SavePredictLog = NULL};
	LOG_FUNC *readTestFunc;

	LogControlInit(&writeTestFunc);
	readTestFunc = LogControlCheckFunc();
	if(writeTestFunc.Reset != NULL) return -1;
	if(writeTestFunc.Reset != readTestFunc->Reset) return -1;
	if(writeTestFunc.Fin != NULL) return -1;
	if(writeTestFunc.Fin != readTestFunc->Fin) return -1;
	if(writeTestFunc.SaveEndLog != NULL) return -1;
	if(writeTestFunc.SaveEndLog != readTestFunc->SaveEndLog) return -1;
	if(writeTestFunc.IsPredictLogRequiredSave != NULL) return -1;
	if(writeTestFunc.IsPredictLogRequiredSave != readTestFunc->IsPredictLogRequiredSave) return -1;
	if(writeTestFunc.SavePredictLog != NULL) return -1;
	if(writeTestFunc.SavePredictLog != readTestFunc->SavePredictLog) return -1;
	LogControlReset();
	LogControlFin();
	LogControlSaveEndLog();
	LogControlIsPredictLogRequiredSave();
	LogControlSavePredictLog();
	return 0;
}


int LogControlLogTest(void)
{
	//AIInitを呼び出した後
	AI_CONTEXT* context = AIGetLatestAIContext();
	RX4111_DATE_TIME datetime = {25,5,5,15,55,0};
	const uint8_t LOG_END = 1;
	LOG_INFO readLog;
	AILogInitialize();
	AILogAllElase();
	
	for(int16_t i = 0; i < AI_CONTEXT_INPUT_SOURCE_SIZE; i++)
	{
		context->LogInfo.InputData[i] = i;
	}
	context->LogInfo.Predict.ChunkNo = 55555;
	context->LogInfo.Predict.Reserve1 = 0;
	context->LogInfo.Predict.Reserve2 = 0;
	context->LogInfo.Predict.Reserve3 = 0;
	for(uint16_t t = 0; t < AI_CONTEXT_FFT_OUTPUT_SIZE; t++)
	{
		context->LogInfo.Predict.Fft[t] = BfloatUtilityFloatToBfloat16(t);
	}
	context->LogInfo.Predict.Anomaly = BfloatUtilityFloatToBfloat16(1000);
	
	ConfigDataSetUint8Value(EN_CONFIG_SAVE_LOG, LOG_END);
	AILogResetNextWriteIndex();
	RX4111SetTime(&datetime);
	LogControlCommonSaveLog(LOG_FACTOR_END,LOG_SAVE_NO);
	
	AILogLoad(0,&readLog);
	//ログ要因
	if(readLog.Factor != LOG_FACTOR_END) return -1;
	//タイムスタンプ
	if(readLog.Year != 25) return -2;
	if(readLog.MonthDayHour.Month != 5) return -2;
	if(readLog.MonthDayHour.Day != 5) return -2;
	if(readLog.MonthDayHour.Hour!= 15) return -2;
	if(readLog.MinuteSecond.Minute != 55) return -2;
	//センサーデータ
	for(int16_t i = 0; i < AI_CONTEXT_INPUT_SOURCE_SIZE; i++)
	{
		if(readLog.InputData[i] != i) return -3;
	}
	//ブロックデータ
	if(readLog.Predict.ChunkNo != 55555) return -4;
	if(readLog.Predict.Reserve1 != 0) return -4;
	if(readLog.Predict.Reserve2 != 0) return -4;
	if(readLog.Predict.Reserve3 != 0) return -4;
	for(uint16_t t = 0; t < AI_CONTEXT_FFT_OUTPUT_SIZE; t++)
	{
		if(readLog.Predict.Fft[t] != BfloatUtilityFloatToBfloat16(t)) return -4;
	}
	if(readLog.Predict.Anomaly != BfloatUtilityFloatToBfloat16(1000)) return -4;
	
	return 0;
}
