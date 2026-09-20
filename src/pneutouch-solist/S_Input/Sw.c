/*****************************************************************************
 * File: Sw.c
 * Title: Sw(プッシュスイッチ/ディップスイッチ)を使う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file Sw.c
 * @brief Sw(プッシュスイッチ/ディップスイッチ)を使う。\n
          プッシュスイッチは回路図のSW2を1、SW3を2、SW4を3、SW5を4とする。\n
          ディップスイッチは回路図のSW1の1を1、2を2、3を3、4を4とする。
 */
 
#include <stdint.h>
#include "Sw.h"
#include "smpl_common.h"

typedef enum
{
	PUSH_DSW4 = ( 1 << 7 ),
	PUSH_DSW3 = ( 1 << 6 ),
	PUSH_DSW2 = ( 1 << 5 ),
	PUSH_DSW1 = ( 1 << 4 ),
	PUSH_PSW4 = ( 1 << 3 ),
	PUSH_PSW3 = ( 1 << 2 ),
	PUSH_PSW2 = ( 1 << 1 ),
	PUSH_PSW1 = ( 1 << 0 )
}PUSH;

#define WAITING_TIME	(2)
#define PULLUP			(0xff)

//dip sw P34~P37 circuit pullup
#define DSW_CONFIG ( ( 0x01U << 24U ) | ( 0x01U << 16U ) | ( 0x01U << 8U) | ( 0x01U << 0 ) )
//push sw P50~P53 circuit pullup
#define PSW_CONFIG ( ( 0x01U << 24U ) | ( 0x01U << 16U ) | ( 0x01U << 8U) | ( 0x01U << 0 ) )

//長押し
volatile static bool keepPsw1PressedFlg = false;
volatile static bool keepPsw2PressedFlg = false;
volatile static bool keepPsw3PressedFlg = false;
volatile static bool keepPsw4PressedFlg = false;
volatile static bool keepDsw1PressedFlg = false;
volatile static bool keepDsw2PressedFlg = false;
volatile static bool keepDsw3PressedFlg = false;
volatile static bool keepDsw4PressedFlg = false;

//短押し
volatile static bool iskeepPsw1Enable = true;
volatile static bool iskeepPsw2Enable = true;
volatile static bool iskeepPsw3Enable = true;
volatile static bool iskeepPsw4Enable = true;

//トグル
volatile static bool togglePsw3Flg = false;
volatile static bool togglePsw4Flg = false;
volatile static bool togglePsw3PressedFlg = false;
volatile static bool togglePsw4PressedFlg = false;
volatile static bool isTogglePsw3Enable = true;
volatile static bool isTogglePsw4Enable = true;

inline static uint8_t readPsw(void);
inline static uint8_t readDsw(void);
inline static uint8_t readDipPushSw(void);
inline static void manageFlg(void);
inline static void managePswFlg(void);
inline static void manageDswFlg(void);
				
//#define TEST
#ifdef TEST
static uint8_t swValueTest = PULLUP;
static void pressSw(uint8_t sw);
static void releaseSw(uint8_t sw);
static int checkPswParameter
( 
	bool keepPSW1SwPressedValue,
	bool keepPSW2SwPressedValue,
	bool keepPSW3SwPressedValue,
	bool keepPSW4SwPressedValue,
	bool togglePsw3Value,
	bool togglePsw4Value,
	bool togglePsw3PressedValue,
	bool togglePsw4PressedValue
);

static int checkPswEnabaleParameter
(
	bool iskeepPsw1EnableValue,
	bool iskeepPsw2EnableValue,
	bool iskeepPsw3EnableValue,
	bool iskeepPsw4EnableValue,
	bool isTogglePsw3EnableValue,
	bool isTogglePsw4EnableValue
);

static int checkDswParameter
( 
	bool keepDSW1SwPressedValue,
	bool keepDSW2SwPressedValue,
	bool keepDSW3SwPressedValue,
	bool keepDSW4SwPressedValue
);
#endif


