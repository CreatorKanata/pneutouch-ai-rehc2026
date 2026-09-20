/*****************************************************************************
 * File: SystemState.c
 * Title: システムの状態を管理する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file SystemState.c
 * @brief システムの状態を管理する。
 */

#include "SystemState.h"

static SYSTEM_STATE currentState = SYSTEM_STATE_INIT;
static SYSTEM_STATE oldState = SYSTEM_STATE_INIT;
static SYSTEM_SHIFT_STATE shiftToAILearnErase = SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED;
static SYSTEM_SHIFT_STATE shiftToStop = SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED;
static SYSTEM_SHIFT_STATE shiftToError = SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED;

//#define TEST
#ifdef TEST
#define SYSTEM_STATE_LESS_THAN_MIN			(-1)
#define SYSTEM_STATE_GRATER_THAN_MAX		(10)
#define SYSTEM_SHIFT_STATE_LESS_THAN_MIN	(-1)
#define SYSTEM_SHIFT_STATE_GRATER_THAN_MAX	(2)
#endif

static bool checkState(SYSTEM_STATE state);
static bool checkShiftState(SYSTEM_SHIFT_STATE state);

void SystemStateInit(void)
{
	currentState = SYSTEM_STATE_INIT;
	oldState = SYSTEM_STATE_INIT;
}


bool SystemStateCheckStateChange(void)
{
	if( SystemStateGetCurrentState() != oldState)
	{
		return true;
	}
	else
	{
		return false;
	}
}

//SYSTEM_STATEの範囲チェック
static bool checkState(SYSTEM_STATE state)
{
	if((state < SYSTEM_STATE_INIT) || ( SYSTEM_STATE_END < state )) return false;
	return true;
}

//現在の状態
void SystemStateSetCurrentState(SYSTEM_STATE current)
{
	if(!checkState(current)) return;
	currentState = current;
}

SYSTEM_STATE SystemStateGetCurrentState(void)
{
	return currentState;
}

//一つ前の状態
void SystemStateSetOldState(SYSTEM_STATE old)
{
	if(!checkState(old)) return;
	oldState = old;
}

SYSTEM_STATE SystemStateGetOldState(void)
{
	return oldState;
}



//SYSTEM_SHIFT_STATEの範囲チェック
static bool checkShiftState(SYSTEM_SHIFT_STATE state)
{
	if((state < SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED) || ( SYSTEM_SHIFT_STATE_SHIFT_PEMITTED < state )) return false;
	return true;
}

//停止画面への移動
SYSTEM_SHIFT_STATE SystemStateGetShiftToStop(void)
{
	return  shiftToStop;
}

void SystemStateSetShiftToStop(SYSTEM_SHIFT_STATE state)
{
	if( !checkShiftState(state)) return;
	shiftToStop = state;
}

//AI学習データ消去画面への移動
SYSTEM_SHIFT_STATE SystemStateGetShiftToAILearnErase(void)
{
	return  shiftToAILearnErase;
}

void SystemStateSetShiftToAILearnErase(SYSTEM_SHIFT_STATE state)
{
	if( !checkShiftState(state)) return;
	shiftToAILearnErase = state;
}

//エラー画面への移動
SYSTEM_SHIFT_STATE SystemStateGetShiftToError(void)
{
	return  shiftToError;
}

void SystemStateSetShiftToError(SYSTEM_SHIFT_STATE state)
{
	if( !checkShiftState(state)) return;
	shiftToError = state;
}

int SystemStateTest(void)
{
#ifdef TEST
	//境界値
	//systemState
	if(checkState((SYSTEM_STATE)SYSTEM_STATE_LESS_THAN_MIN) != false) return -1;
	if(checkState((SYSTEM_STATE)SYSTEM_STATE_GRATER_THAN_MAX) != false) return -1;
	if(checkState((SYSTEM_STATE)SYSTEM_STATE_INIT) != true) return -1;
	if(checkState((SYSTEM_STATE)SYSTEM_STATE_END) != true) return -1;
	//systemShiftState
	if(checkShiftState((SYSTEM_SHIFT_STATE)SYSTEM_SHIFT_STATE_LESS_THAN_MIN) != false ) return -1;
	if(checkShiftState((SYSTEM_SHIFT_STATE)SYSTEM_SHIFT_STATE_GRATER_THAN_MAX) != false ) return -1;
	if(checkShiftState((SYSTEM_SHIFT_STATE)SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED) != true ) return -1;
	if(checkShiftState((SYSTEM_SHIFT_STATE)SYSTEM_SHIFT_STATE_SHIFT_PEMITTED) != true ) return -1;
	
	//境界値
	//systemState
	SystemStateSetCurrentState(SYSTEM_STATE_INIT);
	SystemStateSetCurrentState((SYSTEM_STATE)SYSTEM_STATE_LESS_THAN_MIN);
	if(SystemStateGetCurrentState() != SYSTEM_STATE_INIT)return -1;
	SystemStateSetCurrentState((SYSTEM_STATE)SYSTEM_STATE_GRATER_THAN_MAX);
	if(SystemStateGetCurrentState() != SYSTEM_STATE_INIT)return -1;
	//systemShiftState
	SystemStateSetShiftToStop(SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED);
	SystemStateSetShiftToStop((SYSTEM_SHIFT_STATE)SYSTEM_SHIFT_STATE_LESS_THAN_MIN);
	if(SystemStateGetShiftToStop() != SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED)return -1;
	SystemStateSetShiftToStop((SYSTEM_SHIFT_STATE)SYSTEM_SHIFT_STATE_GRATER_THAN_MAX);
	if(SystemStateGetShiftToStop() != SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED)return -1;
#endif
	//成功
	return 0;
}

