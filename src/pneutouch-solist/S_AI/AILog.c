/*****************************************************************************
 * File: AILog.c
 * Title: 推論結果ログモジュール
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file AILog.c
 * @brief 推論結果ログモジュール
 */
#include "string.h"
#include "AILog.h"
#include "Fram.h"
#include "ConfigData.h"

/*############################################################################*/
/*#                                  Macro                                   #*/
/*############################################################################*/

// 単体テストを有効にする場合1にする
#define LOG_TEST_ENABLE	(0)

// Logデータの個数
#define LOG_COUNT_MAX	(10U)

// Logデータ1つあたりのサイズ
#define LOG_SIZE		(sizeof(LOG_INFO))

// Log管理情報FRAM先頭アドレス
#define FRAM_ADDRES_LOG_MANAGE_TOP	((uint32_t)42000U)

// LogデータのFRAM先頭アドレス
#define FRAM_ADDRES_LOG_DATA_TOP	((uint32_t)42100U)

// LogデータのIndex毎のFRAM先頭アドレス
#define FRAM_ADDRES_INDEX_TOP(x)	(FRAM_ADDRES_LOG_DATA_TOP + ((uint32_t)(x) * (uint32_t)LOG_SIZE))


/*############################################################################*/
/*#                                Variable                                  #*/
/*############################################################################*/

/*############################################################################*/
/*#                               Prototype                                  #*/
/*############################################################################*/

// 使わないが、Log管理情報領域の構造を表す物として定義している
typedef struct 
{
	uint16_t NextWriteIndex;	// 次に書き込む先のログデータインデックス値
}LOG_MANAGE;

// 不正なログファクター値の場合1を、正常な場合0を返す
static int IsInvalidLogFactor(LOG_FACTOR factor);

// 管理領域から、次に書き込むIndex値を取得する
static uint16_t readNextwriteIndex(void);
// 管理領域に、次に書き込むIndex値を書き込む
static void writeNextwriteIndex(uint16_t index);
// 指定Index値のログデータの、ログ要因を取得する
static LOG_FACTOR readLogFactor(uint16_t index);
// 指定Index値のログデータの、ログ要因を書き込む
static void writeLogFactor(uint16_t index, LOG_FACTOR factor);

/*############################################################################*/
/*#                                  API                                     #*/
/*############################################################################*/


// 推論結果ログモジュールを初期化する
// 戻り値	: 成功時0、失敗時-1
int AILogInitialize(void)
{
	LOG_FACTOR factor;

	// 次回書込み先Index値が不正な値なら0から開始する
	if(LOG_COUNT_MAX <= readNextwriteIndex())
	{
		writeNextwriteIndex(0);
	}

	// 全保存領域の先頭を確認して、1つでも不正な物があれば全消去
	// 製造後の初起動時にしか起こらないはず
	for (uint16_t i = 0; i < LOG_COUNT_MAX; i++)
	{
		factor = readLogFactor(i);
		if(IsInvalidLogFactor(factor))
		{
			AILogAllElase();
			break;
		}
	}
	return 0;
}

// 推論結果ログを保存する
// data		: 保存値
// 戻り値	: 成功時0、失敗時-1
int AILogSave(const LOG_INFO* logInfo)
{
	// ログ保存設定による保存するかの判定
	uint8_t logEnable;
	if(ConfigDataGetUint8Value(EN_CONFIG_SAVE_LOG, &logEnable) != CONFIG_DATA_RESULT_NORMAL){ return -1; }
	
	if(IsInvalidLogFactor(logInfo->Factor) || (logInfo->Factor == LOG_FACTOR_UNUSE))
	{
		// ファクター自体が保存する対象でない場合
		return -1;
	}
	else if(logEnable == 1 && logInfo->Factor == LOG_FACTOR_END)
	{
		// 学習推論終了時にログを取る
	}
	else if(logEnable == 2 
		&& ((logInfo->Factor == LOG_FACTOR_END) || (logInfo->Factor == LOG_FACTOR_WARNING_RED)))
	{
		// 学習推論終了時と赤色警告時にログを取る
	}
	else if(logEnable == 3)
	{
		// 学習推論終了時、赤色警告時、黄色警告時にログ取る。つまりいつでもログを取る
	}
	else
	{
		// ログを取らない場合、または各条件に適合しなかった時
		return -1;
	}

	uint16_t index = readNextwriteIndex();
	uint32_t adr = FRAM_ADDRES_INDEX_TOP(index);
	FramWriteBlock(adr, (const void*)logInfo, LOG_SIZE);
	writeNextwriteIndex((index + 1) % LOG_COUNT_MAX);
	return 0;
}

