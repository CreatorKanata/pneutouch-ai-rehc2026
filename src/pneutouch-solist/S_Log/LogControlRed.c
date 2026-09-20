/*****************************************************************************
 * File: LogControlRed.c
 * Title: WarningRedの設定の際にログ機能を取り扱う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file LogControlRed.c
 * @brief WarningRedの設定の際にログ機能を取り扱う。
 */
#include "LogControlRed.h"
#include "AI.h"
#include "RX4111.h"
#include <stdio.h>
#include "Sensor.h"

/*
 * @brief ログの保存状況
 */
typedef enum
{
	PREDICT_LOG_CLEAR = 0,	/**< ログなし */
	PREDICT_LOG_RED_SAVED,	/**< 全てログ済 */
}PREDICT_LOG;



static PREDICT_LOG predictLogState = PREDICT_LOG_CLEAR;

static bool isPredictLogRequiredSave = false;
static void redReset(void);
static bool redSaveEndLog(void);
static bool redIsPredicLogRequiredSave(void);
static void redManageLog(void);
inline static bool isThereCapacityForLog(void);
static bool redSavePredictLog(void);

static bool isConditionForRedLogMetUnderClearState(void);







static void prepareForLog(void);
static void resetLogPreparetion(void);

volatile static ANOMALY currentAnomaly = ANOMALY_NORMAL;


void LogControlRedInit(LOG_FUNC* func)
{
	func->Reset = redReset;
	func->Fin = NULL;
	func->SaveEndLog = redSaveEndLog;
	func->IsPredictLogRequiredSave = redIsPredicLogRequiredSave;
	func->SavePredictLog = redSavePredictLog;
	AISetAIPredictCallBack(redManageLog);
}

int LogControlRedCheckFunc(LOG_FUNC* func)
{
	if(func->Reset != redReset) return -1;
	if(func->Fin != NULL) return -2;
	if(func->SaveEndLog != redSaveEndLog) return -3;
	if(func->IsPredictLogRequiredSave != redIsPredicLogRequiredSave) return -4;
	if(func->SavePredictLog != redSavePredictLog) return -5;
	return 0;
}

static void redReset(void)
{
	predictLogState = PREDICT_LOG_CLEAR;
	isPredictLogRequiredSave = false;
	currentAnomaly = ANOMALY_NORMAL;
}


static bool redSaveEndLog(void)
{
	AI_CONTEXT* aiContext = AIGetLatestAIContext();

	if(aiContext->LogSave == LOG_SAVE_NO)
	{
		LogControlCommonSaveLog(LOG_FACTOR_END,LOG_SAVE_YES);
		return true;
	}
	return false;
}


//推論ログの保存が要求されているか
static bool redIsPredicLogRequiredSave(void)
{
	return isPredictLogRequiredSave;
}


//ログ管理をする。ログを取るならその準備をする。
static void redManageLog(void)
{
	//異常判定結果を更新
	currentAnomaly = AIGetCurrentAnomalyResult();

	if(isThereCapacityForLog()) 
	{
		prepareForLog();
	}
	
}

//推論ログを取ることができるか
inline static bool isThereCapacityForLog(void)
{
	switch(predictLogState)
	{
		case PREDICT_LOG_CLEAR:
			if(isConditionForRedLogMetUnderClearState()) return true;
			else return false;
		
		case PREDICT_LOG_RED_SAVED:
			return false;		
	}
}

//推論時に赤色閾値を超えたらログ。
static bool redSavePredictLog(void)
{
	if(isConditionForRedLogMetUnderClearState())
	{
		LogControlCommonSaveLog(LOG_FACTOR_WARNING_RED,LOG_SAVE_YES);
		predictLogState = PREDICT_LOG_RED_SAVED;
		resetLogPreparetion();
		return true;
	}
	return false;
}

//ログ無し状態で赤色閾値超えログを取る条件を満たしたか
static bool isConditionForRedLogMetUnderClearState(void)
{
	if(predictLogState != PREDICT_LOG_CLEAR) return false;
	if(ANOMALY_RED == currentAnomaly) return true;
	return false;
}

//ログを取るための準備
static void prepareForLog(void)
{
	SensorStop();
	isPredictLogRequiredSave = true;
}

//ログを行うと今までの活動を再開
static void resetLogPreparetion(void)
{
	SensorStart();
	isPredictLogRequiredSave = false;
}
