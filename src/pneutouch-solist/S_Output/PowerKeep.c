/*****************************************************************************
 * File: PowerKeep.c
 * Title: 電圧保持(POWER_KEEP)を制御する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file PowerKeep.c
 * @brief 電圧保持(POWER_KEEP)を制御する。
 */

#include "PowerKeep.h"
#include "Output.h"
#include "smpl_common.h"

//CMOS プルアップ/ダウンなし。
#define POWER_KEEP_VALUE	(0x20)
#define POWER_KEEP_OFF		(0x00)
#define POWER_KEEP_ON		(0x20)
#define POWER_KEEP_CONFIG	(0x02U << 8U)

//#define TEST
#ifdef TEST
static uint32_t powerKeepTest = 0x00;
static uint32_t powerKeepIOTest = 0x00;
#endif


void PowerKeepInit(void)
{
#ifndef TEST
	//port45
	set_reg32(PORT4->P4MOD1,(POWER_KEEP_CONFIG));
#else
	set_bit(powerKeepIOTest,POWER_KEEP_CONFIG);
#endif	
	PowerKeepOff();
}


void PowerKeepOff(void)
{
#ifndef TEST
	OutputOffUInt32(&(PORT4->P4DO),POWER_KEEP_VALUE);
#else
	OutputOffUInt32(&powerKeepTest,POWER_KEEP_VALUE);
#endif
}

void PowerKeepOn(void)
{
#ifndef TEST
	OutputOnUInt32(&(PORT4->P4DO),POWER_KEEP_VALUE);
#else
	OutputOnUInt32(&powerKeepTest,POWER_KEEP_VALUE);
#endif
}

int PowerKeepTest(void)
{
	//初期化
	PowerKeepInit();
#ifdef TEST
	if( ! ( powerKeepIOTest == POWER_KEEP_CONFIG)) return -1;
#endif
	
	//On
	PowerKeepOn();
#ifdef TEST
	if(powerKeepTest != POWER_KEEP_ON) return -1;
#endif
	
	//On
	PowerKeepOn();
#ifdef TEST
	if(powerKeepTest != POWER_KEEP_ON) return -1;
#endif
	
	//Off
	PowerKeepOff();
#ifdef TEST
	if(powerKeepTest != POWER_KEEP_OFF) return -1;
#endif

	//Off
	PowerKeepOff();
#ifdef TEST
	if(powerKeepTest != POWER_KEEP_OFF) return -1;
#endif
	
	return 0;
}
