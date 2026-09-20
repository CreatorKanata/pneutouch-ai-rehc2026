/*****************************************************************************
 * File: Input.c
 * Title: 汎用入力を使う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file Input.c
 * @brief 汎用入力を使う。
 */

#include "Input.h"
#include <stdio.h>

#define NUMBER_OF_INSTANCE				(INPUT_INDEX_END)

/**
 * @brief 読み取った入力状態が変化したかの列挙型
 */
typedef enum
{
	INPUT_STATE_UNCHANGED = 0,			/**< 変化なし */
	INPUT_STATE_CHANGED = 1,			/**< 変化あり */
	INPUT_STATE_WAIT_CHATTERING = 2,	/**< チャタリング待ち時間 */
	INPUT_STATE_END = 0xFF				/**< 1Byte */
}INPUT_STATE;

/**
 * @brief イベント管理用列挙型
 */
typedef enum
{
	EVNET_NOT_OCCURED = 0,				/**< イベント未発生 */
	EVENT_CREATED,						/**< イベント発生 */
}EVENT;

/**
 * @brief 入力のための構造体
 */
typedef struct
{
	volatile uint8_t InputStateFlg;
	volatile EVENT InputEventFlg;
	volatile uint8_t InputEventStatus;
	volatile uint8_t InputStatus;
	volatile uint8_t OldInputStatus;
	volatile uint8_t TmpInputStatus;
}INPUT_MANAGEMENT;


static INPUT_MANAGEMENT inputManeger[NUMBER_OF_INSTANCE];
static bool isIndexOutOfRange(INPUT_INDEX index);
static bool isWaitingTimeOutOfRange(uint8_t time);
inline static void readInput(INPUT_INDEX index, uint8_t input, uint8_t time);

//#define TEST
#ifdef TEST
#define SHIFT		( 4 )
#define PUSH		( 1 << SHIFT )
#define PULLUP		( 0x10 )
#define PMSW_PUSHED	( PULLUP & ~PUSH )
volatile static bool inputFlg = false;
static uint8_t inputValueTest = PULLUP;
static int checkParameter
( 
	INPUT_INDEX index,
	volatile uint8_t inputStatusValue,	
	volatile uint8_t inputValue,
	volatile uint8_t oldInputStatusValue,
	volatile uint8_t tmpInputStatusValue,
	bool isInputValue,
	volatile uint8_t inputStateFlgValue, 
	volatile uint8_t inputEventFlgValue, 
	volatile uint8_t inputEventStatusValue
);
inline static bool isInput(void);
inline static uint8_t readTestInput(void);
inline static void activateTestFunc(void);
#endif

static bool isIndexOutOfRange(INPUT_INDEX index)
{
	if( index >= (sizeof(inputManeger) / sizeof(INPUT_MANAGEMENT)) ) return true;
	else return false;
}

static bool isWaitingTimeOutOfRange(uint8_t time)
{
	if( time < INPUT_STATE_WAIT_CHATTERING ) return true;
	else return false;
}

bool InputInit(INPUT_INDEX index, uint8_t initStatus)
{
#ifdef TEST
	inputValueTest = PULLUP;
	inputFlg = false;
#endif
	if(isIndexOutOfRange(index))return false;
	inputManeger[index].InputStateFlg = INPUT_STATE_UNCHANGED;
	inputManeger[index].InputEventFlg = EVNET_NOT_OCCURED;
	inputManeger[index].InputEventStatus = initStatus;
	inputManeger[index].InputStatus = initStatus;
	inputManeger[index].OldInputStatus = initStatus;
	inputManeger[index].TmpInputStatus = initStatus;
	return true;
}

INPUT_POLLING_RESULT InputPolling(INPUT_INDEX index, uint8_t waitingTime, InputGetTargetValue getValue, InputFunc func)
{
	if( getValue == NULL ) return INPUT_POLLING_RESULT_INVALID_ARGUMENT;
	if( func == NULL ) return INPUT_POLLING_RESULT_INVALID_ARGUMENT;
	if( isIndexOutOfRange(index) ) return INPUT_POLLING_RESULT_INVALID_ARGUMENT;
	if( isWaitingTimeOutOfRange(waitingTime) ) return INPUT_POLLING_RESULT_INVALID_ARGUMENT;
	readInput( index, getValue(),waitingTime);
	if( inputManeger[index].InputEventFlg >= EVENT_CREATED )
	{
		func();
		inputManeger[index].InputEventFlg = EVNET_NOT_OCCURED;
		return INPUT_POLLING_RESULT_EVENT_OCCURED;
	}
	return INPUT_POLLING_RESULT_NORMAL;
}

