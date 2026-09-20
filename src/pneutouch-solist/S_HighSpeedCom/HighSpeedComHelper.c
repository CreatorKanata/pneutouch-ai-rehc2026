/*****************************************************************************
 * File: HighSpeedComHelper.c
 * Title: 高速通信ヘルパー
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file HighSpeedComHelper.c
 * @brief 高速通信ヘルパー
 * @details 
 * HighSpeedComライブラリを使用する際のコールバックを処理するヘルパーモジュールです。
 */

#include "HighSpeedComHelper.h"
#include "HighSpeedCom.h"
#include "AILog.h"
#include <stdio.h>

/*############################################################################*/
/*#                                  Macro                                   #*/
/*############################################################################*/

/*############################################################################*/
/*#                               Prototype                                  #*/
/*############################################################################*/

/**
 * @brief HighSpeedComSetCallbackRequestLogDataに登録する関数
 * 
 * @param logIndex 要求されたログのインデックス値
 * @param data 取得できたログデータへのポインタ格納先
 * @param dataSize 取得できたログデータのサイズ格納先（バイト単位）
 * @return int ログデータ取得成功時は0、失敗時は-1を返す
 */
static int requestLogData(uint16_t logIndex, void** data, int* dataSize);

/*############################################################################*/
/*#                                Variable                                  #*/
/*############################################################################*/
static LOG_INFO logInfo;

/*############################################################################*/
/*#                                  API                                     #*/
/*############################################################################*/

void HighSpeedComHelperLogEnable(void)
{
	HighSpeedComSetCallbackRequestLogData(requestLogData);
}

void HighSpeedComHelperLogDisable(void)
{
	HighSpeedComSetCallbackRequestLogData(NULL);
}

/*############################################################################*/
/*#                              Subroutine                                  #*/
/*############################################################################*/

static int requestLogData(uint16_t logIndex, void** data, int* dataSize)
{
	int ret;
	ret = AILogLoad(logIndex, &logInfo);
	*data = &logInfo;
	*dataSize = sizeof(LOG_INFO);
	return ret;
}
