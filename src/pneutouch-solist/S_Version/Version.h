/*****************************************************************************
 * File: Version.h
 * Title: バージョン情報を取得する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file Version.h
 * @brief バージョン情報を取得する。
 */

#ifndef VERSION_H__
#define VERSION_H__

/**
 * @brief バージョン情報を文字列で取得する。
 * 
 * @return const char* 稼働バージョンの文字列
 */
const char* VersionGetViewName(void);


/**
 * @brief 単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int VersionTest(void);
#endif //VERSION_H__