inline static void readInput(INPUT_INDEX index,uint8_t input, uint8_t time)
{
	inputManeger[index].OldInputStatus = inputManeger[index].InputStatus;
	inputManeger[index].InputStatus = input;
	
	//SWの状態が変わったか
	if(!inputManeger[index].InputStateFlg)
	{
		if( inputManeger[index].InputStatus ^ inputManeger[index].OldInputStatus )
		{
			inputManeger[index].InputStateFlg = INPUT_STATE_CHANGED;	
			inputManeger[index].TmpInputStatus = inputManeger[index].InputStatus;
			return;
		}
	}
	
	//チャタリング調整
	if( 	( inputManeger[index].InputStateFlg >= INPUT_STATE_CHANGED )
			&&( inputManeger[index].InputStateFlg < time ))
	{
		inputManeger[index].InputStateFlg += 1;
		return;
	}
	
	if( inputManeger[index].InputStateFlg >= time)
	{
		if( ! ( inputManeger[index].InputStatus ^ inputManeger[index].TmpInputStatus ) )
		{
			inputManeger[index].InputStateFlg = INPUT_STATE_UNCHANGED;
			inputManeger[index].InputEventFlg = EVENT_CREATED;
			inputManeger[index].InputEventStatus = inputManeger[index].InputStatus;	
		}
		else
		{
			inputManeger[index].InputStateFlg = INPUT_STATE_CHANGED;
			inputManeger[index].TmpInputStatus = inputManeger[index].InputStatus;
			inputManeger[index].InputEventFlg = EVNET_NOT_OCCURED;
		}
	}
}

uint8_t InputGetSystemInputValue(INPUT_INDEX index)
{
	return inputManeger[index].InputEventStatus;
}

#ifdef TEST
static int checkParameter
(
	INPUT_INDEX index,
	uint8_t inputStatusValue, uint8_t inputValue, uint8_t oldInputStatusValue, uint8_t tmpInputStatusValue,  
	bool isInputValue,
	uint8_t inputStateFlgValue, uint8_t inputEventFlgValue, uint8_t inputEventStatusValue
)
{
	if(isIndexOutOfRange(index)) return -1;
	if(inputManeger[index].InputStatus != inputStatusValue) return -1;
	if(inputValueTest != inputValue) return -1;
	if(inputManeger[index].OldInputStatus != oldInputStatusValue) return -1;
	if(inputManeger[index].TmpInputStatus != tmpInputStatusValue) return -1;
	if(isInput()!= isInputValue) return -1;
	if(inputManeger[index].InputStateFlg != inputStateFlgValue) return -1;
	if(inputManeger[index].InputEventFlg != inputEventFlgValue) return -1;
	if(inputManeger[index].InputEventStatus != inputEventStatusValue) return -1;
	
	//成功
	return 0;
}

inline static uint8_t readTestInput(void)
{
	return (uint8_t) ( inputValueTest & PUSH );
}

inline static void activateTestFunc(void)
{
	//PullUpなので逆
	if( ! (inputManeger[0].InputEventStatus & PUSH) )
	{	
		inputFlg = true;
	}
	else
	{
		inputFlg = false;
	}	
}

bool isInput(void)
{
	return inputFlg;
}
#endif

