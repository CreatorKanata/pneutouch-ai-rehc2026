/*****************************************************************************
 * Copyright (C) 2024 ROHM Co., Ltd.
******************************************************************************/
/*****************************************************************************
 * File: Kx134Spi.c
 * Title: Kx134Accで使用するSpiとセンサーデータ読み取り可能割り込みの制御を行う。
 * LastUpdated: 2025.06.13
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

#include <stdio.h>
#include "ssiof0.h"
#include "Kx134Spi.h"
#include "irq.h"
#include "smpl_common.h"
#include "TimeControl.h"

//#define TEST
/**< SPI各種設定 */
#define SSIOF0_PARAM_MODE0			( SSIOF_MST_MASTER      | SSIOF_LG_16BIT      | SSIOF_MDF_DIS     | SSIOF_DIR_MSB      | SSIOF_CPHA_1SM_2SH )
#define SSIOF0_PARAM_MODE1			( SSIOF_CPOL_LOW        | SSIOF_SSZ_OUTPUT   | SSIOF_SOZ_OUTPUT  | SSIOF_MOZ_OUTPUT )                      
#define SSIOF0_PARAM_MODE			( SSIOF0_PARAM_MODE0     | SSIOF0_PARAM_MODE1  )
#define SSIOF0_PARAM_INT			( SSIOF_INT_WR_THRESH_0 )
/**< SPI通信速度 */
#define SSIOF0_BAUDRATE				( 0x0003U )
/**< SPIデータ転送幅毎の待ち時間 */
#define SSIOF0_DELAY_INTERVAL		( SSIOF_LEAD_05 | SSIOF_LAG_05 )
#define SSIOF0_TRANSMIT_INTERVAL	( 0U )
/**< SPIで使用する転送終了割り込み */
#define SPI_INTERRUPT				(0x04)
void SIOF0_IRQHandler( void );
void EXI_IRQHandler(void);
static ssiofCtrlParam_t s_ctrlParam;
static Kx134SpiSensorDataReadyInterrupt exe = NULL;

/** 
 * @brief FIFOに2Byte書き込む。
 */
static void spiWriteFifoTwoByte(void);

/** 
 * @brief FIFOに4Byte書き込む。
 */
static void spiWriteFifoFourByte(void);

/** 
 * @brief FIFOに6Byte書き込む。
 */
static void spiWriteFifoSixByte(void);

/** 
 * @brief FIFOに8Byte書き込む。
 */
static void spiWriteFifoEightByte(void);

/** 
 * @brief SPIの転送終了割り込みを管理する。
 *
 * @return int32_t 転送終了割り込み処理が完了したことを通知。
 */
static int32_t kx134SpiInterrupt( void );

//FIFOを超える数で送受信しないため、オーバーランエラーは発生しない。
void Kx134SpiPeripheralInit(void)
{
	__disable_irq();
	Kx134SpiSensorDataReadyInterruptUnuse();
	irq_siof0_dis();
	smpl_enablePeripheral(SIOF0_PERI);
	//加速度センサーSPI
	set_reg32( PORT4->P4MOD0, ( 0x12 << 24 ) | ( 0x11 << 16 ) | ( 0x12 << 8 ) | ( 0x13 << 0 ) );
	//加速度センサINT1
	set_reg32(PORT2->P2MOD0, (0x01U << 16));
	//SPI設定
	ssiof0_init( SSIOF0_PARAM_MODE, SSIOF0_PARAM_INT );  
	ssiof0_setBaudrate( SSIOF0_DELAY_INTERVAL | SSIOF0_BAUDRATE );
	ssiof0_setIntervalTime( SSIOF0_TRANSMIT_INTERVAL );
	//加速度センサ1B送受信割り込み
	irq_siof0_setLevel(0);
	//加速度センサINT1割り込み
	irq_exi_setLevel(2);
	Kx134SpiSetSensorDataReadyInterrupt(NULL);
	//割り込み許可
	irq_siof0_clearIRQ();
	irq_siof0_ena();
	__enable_irq();
}

