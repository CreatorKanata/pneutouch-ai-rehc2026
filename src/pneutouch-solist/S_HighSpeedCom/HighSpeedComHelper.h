/*****************************************************************************
 * File: HighSpeedComHelper.h
 * Title: 高速通信ヘルパー
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file HighSpeedComHelper.h
 * @brief 高速通信ヘルパー
 * @details 
 * HighSpeedComライブラリを使用する際のコールバックを処理するヘルパーモジュールです。
 */

#ifndef HIGH_SPEED_COM_HELPER_H__
#define HIGH_SPEED_COM_HELPER_H__

/**
 * @brief 高速通信にてログ取得を有効化する。
 */
void HighSpeedComHelperLogEnable(void);

/**
 * @brief 高速通信にてログ取得を無効化する。
 */
void HighSpeedComHelperLogDisable(void);
#endif // HIGH_SPEED_COM_HELPER_H__
