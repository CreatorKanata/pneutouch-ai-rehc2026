/*****************************************************************************
 * File: SystemError.c
 * Title: システムエラーを取り扱う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file SystemError.c
 * @brief システムエラーを取り扱う。
 */

#include "SystemError.h"
#include <stdint.h>
#include <stdio.h>
#include "Lcd.h"
#define ERROR_STIRNG	(4)

volatile static uint8_t systemError = SYSTEM_ERROR_E00;
static bool systemErrorChanged = false;
static char systemErrorString[ERROR_STIRNG] = "   ";

/**
 * @brief エラーが出ているかどうか参照。
 *
 * @return Errorがないならtrue
 */
static bool isNonError(void);

/**
 * @brief エラー状態の参照。
 *
 * @param error 発生しているか確認したいエラー。
 * @return bool 発生しているならならtrue
 */
static bool seeErrorAlreadyExists(SYSTEM_ERROR error);

/**
 * @brief 最も優先度が高いエラーの文字列を作成。
 */
static void setBiggestError(void);

/**
 * @brief エラー文字列を作成しているか確認する。
 *
 * @param error 確認したいエラー。
 * @return bool 作成しているならtrue
 */
static bool checkIsSameCurrentSystemErrorString(SYSTEM_ERROR error);




void SystemErrorInit(void)
{
	systemError = SYSTEM_ERROR_E00;
}

void SystemErrorInsert(SYSTEM_ERROR error)
{
	if(seeErrorAlreadyExists(error))return;
	systemErrorChanged = true;
	systemError |= (uint8_t)error;
}

void SystemErrorRemoveAt(SYSTEM_ERROR error)
{
	if(!seeErrorAlreadyExists(error))return;
	systemErrorChanged = true;
	systemError &= ~(uint8_t)error;
}

bool SystemErrorNotifyErrorChanged(void)
{
	if(systemErrorChanged)
	{
		systemErrorChanged = false;
		return true;
	}
	else
	{
		return false;
	}
}

SYSTEM_ERROR SystemErrorGetError(void)
{
	SYSTEM_ERROR result = SYSTEM_ERROR_E00;
	//番号が若いほど優先順位が高い。
	if(systemError & SYSTEM_ERROR_E01)
	{
		result = SYSTEM_ERROR_E01;
	}
	else if( systemError & SYSTEM_ERROR_E02)
	{
		result = SYSTEM_ERROR_E02;
	}
	else if( systemError & SYSTEM_ERROR_E03)
	{
		result = SYSTEM_ERROR_E03;
	}
	return result;
}

char* SystemErrorGetStringError(void)
{
	setBiggestError();
	return systemErrorString;
}

void SystemErrorClearErrorOnce(void)
{
	if(isNonError()) return;
	SystemErrorRemoveAt(SystemErrorGetError());
}




static bool isNonError(void)
{
	if(systemError == (uint8_t)SYSTEM_ERROR_E00)
	{
		return true;
	}
	else
	{
		return false;
	}
}

static bool seeErrorAlreadyExists(SYSTEM_ERROR error)
{
	if(systemError & error)
	{
		return true;
	}
	else
	{
		return false;
	}
}

static void setBiggestError(void)
{
	switch(SystemErrorGetError())
	{
		case SYSTEM_ERROR_E01:
			if(checkIsSameCurrentSystemErrorString(SYSTEM_ERROR_E01))return;
			snprintf(systemErrorString,ERROR_STIRNG,"%s","E01");
			break;
		case SYSTEM_ERROR_E02:
			if(checkIsSameCurrentSystemErrorString(SYSTEM_ERROR_E02))return;
			snprintf(systemErrorString,ERROR_STIRNG,"%s","E02");
			break;
		case SYSTEM_ERROR_E03:
			if(checkIsSameCurrentSystemErrorString(SYSTEM_ERROR_E03))return;
			snprintf(systemErrorString,ERROR_STIRNG,"%s","E03");
			break;		
		case SYSTEM_ERROR_E00:
			if(checkIsSameCurrentSystemErrorString(SYSTEM_ERROR_E00))return;
			snprintf(systemErrorString,ERROR_STIRNG,"%s","   ");
			break;
	}
}

static inline bool checkIsSameCurrentSystemErrorString(SYSTEM_ERROR error)
{
	bool result = false;
	
	switch(error)
	{
		case SYSTEM_ERROR_E01:
			if(systemErrorString[2] == '1') result = true;
			break;
		case SYSTEM_ERROR_E02:
			if(systemErrorString[2] == '2') result = true;
			break;
		case SYSTEM_ERROR_E03:
			if(systemErrorString[2] == '3') result = true;
			break;
		case SYSTEM_ERROR_E00:
			if(systemErrorString[2] == ' ') result = true;
			break;
	}
	return result;
}





int SystemErrorTest(void)
{
	//エラーなし
	SystemErrorInit();
	if(SYSTEM_ERROR_E00 != SystemErrorGetError()) return -1;
	
	//エラー確認
	if(seeErrorAlreadyExists(SYSTEM_ERROR_E01))return -1;
	if(!isNonError()) return -1 ;
	
	
	//エラー追加削除
	if(SystemErrorNotifyErrorChanged()) return -1;
	SystemErrorRemoveAt(SYSTEM_ERROR_E01);
	SystemErrorInsert(SYSTEM_ERROR_E01);
	if(!SystemErrorNotifyErrorChanged()) return -1;
	SystemErrorInsert(SYSTEM_ERROR_E01);
	SystemErrorRemoveAt(SYSTEM_ERROR_E01);
	
	//エラー追加削除E01
	SystemErrorInsert(SYSTEM_ERROR_E01);
	LcdDraw(1,SystemErrorGetStringError());
	SystemErrorRemoveAt(SYSTEM_ERROR_E01);
	LcdDraw(1,SystemErrorGetStringError());
	
	//エラー追加削除E03
	SystemErrorInsert(SYSTEM_ERROR_E02);
	LcdDraw(1,SystemErrorGetStringError());
	SystemErrorRemoveAt(SYSTEM_ERROR_E02);
	LcdDraw(1,SystemErrorGetStringError());
	
	//エラー追加削除E03
	SystemErrorInsert(SYSTEM_ERROR_E03);
	LcdDraw(1,SystemErrorGetStringError());
	SystemErrorRemoveAt(SYSTEM_ERROR_E03);
	LcdDraw(1,SystemErrorGetStringError());
	
	//優先度　エラー加算
	if(SYSTEM_ERROR_E00 != SystemErrorGetError()) return -1;
	SystemErrorInsert(SYSTEM_ERROR_E03);
	if(SYSTEM_ERROR_E03 != SystemErrorGetError()) return -1;
	SystemErrorInsert(SYSTEM_ERROR_E02);
	if(SYSTEM_ERROR_E02 != SystemErrorGetError()) return -1;
	LcdDraw(1,SystemErrorGetStringError());
	SystemErrorInsert(SYSTEM_ERROR_E01);
	if(SYSTEM_ERROR_E01 != SystemErrorGetError()) return -1;
	LcdDraw(1,SystemErrorGetStringError());
	//優先度　エラー減算
	SystemErrorClearErrorOnce();
	if(SYSTEM_ERROR_E02 != SystemErrorGetError()) return -1;
	SystemErrorClearErrorOnce();
	if(SYSTEM_ERROR_E03 != SystemErrorGetError()) return -1;
	SystemErrorClearErrorOnce();
	if(SYSTEM_ERROR_E00 != SystemErrorGetError()) return -1;
	return 0;
}
