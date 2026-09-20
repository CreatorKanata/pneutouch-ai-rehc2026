/*****************************************************************************
 * Copyright (C) 2024 ROHM Co., Ltd.
******************************************************************************/
/*****************************************************************************
 * File: Uart1.c
 * Title: UARTF1の制御を行う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file Uart1.c
 * @brief UARTF1の制御を行う。
 */
 
#include <stdio.h>
#include "Uart1.h"
#include "uartf1.h"
#include "irq.h"
#include "smpl_common.h"

/**< UARTF1設定 */
#define UARTF1_PARAM_MODE	( UARTF_LG_8BIT | UARTF_STP_1BIT | UARTF_PT_NON | UARTF_BC_DIS | UARTF_DLAB_RBR_THR  | UARTF_RFR_KEEP | UARTF_TFR_KEEP | UARTF_FTL_2BYTE )  
#define UARTF1_PARAM_CAJ	( UARTF_RMV_ENA | 0x0019U )   
/**< 115200bps */
//#define UARTF1_PARAM_DLR	( 0x0019U )
/**< 9600bps */
#define UARTF1_PARAM_DLR	( 0x012CU ) 

/**< 送信用 */
static uartfCtrlParam_t writeCtrlParam;   
/**< 受信用 */
static uartfCtrlParam_t readCtrlParam;    


void UAF1_IRQHandler( void );

/**
 * @brief uart1パラメータ初期化。
 *
 * @note uart1としているのはバッファを使わないため。
 */
static void uart1Init( uint16_t uafnmod, uint16_t uafncaj, uint16_t brDivisorLatch);

/**
 * @brief データを一つ送信する。
 *
 * @param param 
 */
__INLINE static void writeSingleData( uartfCtrlParam_t *param );

/**
 * @brief 受信時処理。
 */
static void uart1ContinueReadByte( void );

/**
 * @brief 送信時処理。
 */
static int32_t uart1ContinueWriteByte( uint16_t intStatus );

/**
 * @brief 割り込み処理。
 */
static void uart1Interrupt( void );






void Uart1PeripheralInit(void)
{
	__disable_irq();
	irq_uaf1_dis();
	
	smpl_enablePeripheral(UAF1_PERI);
	//(RXDF0, TXDF0)
	set_reg32(PORT7->P7MOD0, (0x22U << 8U) | (0x21U << 0U)); 
	//パラメータ設定用通信1B受信割り込み
	irq_uaf1_setLevel(0);
	uart1Init( (uint16_t)UARTF1_PARAM_MODE, (uint16_t)UARTF1_PARAM_CAJ, (uint16_t)UARTF1_PARAM_DLR);
	
	irq_uaf1_clearIRQ();
	irq_uaf1_ena();
	__enable_irq();
}

void Uart1Write( uint8_t *data, uint32_t size, cbfUartF_t func )
{
	/*=== Transmission of a message system order parameter setting. ===*/
	writeCtrlParam.data      = data;
	writeCtrlParam.size      = size;
	writeCtrlParam.cnt       = 0;
	writeCtrlParam.callBack  = func;
	writeCtrlParam.errStat   = 0;

	/*=== transmit a message, and it is worked to start. ===*/
	/* Write next data block */
	writeSingleData(&writeCtrlParam);
	
	/* Interrupt enable */
	set_reg32( UARTF1->UAF0IER, UARTF_ETBEI_ENA);
}

void Uart1StartReadByte( cbfUartF_t func )
{
	/*===   Clear status    ===*/
	uartf1_getStatus();                /* clear UF0OER,UF0PER,UF0FER */

	/*=== Transmission of a message system order parameter setting. ===*/
	readCtrlParam.data       = NULL;
	readCtrlParam.size       = 0;
	readCtrlParam.cnt        = 0;
	readCtrlParam.callBack   = func;
	readCtrlParam.errStat    = 0;

	/* Interrupt enable */
	set_reg32( UARTF1->UAF0IER, (UARTF_ERBFI_ENA | UARTF_ELSI_ENA) );
}

