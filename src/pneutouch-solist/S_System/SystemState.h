/*****************************************************************************
 * File: SystemState.h
 * Title: システムの状態を管理する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file SystemState.h
 * @brief システムの状態を管理する。
 */
#ifndef SYSTEM_STATE_H__
#define SYSTEM_STATE_H__

#include <stdbool.h>

/**
 * @brief システムの状態の列挙型
 */
typedef enum
{
	SYSTEM_STATE_INIT = 0, 						/**< 初期状態 */
	SYSTEM_STATE_STOP = 1,						/**< 停止状態 */
	SYSTEM_STATE_LEARN = 2,						/**< 学習状態 */
	SYSTEM_STATE_PREDICT = 3,					/**< 推論状態 */
	SYSTEM_STATE_TIME = 4,						/**< 時刻設定状態 */
	SYSTEM_STATE_AI_LEARN_ERASE = 5,			/**< 学習した重みデータの消去制御状態*/
	SYSTEM_STATE_CHUNK_NO_CLEAR = 6,			/**< チャンク番号のリセットの制御状態 */
	SYSTEM_STATE_ERROR = 7,						/**< システムエラー制御状態 */
	SYSTEM_STATE_INSPECTION = 8,				/**< 検査状態 */
	SYSTEM_STATE_END = 9,						/**< 終了状態 */
}SYSTEM_STATE;

/**
 * @brief システムの状態移行禁止、許可の列挙型
 */
typedef enum
{
	SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED = 0,	/**< 状態移行禁止状態 */
	SYSTEM_SHIFT_STATE_SHIFT_PEMITTED,			/**< 状態移行許可状態 */
}SYSTEM_SHIFT_STATE;


/**
 * @brief システムの状態の初期化を行う。
 */
void SystemStateInit(void);

/**
 * @brief システムの状態が変化したかを確認する。
 *
 * @return bool
 */
bool SystemStateCheckStateChange(void);

/**
 * @brief システムの現在の状態を設定する。
 * 
 * @param current
 */
void SystemStateSetCurrentState(SYSTEM_STATE current);

/**
 * @brief システムの現在の状態を取得する。
 * 
 * @return SYSTEM_STATE
 */
SYSTEM_STATE SystemStateGetCurrentState(void);

/**
 * @brief システムの一つ前の状態を設定する。
 *
 * @param old
 */
void SystemStateSetOldState(SYSTEM_STATE old);

/**
 * @brief システムの一つ前の状態を取得する。
 *
 * @return SYSTEM_STATE
 */
SYSTEM_STATE SystemStateGetOldState(void);

/**
 * @brief 停止状態への移行を制御する。
 * 
 * @param state
 */
void SystemStateSetShiftToStop(SYSTEM_SHIFT_STATE state);

/**
 * @brief 停止状態への移行の制御状態を確認する。
 *
 * @return SYSTEM_SHIFT_STATE
 */
SYSTEM_SHIFT_STATE SystemStateGetShiftToStop(void);

/**
 * @brief 学習した重みデータの消去状態への移行を制御する。
 * 
 * @param state
 */
void SystemStateSetShiftToAILearnErase(SYSTEM_SHIFT_STATE state);

/**
 * @brief 学習した重みデータの消去状態への移行の制御状態を確認する。
 *
 * @return SYSTEM_SHIFT_STATE
 */
SYSTEM_SHIFT_STATE SystemStateGetShiftToAILearnErase(void);

/**
 * @brief システムエラー状態への移行を制御する。
 * 
 * @param state
 */
void SystemStateSetShiftToError(SYSTEM_SHIFT_STATE state);

/**
 * @brief システムエラー状態への移行の制御状態を確認する。
 *
 * @return SYSTEM_SHIFT_STATE
 */
SYSTEM_SHIFT_STATE SystemStateGetShiftToError(void);

/**
 * @brief 単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int SystemStateTest(void);
#endif //SYSTEM_STATE_H__
