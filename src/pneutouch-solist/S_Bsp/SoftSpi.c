/*****************************************************************************
 * File: SoftSpi.c
 * Title: ソフトウェアSPIモジュール
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file SoftSpi.c
 * @brief ソフトウェアSPIモジュール
 */

#include "SoftSpi.h"
#include "mcu.h"
#include "smpl_common.h"
#include "rdwr_reg.h"
#include "irq.h"

/*############################################################################*/
/*#                                  Macro                                   #*/
/*############################################################################*/

/**< P83:In PullUp P82:Out P81:Out P80:In */
#define P8MOD0_PARAM	0x05020201
#define P8MOD0_MASK		0xFFFFFFFF
/**< P85:Out P84:Out */
#define P8MOD1_PARAM	0x00000202
#define P8MOD1_MASK		0x0000FFFF

#define rtcCsOn()		( set_bit( PORT8->P8DO, (1 << 1U) ) )
#define rtcCsOff()		( clear_bit( PORT8->P8DO, (1 << 1U) ) )
#define framCsOn()		( clear_bit( PORT8->P8DO, (1 << 2U) ) )
#define framCsOff()		( set_bit( PORT8->P8DO, (1 << 2U) ) )

#define mosiHigh()		( set_bit( PORT8->P8DO, (1 << 4U) ) )
#define mosiLow()		( clear_bit( PORT8->P8DO, (1 << 4U) ) )
#define clkHigh()		( set_bit( PORT8->P8DO, (1 << 5U) ) )
#define clkLow()		( clear_bit( PORT8->P8DO, (1 << 5U) ) )

#define MISO_BIT		( get_bit( PORT8->P8DI, (1 << 3U) ) )

/*############################################################################*/
/*#                                Variable                                  #*/
/*############################################################################*/

/*############################################################################*/
/*#                               Prototype                                  #*/
/*############################################################################*/
static void oneClock(void);
static uint8_t duplexByte(uint8_t data);

/*############################################################################*/
/*#                              Subroutine                                  #*/
/*############################################################################*/

/*############################################################################*/
/*#                                  API                                     #*/
/*############################################################################*/

void SoftSpiPeripheralInit(void)
{
	SoftSpiDeviceDisable();
	clkHigh();

	write_bit( PORT8->P8MOD0, P8MOD0_MASK, P8MOD0_PARAM);
	write_bit( PORT8->P8MOD1, P8MOD1_MASK, P8MOD1_PARAM);
}

void SoftSpiDeviceEnable(SOFT_SPI_DEVICE dev)
{
	SoftSpiDeviceDisable();
	clkHigh();
	switch (dev)
	{
	case SOFT_SPI_DEVICE_RTC:	rtcCsOn();	break;
	case SOFT_SPI_DEVICE_FRAM:	framCsOn();	break;
	}
}

void SoftSpiDeviceDisable(void)
{
	rtcCsOff();
	framCsOff();
}

// SPI書込をMSBファーストのSPIモード3(CPOL=1, CPHA=1)で行う
// 必ずSoftSpiDeviceEnableにより準備を行った後に実行すること
// writeBuf         : 書込みデータ
// length           : 書込みデータのバイト数
void SoftSpiWrite(const void* writeBuf, int length)
{
	int i;
	const uint8_t* w = (const uint8_t*)writeBuf;
	for (i = 0; i < length; i++)
	{
		duplexByte(*w);
		w++;
	}
}

// SPI書込をMSBファーストのSPIモード3(CPOL=1, CPHA=1)で行う
// 必ずSoftSpiDeviceEnableにより準備を行った後に実行すること
// readBuf          : データの格納先
// length           : 読込バイト数
// dummyData        : 読込のため送出する際の値
void SoftSpiRead(void* readBuf, int length, uint8_t dummyData)
{
	int i;
	uint8_t* r = (uint8_t*)readBuf;
	for (i = 0; i < length; i++)
	{
		*r = duplexByte(dummyData);
		r++;
	}
}

void SoftSpiDuplex(
	const void* writeBuf,
	void* readBuf,
	int writeBufLength,
	uint8_t dummyData,
	int dummyLength)
{
	int i;
	const uint8_t* w = (const uint8_t*)writeBuf;
	uint8_t* r = (uint8_t*)readBuf;
	for (i = 0; i < writeBufLength; i++)
	{
		*r = duplexByte(*w);
		w++;
		r++;
	}
	for (i = 0; i < dummyLength; i++)
	{
		*r = duplexByte(dummyData);
		r++;
	}
}


/*############################################################################*/
/*#                                  Local                                   #*/
/*############################################################################*/

static void oneClock(void)
{
	// クロックパルス幅を稼ぐため
	volatile int delay = 0;
	clkLow();
	delay++;
	delay++;
	clkHigh();
	delay++;
	delay++;
}

static uint8_t duplexByte(uint8_t data)
{
	uint8_t read = 0;

	// MSBファースト
	for (int i = 7; 0 <= i; i--)
	{
		if(data & (1U << i)){ mosiHigh(); }
		else{ mosiLow(); }
		oneClock();
		if(MISO_BIT)
		{
			read |= (1U << i);
		}
	}
	return read;
}
