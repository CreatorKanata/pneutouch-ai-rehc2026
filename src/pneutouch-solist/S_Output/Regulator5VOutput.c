/*****************************************************************************
 * File: Regulator5VOutput.c
 * Title: 5Vレギュレータを制御する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file Regulator5VOutput.c
 * @brief 5Vレギュレータを制御する。
 */

#include "Regulator5VOutput.h"
#include "smpl_common.h"

//P46 
#define REGULATOR_5V_CONFIG (0x02 << 16)
#define REGULATOR_5V_VALUE	(0x40)
#define POWER_KEEP_VALUE	(0x000000DF)
#define TEST_VALUE_5V_OFF	(0x00000020)
#define TEST_VALUE_5V_ON	(0x00000060)

static OUTPUT_STATUS outputStatus = OUTPUT_STATUS_OFF;

void Regulator5VOutputInit(void)
{
	set_bit(PORT4->P4MOD1, REGULATOR_5V_CONFIG);
	Regulator5VOutputOff();
}

void Regulator5VOutputOn(void)
{
	outputStatus = OutputOnUInt32(&(PORT4->P4DO),REGULATOR_5V_VALUE);
}

void Regulator5VOutputOff(void)
{
	outputStatus = OutputOffUInt32(&(PORT4->P4DO),REGULATOR_5V_VALUE);
}

OUTPUT_STATUS Regulator5VOutputGetOutputStatus(void)
{
	return outputStatus;
}

int Regulator5VOutputTest(void)
{
	Regulator5VOutputInit();
	if((read_reg32(PORT4->P4MOD1) & 0x00ff0000) != REGULATOR_5V_CONFIG) return -1;
	if(Regulator5VOutputGetOutputStatus() != OUTPUT_STATUS_OFF) return -1;
	OutputOffUInt32(&(PORT4->P4DO),POWER_KEEP_VALUE);
	if(read_reg32(PORT4->P4DO) != TEST_VALUE_5V_OFF) return -1;
	if(Regulator5VOutputGetOutputStatus() != OUTPUT_STATUS_OFF) return -1;
	Regulator5VOutputOn();
	if(read_reg32(PORT4->P4DO) != TEST_VALUE_5V_ON) return -1;
	if(Regulator5VOutputGetOutputStatus() != OUTPUT_STATUS_ON) return -1;
	Regulator5VOutputOn();
	if(read_reg32(PORT4->P4DO) != TEST_VALUE_5V_ON) return -1;
	if(Regulator5VOutputGetOutputStatus() != OUTPUT_STATUS_ON) return -1;
	Regulator5VOutputOff();
	if(read_reg32(PORT4->P4DO) != TEST_VALUE_5V_OFF) return -1;
	if(Regulator5VOutputGetOutputStatus() != OUTPUT_STATUS_OFF) return -1;
	Regulator5VOutputOff();
	if(read_reg32(PORT4->P4DO) != TEST_VALUE_5V_OFF) return -1;
	if(Regulator5VOutputGetOutputStatus() != OUTPUT_STATUS_OFF) return -1;
	return 0;
}

