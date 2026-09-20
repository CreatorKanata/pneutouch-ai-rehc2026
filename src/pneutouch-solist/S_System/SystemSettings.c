/*****************************************************************************
 * File: SystemSettings.c
 * Title: 設定を読み込んで適切な機能を注入する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file SystemSettings.c
 * @brief 設定を読み込んで適切な機能を注入する。
 */

#include "SystemSettings.h"
#include "ConfigData.h"
#include <stdio.h>
#include "SystemStatePredictDraw.h"
#include "SystemStatePredictWithoutWarningLatch.h"
#include "SystemStatePredictWarningLatch.h"
#include "AILog.h"
#include "LogControl.h"
#include "LogControlEnd.h"
#include "LogControlRed.h"
#include "LogControlYellow.h"

//設定
typedef void (*SettingFunc)(void);
//設定用構造体
typedef struct
{
	SYSTEM_SETTINGS Setting;
	SettingFunc Func;
}SETTING_TABLE;


//警告ラッチを示す列挙型
typedef enum
{
	PREDICT_WITHOUT_WARNING_LATCH = 0,	/**< 警告ラッチなし */
	PREDICT_WARNING_LATCH				/**< 警告ラッチあり */
}PREDICT;
//推論画面テーブル用構造体
typedef struct
{
	PREDICT WarningLatch;
	SystemStatePredictDrawFuncInit DrawInit;
}PREDICT_TABLE;


// ログの保存設定
typedef enum
{
	LOG_UNUSE = 0,						/**< ログ不使用 */
	LOG_END,							/**< 学習推論終了時にログ */
	LOG_WARNING_RED,					/**< ↑に加え赤色警告検出時にログ */
	LOG_WARNING_YELLOW,					/**< ↑に加え黄色警告検出時にログ */
}LOG;
// ログ保存用構造体
typedef struct
{
	LOG LogConfig;
	LogControlFuncInit LogInit;
}LOG_TABLE;


//推論画面設定
static void settingPredict(void);
//ログ設定
static void settingLog(void);
//設定用テーブル
static const SETTING_TABLE settingTable[] =
{
	{SYSTEM_SETTINGS_PREDICT,settingPredict},
	{SYSTEM_SETTINGS_LOG,settingLog}
};

//推論画面初期化
static void predictDrawInit(PREDICT_DRAW_FUNC* drawFunc);
//推論画面初期化チェック
static int checkPredictDrawFuncInit(PREDICT_DRAW_FUNC* drawFunc);
//推論画面設定用テーブル
static const PREDICT_TABLE predictControlInitTable[] =
{
	{PREDICT_WITHOUT_WARNING_LATCH,SystemStatePredictWithoutWarningLatchInit},
	{PREDICT_WARNING_LATCH,SystemStatePredictWarningLatchInit}
};

//ログ不使用設定
static void logUnuseInit(LOG_FUNC* logFunc);
//ログ設定用テーブル
static const LOG_TABLE logControlInitTable[] =
{
	{LOG_UNUSE,logUnuseInit},
	{LOG_END,LogControlEndInit},
	{LOG_WARNING_RED,LogControlRedInit},
	{LOG_WARNING_YELLOW,LogControlYellowInit}	
};

void SystemSettingsInit(void)
{
	PREDICT_DRAW_FUNC drawFunc;
	LOG_FUNC logFunc;
	
	predictDrawInit(&drawFunc);
	SystemStatePredictDrawInit(&drawFunc);
	logUnuseInit(&logFunc);
	LogControlInit(&logFunc);
}

void SystemSettingsRegister(SYSTEM_SETTINGS Setting)
{
	int size = (sizeof(settingTable) / sizeof(SETTING_TABLE));
	for(int i = 0; i < size; i++)
	{
		if( Setting == i )
 		{
			//適切な関数を設定する。
			settingTable[i].Func();
			break;
		}
	}
}

static void settingPredict(void)
{
	uint8_t warningLatch;
	PREDICT_DRAW_FUNC* instance = SystemStatePredictDrawCheckFunc();
	int size = (sizeof(predictControlInitTable) / sizeof(PREDICT_TABLE));
	
	//ラッチ設定を読み込む
	ConfigDataGetUint8Value(EN_CONFIG_WARNING_LATCH,&warningLatch);
	
	//探索
	for(int i = 0; i < size; i++)
	{
		if( warningLatch == i )
 		{
			//適切な関数を設定する。
			predictControlInitTable[i].DrawInit(instance);
			SystemStatePredictDrawInit(instance);
			break;
		}
	}
}

static void predictDrawInit(PREDICT_DRAW_FUNC* drawFunc)
{
	drawFunc->Reset = NULL;
	drawFunc->DrawAnomaly = NULL;
}

static int checkPredictDrawFuncInit(PREDICT_DRAW_FUNC* drawFunc)
{
	if(drawFunc->Reset != NULL) return -1;
	if(drawFunc->DrawAnomaly != NULL) return -1;
	return 0;
}