int Kx134SpiTest(void)
{
	uint16_t receive[10] = {0,0,0,0,0,0,0,0,0,0};
	uint16_t transmit[10] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A};
	uint16_t txBuffer[4] = {0xAA,0xAA,0xAA,0xAA};
	uint16_t rxBuffer[4] = {0xDD,0xDD,0xDD,0xDD};
	const uint16_t testDataCnt = 4;

	//初期化
	Kx134SpiPeripheralInit();
	TimeControlInit();
	
	//レジスタ
	if(read_reg32(PORT4->P4MOD0) != ( ( 0x12 << 24 ) | ( 0x11 << 16 ) | ( 0x12 << 8 ) | ( 0x13 << 0 ) )) return -1;
	if((read_reg32(PORT2->P2MOD0) & 0x00ff0000) != (0x01U << 16) ) return -1;
	//spi
	if(read_reg32(SSIOF0->SF0CTRL) != SSIOF0_PARAM_MODE) return -1;
	if(read_reg32(SSIOF0->SF0INTC) != SSIOF0_PARAM_INT) return -1; //
	if(read_reg32(SSIOF0->SF0BRR) != ( SSIOF0_DELAY_INTERVAL | SSIOF0_BAUDRATE) ) return -1;
	if(read_reg32(SSIOF0->SF0TRAC) != SSIOF0_TRANSMIT_INTERVAL) return -1;
	//SPI割込み NVIC有効、ペリフェラル無効、優先度0、割り込み未発生、保留無
	if(read_reg32(SSIOF0->SF0INTC & 0x1F) != 0) return -1; 
	if(!NVIC_GetEnableIRQ(SIOF0_IRQn)) return -1;
	if(NVIC_GetPriority(SIOF0_IRQn) != 0 ) return -1;
	if((ssiof0_getStatus() & 0x1F) != 0) return -1;
	if(irq_siof0_checkIRQ())return -1;
	//Irq割込み NVIC有効、ペリフェラル無効、優先度1(64)、割り込み未発生、保留無
	if((read_reg32(NINT->EXICON) & 0x03) != EXI0_EDGE_BIT_NO) return -1;
	if(NVIC_GetEnableIRQ(EXI_IRQn)) return -1;
	if(NVIC_GetPriority(EXI_IRQn) != 2 ) return -1;
	if((read_reg32(NINT->EXIST) & 0x000000ff) != 0) return -1;
	if(irq_exi_checkIRQ())return -1;
	//コールバック
	if(exe != NULL) return -1;
	
	
	//割込み操作
	//SPI
	Kx134SpiInterruptUse();
	if(read_reg32(SSIOF0->SF0INTC & 0x1F) != SPI_INTERRUPT) return -1; 
	if(!NVIC_GetEnableIRQ(SIOF0_IRQn)) return -1;
	if((ssiof0_getStatus() & 0x1F) != 0) return -1;
	if(irq_siof0_checkIRQ())return -1;
	//割り込み禁止しないとすぐに割り込み発生してしまうので注意
	__disable_irq();
	NVIC_SetPendingIRQ(SIOF0_IRQn);
	if((ssiof0_getStatus() & 0x1F) != 0) return -1;
	if(!irq_siof0_checkIRQ())return -1;
	Kx134SpiInterruptUnuse();
	if(read_reg32(SSIOF0->SF0INTC & 0x1F) != 0) return -1; 
	if(NVIC_GetEnableIRQ(SIOF0_IRQn)) return -1;
	if((ssiof0_getStatus() & 0x1F) != 0) return -1;
	if(irq_siof0_checkIRQ())return -1;
	__enable_irq();
	//EXI
	Kx134SpiSensorDataReadyInterruptUse();
	if((read_reg32(NINT->EXICON) & 0x0F) != ( (EXIn_FILTER_DIS << 3) | (EXIn_SAMPLING_DIS << 2) |  EXIn_EDGE_FALLING) ) return -1;
	if((read_reg32(NINT->EXI03SEL) & 0xFF) != EXIn_PORT_SEL_P22) return -1; 
	if(!NVIC_GetEnableIRQ(EXI_IRQn)) return -1;
	//MEMSセンサーを外していないと割り込み要求が来るので注意。
	if((read_reg32(NINT->EXIST) & 0x000000ff) != 0) return -1;
	if(irq_exi_checkIRQ())return -1;
	__disable_irq();
	NVIC_SetPendingIRQ(EXI_IRQn);
	if((read_reg32(NINT->EXIST) & 0x000000ff) != 0) return -1;
	if(!irq_exi_checkIRQ())return -1;
	Kx134SpiSensorDataReadyInterruptUnuse();
	if((read_reg32(NINT->EXICON) & 0x03) != EXI0_EDGE_BIT_NO) return -1;
	if(NVIC_GetEnableIRQ(EXI_IRQn)) return -1;
	if((read_reg32(NINT->EXIST) & 0x000000ff) != 0) return -1;
	if(irq_exi_checkIRQ())return -1;	
	__enable_irq();
	
	
	//SPI通信(割り込みあり)
	__disable_irq();
	Kx134SpiInterruptUse();
	Kx134SpiStart(receive,transmit, testDataCnt,NULL);
	if(s_ctrlParam.callBack != NULL) return -1;
	//fifo書き込みAPIチェック API16bit×4のFifoなので最後尾に0x04が入っているはず。
	if(read_reg32(SSIOF0->SF0DWR) != 0x04) return -1;
	__enable_irq();
	TimeControlDelayMs(10);
	//転送終了割り込み、SPI割込みチェック 割込みが実行されたかのフラグを確認する。
	if(!s_ctrlParam.errStat) return -1;
	if(read_reg32(SSIOF0->SF0INTC & 0x1F) != SPI_INTERRUPT) return -1; 
	if(!NVIC_GetEnableIRQ(SIOF0_IRQn)) return -1;
	if(irq_siof0_checkIRQ())return -1;
	//SPI通信(割り込みなし)
	__disable_irq();
	Kx134SpiInterruptUnuse();
	Kx134SpiStart(receive,transmit, testDataCnt,NULL);
	if(s_ctrlParam.callBack != NULL) return -1;
	if(read_reg32(SSIOF0->SF0DWR) != 0x04) return -1;
	__enable_irq();
	TimeControlDelayMs(10);
	//転送終了割り込み、SPI割込みチェック　割込みが実行されたかのフラグを確認する。
	if(s_ctrlParam.errStat) return -1;
	//fifo読み込みAPIチェック API16bit×4のFifoなので最後尾に0x04が入っているはず。
	Kx134SpiReadFifoEightByte();
	if(receive[3] != 0xFFFF) return -1;
	if(read_reg32(SSIOF0->SF0INTC & 0x1F) != 0) return -1; 
	if(NVIC_GetEnableIRQ(SIOF0_IRQn)) return -1;
	if(irq_siof0_checkIRQ()) return -1;
	
	

	Kx134SpiInterruptUse();
	
	//正常
	if(SSIOF_R_OK != Kx134SpiStart(rxBuffer,txBuffer, 4, NULL)) return -1;
	TimeControlDelayMs(10);
	if(s_ctrlParam.cnt != 4) return -1;

	if(SSIOF_R_OK != Kx134SpiStart(rxBuffer,txBuffer, 3, NULL)) return -1;
	TimeControlDelayMs(10);
	if(s_ctrlParam.cnt != 3) return -1;
	
	if(SSIOF_R_OK != Kx134SpiStart(rxBuffer,txBuffer, 2, NULL)) return -1;
	TimeControlDelayMs(10);
	if(s_ctrlParam.cnt != 2) return -1;
	
	if(SSIOF_R_OK != Kx134SpiStart(rxBuffer,txBuffer, 1, NULL)) return -1;
	TimeControlDelayMs(10);
	if(s_ctrlParam.cnt != 1) return -1;
	
	Kx134SpiInterruptUnuse();
	rxBuffer[0] = 0xDD;
	rxBuffer[1] = 0xDD;
	rxBuffer[2] = 0xDD;
	rxBuffer[3] = 0xDD;
	Kx134SpiStart(rxBuffer,txBuffer, 4, NULL);
	TimeControlDelayMs(10);
	Kx134SpiReadFifoEightByte();
	if(rxBuffer[0] == 0xDD) return -1;
	if(rxBuffer[1] == 0xDD) return -1;
	if(rxBuffer[2] == 0xDD) return -1;
	if(rxBuffer[3] == 0xDD) return -1;
	
	//異常
	if(SSIOF_R_ERR != Kx134SpiStart(NULL,txBuffer, 1, NULL)) return -1;
	if(SSIOF_R_ERR != Kx134SpiStart(rxBuffer, NULL, 1, NULL)) return -1;
	if(SSIOF_R_ERR != Kx134SpiStart(NULL, NULL, 1, NULL)) return -1;
	
	return 0;
}

