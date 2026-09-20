/*****************************************************************************
 * File: UartCommand.h
 * Title: UARTで受信したコマンドを実行する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file UartCommand.h
 * @brief UARTで受信したコマンドを実行する。
 */

#ifndef UART_COMMAND_H__
#define UART_COMMAND_H__

#include <stdint.h>
#include "UartCommandCommon.h"

/**
 * @brief UARTで受信したコマンドを実行する。
 *
 * @param rcv パース済みの受信データ
 */
void UartCommandExecuteCommand(UART_PARSE* rcv);

/**
 * @brief UARTで受信したコマンドのレスポンスを取得する。
 *
 * @return char*
 */
char* UartCommandGetResponseString(void);

/**
 * @brief UARTで受信したコマンドのレスポンスの文字数を取得する。
 *
 * @return uint32_t
 */
uint32_t UartCommandGetResponseStringLength(void);

#endif //UART_COMMAND_H__
