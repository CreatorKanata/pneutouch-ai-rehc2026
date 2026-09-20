/*****************************************************************************
 * File: LogControlEnd.c
 * Title: End設定の際にログ機能を取り扱う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file LogControlEnd.c
 * @brief End設定の際にログ機能を取り扱う。
 */

#include "LogControlEnd.h"
#include "AI.h"
#include "RX4111.h"
#include <stdio.h>

static bool endSaveEndLog(void);


void LogControlEndInit(LOG_FUNC* func)
{
	func->Reset = NULL;
	func->Fin = NULL;
	func->SaveEndLog = endSaveEndLog;
	func->IsPredictLogRequiredSave = NULL;
	func->SavePredictLog = NULL;
}

int LogControlEndCheckFunc(LOG_FUNC* func)
{
	if(func->Reset != NULL) return -1;
	if(func->Fin != NULL) return -2;
	if(func->SaveEndLog != endSaveEndLog) return -3;
	if(func->IsPredictLogRequiredSave != NULL) return -4;
	if(func->SavePredictLog != NULL) return -5;
	return 0;
}

static bool endSaveEndLog(void)
{
	//ログ状態を設定を見る。
	AI_CONTEXT* aiContext = AIGetLatestAIContext();

	//学習推論終了時にログ 既に同じデータをログしていた場合はしない。
	if(aiContext->LogSave == LOG_SAVE_NO)
	{
		LogControlCommonSaveLog(LOG_FACTOR_END,LOG_SAVE_YES);
		return true;
	}
	return false;
}


int LogControlEndTest(void)
{
	LOG_FUNC func;
	AI_CONTEXT* aiContext = AIGetLatestAIContext();
	RX4111_DATE_TIME dateTime = 
	{
		.Year = 25,
		.Month = 4,
		.Day = 1,
		.Hour = 6,
		.Minute = 0,
		.Sec = 0,
	};
	
	LogControlEndInit(&func);
	if(LogControlEndCheckFunc(&func))return -1;
	
	RX4111SetTime(&dateTime);
	aiContext->LogSave = LOG_SAVE_YES;
	if(endSaveEndLog()) return -1;
	//1件ログ計1
	aiContext->LogSave = LOG_SAVE_NO;
	if(!endSaveEndLog()) return -1;
	return 0;
}