void Kx134SpiClearFifo(void)
{
	//通信禁止
	clear_bit( SSIOF0->SF0CTRL, (1 << 0) );
	//FIFOクリア
	set_bit( SSIOF0->SF0CTRL, (1 << 8) );
	clear_bit( SSIOF0->SF0CTRL, (1 << 8) );
}

static void spiWriteFifoTwoByte(void)
{
	ssiofCtrlParam_t *param = &s_ctrlParam;
	ssiof0_putcWord( *((unsigned short *)param->txData) );
	param->txData = (void*)(( (unsigned short *)s_ctrlParam.txData ) + 1);
	param->cnt++;
}

static void spiWriteFifoFourByte(void)
{
	spiWriteFifoTwoByte();
	spiWriteFifoTwoByte();
}

static void spiWriteFifoSixByte(void)
{
	spiWriteFifoTwoByte();
	spiWriteFifoTwoByte();
	spiWriteFifoTwoByte();
}

static void spiWriteFifoEightByte(void)
{
	spiWriteFifoTwoByte();
	spiWriteFifoTwoByte();
	spiWriteFifoTwoByte();
	spiWriteFifoTwoByte();
}

void Kx134SpiReadFifoTwoByte(void)
{
	ssiofCtrlParam_t *param = &s_ctrlParam;
	*( (unsigned short *)param->rxData ) = (unsigned short)ssiof0_getcWord();
	param->rxData = (void*)(( (unsigned short *)param->rxData ) + 1);
}

