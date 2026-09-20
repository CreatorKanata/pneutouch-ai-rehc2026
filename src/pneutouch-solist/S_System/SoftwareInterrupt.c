/*****************************************************************************
 * File: SoftwareInterrupt.c
 * Title: ソフトウェア割り込みを使用する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file SoftwareInterrupt.c
 * @brief ソフトウェア割り込みを使用する。
 */
 
#include <stdio.h>
#include "rdwr_reg.h"
#include "irq.h"
#include "SoftwareInterrupt.h"
#include "AI.h"

void PendSV_Handler(void);
static SoftwareInterruptPendSVExecute pendSVExe = NULL;


void SoftwareInterruptPendSVActivate(void)
{
	if(pendSVExe == NULL)
	{
		return;
	}
	else
	{
		//ソフトウェア割り込みを起動
		write_reg32(SCB->ICSR, SCB_ICSR_PENDSVSET_Msk);
	}
}

bool SoftwareInterruptExeIsNull(void)
{
	if(pendSVExe == NULL) return true;
	else return false;
}


void SoftwareInterruptInit(void)
{
	//PendSVの割り込み優先度をセット
	NVIC_SetPriority(PendSV_IRQn,3);
}


void PendSV_Handler(void)
{
	pendSVExe();
	//ソフトウェア割り込みをクリア
	write_reg32(SCB->ICSR, SCB_ICSR_PENDSVCLR_Msk);
}


void SoftwareInterruptSetCallback(SoftwareInterruptPendSVExecute exe)
{
	pendSVExe = exe;
}
