/*****************************************************************************
 * File: UartCommandCommon.h
 * Title: UARTで受信したコマンドをパース、実行するための情報を提供する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file UartCommandCommon.h
 * @brief UARTで受信したコマンドをパース、実行するための情報を提供する。
 */

#ifndef UART_COMMAND_COMMON_H__
#define UART_COMMAND_COMMON_H__
#include <stdint.h>
/**
 * @brief コマンドのパース、実行で使用するバッファサイズ
 */
#define MAXIMUM_NUMBER_OF_UART_BUFFER				(256)

/**
 * @brief コマンドのパース、実行で使用する構造体
 */
typedef struct 
{
	uint8_t Buffer[MAXIMUM_NUMBER_OF_UART_BUFFER];	/**< バッファ */
	uint8_t* Start;									/**< バッファの先頭 */
	uint8_t* End;									/**< バッファの末尾 */
	volatile uint8_t* Current;						/**< 現在位置 */
	uint8_t* Get;									/**< 取得する位置 */
	uint8_t* Token[50];								/**< トークン */
	volatile uint8_t DataSize;						/**< データサイズ */
	uint8_t  TokenSize;								/**< トークンサイズ */
	uint8_t  Padding[2];
}UART_PARSE;

#endif //UART_COMMAND_H__