void SwInit(void)
{
	set_reg32(PORT3->P3MOD1, DSW_CONFIG);
	set_reg32(PORT5->P5MOD0, PSW_CONFIG);
#ifdef TEST
	swValueTest = PULLUP;
#endif
	InputInit(INPUT_INDEX_DIP_PSH,PULLUP);
	keepPsw1PressedFlg = false;
	keepPsw2PressedFlg = false;
	keepPsw3PressedFlg = false;
	keepPsw4PressedFlg = false;
	keepDsw1PressedFlg = false;
	keepDsw2PressedFlg = false;
	keepDsw3PressedFlg = false;
	keepDsw4PressedFlg = false;
	
	iskeepPsw1Enable = true;
	iskeepPsw2Enable = true;
	iskeepPsw3Enable = true;
	iskeepPsw4Enable = true;
	
	togglePsw3Flg = false;
	togglePsw4Flg = false;
	togglePsw3PressedFlg = false;
	togglePsw4PressedFlg = false;
	
	isTogglePsw3Enable = true;
	isTogglePsw4Enable = true;
}

inline static uint8_t readPsw(void)
{
#ifndef TEST
	return (uint8_t) ( ( PORT5->P5DI ) & 0x0f );
#else
	return (uint8_t) ( ( swValueTest) & 0x0f );
#endif
}

inline static uint8_t readDsw(void)
{
#ifndef TEST
	return (uint8_t) ( ( PORT3->P3DI ) & 0xf0 );
#else
	return (uint8_t) ( ( swValueTest) & 0xf0 );
#endif
}

inline static uint8_t readDipPushSw(void)
{
	return ( readDsw() + readPsw() );
}

INPUT_POLLING_RESULT SwPolling(void)
{
	return InputPolling(INPUT_INDEX_DIP_PSH,WAITING_TIME,readDipPushSw,manageFlg);
}

inline static void manageFlg(void)
{
	managePswFlg();
	manageDswFlg();
}

inline static void managePswFlg(void)
{
	//PSW1
	if( ! ( InputGetSystemInputValue(INPUT_INDEX_DIP_PSH) & PUSH_PSW1) )
	{	
		keepPsw1PressedFlg = true;
	}
	else
	{
		keepPsw1PressedFlg = false;
		iskeepPsw1Enable = true;
	}	
	
	//PSW2
	if( ! ( InputGetSystemInputValue(INPUT_INDEX_DIP_PSH) & PUSH_PSW2) )
	{	
		keepPsw2PressedFlg = true;
	}
	else
	{
		keepPsw2PressedFlg = false;
		iskeepPsw2Enable = true;
	}	
		
	//PSW3
	if( ! ( InputGetSystemInputValue(INPUT_INDEX_DIP_PSH) & PUSH_PSW3) )
	{
		keepPsw3PressedFlg = true;
		
		if(!togglePsw3Flg)
		{
			togglePsw3Flg = true;
			isTogglePsw3Enable = true;
			if(togglePsw3PressedFlg)
			{
				togglePsw3PressedFlg = false;
			}
			else if(!togglePsw3PressedFlg)
			{
				togglePsw3PressedFlg = true;
			}
		}
	}
	else
	{
		keepPsw3PressedFlg = false;
		iskeepPsw3Enable = true;
		
		if(togglePsw3Flg)
		{
			togglePsw3Flg = false;
		}
	}	
	
	//PSW4
	if( ! (InputGetSystemInputValue(INPUT_INDEX_DIP_PSH) & PUSH_PSW4) )
	{			
		keepPsw4PressedFlg = true;
		
		if(!togglePsw4Flg)
		{
			togglePsw4Flg = true;
			isTogglePsw4Enable = true;
			if(togglePsw4PressedFlg)
			{
				togglePsw4PressedFlg = false;
			}
			else if(!togglePsw4PressedFlg)
			{
				togglePsw4PressedFlg = true;
			}
		}
	}
	else
	{
		keepPsw4PressedFlg = false;
		iskeepPsw4Enable = true;
		
		if(togglePsw4Flg)
		{
			togglePsw4Flg = false;
		}
	}
}