void Kx134SpiReadFifoFourByte( void )
{
	Kx134SpiReadFifoTwoByte();
	Kx134SpiReadFifoTwoByte();
}

void Kx134SpiReadFifoSixByte( void )
{
	Kx134SpiReadFifoTwoByte();
	Kx134SpiReadFifoTwoByte();
	Kx134SpiReadFifoTwoByte();
}

void Kx134SpiReadFifoEightByte( void )
{
	Kx134SpiReadFifoTwoByte();
	Kx134SpiReadFifoTwoByte();
	Kx134SpiReadFifoTwoByte();
	Kx134SpiReadFifoTwoByte();
}


int32_t Kx134SpiStart( void *rxData, void *txData, uint32_t dataCnt, cbfSsiof_t func)
{
	s_ctrlParam.rxData      = rxData;
	s_ctrlParam.txData      = txData;
	s_ctrlParam.dataSize    = dataCnt;
	s_ctrlParam.cnt         = 0;
	
	if(rxData == NULL) return SSIOF_R_ERR;
	if(txData == NULL) return SSIOF_R_ERR;
	
	if(func != NULL)
	{
		s_ctrlParam.callBack = func;
	}
	else
	{
		s_ctrlParam.callBack = NULL;
	}
	s_ctrlParam.errStat     = 0;
	
	//FIFO書き込み
	if(s_ctrlParam.dataSize == 1)
	{
		spiWriteFifoTwoByte();
	}
	else if(s_ctrlParam.dataSize == 2)
	{
		spiWriteFifoFourByte();
	}
	else if(s_ctrlParam.dataSize == 3)
	{
		spiWriteFifoSixByte();
	}
	else if(s_ctrlParam.dataSize == 4)
	{
		spiWriteFifoEightByte();
	}
	
	//通信開始
	set_bit( SSIOF0->SF0CTRL, (1 << 0) );

	return ( SSIOF_R_OK );
}


