/*****************************************************************************
 * File: SoftwareInterrupt.h
 * Title: ソフトウェア割り込みを使用する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file SoftwareInterrupt.h
 * @brief ソフトウェア割り込みを使用する。
 */
 
#ifndef SOFTWARE_INTERRUPT_H__
#define	SOFTWARE_INTERRUPT_H__
#include <stdbool.h>
/**
 * @brief ソフトウェア割り込みで実行する関数型
 */
typedef void (*SoftwareInterruptPendSVExecute)(void);

/**
 * @brief ソフトウェア割り込み機能を初期化する。
 */
void SoftwareInterruptInit(void);

/**
 * @brief ソフトウェア割り込みで実行する関数のポインタがnullか確認する。
 */
bool SoftwareInterruptExeIsNull(void);

/**
 * @brief ソフトウェア割り込みを実行する。
 *
 * @note 割り込み処理実行後、割り込みクリアを行う。
 */
void SoftwareInterruptPendSVActivate(void);

/**
 * @brief ソフトウェア割り込みで実行する関数を設定する。
 *
 * @param exe ソフトウェア割り込みで使用する関数 
 */
void SoftwareInterruptSetCallback(SoftwareInterruptPendSVExecute exe);

#endif //SOFTWARE_INTERRUPT_H__
