/*****************************************************************************
 * File: UartReceiveCommand.h
 * Title: UARTで受信したコマンドをパースする。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file UartReceiveCommand.h
 * @brief UARTで受信したコマンドをパースする。
 */
#ifndef UART_RECEIVE_COMMAND_H__
#define UART_RECEIVE_COMMAND_H__

#include <stdint.h>

/**
 * @brief UARTコマンドパース機能を初期化する。
 */
void UartReceiveCommandInit(void);

/**
 * @brief UARTで受信したデータを管理する。
 */
void UartReceiveCommandReceiveBlock(void);

/**
 * @brief コマンドのパースを行う。
 */
void UartReceiveCommandReceiveAnalizer(void);

/**
 * @brief コマンド受信のための割込みの設定を行う。Uart1の受信のコールバックに設定する。
 *
 * @param data 受信したデータ
 * @param errStatus 不使用
 */
void UartReceiveCommandUart1InterruptReadCallback( uint32_t data, uint16_t errStatus );

/**
 * @brief テスト
 * 
 * @note configデータを読み込んで送信するコマンドのテスト。設定読み込み後に読み出し推奨。
 * @return int 成功時0。失敗時は0以外。
 */
int UartReceiveCommandTest(void);
#endif //UART_RECEIVE_COMMAND_H__