inline static void manageDswFlg(void)
{
	if( ! (InputGetSystemInputValue(INPUT_INDEX_DIP_PSH) & PUSH_DSW1) ) 
	{
		keepDsw1PressedFlg = true;
	}
	else
	{
		keepDsw1PressedFlg = false;
	}	
	
	if( ! (InputGetSystemInputValue(INPUT_INDEX_DIP_PSH) & PUSH_DSW2) )
	{
		keepDsw2PressedFlg = true;
	}
	else
	{
		keepDsw2PressedFlg = false;
	}
	
	if( ! (InputGetSystemInputValue(INPUT_INDEX_DIP_PSH) & PUSH_DSW3) )
	{
		keepDsw3PressedFlg = true;
	}
	else
	{
		keepDsw3PressedFlg = false;
	}
	
	if( ! (InputGetSystemInputValue(INPUT_INDEX_DIP_PSH) & PUSH_DSW4) )
	{
		keepDsw4PressedFlg = true;
	}
	else
	{
		keepDsw4PressedFlg = false;
	}
}

//押し続け
bool SwIsPsw1Entered(void)
{
	if(iskeepPsw1Enable) return keepPsw1PressedFlg;
	else return false;
}

bool SwIsPsw2Entered(void)
{
	if(iskeepPsw2Enable) return keepPsw2PressedFlg;
	else return false;
}

bool SwIsPsw3Entered(void)
{
	if(iskeepPsw3Enable) return keepPsw3PressedFlg;
	else return false;
}

bool SwIsPsw4Entered(void)
{
	if(iskeepPsw4Enable) return keepPsw4PressedFlg;
	else return false;
}


//押しフラグを次の押下まで解除
//排他制御のため、割り込み禁止
void SwDisablePsw1UntilNextPress(void)
{
	__disable_irq();
	//Swがオンなら入力停止する。
	if(SwIsPsw1Entered())
	{
		keepPsw1PressedFlg = false;
		iskeepPsw1Enable = false;
	}
	__enable_irq();
}

void SwDisablePsw2UntilNextPress(void)
{
	__disable_irq();
	if(SwIsPsw2Entered())
	{
		keepPsw2PressedFlg = false;
		iskeepPsw2Enable = false;
	}
	__enable_irq();
}

void SwDisablePsw3UntilNextPress(void)
{
	__disable_irq();
	if(SwIsPsw3Entered())
	{
		keepPsw3PressedFlg = false;
		iskeepPsw3Enable = false;
	}
	if(SwIsTogglePsw3Valid())
	{
		togglePsw3PressedFlg = false;
		isTogglePsw3Enable = false;
	}
	__enable_irq();
}

void SwDisablePsw4UntilNextPress(void)
{
	__disable_irq();
	if(SwIsPsw4Entered())
	{
		keepPsw4PressedFlg = false;
		iskeepPsw4Enable = false;
	}
	if(SwIsTogglePsw4Valid())
	{
		togglePsw4PressedFlg = false;
		isTogglePsw4Enable = false;
	}
	__enable_irq();
}

void SwDisableAllPswUntilNextPress(void)
{
	SwDisablePsw1UntilNextPress();
	SwDisablePsw2UntilNextPress();
	SwDisablePsw3UntilNextPress();
	SwDisablePsw4UntilNextPress();
}

//トグル
bool SwIsTogglePsw3Valid(void)
{
	if(isTogglePsw3Enable) return togglePsw3PressedFlg;
	else return false;
}

bool SwIsTogglePsw4Valid(void)
{
	if(isTogglePsw4Enable) return togglePsw4PressedFlg;
	else return false;
}

//DSW
bool SwIsDsw1Entered(void)
{
	return keepDsw1PressedFlg;
}

bool SwIsDsw2Entered(void)
{
	return keepDsw2PressedFlg;
}

bool SwIsDsw3Entered(void)
{
	return keepDsw3PressedFlg;
}

bool SwIsDsw4Entered(void)
{
	return keepDsw4PressedFlg;
}

