/*****************************************************************************
 * File: Shutdown.c
 * Title: Shutdown機能を使用する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file Shutdown.c
 * @brief Shutdown機能を使用する。
 */

#include "Shutdown.h"
#include <stdint.h>
#include <stdio.h>
#include "SystemPowerControl.h"
#include "wdt.h"
#include "irq.h"

#define LIST_SIZE	(5)
//シャットダウン状態
//シャットダウンリスト
static uint8_t readyListCnt = 0;
static ShutdownIsReady shutdownIsReadyList[LIST_SIZE];
static uint8_t completeListCnt = 0;
static ShutdownIsComplete shutdownIsCompleteList[LIST_SIZE];

//シャットダウン準備が完了したか
static bool checkIsShutdownReady(void);
//シャットダウン処理が完了したか
static bool checkIsShutdownComplete(void);
//システム全体のシャットダウンを行う
static bool shutdownProcess(void);

void ShutdownInit(void)
{
	for(int i = 0; i < LIST_SIZE; i++)
	{
		shutdownIsReadyList[readyListCnt++] = NULL;
		shutdownIsCompleteList[completeListCnt++] = NULL;
	}
	readyListCnt = 0;
	completeListCnt = 0;
}

bool ShutdownAdd(ShutdownIsReady ready, ShutdownIsComplete complete)
{
	if(readyListCnt >= LIST_SIZE) return false;
	if(completeListCnt >= LIST_SIZE) return false; 
	if((ready == NULL) && (complete == NULL)) return false;
	if(ready != NULL)shutdownIsReadyList[readyListCnt++] = ready;
	if(complete != NULL)shutdownIsCompleteList[completeListCnt++] = complete;
	return true;
}

bool ShutdownExecute(void)
{
	//シャットダウン準備
	if(!checkIsShutdownReady())return false;
	if(!checkIsShutdownComplete())return false;
	//シャットダウン処理
	return shutdownProcess();
}

static bool checkIsShutdownReady(void)
{
	bool isReady = false;
	for(uint8_t i = 0; i < readyListCnt; i++)
	{
		if(shutdownIsReadyList[i] != NULL) isReady = shutdownIsReadyList[i]();
		if(!isReady)break;
	}
	return isReady;
}

static bool checkIsShutdownComplete(void)
{
	bool isComplete = false;
	for(uint8_t i = 0; i < readyListCnt; i++)
	{
		if(shutdownIsCompleteList[i] != NULL) isComplete = shutdownIsCompleteList[i]();
		if(!isComplete)break;
	}
	return isComplete;
}

static bool shutdownProcess(void)
{
	//全ての割り込みを禁止する
	__disable_irq();
	//電源保持ポートをOFFにする。
	SystemPowerControlFin();
	//電源オフ
	while(1) 
	{
		wdt_clear();
	}
}
