/*****************************************************************************
 * File: Shutdown.h
 * Title: Shutdown機能を使用する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file Shutdown.h
 * @brief Shutdown機能を使用する。
 */

#ifndef SHUTDOWN_H__
#define	SHUTDOWN_H__
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief シャットダウン準備処理を行う関数型
 */
typedef bool (*ShutdownIsReady)(void);

/**
 * @brief シャットダウン処理を行う関数型
 */
typedef bool (*ShutdownIsComplete)(void);

/**
 * @brief シャットダウン初期化。
 */
void ShutdownInit(void);

/**
 * @brief シャットダウンを行いたいモジュールを追加する。
 *
 * @param ready シャットダウン準備処理を行う関数
 * @param complete シャットダウン処理を行う関数
 * @return bool 追加に成功するとtrue
 */
bool ShutdownAdd(ShutdownIsReady ready, ShutdownIsComplete complete);

/**
 * @brief シャットダウンを実行する。
 */
bool ShutdownExecute(void);
#endif //SHUTDOWN_H__
