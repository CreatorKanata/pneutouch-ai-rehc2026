/*****************************************************************************
 * File: LogControlYellow.c
 * Title: WarningYellowの設定の際にログ機能を取り扱う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file LogControlYellow.c
 * @brief WarningYellowの設定の際にログ機能を取り扱う。
 */


#include "LogControlYellow.h"
#include "AI.h"
#include "RX4111.h"
#include <stdio.h>
#include "Sensor.h"


/*
 * @brief ログの保存状況
 */
typedef enum
{
	PREDICT_LOG_CLEAR = 0,			/**< ログなし */
	PREDICT_LOG_YELLOW_SAVED,		/**< 黄色閾値超えログ済 */
	PREDICT_LOG_RED_SAVED,			/**< 赤色閾値超えログ済 */
	PREDICT_LOG_YELLOW_RED_SAVED,	/**< 全てログ済 */
}PREDICT_LOG;

static PREDICT_LOG predictLogState = PREDICT_LOG_CLEAR;

static bool isPredictLogRequiredSave = false;
static void yellowReset(void);
static bool yellowSaveEndLog(void);
static bool yellowIsPredicLogRequiredSave(void);
static void yellowManageLog(void);
inline static bool isThereCapacityForLog(void);
static bool yellowSavePredictLog(void);

static bool isConditionForYellowLogMetUnderClearState(void);
static bool isConditionForRedLogMetUnderClearState(void);
static bool isConditionForRedLogMetUnderYellowSavedState(void);
static bool isConditionForYellowLogMetUnderRedSavedState(void);

static bool savePredictLogInClearState(void);
static bool savePredictLogInYellowlogState(void);
static bool savePredictLogInRedLogState(void);

static void prepareForLog(void);
static void resetLogPreparetion(void);

volatile static ANOMALY currentAnomaly = ANOMALY_NORMAL;
volatile static ANOMALY oldAnomaly = ANOMALY_NORMAL;

void LogControlYellowInit(LOG_FUNC* func)
{
	func->Reset = yellowReset;
	func->Fin = NULL;
	func->SaveEndLog = yellowSaveEndLog;
	func->IsPredictLogRequiredSave = yellowIsPredicLogRequiredSave;
	func->SavePredictLog = yellowSavePredictLog;
	AISetAIPredictCallBack(yellowManageLog);
}

int LogControlYellowCheckFunc(LOG_FUNC* func)
{
	if(func->Reset != yellowReset) return -1;
	if(func->Fin != NULL) return -2;
	if(func->SaveEndLog != yellowSaveEndLog) return -3;
	if(func->IsPredictLogRequiredSave != yellowIsPredicLogRequiredSave) return -4;
	if(func->SavePredictLog != yellowSavePredictLog) return -5;
	return 0;
}

static void yellowReset(void)
{
	predictLogState = PREDICT_LOG_CLEAR;
	isPredictLogRequiredSave = false;
	currentAnomaly = ANOMALY_NORMAL;
	oldAnomaly = ANOMALY_NORMAL;
}

static bool yellowSaveEndLog(void)
{
	AI_CONTEXT* aiContext = AIGetLatestAIContext();

	//学習推論終了時にログ 既に同じデータをログしていた場合はしない。
	if(aiContext->LogSave == LOG_SAVE_NO)
	{
		LogControlCommonSaveLog(LOG_FACTOR_END,LOG_SAVE_YES);
		return true;
	}		
	return false;
}

//推論ログの保存が要求されているか
static bool yellowIsPredicLogRequiredSave(void)
{
	return isPredictLogRequiredSave;
}

