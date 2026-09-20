/*****************************************************************************
 * File: SystemStatePredictDraw.c
 * Title: 推論画面の表示を制御する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file SystemStatePredictDraw.c
 * @brief 推論画面の表示を制御する。
 */
 
#include "SystemStatePredictDraw.h"
#include <stdio.h>
#include "ConfigData.h"


static PREDICT_DRAW_FUNC func;


void SystemStatePredictDrawInit(PREDICT_DRAW_FUNC* drawFunc)
{
	func = *drawFunc;
}

PREDICT_DRAW_FUNC* SystemStatePredictDrawCheckFunc(void)
{
	return &func;
}

void SystemStatePredictDrawReset(void)
{
	if(func.Reset != NULL) func.Reset();
}

void SystemStatePredictDrawDraw(float anomalyValue, ANOMALY anomalyResult)
{
	if(func.DrawAnomaly != NULL) func.DrawAnomaly(anomalyValue,anomalyResult);
}




int SystemStatePredictDrawTest(void)
{
	PREDICT_DRAW_FUNC writeTestFunc = {.Reset = NULL, .DrawAnomaly = NULL};
	PREDICT_DRAW_FUNC* readTestFunc;
	SystemStatePredictDrawInit(&writeTestFunc);
	readTestFunc = SystemStatePredictDrawCheckFunc();
	if(readTestFunc->Reset != NULL) return -1;
	if(readTestFunc->DrawAnomaly!= NULL) return -1;
	if(writeTestFunc.Reset != readTestFunc->Reset) return -1;
	if(writeTestFunc.DrawAnomaly != readTestFunc->DrawAnomaly)return -1;
	SystemStatePredictDrawDraw(0.5f,ANOMALY_NORMAL);
	SystemStatePredictDrawReset();
	
	return 0;
}