// 推論結果ログを取得する
// index	: 0から始まる読込先インデックス値
// logInfo	: 取得先
// 戻り値	: 成功時0、失敗時-1
int AILogLoad(uint16_t index, LOG_INFO* logInfo)
{
	uint32_t adr;
	
	if(LOG_COUNT_MAX <= index){ return -1; }
	adr = FRAM_ADDRES_INDEX_TOP(index);
	FramReadBlock(adr, (void*)logInfo, LOG_SIZE);
	return 0;
}

// 全ての推論ログを消去する
int AILogAllElase(void)
{
	for (uint16_t i = 0; i < LOG_COUNT_MAX; i++)
	{
		// ログ領域の要因部分初期化
		writeLogFactor(i, LOG_FACTOR_UNUSE);
	}
	return 0;
}

// 単体テスト
#if LOG_TEST_ENABLE == 1
LOG_INFO testLogInfo;

static void writeTestData(LOG_INFO* log, uint16_t seed)
{
	int i;
	log->Year = 2000 + seed;
	log->MonthDayHour.Month = 1 + seed;
	log->MonthDayHour.Day = 2 + seed;
	log->MonthDayHour.Hour = 3 + seed;
	log->MinuteSecond.Minute = 4 + seed;
	log->MinuteSecond.Second = 5 + seed;
	log->Factor = LOG_FACTOR_END;
	for (i = 0; i < AI_CONTEXT_INPUT_SOURCE_SIZE; i++)
	{
		log->InputData[i] = i + seed;
	}
	log->Predict.Anomaly = (bfloat16)(0x2345 + seed);
	log->Predict.ChunkNo = 1 + seed;
	log->Predict.Reserve1 = 2 + seed;
	log->Predict.Reserve2 = 3 + seed;
	log->Predict.Reserve3 = 4 + seed;
	for (i = 0; i < AI_CONTEXT_FFT_OUTPUT_SIZE; i++)
	{
		log->Predict.Fft[i] = (bfloat16)(i + 0x1000 + seed);
	}
}

static int verifyTestData(const LOG_INFO* log, uint16_t seed)
{
	int i;
	if(log->Year != 2000 + seed){ return 1; }
	if(log->MonthDayHour.Month != 1 + seed){ return 1; }
	if(log->MonthDayHour.Day != 2 + seed){ return 1; }
	if(log->MonthDayHour.Hour != 3 + seed){ return 1; }
	if(log->MinuteSecond.Minute != 4 + seed){ return 1; }
	if(log->MinuteSecond.Second != 5 + seed){ return 1; }

	
	if(log->Factor != LOG_FACTOR_END){ return 1; }
	for (i = 0; i < AI_CONTEXT_INPUT_SOURCE_SIZE; i++)
	{
		if(log->InputData[i] != i + seed){ return 1; }
	}
	if(log->Predict.Anomaly != (bfloat16)(0x2345 + seed)){ return 1; }
	if(log->Predict.ChunkNo != 1 + seed){ return 1; }
	if(log->Predict.Reserve1 != 2 + seed){ return 1; }
	if(log->Predict.Reserve2 != 3 + seed){ return 1; }
	if(log->Predict.Reserve3 != 4 + seed){ return 1; }
	for (i = 0; i < AI_CONTEXT_FFT_OUTPUT_SIZE; i++)
	{
		if(log->Predict.Fft[i] != (bfloat16)(i + 0x1000 + seed)){ return 1; }
	}
	return 0;
}


