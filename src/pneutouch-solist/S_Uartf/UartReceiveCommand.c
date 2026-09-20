/*****************************************************************************
 * File: UartReceiveCommand.c
 * Title: UARTで受信したコマンドをパースする。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file UartReceiveCommand.c
 * @brief UARTで受信したコマンドをパースする。
 */
#include <stdio.h>
#include <string.h>
#include "irq.h"
#include "Uart1.h"
#include "UartReceiveCommand.h"
#include "UartCommandCommon.h"
#include "UartCommand.h"
#include "UartQueue.h"
#include "TimeControl.h"
/**
 * @brief コマンドのパースの状態を示す列挙型
 */
typedef enum
{
	UART_PARSE_STATE_NOT_RECEIVE,					/**< コマンド受信中でない */
	UART_PARSE_STATE_RECEIVING,						/**< コマンド受信、パース中 */
}UART_PARSE_STATE;

static UART_PARSE parseData;
static UART_PARSE_STATE uartParseState = UART_PARSE_STATE_NOT_RECEIVE;

/**
 * @brief 受信したデータをパースする。
 *
 * @param rcv 
 * @param byte
 */
static void parsebyte(UART_PARSE* rcv, uint8_t byte);

/**
 * @brief パース用データのバッファをクリアする。
 *
 * @param rcv
 */
static void clearBuffer(UART_PARSE* rcv);

/**
 * @brief パース用データのバッファに書き込む。
 *
 * @param rcv
 * @param byte
 */
static void putChar(UART_PARSE* rcv, uint8_t byte);

/**
 * @brief パケットをパースする。
 *
 * @param rcvs
 */
static void parsePacket(UART_PARSE* rcv);

void UartReceiveCommandInit(void)
{
	UART_PARSE* rcv = &parseData;
	rcv->Start = rcv->Buffer;
	rcv->End = rcv->Start + sizeof( rcv->Buffer ) / sizeof(uint8_t);
	rcv->Current = rcv->Start;
	rcv->Get = rcv->Start;
	rcv->DataSize = 0;
	rcv->TokenSize = 0;
}

void UartReceiveCommandReceiveBlock(void)
{
	uint8_t data = 0;
	irq_uaf1_dis();
	
	if(UartQueueIsInterruptEmpty())
	{
		irq_uaf1_ena();
		return;
	}

	for( ; ; )
	{
		if( ! UartQueueInterruptDequeue(&data))
		{
			irq_uaf1_ena();
			return;
		}
		UartQueueMainEnqueue(data);
	}
	irq_uaf1_ena();
}


void UartReceiveCommandReceiveAnalizer(void)
{
	uint8_t data = 0;
	UART_PARSE* rcv = &parseData;
	if(UartQueueIsMainEmpty())
	{
		return;
	}

	for( ; ; )
	{	
		if( ! UartQueueMainDequeue(&data) )
		{
			return;
		}
		parsebyte(rcv, data);
	}
}


void UartReceiveCommandUart1InterruptReadCallback( uint32_t data, uint16_t errStatus )
{
	UartQueueInterruptEnqueue((uint8_t)data);
}


static void parsebyte(UART_PARSE* rcv, uint8_t byte)
{
	switch(uartParseState)
	{
		case UART_PARSE_STATE_NOT_RECEIVE:
			if(byte == '#')
			{
				clearBuffer(rcv);
				putChar(rcv,byte);
				uartParseState = UART_PARSE_STATE_RECEIVING;
			}
			break;
		case UART_PARSE_STATE_RECEIVING:
			if(byte == '#')
			{
				clearBuffer(rcv);
				putChar(rcv,byte);
			}
			else if ( byte == '\r')
			{
				putChar(rcv,byte);
				parsePacket(rcv);
				UartCommandExecuteCommand(rcv);
				Uart1Write((uint8_t*)(UartCommandGetResponseString()),UartCommandGetResponseStringLength(),NULL);
				uartParseState = UART_PARSE_STATE_NOT_RECEIVE;
			}
			else
			{
				putChar(rcv,byte);
			}
			break;
	}
}

static void clearBuffer(UART_PARSE* rcv)
{
	rcv->Current = rcv->Start;
	rcv->DataSize = 0;
	rcv->TokenSize = 0;
}

static void putChar(UART_PARSE* rcv, uint8_t byte)
{
	if( rcv->DataSize < ( sizeof(rcv->Buffer) / sizeof(uint8_t) - 1 ) )
	{
		*rcv->Current = byte;
		rcv->Current++;
		rcv->DataSize++;
	}
	else
	{

	}	
}

static void parsePacket(UART_PARSE* rcv)
{
	uint8_t* dataaddr = rcv->Buffer;
	uint8_t** pToken = rcv->Token;
	
	for( ; rcv->DataSize > (uint8_t)0; dataaddr++, rcv->DataSize--)
	{
		switch(*dataaddr)
		{
			case '#':
				*rcv->Token = dataaddr;
				pToken++;
				rcv->TokenSize++;
				break;
			
			case ',':
				*dataaddr = '\0';
				dataaddr++;
				*pToken = dataaddr;
				dataaddr--;
				pToken++;
				rcv->TokenSize++;
				break;
			
			case '\r':
				*dataaddr = '\0';
				dataaddr = rcv->Buffer;
				return;
		}	
	}
}


int UartReceiveCommandTest(void)
{
	//初期化
	char getCommandArray[20] = "#GETPARAM,0\r";
	UartReceiveCommandInit();
	Uart1PeripheralInit();
	if(parseData.Start != parseData.Buffer) return -1;
	if(parseData.End != ( parseData.Start + sizeof( parseData.Buffer ) / sizeof(uint8_t) ) ) return -1;
	if(parseData.Current != parseData.Start) return -1;
	if(parseData.Get != parseData.Start) return -1;
	if(parseData.DataSize != 0) return -1;
	if(parseData.TokenSize != 0) return -1;
	
	//ゲットコマンドでテスト
	UartQueueInit(0);
	for(int i = 0; i < (int)strlen("#GETPARAM,0\r"); i++) UartQueueInterruptEnqueue(getCommandArray[i]);
	for(int i = 0; i < 100; i++)
	{
		UartReceiveCommandReceiveBlock();
		UartReceiveCommandReceiveAnalizer();
	}
	TimeControlInit();
	TimeControlDelayMs(1000);

	//成功
	return 0;
}