void Uart1StopReadByte(void)
{
	/*===   Clear status    ===*/
	uartf1_getStatus();                /* clear UF0OER,UF0PER,UF0FER */

	/*=== Transmission of a message system order parameter setting. ===*/
	readCtrlParam.data       = NULL;
	readCtrlParam.size       = 0;
	readCtrlParam.cnt        = 0;
	readCtrlParam.callBack   = NULL;
	readCtrlParam.errStat    = 0;

	/* Interrupt disable */
	clear_reg32( UARTF1->UAF0IER,(UARTF_ERBFI_ENA | UARTF_ELSI_ENA));
}

void UAF1_IRQHandler( void )
{
	uart1Interrupt();
}





static void uart1Init( uint16_t uafnmod, uint16_t uafncaj, uint16_t brDivisorLatch )
{
	/*=== Register setting ===*/
	/*---   Baud rate clock setting   ---*/
	set_bit( UARTF1->UAF0MOD, (1 << 7) );               /* enable DLR(Divisor Latch Resister) access */
	write_reg32( UARTF1->UAF0BUF, brDivisorLatch );     /* set DLR */
	clear_bit( UARTF1->UAF0MOD, (1 << 7) );             /* disable DLR(Divisor Latch Resister) access */
	write_reg32( UARTF1->UAF0CAJ, uafncaj );            /* set adjustment mode and value */

	/*---   Communication setting   ---*/
	write_reg32( UARTF1->UAF0MOD, uafnmod );
	write_reg32( UARTF1->UAF0IER, (uint16_t)(UARTF_ERBFI_DIS | UARTF_ETBEI_DIS | UARTF_ELSI_DIS) );

	/*---   Communication status is clear   ---*/
	uartf1_getStatus();      /* clear UF0OER,UF0PER,UF0FER */
	uartf1_getIntCause();    /* clear write request */
}

__INLINE static void writeSingleData( uartfCtrlParam_t *param )
{
	uartf1_putc( *param->data );
	param->data++;
	param->cnt++;
}

static void uart1ContinueReadByte( void )
{
	uint8_t   data;
	uint16_t   errStat = 0;
	data = (uint8_t)uartf1_getc();
	errStat = (uint16_t)uartf1_getStatus();
	if( readCtrlParam.callBack != NULL ){
		readCtrlParam.callBack( (uint32_t)data, errStat );
	}
}

static int32_t uart1ContinueWriteByte( uint16_t intStatus )
{
	int32_t ret = UARTF_R_TRANS_CONT_OK;

	intStatus = intStatus & UARTF_IRID_MASK;
	if ( (intStatus != UARTF_IRID_WRITE_REQ) && (intStatus != UARTF_IRID_TRANS_COMP) ) {
		return UARTF_R_TRANS_CONT_OK;
	}
	/*---   Are transmission of a message data left?    ---*/
	if( writeCtrlParam.size > writeCtrlParam.cnt ){
		/*=== I transmit a message, and it is worked to continue. ===*/
		/*--- There are data in the transmission of a message buffer? ---*/
		/* Write next data block */
		writeSingleData( &writeCtrlParam );
		ret = (int32_t)( UARTF_R_TRANS_CONT_OK );
	}
	
	else{
		//=== It is returned that the transmission ended. ===
		if(intStatus == UARTF_IRID_WRITE_REQ){
			clear_reg32( UARTF1->UAF0IER, UARTF_ETBEI_ENA);
			set_reg32( UARTF1->UAF0IER, UARTF_TEMTI_ENA);
			ret = (int32_t)( UARTF_R_TRANS_CONT_OK );
		}
		
		else{
			clear_reg32( UARTF1->UAF0IER, (UARTF_ETBEI_ENA | UARTF_TEMTI_ENA));

			// Callback 
			if( writeCtrlParam.callBack != NULL ){
				writeCtrlParam.callBack( writeCtrlParam.cnt, writeCtrlParam.errStat );
			}
			ret = (int32_t)( UARTF_R_TRANS_FIN );
		}
	}
	return ret;
}



static void uart1Interrupt( void )
{
	uint32_t intStat;

	intStat = uartf1_getIntCause() & UARTF_IRID_MASK;
	if(intStat == UARTF_IRID_READ_REQ)
	{
		uart1ContinueReadByte();
	}
	else if( ( intStat == UARTF_IRID_WRITE_REQ) || (intStat == UARTF_IRID_TRANS_COMP))
	{
		uart1ContinueWriteByte((uint16_t)intStat);
	}
	else //if( intStat == UARTF_IRID_DATA_ERR )
	{
		//clear data error
		uartf1_getStatus();
	}
}