int InputTest(void)
{
#ifdef TEST
	if(!InputInit(INPUT_INDEX_DIP_PSH,0)) return -1;
	if(!InputInit(INPUT_INDEX_POWER_MONITORING,0)) return -1;
	if(!InputInit(INPUT_INDEX_PHOTO_COUPLER_INPUT,0)) return 99;
	if(InputInit(INPUT_INDEX_END,0)) return -1;
	//バリデーションチェック
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,NULL,NULL) != INPUT_POLLING_RESULT_INVALID_ARGUMENT) return -1;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,NULL) != INPUT_POLLING_RESULT_INVALID_ARGUMENT) return -1;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,NULL,activateTestFunc) != INPUT_POLLING_RESULT_INVALID_ARGUMENT) return -1;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(InputPolling(1,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(InputPolling(2,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return 99;
	if(InputPolling(3,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_INVALID_ARGUMENT) return -1;
	if(InputPolling(-100,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_INVALID_ARGUMENT) return -1;
	if(InputPolling(1,INPUT_STATE_WAIT_CHATTERING-1,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_INVALID_ARGUMENT) return -1;
	if(InputPolling(1,INPUT_STATE_WAIT_CHATTERING-2,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_INVALID_ARGUMENT) return -1;
	if(InputPolling(1,INPUT_STATE_WAIT_CHATTERING-3,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	
	//Input ONの後チャタリングなし
	InputInit(0,PULLUP);
	if(checkParameter(0,PULLUP,PULLUP,PULLUP,PULLUP,false,INPUT_STATE_UNCHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	inputValueTest &= ~PUSH;
	readInput(0,readTestInput(),INPUT_STATE_WAIT_CHATTERING);
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PULLUP,PMSW_PUSHED,false,INPUT_STATE_CHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	readInput(0,readTestInput(),INPUT_STATE_WAIT_CHATTERING);
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,false,INPUT_STATE_WAIT_CHATTERING,EVNET_NOT_OCCURED,PULLUP))return -1;
	readInput(0,readTestInput(),INPUT_STATE_WAIT_CHATTERING);
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,false,INPUT_STATE_UNCHANGED,EVENT_CREATED,PMSW_PUSHED))return -1;
	
	
	//Input ONの後チャタリングあり
	InputInit(0,PULLUP);
	if(checkParameter(0,PULLUP,PULLUP,PULLUP,PULLUP,false,INPUT_STATE_UNCHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	inputValueTest &= ~PUSH;
	readInput(0,readTestInput(),INPUT_STATE_WAIT_CHATTERING);
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PULLUP,PMSW_PUSHED,false,INPUT_STATE_CHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	readInput(0,readTestInput(),INPUT_STATE_WAIT_CHATTERING);
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,false,INPUT_STATE_WAIT_CHATTERING,EVNET_NOT_OCCURED,PULLUP))return -1;
	//チャタリング
	inputValueTest = PULLUP;
	readInput(0,readTestInput(),INPUT_STATE_WAIT_CHATTERING);
	if(checkParameter(0,PULLUP,PULLUP,PMSW_PUSHED,PULLUP,false,INPUT_STATE_CHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	readInput(0,readTestInput(),INPUT_STATE_WAIT_CHATTERING);
	if(checkParameter(0,PULLUP,PULLUP,PULLUP,PULLUP,false,INPUT_STATE_WAIT_CHATTERING,EVNET_NOT_OCCURED,PULLUP))return -1;
	readInput(0,readTestInput(),INPUT_STATE_WAIT_CHATTERING);
	if(checkParameter(0,PULLUP,PULLUP,PULLUP,PULLUP,false,INPUT_STATE_UNCHANGED,EVENT_CREATED,PULLUP))return -1;
	
	
	//Input　Onの後チャタリングなし
	InputInit(0,PULLUP);
	if(checkParameter(0,PULLUP,PULLUP,PULLUP,PULLUP,false,INPUT_STATE_UNCHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	inputValueTest &= ~PUSH;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PULLUP,PMSW_PUSHED,false,INPUT_STATE_CHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,false,INPUT_STATE_WAIT_CHATTERING,EVNET_NOT_OCCURED,PULLUP))return -1;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_EVENT_OCCURED) return -1;
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,true,INPUT_STATE_UNCHANGED,EVNET_NOT_OCCURED,PMSW_PUSHED))return -1;
	
	
	//Input ONの後チャタリングあり1
	InputInit(0,PULLUP);
	if(checkParameter(0,PULLUP,PULLUP,PULLUP,PULLUP,false,INPUT_STATE_UNCHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	inputValueTest &= ~PUSH;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PULLUP,PMSW_PUSHED,false,INPUT_STATE_CHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,false,INPUT_STATE_WAIT_CHATTERING,EVNET_NOT_OCCURED,PULLUP))return -1;
	//チャタリング1
	inputValueTest = PULLUP;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(checkParameter(0,PULLUP,PULLUP,PMSW_PUSHED,PULLUP,false,INPUT_STATE_CHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(checkParameter(0,PULLUP,PULLUP,PULLUP,PULLUP,false,INPUT_STATE_WAIT_CHATTERING,EVNET_NOT_OCCURED,PULLUP))return -1;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_EVENT_OCCURED) return -1;
	if(checkParameter(0,PULLUP,PULLUP,PULLUP,PULLUP,false,INPUT_STATE_UNCHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	
	
	//Input ONの後チャタリングあり2
	InputInit(0,PULLUP);
	if(checkParameter(0,PULLUP,PULLUP,PULLUP,PULLUP,false,INPUT_STATE_UNCHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	inputValueTest &= ~PUSH;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PULLUP,PMSW_PUSHED,false,INPUT_STATE_CHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,false,INPUT_STATE_WAIT_CHATTERING,EVNET_NOT_OCCURED,PULLUP))return -1;
	//チャタリング1
	inputValueTest = PULLUP;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(checkParameter(0,PULLUP,PULLUP,PMSW_PUSHED,PULLUP,false,INPUT_STATE_CHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	//チャタリング2
	inputValueTest &= ~PUSH;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PULLUP,PULLUP,false,INPUT_STATE_WAIT_CHATTERING,EVNET_NOT_OCCURED,PULLUP))return -1;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,false,INPUT_STATE_CHANGED,EVNET_NOT_OCCURED,PULLUP))return -1;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_NORMAL) return -1;
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,false,INPUT_STATE_WAIT_CHATTERING,EVNET_NOT_OCCURED,PULLUP))return -1;
	if(InputPolling(0,INPUT_STATE_WAIT_CHATTERING,readTestInput,activateTestFunc) != INPUT_POLLING_RESULT_EVENT_OCCURED) return -1;
	if(checkParameter(0,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,PMSW_PUSHED,true,INPUT_STATE_UNCHANGED,EVNET_NOT_OCCURED,PMSW_PUSHED))return -1;
#endif
	//成功
	return 0;
}
