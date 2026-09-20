/*****************************************************************************
 * File: RelayOutput.h
 * Title: リレー出力を使う。
 * LastUpdated: 2025.06.13
******************************************************************************/

/**
 * @file RelayOutput.h
 * @brief リレー出力を使う。\n
 *		  回路図のISOOUT0をリレー0とする。\n
 *		  回路図のISOOUT1をリレー1とする。
 */
#ifndef RELAY_OUTPUT_H__
#define RELAY_OUTPUT_H__

/**
 * @brief リレー制御の初期化をする。
 */
void RelayOutputInit(void);

/**
 * @brief リレー0に出力する。
 */
void RelayOutputRelay0On(void);

/**
 * @brief リレー0への出力を中止する。
 */
void RelayOutputRelay0Off(void);

/**
 * @brief リレー1に出力する。
 */
void RelayOutputRelay1On(void);

/**
 * @brief リレー1への出力を中止する。
 */
void RelayOutputRelay1Off(void);

/**
 * @brief 単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int RelayOutputTest(void);
#endif //RELAY_OUTPUT_H__
