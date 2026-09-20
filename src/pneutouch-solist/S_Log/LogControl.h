/*****************************************************************************
 * File: LogControl.h
 * Title: ログ機能を取り扱う。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file LogControl.h
 * @brief ログ機能を取り扱う。
 */

#ifndef LOG_CONTROL_H__
#define LOG_CONTROL_H__
#include <stdbool.h>
#include "AIContext.h"
#include "SystemState.h"
/**
 * @brief ログモジュールの初期化、再初期化処理のための関数型
 */
typedef void (*LogControlFuncReset)(void);

/**
 * @brief ログモジュールの終了処理のための関数型
 */
typedef void (*LogControlFuncFin)(void);

/**
 * @brief 学習・推論の終了後にログを保存するための関数型
 */
typedef bool (*LogControlFuncSaveEndLog)(void);

/**
 * @brief 推論ログの保存が要求されているか確認するための関数型
 */
typedef bool (*LogControlFuncIsPredictLogRequiredSave)(void);

/**
 * @brief 推論ログを保存するための関数型
 */
typedef bool (*LogControlFuncSavePredictLog)(void);

/**
 * @brief ログ機能を取り扱うための関数型の構造体
 */
typedef struct
{
	LogControlFuncReset Reset;											/**< モジュールの初期化、再初期化。 */
	LogControlFuncFin Fin;												/**< モジュールの終了。 */
	LogControlFuncSaveEndLog SaveEndLog;								/**< 学習・推論の終了後にログを保存する。 */
	LogControlFuncIsPredictLogRequiredSave IsPredictLogRequiredSave;	/**< 推論ログの保存が要求されているか。 */
	LogControlFuncSavePredictLog SavePredictLog;						/**< 推論ログを保存する。 */
}LOG_FUNC;

/**
 * @brief ログ機能を初期化する関数型
 */
typedef void (*LogControlFuncInit)(LOG_FUNC* logFunc);

/**
 * @brief 設定に応じて使用するログ機能の初期化を行う。
 */
void LogControlInit(LOG_FUNC* instance);

/**
 * @brief ログ機能を取得する。
 *
 * @return 現在設定されているログ機能の構造体
 */
LOG_FUNC* LogControlCheckFunc(void);

/**
 * @brief 使用するログモジュール初期化、再初期化。
 */
void LogControlReset(void);

/**
 * @brief 使用するログモジュールの終了。
 */
void LogControlFin(void);

/**
 * @brief 学習・推論の終了後にログを保存する。
 */
void LogControlSaveEndLog(void);

/**
 * @brief 推論ログの保存が要求されているか確認する。
 *
 * @return bool
 */
bool LogControlIsPredictLogRequiredSave(void);

/**
 * @brief 推論ログを保存する。
 */
void LogControlSavePredictLog(void);

/**
 * @brief ログ情報(ログ要因、時刻)の準備とログを保存する。
 *
 * @param factor ログ要因
 * @param save データをログ保存するのに使用したか
 */
void LogControlCommonSaveLog(LOG_FACTOR factor,LOG_SAVE save);


/**
 * @brief インターフェースの単体テスト
 *
 * @return int 成功時0。失敗時は0以外。
 */
 int LogControlTest(void);
/**
 * @brief ログ情報保存の単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int LogControlLogTest(void);
#endif //LOG_CONTROL_H__
