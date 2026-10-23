/*****************************************************************************
 * File: SoftSpi.c
 * Title: ソフトウェアSPIモジュール
 * LastUpdated: 2026.10.24
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

#define MOSI_MASK		(1U << 4U)	/**< P84 */
#define CLK_MASK		(1U << 5U)	/**< P85 */
#define clkHigh()		( set_bit( PORT8->P8DO, CLK_MASK ) )

#define MISO_BIT		( get_bit( PORT8->P8DI, (1 << 3U) ) )

/*############################################################################*/
/*#                                Variable                                  #*/
/*############################################################################*/

/*############################################################################*/
/*#                               Prototype                                  #*/
/*############################################################################*/
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

// 1バイトをMSBファーストで送受信する（モード3: SCKはHighで待機）
// 1ビットにつきP8DOへの単純な書込み2回だけで済ませる（読み-変更-書きと遅延ループは廃止）。
// CSなどSCK/MOSI以外のビットはバイト開始時の値のまま変えない。
// MB85RS2MTAは40MHzまで対応し、48MHzで連続2回の書込みより十分速いので遅延は不要（RX4111は実機で要確認）。
static uint8_t duplexByte(uint8_t data)
{
	uint32_t base = read_reg32(PORT8->P8DO) & ~(MOSI_MASK | CLK_MASK);
	uint8_t read = 0;
	int i;

	for (i = 7; i >= 0; i--)
	{
		uint32_t mosi = ((data >> i) & 1U) ? MOSI_MASK : 0U;
		write_reg32(PORT8->P8DO, base | mosi);				// SCK Low: 立下りでMOSIを変える（スレーブはMISOを出す）
		write_reg32(PORT8->P8DO, base | mosi | CLK_MASK);	// SCK High: 立上りでスレーブがMOSIを取り込む
		if (MISO_BIT)
		{
			read |= (uint8_t)(1U << i);
		}
	}
	return read;
}
