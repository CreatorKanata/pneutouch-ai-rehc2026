/*****************************************************************************
 * File: Uart1.h
 * Title: UARTF1の制御を行う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file Uart1.h
 * @brief UARTF1の制御を行う。
 */

#ifndef UART1_H__
#define	UART1_H__

#include <stdint.h>
#include "mcu.h"
#include "rdwr_reg.h"
#include "uartf_common.h"


/**
 * @brief UARTF1初期化。
 *
 * @note 命名をUart1としているのはバッファ不使用のため。
 */
void Uart1PeripheralInit(void);

/**
 * @brief UARTで送信。
 *
 * @param data 送信バッファ
 * @param size 送信バッファのサイズ
 * @param func コールバック
 */
void Uart1Write(uint8_t *data, uint32_t size, cbfUartF_t func);

/**
 * @brief UARTで受信待ちを行う。
 *
 * @param func 1B受信時に実行される関数
 */
void Uart1StartReadByte(cbfUartF_t func);

/**
 * @brief UARTの受信待ちを停止する。
 */
void Uart1StopReadByte(void);
#endif //UART1_H__