//転送終了割り込み
static int32_t kx134SpiInterrupt( void )
{
	unsigned short status = 0;

	//ペリフェラルの割り込みクリア
	status = (unsigned short)ssiof0_getStatus();
	ssiof0_clearStatus( status );
	//FIFO読出し
	switch(s_ctrlParam.cnt)
	{
		case 1:
			Kx134SpiReadFifoTwoByte();
			break;
		case 2:
			Kx134SpiReadFifoFourByte();
			break;
		case 3:
			Kx134SpiReadFifoSixByte();
			break;
		case 4:
			Kx134SpiReadFifoEightByte();
			break;
	}

	//コールバック
	if( s_ctrlParam.callBack != NULL)
	{
		s_ctrlParam.callBack( s_ctrlParam.cnt, s_ctrlParam.errStat );
	}
#ifdef TEST
	//テスト用
	s_ctrlParam.errStat = 1;
#endif
	return ( SSIOF_R_TRANS_FIN );
}

//ssiof0 割り込みハンドラ
void SIOF0_IRQHandler( void )
{
	kx134SpiInterrupt();
}

void Kx134SpiSetSensorDataReadyInterrupt(Kx134SpiSensorDataReadyInterrupt func)
{
	exe = func;
}

//kx134加速度センサ　データ読み取り可能割り込み
void EXI_IRQHandler( void )
{
	if(exe != NULL) exe();
	//ペリフェラルの割り込みクリア
	irq_ext0_clearIRQ();
}

void Kx134SpiSensorDataReadyInterruptUse(void)
{
	//NVICの割り込み許可
	irq_exi_ena();
	//ペリフェラルの割り込み許可
	irq_ext0_init( EXIn_EDGE_FALLING, EXIn_SAMPLING_DIS, EXIn_FILTER_DIS, EXIn_PORT_SEL_P22 );
}

void Kx134SpiSensorDataReadyInterruptUnuse(void)
{
	__disable_irq();
	//ペリフェラルの割り込み禁止
	irq_ext0_dis();
	//NVICの割り込み禁止
	irq_exi_dis();
	//NVICの割り込みクリア
	irq_exi_clearIRQ();
	//ペリフェラルの割り込みクリア
	irq_ext0_clearIRQ();
	__enable_irq();
}

void Kx134SpiInterruptUse(void)
{
	//NVICの割り込み許可
	irq_siof0_ena();
	//ペリフェラルの割り込み許可
	set_bit( SSIOF0->SF0INTC, SPI_INTERRUPT);
}

void Kx134SpiInterruptUnuse(void)
{
	__disable_irq();
	//ペリフェラルの割り込み禁止
	clear_bit( SSIOF0->SF0INTC, SPI_INTERRUPT);
	//NVICの割り込み禁止
	irq_siof0_dis();
	//NVICの割り込みクリア
	irq_siof0_clearIRQ();
	//ペリフェラルの割り込みクリア
	ssiof0_clearStatus((unsigned short)ssiof0_getStatus());
	__enable_irq();
}
