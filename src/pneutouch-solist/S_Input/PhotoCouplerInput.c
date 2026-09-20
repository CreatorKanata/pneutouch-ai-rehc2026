/*****************************************************************************
 * File: PhotoCouplerInput.c
 * Title: フォトカプラ入力を取り扱う。
 * LastUpdated: 2025.06.13
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file PhotoCouplerInput.c
 * @brief フォトカプラ入力を取り扱う。\n
 *		  回路図のISOIN0をフォトカプラ入力0とする。\n
 *		  回路図のISOIN1をフォトカプラ入力1とする。
 */

#include "PhotoCouplerInput.h"
#include "smpl_common.h"
#include "Output.h"

/**< /P64 フォトカプラ入力0　プルアップ P65 フォトカプラ入力1　プルアップ */
#define PHOTO_COUPLER_INPUT_CONFIG	( (0x01U << 8U) | (0x01U << 0) )
#define PHOTO_COUPLER_INPUT0_PUSH	(1 << 4)
#define PHOTO_COUPLER_INPUT1_PUSH	(1 << 5)
#define PHOTO_COUPLER_INPUT			(0x30)
#define WAITING_TIME				(2)
#define PULLUP						(0x30)

volatile static bool photoCouplerInput0StatusFlg = false;
volatile static bool photoCouplerInput1StatusFlg = false;

/**
 * @brief 入力値を読み取る。
 *
 * @return uint8_t 
 */
inline static uint8_t readPhotoCouplerInput(void);

/**
 * @brief 入力値の変化に応じてフラグ管理を行う。
 */
static void managePhotoCouplerInput(void);

//#define TEST
#ifdef TEST
static uint8_t photoCouplerIOtest = false;
inline static uint8_t readPhotoCouplerInputTest(void);
static int checkParameter(bool photoCouplerInput0Value, bool photoCouplerInput1Value);
#endif

void PhotoCouplerInputInit(void)
{
	set_reg32(PORT6->P6MOD1,PHOTO_COUPLER_INPUT_CONFIG);
	InputInit(INPUT_INDEX_PHOTO_COUPLER_INPUT,PULLUP);
	photoCouplerInput0StatusFlg = false;
	photoCouplerInput1StatusFlg = false;
}


//test時はread関数を変更
INPUT_POLLING_RESULT PhotoCouplerInputPolling(void)
{
	return InputPolling(INPUT_INDEX_PHOTO_COUPLER_INPUT,WAITING_TIME,readPhotoCouplerInput,managePhotoCouplerInput);
}


inline static uint8_t readPhotoCouplerInput(void)
{
	return (uint8_t) ( ( PORT6->P6DI ) & PHOTO_COUPLER_INPUT );
}

#ifdef TEST
inline static uint8_t readPhotoCouplerInputTest(void)
{
	return (uint8_t) ( photoCouplerIOtest & PHOTO_COUPLER_INPUT );
}
#endif

static void managePhotoCouplerInput(void)
{
	//PullUpなので逆
	//フォトカプラ入力0
	if( ! ( InputGetSystemInputValue(INPUT_INDEX_PHOTO_COUPLER_INPUT) & PHOTO_COUPLER_INPUT0_PUSH) )
	{	
		photoCouplerInput0StatusFlg = true;
	}
	else
	{
		photoCouplerInput0StatusFlg = false;
	}	
	//フォトカプラ入力1
	if( ! ( InputGetSystemInputValue(INPUT_INDEX_PHOTO_COUPLER_INPUT) & PHOTO_COUPLER_INPUT1_PUSH) )
	{	
		photoCouplerInput1StatusFlg = true;
	}
	else
	{
		photoCouplerInput1StatusFlg = false;
	}	
	
}

bool PhotoCouplerInput0IsEntered(void)
{
	return photoCouplerInput0StatusFlg;
}


bool PhotoCouplerInput1IsEntered(void)
{
	return photoCouplerInput1StatusFlg;
}


#ifdef TEST
static int checkParameter( bool photoCouplerInput0Value , bool photoCouplerInput1Value)
{
	if(PhotoCouplerInput0IsEntered() != photoCouplerInput0Value) return -1;
	if(PhotoCouplerInput1IsEntered() != photoCouplerInput1Value) return -1;
	return 0;
}
#endif


int PhotoCouplerInputTest(void)
{
#ifdef TEST
	PhotoCouplerInputInit();
	photoCouplerIOtest = PULLUP;
	if(checkParameter(false,false))return -1;
	
	//フォトカプラ入力0へのみ入力(pullupなのでon/off逆)
	OutputOffUInt8(&photoCouplerIOtest,PHOTO_COUPLER_INPUT0_PUSH);
	while( PhotoCouplerInputPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkParameter(false,false))return -1;
	}
	if(checkParameter(true,false))return -1;
	//フォトカプラ入力1へのみ入力
	OutputOffUInt8(&photoCouplerIOtest,PHOTO_COUPLER_INPUT1_PUSH);
	while( PhotoCouplerInputPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkParameter(true,false))return -1;
	}
	if(checkParameter(true,true))return -1;
	//フォトカプラ入力01への入力解除
	OutputOnUInt8(&photoCouplerIOtest,PHOTO_COUPLER_INPUT0_PUSH);
	OutputOnUInt8(&photoCouplerIOtest,PHOTO_COUPLER_INPUT1_PUSH);
	while( PhotoCouplerInputPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkParameter(true,true))return -1;
	}
	if(checkParameter(false,false))return -1;
	//フォトカプラ入力01へ入力
	OutputOffUInt8(&photoCouplerIOtest,PHOTO_COUPLER_INPUT0_PUSH);
	OutputOffUInt8(&photoCouplerIOtest,PHOTO_COUPLER_INPUT1_PUSH);
	while( PhotoCouplerInputPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkParameter(false,false))return -1;
	}
	if(checkParameter(true,true))return -1;
#endif
	
	return 0;
}

int PhotoCouplerInputOutputTest(void)
{
	int testResult = 0x00000000;
	if(PhotoCouplerInput0IsEntered()) OutputOnUInt32(&testResult,0x0000FFFF);
	else OutputOffUInt32(&testResult,0x0000FFFF);
	if(PhotoCouplerInput1IsEntered()) OutputOnUInt32(&testResult,0xFFFF0000);
	else OutputOffUInt32(&testResult,0xFFFF0000);
	return testResult;
}