#endif
int AILogTest(void)
{
#if LOG_TEST_ENABLE == 1
	uint16_t i;

	// 全てのファクターを保存するようにしておく
	if(ConfigDataSetUint8Value(EN_CONFIG_SAVE_LOG, 3) != CONFIG_DATA_RESULT_NORMAL){ return 1; }

	//////////////////////////////////////////////////
	// 初期化時に、Log情報が不正な状態から抜け出せるか
	writeNextwriteIndex(10);
	for (i = 0; i < LOG_COUNT_MAX; i++)
	{
		writeLogFactor(i, (LOG_FACTOR)i);
	}
	AILogInitialize();

	if(readNextwriteIndex() != 0){ return 1; }	// INDEX範囲外なら0に戻ってほしい
	if(readLogFactor(0) != LOG_FACTOR_UNUSE){ return 1; }
	if(readLogFactor(1) != LOG_FACTOR_UNUSE){ return 1; }
	if(readLogFactor(2) != LOG_FACTOR_UNUSE){ return 1; }
	if(readLogFactor(3) != LOG_FACTOR_UNUSE){ return 1; }	// 不正なのでLOG_FACTOR_UNUSEに戻してほしい
	if(readLogFactor(4) != LOG_FACTOR_UNUSE){ return 1; }	// 不正なのでLOG_FACTOR_UNUSEに戻してほしい
	if(readLogFactor(5) != LOG_FACTOR_UNUSE){ return 1; }	// 不正なのでLOG_FACTOR_UNUSEに戻してほしい
	if(readLogFactor(6) != LOG_FACTOR_UNUSE){ return 1; }	// 不正なのでLOG_FACTOR_UNUSEに戻してほしい
	if(readLogFactor(7) != LOG_FACTOR_UNUSE){ return 1; }	// 不正なのでLOG_FACTOR_UNUSEに戻してほしい
	if(readLogFactor(8) != LOG_FACTOR_UNUSE){ return 1; }	// 不正なのでLOG_FACTOR_UNUSEに戻してほしい
	if(readLogFactor(9) != LOG_FACTOR_UNUSE){ return 1; }	// 不正なのでLOG_FACTOR_UNUSEに戻してほしい

	//////////////////////////////////////////////////
	// 初期化時に正常な物を壊さないか
	writeNextwriteIndex(9);
	writeLogFactor(0, (LOG_FACTOR)0);
	writeLogFactor(1, (LOG_FACTOR)1);
	writeLogFactor(2, (LOG_FACTOR)2);
	AILogInitialize();
	if(readNextwriteIndex() != 9){ return 1; }
	if(readLogFactor(0) != 0){ return 1; }
	if(readLogFactor(1) != 1){ return 1; }
	if(readLogFactor(2) != 2){ return 1; }
	if(readLogFactor(3) != LOG_FACTOR_UNUSE){ return 1; }
	if(readLogFactor(4) != LOG_FACTOR_UNUSE){ return 1; }
	if(readLogFactor(5) != LOG_FACTOR_UNUSE){ return 1; }
	if(readLogFactor(6) != LOG_FACTOR_UNUSE){ return 1; }
	if(readLogFactor(7) != LOG_FACTOR_UNUSE){ return 1; }
	if(readLogFactor(8) != LOG_FACTOR_UNUSE){ return 1; }
	if(readLogFactor(9) != LOG_FACTOR_UNUSE){ return 1; }

	//////////////////////////
	// 10個分読み書きできるか

	writeNextwriteIndex(0);
	for (i = 0; i < LOG_COUNT_MAX; i++)
	{
		writeTestData(&testLogInfo, i);
		AILogSave(&testLogInfo);
		AILogLoad(i, &testLogInfo);
		if(verifyTestData(&testLogInfo, i) == 1){ return 1; }
		AILogLoad((i + 1) % 10, &testLogInfo);
		if(verifyTestData(&testLogInfo, i) != 1){ return 1; }	// 一致したら要らないところまで書いている
	}

	/////////////////////////////////////
	// 10個を超えたら古い所から消えるか
	for (i = 0; i < LOG_COUNT_MAX; i++)
	{
		writeTestData(&testLogInfo, i + 10);
		AILogSave(&testLogInfo);
		AILogLoad(i, &testLogInfo);
		if(verifyTestData(&testLogInfo, i + 10) == 1){ return 1; }
		AILogLoad((i + 1) % 10, &testLogInfo);
		if(verifyTestData(&testLogInfo, i) != 1){ return 1; }	// 一致したら要らないところまで書いている
	}

	/////////////////
	// 全消去できるか
	AILogAllElase();
	for (i = 0; i < LOG_COUNT_MAX; i++)
	{
		if(readLogFactor(i) != LOG_FACTOR_UNUSE){ return 1; }
	}

	////////////////////////
	// ログ保存設定による動作

	// 学習・推論終了時のみ
	if(ConfigDataSetUint8Value(EN_CONFIG_SAVE_LOG, 1) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_END;
	if(AILogSave(&testLogInfo) != 0){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_WARNING_RED;
	if(AILogSave(&testLogInfo) == 0){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_WARNING_YELLOW;
	if(AILogSave(&testLogInfo) == 0){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_UNUSE;
	if(AILogSave(&testLogInfo) == 0){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_SENTINEL;
	if(AILogSave(&testLogInfo) == 0){ return 1; }

	// 赤色警告検出もログを取る
	if(ConfigDataSetUint8Value(EN_CONFIG_SAVE_LOG, 2) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_END;
	if(AILogSave(&testLogInfo) != 0){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_WARNING_RED;
	if(AILogSave(&testLogInfo) != 0){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_WARNING_YELLOW;
	if(AILogSave(&testLogInfo) == 0){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_UNUSE;
	if(AILogSave(&testLogInfo) == 0){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_SENTINEL;
	if(AILogSave(&testLogInfo) == 0){ return 1; }

	// 黄色警告検出もログを取る
	if(ConfigDataSetUint8Value(EN_CONFIG_SAVE_LOG, 3) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_END;
	if(AILogSave(&testLogInfo) != 0){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_WARNING_RED;
	if(AILogSave(&testLogInfo) != 0){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_WARNING_YELLOW;
	if(AILogSave(&testLogInfo) != 0){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_UNUSE;
	if(AILogSave(&testLogInfo) == 0){ return 1; }
	testLogInfo.Factor = LOG_FACTOR_SENTINEL;
	if(AILogSave(&testLogInfo) == 0){ return 1; }

	// ホストテスト用のデータを記録しておわる
	AILogAllElase();

	writeTestData(&testLogInfo, 0);
	testLogInfo.Factor = LOG_FACTOR_END;
	AILogSave(&testLogInfo);

	writeTestData(&testLogInfo, 1);
	testLogInfo.Factor = LOG_FACTOR_WARNING_RED;
	AILogSave(&testLogInfo);

	writeTestData(&testLogInfo, 2);
	testLogInfo.Factor = LOG_FACTOR_WARNING_YELLOW;
	AILogSave(&testLogInfo);

#endif
	return 0;
}

void AILogResetNextWriteIndex(void)
{
	writeNextwriteIndex(0);
}
/*############################################################################*/
/*#                              Subroutine                                  #*/
/*############################################################################*/

static int IsInvalidLogFactor(LOG_FACTOR factor)
{
	switch (factor)
	{
	case LOG_FACTOR_UNUSE:
	case LOG_FACTOR_END:
	case LOG_FACTOR_WARNING_RED:
	case LOG_FACTOR_WARNING_YELLOW:
		return 0;
	case LOG_FACTOR_SENTINEL:
	default:
		return 1;
	}
}
static uint16_t readNextwriteIndex(void)
{
	uint16_t ret;
	FramReadHalfWord(FRAM_ADDRES_LOG_MANAGE_TOP, &ret);
	return ret;
}

static void writeNextwriteIndex(uint16_t index)
{
	FramWriteHalfWord(FRAM_ADDRES_LOG_MANAGE_TOP, index);
}

static LOG_FACTOR readLogFactor(uint16_t index)
{
	uint16_t ret;
	FramReadHalfWord(FRAM_ADDRES_INDEX_TOP(index), &ret);
	return (LOG_FACTOR)ret;
}

static void writeLogFactor(uint16_t index, LOG_FACTOR factor)
{
	FramWriteHalfWord(FRAM_ADDRES_INDEX_TOP(index), (uint16_t)factor);
}
