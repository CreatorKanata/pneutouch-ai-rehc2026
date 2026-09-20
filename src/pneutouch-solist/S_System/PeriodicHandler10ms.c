/*****************************************************************************
 * File: PeriodicHandler10ms.c
 * Title: 10msタイマー割込みを使用する。
 * LastUpdated: 2025.06.16
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file PeriodicHandler10ms.c
 * @brief 10msタイマー割込みを使用する。
 */

#include <stdio.h>
#include "PeriodicHandler10ms.h"
#include "timer0_1.h"
#include "irq.h"
#include "Sw.h"
#include "smpl_common.h"

/** 10msタイマー設定 */
#define TIMER0_PARAM_MODE	( TM_CS_LSCLK | TM_DIV1 | TM_MODE_16BIT | TM_OST_RELOAD ) 

/** LSCLK */
#define LSCLK_MS			(32.768)
/** 10msカウンタ値 MS単位で調整 */
#define TIMER_CNT_10MSEC	( LSCLK_MS * 10 - 1 )  

void TM0_IRQHandler( void );
static PeriodicHandler10msExe periodicExe = NULL;


void PeriodicHandler10msInit(void)
{
	__disable_irq();
	
	smpl_enablePeripheral(TM0_PERI | TM1_PERI);
	irq_tm0_dis();
	timer0_init( TIMER0_PARAM_MODE );
	timer0_setCnt( (uint32_t)TIMER_CNT_10MSEC );
	irq_tm0_setLevel(1);
		
	irq_tm0_clearIRQ();
	irq_tm0_ena();
	
	__enable_irq();
}

void TM0_IRQHandler( void )
{
	if( periodicExe != NULL)
	{
		periodicExe();
	}
}

void PeriodicHandler10msSetCallBack(PeriodicHandler10msExe exe)
{
	periodicExe = exe;
}