//ログ管理をする。ログを取るならその準備をする。
static void yellowManageLog(void)
{
	//異常判定結果を更新
	currentAnomaly = AIGetCurrentAnomalyResult();
	oldAnomaly = AIGetOldAnomalyResult();
	
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
			if(isConditionForYellowLogMetUnderClearState()) return true;
			else if(isConditionForRedLogMetUnderClearState()) return true;
			else return false;
		
		case PREDICT_LOG_YELLOW_SAVED:
			if(isConditionForRedLogMetUnderYellowSavedState()) return true;
			else return false;
		
		case PREDICT_LOG_RED_SAVED:
			if(isConditionForYellowLogMetUnderRedSavedState()) return true;
			else return false;
			
		case PREDICT_LOG_YELLOW_RED_SAVED:
			return false;		
	}
}

//推論ログを取る
static bool yellowSavePredictLog(void)
{
	switch(predictLogState)
	{
		case PREDICT_LOG_CLEAR:
			return savePredictLogInClearState();
		case PREDICT_LOG_YELLOW_SAVED:
			return savePredictLogInYellowlogState();
		case PREDICT_LOG_RED_SAVED:
			return savePredictLogInRedLogState();
		case PREDICT_LOG_YELLOW_RED_SAVED:
			break;
	}
	return false;
}

//CLEAR状態でログを取る
static bool savePredictLogInClearState(void)
{
	if(isConditionForYellowLogMetUnderClearState())
	{
		LogControlCommonSaveLog(LOG_FACTOR_WARNING_YELLOW,LOG_SAVE_YES);
		predictLogState = PREDICT_LOG_YELLOW_SAVED;
		resetLogPreparetion();
		return true;
	}
	
	if(isConditionForRedLogMetUnderClearState())
	{
		LogControlCommonSaveLog(LOG_FACTOR_WARNING_RED,LOG_SAVE_YES);
		predictLogState = PREDICT_LOG_RED_SAVED;
		resetLogPreparetion();
		return true;
	}
	return false;
}

//黄色閾値超えログ済み状態でログを取る
static bool savePredictLogInYellowlogState(void)
{
	if(isConditionForRedLogMetUnderYellowSavedState())
	{
		LogControlCommonSaveLog(LOG_FACTOR_WARNING_RED,LOG_SAVE_YES);
		predictLogState = PREDICT_LOG_YELLOW_RED_SAVED;
		resetLogPreparetion();
		return true;
	}
	return false;
}

//赤色閾値超えログ済み状態でログを取る
static bool savePredictLogInRedLogState(void)
{
	if(isConditionForYellowLogMetUnderRedSavedState())
	{
		LogControlCommonSaveLog(LOG_FACTOR_WARNING_YELLOW,LOG_SAVE_YES);
		predictLogState = PREDICT_LOG_YELLOW_RED_SAVED;
		resetLogPreparetion();
		return true;
	}
	return false;
}


//ログ無し状態で黄色閾値超えログを取る条件を満たしたか
static bool isConditionForYellowLogMetUnderClearState(void)
{
	if(ANOMALY_NORMAL != oldAnomaly) return false;
	if(ANOMALY_YELLOW == currentAnomaly) return true;
	return false;
}

//ログ無し状態で赤色閾値超えログを取る条件を満たしたか
static bool isConditionForRedLogMetUnderClearState(void)
{
	if(ANOMALY_NORMAL != oldAnomaly) return false;
	if(ANOMALY_RED == currentAnomaly) return true;
	return false;
}

//黄色閾値超えログ済状態で赤色閾値超えログを取る条件を満たしたか
static bool isConditionForRedLogMetUnderYellowSavedState(void)
{
	if(predictLogState != PREDICT_LOG_YELLOW_SAVED) return false;
	if(ANOMALY_RED == oldAnomaly) return false;
	if(ANOMALY_RED == currentAnomaly) return true;
	return false;
}

//赤色閾値超えログ済状態で黄色閾値超えログを取る条件を満たしたか
static bool isConditionForYellowLogMetUnderRedSavedState(void)
{
	if(predictLogState != PREDICT_LOG_RED_SAVED) return false;
	if(ANOMALY_NORMAL != oldAnomaly) return false;
	if(ANOMALY_YELLOW == currentAnomaly) return true;
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