#ifdef TEST
//テスト用:sw押下
static void pressSw(uint8_t sw)
{
	switch(sw)
	{
		//DSW
		case PUSH_DSW4:
			swValueTest &= ~PUSH_DSW4;
			break;
		case PUSH_DSW3:
			swValueTest &= ~PUSH_DSW3;
			break;
		case PUSH_DSW2:
			swValueTest &= ~PUSH_DSW2;
			break;
		case PUSH_DSW1:
			swValueTest &= ~PUSH_DSW1;
			break;
		//PSW
		case PUSH_PSW4:
			swValueTest &= ~PUSH_PSW4;
			break;
		case PUSH_PSW3:
			swValueTest &= ~PUSH_PSW3;
			break;
		case PUSH_PSW2:
			swValueTest &= ~PUSH_PSW2;
			break;
		case PUSH_PSW1:
			swValueTest &= ~PUSH_PSW1;
			break;
	}
}

//テスト用:sw離す
static void releaseSw(uint8_t sw)
{
	switch(sw)
	{
		//DSW
		case PUSH_DSW4:
			swValueTest |= PUSH_DSW4; 
			break;
		case PUSH_DSW3:
			swValueTest |= PUSH_DSW3;
			break;
		case PUSH_DSW2:
			swValueTest |= PUSH_DSW2; 
			break;
		case PUSH_DSW1:
			swValueTest |= PUSH_DSW1; 
			break;
		
		//PSW
		case PUSH_PSW4:
			swValueTest |= PUSH_PSW4; 
			break;
		case PUSH_PSW3:
			swValueTest |= PUSH_PSW3;
			break;
		case PUSH_PSW2:
			swValueTest |= PUSH_PSW2; 
			break;
		case PUSH_PSW1:
			swValueTest |= PUSH_PSW1; 
			break;
	}
}

static int checkPswParameter
( bool keepPSW1SwPressedValue, bool keepPSW2SwPressedValue, bool keepPSW3SwPressedValue, bool keepPSW4SwPressedValue,
	bool togglePsw3Value, bool togglePsw4Value, bool togglePsw3PressedValue, bool togglePsw4PressedValue)
{
	if(SwIsPsw1Entered() != keepPSW1SwPressedValue) return -1;
	if(SwIsPsw2Entered() != keepPSW2SwPressedValue) return -1;
	if(SwIsPsw3Entered() != keepPSW3SwPressedValue) return -1;
	if(SwIsPsw4Entered() != keepPSW4SwPressedValue) return -1;
	if(togglePsw3Flg != togglePsw3Value) return -1;
	if(togglePsw4Flg != togglePsw4Value) return -1;
	if(SwIsTogglePsw3Valid()  != togglePsw3PressedValue) return -1;
	if(SwIsTogglePsw4Valid()  != togglePsw4PressedValue) return -1;
	return 0;
}

static int checkPswEnabaleParameter
(bool iskeepPsw1EnableValue, bool iskeepPsw2EnableValue, bool iskeepPsw3EnableValue, bool iskeepPsw4EnableValue, 
 bool isTogglePsw3EnableValue, bool isTogglePsw4EnableValue)
{
	if(iskeepPsw1Enable != iskeepPsw1EnableValue) return -1;
	if(iskeepPsw2Enable != iskeepPsw2EnableValue) return -1;
	if(iskeepPsw3Enable != iskeepPsw3EnableValue) return -1;
	if(iskeepPsw4Enable != iskeepPsw4EnableValue) return -1;
	if(isTogglePsw3Enable != isTogglePsw3EnableValue) return -1;
	if(isTogglePsw4Enable != isTogglePsw4EnableValue) return -1;
	return 0;
}

static int checkDswParameter(bool keepDSW1SwPressedValue, bool keepDSW2SwPressedValue, bool keepDSW3SwPressedValue, bool keepDSW4SwPressedValue)
{
	if(SwIsDsw1Entered() != keepDSW1SwPressedValue) return -1;
	if(SwIsDsw2Entered() != keepDSW2SwPressedValue) return -1;
	if(SwIsDsw3Entered() != keepDSW3SwPressedValue) return -1;
	if(SwIsDsw4Entered() != keepDSW4SwPressedValue) return -1;
	return 0;	
}
#endif


