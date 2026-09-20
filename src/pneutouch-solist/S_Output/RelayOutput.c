/*****************************************************************************
 * File: RelayOutput.c
 * Title: リレー出力を使う。
 * LastUpdated: 2025.06.13
******************************************************************************/

/**
 * @file RelayOutput.c
 * @brief リレー出力を使う。\n
 *		  回路図のISOOUT0をリレー0とする。\n
 *		  回路図のISOOUT1をリレー1とする。
 */

#include "RelayOutput.h"
#include "mcu.h"
#include "rdwr_reg.h"
#include "Output.h"

//オープンドレイン　プルアップ
//relay0 / 00 00(1次) 1(オープンドレイン) 0(プルアップなし) 1(出力有効) 0(入力無効)
//relay1 / 00 00(1次) 1(オープンドレイン) 0(プルアップなし) 1(出力有効) 0(入力無効)

#define RELAY0_CONFIG	( (0x0A) << 16 )
#define RELAY1_CONFIG	( (0x0A) << 24 )
#define RELAY0_VALUE	(0x40)
#define RELAY1_VALUE	(0x80)
#define RELAY0_OFF		(0x40)
#define RELAY1_OFF		(0x80)
#define RELAY0_ON		(0x00)
#define RELAY1_ON		(0x00)

//#define TEST
#ifdef TEST
static uint32_t relay0IOTest = 0x00;
static uint32_t relay1IOTest = 0x00;
static uint32_t relay0Test = 0x00;
static uint32_t relay1Test = 0x00;
#endif

void RelayOutputInit(void)
{
#ifndef TEST
	//port66 relay0 P6MOD1 67 66 65 64 (それぞれ8bit)
	set_bit(PORT6->P6MOD1, RELAY0_CONFIG);
	//port57 relay1 P5MOD1 57 56 55 54
	set_bit(PORT5->P5MOD1, RELAY1_CONFIG);
#else
	set_bit(relay0IOTest,RELAY0_CONFIG);
	set_bit(relay1IOTest,RELAY1_CONFIG);
#endif
	
	//プルアップで出力されているためオフにする。
	RelayOutputRelay0Off();
	RelayOutputRelay1Off();
}

void RelayOutputRelay0Off(void)
{
#ifndef TEST
	OutputOnUInt32(&(PORT6->P6DO), (uint32_t)RELAY0_VALUE);
#else
	OutputOnUInt32(&relay0Test, (uint32_t)RELAY0_VALUE);
#endif
}

void RelayOutputRelay0On(void)
{
#ifndef TEST
	OutputOffUInt32(&(PORT6->P6DO), (uint32_t)RELAY0_VALUE);
#else
	OutputOffUInt32(&relay0Test, (uint32_t)RELAY0_VALUE);
#endif
}

void RelayOutputRelay1Off(void)
{
#ifndef TEST
	OutputOnUInt32(&(PORT5->P5DO), (uint32_t)RELAY1_VALUE);
#else
	OutputOnUInt32(&relay1Test, (uint32_t)RELAY1_VALUE);
#endif
}

void RelayOutputRelay1On(void)
{
#ifndef TEST
	OutputOffUInt32(&(PORT5->P5DO), (uint32_t)RELAY1_VALUE);
#else
	OutputOffUInt32(&relay1Test, (uint32_t)RELAY1_VALUE);
#endif
}

int RelayOutputTest(void)
{
	//初期化
	RelayOutputInit();
#ifdef TEST
	if( ! ( relay0IOTest == RELAY0_CONFIG ) && ( relay1IOTest == RELAY1_CONFIG ) ) return -1;
#endif
	
	//リレー0 On
	RelayOutputRelay0On();
#ifdef TEST
	if(relay0Test != RELAY0_ON) return -1;
#endif
	
	//リレー0 On
	RelayOutputRelay0On();
#ifdef TEST
	if(relay0Test != RELAY0_ON) return -1;
#endif
	
	//リレー0 Off
	RelayOutputRelay0Off();
#ifdef TEST
	if(relay0Test != RELAY0_OFF) return -1;
#endif

	//リレー0 Off
	RelayOutputRelay0Off();
#ifdef TEST
	if(relay0Test != RELAY0_OFF) return -1;
#endif
	
	//リレー1 On
	RelayOutputRelay1On();
#ifdef TEST
	if(relay1Test != RELAY1_ON) return -1;
#endif

	//リレー1 On
	RelayOutputRelay1On();
#ifdef TEST
	if(relay1Test != RELAY1_ON) return -1;
#endif

	//リレー1 Off
	RelayOutputRelay1Off();
#ifdef TEST
	if(relay1Test != RELAY1_OFF) return -1;
#endif

	//リレー1 Off
	RelayOutputRelay1Off();
#ifdef TEST
	if(relay1Test != RELAY1_OFF) return -1;
#endif

	return 0;
}
