/*****************************************************************************
 * File: PowerMonitoringSw.c
 * Title: 電源監視用SW(POWSW_CHK)を取り扱う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file PowerMonitoringSw.c
 * @brief 電源監視用SW(POWSW_CHK)を取り扱う。
 */

#include <stdint.h>
#include "PowerMonitoringSw.h"
#include "smpl_common.h"
#include "Output.h"
#include "wdt.h"
//#define TEST
#define POWER_MONITORING_SHIFT 	(4)
#define POWER_MONITORING_PUSH	(1 << POWER_MONITORING_SHIFT)
#define POWER_MONITORING_CONFIG	(0x09U << 0)
#define WAITING_TIME			(50)
#define PULLUP					(0x10)
#define SOFT_PULLDOWN			(0x00)

volatile static bool keepPowerMonitoringSwPressedFlg = false;

/**
 * @brief 入力値を読み取る関数
 *
 * @return uint8_t 
 */
inline static uint8_t readPowerMonitoringSw(void);

/**
 * @brief 入力値の変化に応じてフラグ管理を行う。
 */
inline static void managePowerMonitoringSwFlg(void);


#ifdef TEST
#define PMSW_PUSHED (PULLUP & ~POWER_MONITORING_PUSH)
static uint8_t powerMonitoringIOTest = SOFT_PULLDOWN;
static int checkParameter( bool keepPowerMonitoringSwPressedFlgValue);
#endif

void PowerMonitoringSwInit(void)
{
	set_reg32(PORT4->P4MOD1,POWER_MONITORING_CONFIG);
	InputInit(INPUT_INDEX_POWER_MONITORING,SOFT_PULLDOWN);
	keepPowerMonitoringSwPressedFlg = false;
}


INPUT_POLLING_RESULT PowerMonitoringSwPolling(void)
{
	return InputPolling(INPUT_INDEX_POWER_MONITORING,WAITING_TIME,readPowerMonitoringSw,managePowerMonitoringSwFlg);
}


inline static uint8_t readPowerMonitoringSw(void)
{
#ifndef TEST
	return (uint8_t) ( ( PORT4->P4DI ) & POWER_MONITORING_PUSH );
#else
	return (uint8_t) ( powerMonitoringIOTest & POWER_MONITORING_PUSH );
#endif
}

inline static void managePowerMonitoringSwFlg(void)
{
	//PullUpなので逆
	if( ! ( InputGetSystemInputValue(INPUT_INDEX_POWER_MONITORING) & POWER_MONITORING_PUSH) )
	{	
		keepPowerMonitoringSwPressedFlg = true;
	}
	else
	{
		keepPowerMonitoringSwPressedFlg = false;
	}	
}


bool PowerMonitoringSwIsPressed(void)
{
	return keepPowerMonitoringSwPressedFlg;
}


#ifdef TEST
static int checkParameter( bool keepPowerMonitoringSwPressedFlgValue)
{
	if(PowerMonitoringSwIsPressed()!= keepPowerMonitoringSwPressedFlgValue) return -1;
	//成功
	return 0;
}
#endif

int PowerMonitoringSwTest(void)
{
#ifdef TEST
	const int BREAK_COUNT = 1000;
	int breakCounter = 0;
	//PowerMonitoringSw 押下 初期状態はプルダウンなので状態変化なし
	PowerMonitoringSwInit();
	if(checkParameter(false))return -1;
	OutputOffUInt8(&powerMonitoringIOTest, (uint8_t)POWER_MONITORING_PUSH);
	while( PowerMonitoringSwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(breakCounter++ >= BREAK_COUNT)break;
		if(checkParameter(false))return -1;
	}
	if(checkParameter(false))return -1;
	
	//PowerMonitoringSw　離す
	OutputOnUInt8(&powerMonitoringIOTest, (uint8_t)POWER_MONITORING_PUSH);
	while( PowerMonitoringSwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkParameter(false))return -1;
		wdt_clear();
	}
	if(checkParameter(false))return -1;
	
	//PowerMonitoringSw 押下
	OutputOffUInt8(&powerMonitoringIOTest, (uint8_t)POWER_MONITORING_PUSH);
	while( PowerMonitoringSwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkParameter(false))return -1;
		wdt_clear();
	}
	if(checkParameter(true))return -1;
	
	
	//PowerMonitoringSw　離す
	OutputOnUInt8(&powerMonitoringIOTest, (uint8_t)POWER_MONITORING_PUSH);
	while( PowerMonitoringSwPolling() != INPUT_POLLING_RESULT_EVENT_OCCURED)
	{
		if(checkParameter(true))return -1;
		wdt_clear();
	}
	if(checkParameter(false))return -1;
#endif
	//成功
	return 0;
}


