/*****************************************************************************
 * File: Regulator24VOutput.c
 * Title: 24Vレギュレータを制御する。
 * LastUpdated: 2025.05.23
******************************************************************************/

#include "Regulator24VOutput.h"
#include "smpl_common.h"

//P47
#define REGULATOR_24V_CONFIG	(0x02 << 24)
#define REGULATOR_24V_VALUE		(0x80)
#define POWER_KEEP_VALUE		(0x000000DF)
#define TEST_VALUE_24V_OFF		(0x00000020)
#define TEST_VALUE_24V_ON		(0x000000A0)

static OUTPUT_STATUS outputStatus = OUTPUT_STATUS_OFF;

void Regulator24VOutputInit(void)
{
	set_bit(PORT4->P4MOD1, REGULATOR_24V_CONFIG);
	Regulator24VOutputOff();
}

void Regulator24VOutputOn(void)
{
	outputStatus = OutputOnUInt32(&(PORT4->P4DO),REGULATOR_24V_VALUE);
}

void Regulator24VOutputOff(void)
{
	outputStatus = OutputOffUInt32(&(PORT4->P4DO),REGULATOR_24V_VALUE);
}

OUTPUT_STATUS Regulator24VOutputGetOutputStatus(void)
{
	return outputStatus;
}

int Regulator24VOutputTest(void)
{
	Regulator24VOutputInit();
	if((read_reg32(PORT4->P4MOD1) & 0xff000000) != REGULATOR_24V_CONFIG) return -1;
	if(Regulator24VOutputGetOutputStatus() != OUTPUT_STATUS_OFF) return -1;
	OutputOffUInt32(&(PORT4->P4DO),POWER_KEEP_VALUE);
	if(read_reg32(PORT4->P4DO) != TEST_VALUE_24V_OFF) return -1;
	if(Regulator24VOutputGetOutputStatus() != OUTPUT_STATUS_OFF) return -1;
	Regulator24VOutputOn();
	if(read_reg32(PORT4->P4DO) != TEST_VALUE_24V_ON) return -1;
	if(Regulator24VOutputGetOutputStatus() != OUTPUT_STATUS_ON) return -1;
	Regulator24VOutputOn();
	if(read_reg32(PORT4->P4DO) != TEST_VALUE_24V_ON) return -1;
	if(Regulator24VOutputGetOutputStatus() != OUTPUT_STATUS_ON) return -1;
	Regulator24VOutputOff();
	if(read_reg32(PORT4->P4DO) != TEST_VALUE_24V_OFF) return -1;
	if(Regulator24VOutputGetOutputStatus() != OUTPUT_STATUS_OFF) return -1;
	Regulator24VOutputOff();
	if(read_reg32(PORT4->P4DO) != TEST_VALUE_24V_OFF) return -1;
	if(Regulator24VOutputGetOutputStatus() != OUTPUT_STATUS_OFF) return -1;
	return 0;
}