static void settingLog(void)
{
	LOG logConfig;
	LOG_FUNC* instance = LogControlCheckFunc();
	int size = (sizeof(logControlInitTable) / sizeof(LOG_TABLE));
	ConfigDataGetUint8Value(EN_CONFIG_SAVE_LOG,&logConfig);
	
	for(int i = 0; i < size; i++)
	{
		if( logConfig == i )
 		{
			//適切な関数を設定する。
			logControlInitTable[i].LogInit(instance);
			LogControlInit(instance);
			break;
		}
	}
}

static void logUnuseInit(LOG_FUNC* logFunc)
{
	logFunc->Reset = NULL;
	logFunc->Fin = NULL;
	logFunc->SaveEndLog = NULL;
	logFunc->IsPredictLogRequiredSave = NULL;
	logFunc->SavePredictLog = NULL;
}

static int checkLogFuncInit(LOG_FUNC* logFunc)
{
	if(logFunc->Reset != NULL) return -1;
	if(logFunc->Fin != NULL) return -1;
	if(logFunc->SaveEndLog != NULL) return -1;
	if(logFunc->IsPredictLogRequiredSave != NULL) return -1;
	if(logFunc->SavePredictLog != NULL) return -1;
	return 0;
}



int SystemSettingsTest(void)
{
	PREDICT_DRAW_FUNC* testPredictDrawFunc;
	LOG_FUNC* testLogFunc;
	
	//初期化
	SystemSettingsInit();
	testPredictDrawFunc = SystemStatePredictDrawCheckFunc();
	testLogFunc = LogControlCheckFunc();
	
	if(checkPredictDrawFuncInit(testPredictDrawFunc)) return -1;
	if(checkLogFuncInit(testLogFunc)) return -1;
	if(!LogControlEndCheckFunc(testLogFunc)) return -1;
	if(!LogControlRedCheckFunc(testLogFunc)) return -1;
	if(!LogControlYellowCheckFunc(testLogFunc)) return -1;
	
	
	//推論
	//yellow = 0.3f, red = 0.7f, fullScale = 1.0f
	ConfigDataSetBfloat16Value(EN_CONFIG_YELLOW_THRESHOLD,0.3f);
	ConfigDataSetBfloat16Value(EN_CONFIG_RED_THRESHOLD,0.7f);
	ConfigDataSetBfloat16Value(EN_CONFIG_MAX_ABNORMAL,1.0f);
	
	//警告ラッチなし
	ConfigDataSetUint8Value(EN_CONFIG_WARNING_LATCH,PREDICT_WITHOUT_WARNING_LATCH);
	SystemSettingsRegister(SYSTEM_SETTINGS_PREDICT);
	testPredictDrawFunc = SystemStatePredictDrawCheckFunc();
	if(SystemStatePredictWithoutWarningLatchCheckFunc(testPredictDrawFunc))return -1;
	SystemStatePredictDrawDraw(0.9f,ANOMALY_RED);
	
	//警告ラッチあり
	ConfigDataSetUint8Value(EN_CONFIG_WARNING_LATCH,PREDICT_WARNING_LATCH);
	SystemSettingsRegister(SYSTEM_SETTINGS_PREDICT);
	testPredictDrawFunc = SystemStatePredictDrawCheckFunc();
	if(SystemStatePredictWarningLatchCheckFunc(testPredictDrawFunc))return -1;
	SystemStatePredictDrawDraw(0.9f,ANOMALY_RED);
	SystemStatePredictDrawDraw(0.1f,ANOMALY_NORMAL);
	
	//ログ
	//unuse
	ConfigDataSetUint8Value(EN_CONFIG_SAVE_LOG,LOG_UNUSE);
	SystemSettingsRegister(SYSTEM_SETTINGS_LOG);
	testLogFunc = LogControlCheckFunc();
	if(checkLogFuncInit(testLogFunc))return -1;

	//end
	ConfigDataSetUint8Value(EN_CONFIG_SAVE_LOG,LOG_END);
	SystemSettingsRegister(SYSTEM_SETTINGS_LOG);
	testLogFunc = LogControlCheckFunc();
	if(LogControlEndCheckFunc(testLogFunc)) return -1;

	//red
	ConfigDataSetUint8Value(EN_CONFIG_SAVE_LOG,LOG_WARNING_RED);
	SystemSettingsRegister(SYSTEM_SETTINGS_LOG);
	testLogFunc = LogControlCheckFunc();
	if(LogControlRedCheckFunc(testLogFunc)) return -1;
	
	//yellow
	ConfigDataSetUint8Value(EN_CONFIG_SAVE_LOG,LOG_WARNING_YELLOW);
	SystemSettingsRegister(SYSTEM_SETTINGS_LOG);
	testLogFunc = LogControlCheckFunc();
	if(LogControlYellowCheckFunc(testLogFunc)) return -1;


	//推論
	if(SystemStatePredictWithoutWarningLatchTest())return -1;
	if(SystemStatePredictWarningLatchTest())return -1;

	//ログ
	AILogInitialize();
	AILogAllElase();
	if(LogControlEndTest()) return -1;
	return 0;
}
