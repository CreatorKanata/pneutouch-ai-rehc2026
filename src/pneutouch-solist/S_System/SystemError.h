/*****************************************************************************
 * File: SystemError.h
 * Title: システムエラーを取り扱う。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file SystemError.h
 * @brief システムエラーを取り扱う。
 */

#ifndef SYSTEM_ERROR_H__
#define SYSTEM_ERROR_H__
#include <stdbool.h>
/**
 * @brief システムエラーの列挙体
 * @note 番号が若いほど優先度が高い。
 */
typedef enum
{
	SYSTEM_ERROR_E00 = 0,	/**< 正常 */
	SYSTEM_ERROR_E01 = 1,	/**< センサーデータがオーバーフロー */
	SYSTEM_ERROR_E02 = 2,	/**< 推論バッファがオーバーフロー */
	SYSTEM_ERROR_E03 = 4,	/**< MEMS加速度センサー未接続 */
}SYSTEM_ERROR;

/**
 * @brief システムエラーの初期化。
 */
void SystemErrorInit(void);

/**
 * @brief システムエラーの追加。
 * 
 * @param error
 */
void SystemErrorInsert(SYSTEM_ERROR error);

/**
 * @brief システムエラーの削除。
 * 
 * @param error
 */
void SystemErrorRemoveAt(SYSTEM_ERROR error);

/**
 * @brief システムエラーが変化したかどうか通知。
 * 
 * @return bool
 */
bool SystemErrorNotifyErrorChanged(void);

/**
 * @brief 最も優先度の高いシステムエラーを取得する。
 * 
 * @return SYSTEM_ERROR
 */
SYSTEM_ERROR SystemErrorGetError(void);

/**
 * @brief 最も優先度の高いシステムエラーの文字列を取得。
 * 
 * @return char*
 */
char* SystemErrorGetStringError(void);

/**
 * @brief 最も優先度の高いエラーをクリア。
 */
void SystemErrorClearErrorOnce(void);

/**
 * @brief 単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int SystemErrorTest(void);

#endif //SYSTEM_ERROR_H__