int SwTest(void)
{
#ifdef TEST
	//PSW
	//初期化
	SwInit();
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	//PSW1
	pressSw(PUSH_PSW1);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(true,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	releaseSw(PUSH_PSW1);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(true,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	
	//長押しdisable
	pressSw(PUSH_PSW1);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(true,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	SwDisablePsw1UntilNextPress();
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(false,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	releaseSw(PUSH_PSW1);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(false,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	pressSw(PUSH_PSW1);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}	
	if(checkPswParameter(true,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	//PSW2
	SwInit();
	pressSw(PUSH_PSW2);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,true,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	releaseSw(PUSH_PSW2);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,true,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	//長押しdisable
	pressSw(PUSH_PSW2);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,true,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	SwDisablePsw2UntilNextPress();
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,false,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	releaseSw(PUSH_PSW2);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,false,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	pressSw(PUSH_PSW2);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,true,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	SwDisablePsw2UntilNextPress();
	
	
	//PSW3、PSW3トグル
	//敢えて初期化なしで続ける。
	releaseSw(PUSH_PSW2);
	pressSw(PUSH_PSW3);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,false,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,true,false,true,false,true,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	releaseSw(PUSH_PSW3);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,true,false,true,false,true,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,true,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	
	//PSW3トグル
	pressSw(PUSH_PSW3);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,true,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,true,false,true,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	releaseSw(PUSH_PSW3);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,true,false,true,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	
	//長押しdisable
	pressSw(PUSH_PSW3);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,true,false,true,false,true,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	SwDisablePsw3UntilNextPress();
	if(checkPswParameter(false,false,false,false,true,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,false,true,false,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;

	releaseSw(PUSH_PSW3);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,true,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,false,true,false,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,false,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	pressSw(PUSH_PSW3);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,false,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,true,false,true,false,true,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	
	//PSW4、PSW4トグル
	SwInit();
	pressSw(PUSH_PSW4);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,true,false,true,false,true)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	releaseSw(PUSH_PSW4);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,true,false,true,false,true)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,true)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	//PSW4トグル
	pressSw(PUSH_PSW4);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,true)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,true,false,true,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	releaseSw(PUSH_PSW4);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,true,false,true,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	//長押しdisable
	pressSw(PUSH_PSW4);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,true,false,true,false,true)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	SwDisablePsw4UntilNextPress();
	if(checkPswParameter(false,false,false,false,false,true,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,false,true,false)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	releaseSw(PUSH_PSW4);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,true,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,false,true,false)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,false)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	pressSw(PUSH_PSW4);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,false)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,true,false,true,false,true)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	//AllDisable
	SwInit();
	pressSw(PUSH_PSW1);
	pressSw(PUSH_PSW2);
	pressSw(PUSH_PSW3);
	pressSw(PUSH_PSW4);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(true,true,true,true,true,true,true,true)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	SwDisableAllPswUntilNextPress();
	if(checkPswParameter(false,false,false,false,true,true,false,false)) return -1;
	if(checkPswEnabaleParameter(false,false,false,false,false,false)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	releaseSw(PUSH_PSW1);
	releaseSw(PUSH_PSW2);
	releaseSw(PUSH_PSW3);
	releaseSw(PUSH_PSW4);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,true,true,false,false)) return -1;
		if(checkPswEnabaleParameter(false,false,false,false,false,false)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,false,false)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	//DSW
	SwInit();
	//DSW1
	pressSw(PUSH_DSW1);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(true,false,false,false)) return -1;
	
	releaseSw(PUSH_DSW1);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(true,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	//DSW2
	pressSw(PUSH_DSW2);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,true,false,false)) return -1;
	
	releaseSw(PUSH_DSW2);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,true,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	//DSW3
	pressSw(PUSH_DSW3);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,true,false)) return -1;
	
	releaseSw(PUSH_DSW3);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,true,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
	
	//DSW4
	pressSw(PUSH_DSW4);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,false)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,true)) return -1;
	
	releaseSw(PUSH_DSW4);
	while( SwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
		if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
		if(checkDswParameter(false,false,false,true)) return -1;
	}
	if(checkPswParameter(false,false,false,false,false,false,false,false)) return -1;
	if(checkPswEnabaleParameter(true,true,true,true,true,true)) return -1;
	if(checkDswParameter(false,false,false,false)) return -1;
#endif
	return 0;
}
