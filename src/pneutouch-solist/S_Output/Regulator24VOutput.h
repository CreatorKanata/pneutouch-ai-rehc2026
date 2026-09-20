/*****************************************************************************
 * File: Regulator24VOutput.h
 * Title: 24Vレギュレータを制御する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file Regulator24VOutput.h
 * @brief 24Vレギュレータを制御する。
 */
#ifndef REGULATOR_24V_OUTPUT_H__
#define	REGULATOR_24V_OUTPUT_H__
#include "Output.h"

/**
 * @brief 24Vレギュレータ制御の初期化をする。
 */
void Regulator24VOutputInit(void);

/**
 * @brief 24Vレギュレータを使用する。
 */
void Regulator24VOutputOn(void);

/**
 * @brief 24Vレギュレータの使用を中止する。
 */
void Regulator24VOutputOff(void);

/**
 * @brief 24Vレギュレータの使用状態を確認する。
 *
 * @return OUTPUT_STATUS
 */
OUTPUT_STATUS Regulator24VOutputGetOutputStatus(void);

/**
 * @brief 単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int Regulator24VOutputTest(void);
#endif //REGULATOR_24V_OUTPUT_H__
