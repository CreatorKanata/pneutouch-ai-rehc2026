/*****************************************************************************
 * File: ConfigData.c
 * Title: 設定データ
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file ConfigData.c
 * @brief 設定データ
 */

#include "ConfigData.h"
#include "Fram.h"
#include "float.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include "BfloatUtility.h"

/*############################################################################*/
/*#                                  Macro                                   #*/
/*############################################################################*/

#define CHECK_CODE_ADDRESS		(96)			// CONFIG_GROUP_ADDRESS_GROUP0のデータが初期化済みかをチェックするためのコードがあるアドレス
#define CHECK_CODE_SUCCESS_CODE	(0xAAAA5555)	// 初期化済みの時の値

typedef enum
{
	CONFIG_GROUP_ADDRESS_GROUP0 = 0,
}CONFIG_GROUP_ADDRESS;

// GROOUP0に含まれるデータの総バイト数
// Groupが増えない限り、(ATTRIBUTE_TABLE[EN_CONFIG_END - 1].Address + ATTRIBUTE_TABLE[EN_CONFIG_END - 1].Size) と同義。
#define GROUP0_SIZE	(47)

typedef struct 
{
	EN_CONFIG 		Id;
	uint32_t		Address;		// FRAM上のアドレス
	EN_CONFIG_TYPE	DataType;
	uint8_t			Size;
	uint8_t			UI8Min;
	uint8_t			UI8Max;
	uint8_t			UI8Default;
	uint16_t		UI16Min;
	uint16_t 		UI16Max;
	uint16_t		UI16Default;
	int16_t			I16Min;
	int16_t			I16Max;
	int16_t			I16Default;
	float			FMin;
	float			FMax;
	float			FDefault;
}CONFIG_ATTRIBUTE;

static const CONFIG_ATTRIBUTE ATTRIBUTE_TABLE[EN_CONFIG_END + 1] =
{
	{ 0,	CONFIG_GROUP_ADDRESS_GROUP0 + 0,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	1U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_USE_SENSOR

	{ 1,	CONFIG_GROUP_ADDRESS_GROUP0 + 1,	EN_CONFIG_TYPE_UINT8_T,		1,	7U,	15U,13U,	0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_IEPE_SAMPLING_FREQUENCY
	{ 2,	CONFIG_GROUP_ADDRESS_GROUP0 + 2,	EN_CONFIG_TYPE_UINT16_T,	2,	0U,	0U,	0U,		1U, 512U, 512U,		0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_IEPE_SAMPLING_NUM
	{ 3,	CONFIG_GROUP_ADDRESS_GROUP0 + 4,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	15U,8U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_IEPE_GAIN

	{ 4,	CONFIG_GROUP_ADDRESS_GROUP0 + 5,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	2U,	2U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_MEMS_DATA_KIND
	{ 5,	CONFIG_GROUP_ADDRESS_GROUP0 + 6,	EN_CONFIG_TYPE_UINT8_T,		1,	7U,	15U,13U,	0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_MEMS_SAMPLING_FREQUENCY
	{ 6,	CONFIG_GROUP_ADDRESS_GROUP0 + 7,	EN_CONFIG_TYPE_UINT16_T,	2,	0U,	0U,	0U,		1U, 512U, 512U,		0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_MEMS_SAMPLING_NUM
	{ 7,	CONFIG_GROUP_ADDRESS_GROUP0 + 9,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	15U,12U,	0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_MEMS_GAIN
	{ 8,	CONFIG_GROUP_ADDRESS_GROUP0 + 10,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	1U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_MEMS_LPF

	{ 9,	CONFIG_GROUP_ADDRESS_GROUP0 + 11,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_OVERLAP

	{ 10,	CONFIG_GROUP_ADDRESS_GROUP0 + 12,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	1U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_FFT_ENABLE
	{ 11,	CONFIG_GROUP_ADDRESS_GROUP0 + 13,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	255U,	0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_FFT_SKIP
	{ 12,	CONFIG_GROUP_ADDRESS_GROUP0 + 14,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	1U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_WINDOW
	{ 13,	CONFIG_GROUP_ADDRESS_GROUP0 + 15,	EN_CONFIG_TYPE_UINT16_T,	2,	0U,	0U,	0U, 	1U,	64U, 64U,		0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_HIDDEN_LAYER_NUM
	{ 14,	CONFIG_GROUP_ADDRESS_GROUP0 + 17,	EN_CONFIG_TYPE_BFLOAT,		2,	0U,	0U,	0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 1.0f, 1.0f },				// EN_CONFIG_FORGET_RATE
	{ 15,	CONFIG_GROUP_ADDRESS_GROUP0 + 19,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	2U,	1U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_ACTIVATION_FUNC
	{ 16,	CONFIG_GROUP_ADDRESS_GROUP0 + 20,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	1U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_LOSS_FUNC
	{ 17,	CONFIG_GROUP_ADDRESS_GROUP0 + 21,	EN_CONFIG_TYPE_UINT16_T,	2,	0U,	0U,	0U,		1U, 32767U, 1U,		0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_WEIGHT_A_RANDOM_SEED

	{ 18,	CONFIG_GROUP_ADDRESS_GROUP0 + 23,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_AI_MODE
	{ 19,	CONFIG_GROUP_ADDRESS_GROUP0 + 24,	EN_CONFIG_TYPE_UINT16_T,	2,	0U,	0U,	0U,		1U, 65535U, 40U,	0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_AI_LEARN_NUM
	{ 20,	CONFIG_GROUP_ADDRESS_GROUP0 + 26,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	2U,	2U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_AI_CALC_THRESHOLD
	{ 21,	CONFIG_GROUP_ADDRESS_GROUP0 + 27,	EN_CONFIG_TYPE_BFLOAT,		2,	0U,	0U,	0U,		0U, 0U, 0U,			0, 0, 0,	-FLT_MAX, FLT_MAX, 0.0f },		// EN_CONFIG_AI_FAR_THRESHOLD
	{ 22,	CONFIG_GROUP_ADDRESS_GROUP0 + 29,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U, 0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_AVELAGE_ENABLE
	{ 23,	CONFIG_GROUP_ADDRESS_GROUP0 + 30,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	2U,	0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_THINNING
	{ 24,	CONFIG_GROUP_ADDRESS_GROUP0 + 31,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_PREDICT_LAST_ONCE
	{ 25,	CONFIG_GROUP_ADDRESS_GROUP0 + 32,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	255U,0U,	0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_ABNORMAL_AVERAGE
	{ 26,	CONFIG_GROUP_ADDRESS_GROUP0 + 33,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	1U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_ABNORMAL_FORMAT
	{ 27,	CONFIG_GROUP_ADDRESS_GROUP0 + 34,	EN_CONFIG_TYPE_BFLOAT,		2,	0U,	0U,	0U,		0U, 0U, 0U,			0, 0, 0,	-FLT_MAX, FLT_MAX, 0.0f },		// EN_CONFIG_YELLOW_THRESHOLD
	{ 28,	CONFIG_GROUP_ADDRESS_GROUP0 + 36,	EN_CONFIG_TYPE_BFLOAT,		2,	0U,	0U,	0U,		0U, 0U, 0U,			0, 0, 0,	-FLT_MAX, FLT_MAX, 0.0f },		// EN_CONFIG_RED_THRESHOLD
	{ 29,	CONFIG_GROUP_ADDRESS_GROUP0 + 38,	EN_CONFIG_TYPE_BFLOAT,		2,	0U,	0U,	0U,		0U, 0U, 0U,			0, 0, 0,	-FLT_MAX, FLT_MAX, 1.0f },		// EN_CONFIG_MAX_ABNORMAL
	//{ 27,	CONFIG_GROUP_ADDRESS_GROUP0 + 34,	EN_CONFIG_TYPE_BFLOAT,		2,	0U,	0U,	0U,		0U, 0U, 0U,			0, 0, 0,	-FLT_MAX, FLT_MAX, 0.3f },	// EN_CONFIG_AI_YELLOW_THRESHOLD
	//{ 28,	CONFIG_GROUP_ADDRESS_GROUP0 + 36,	EN_CONFIG_TYPE_BFLOAT,		2,	0U,	0U,	0U,		0U, 0U, 0U,			0, 0, 0,	-FLT_MAX, FLT_MAX, 0.7f },	// EN_CONFIG_AI_RED_THRESHOLD
	//{ 29,	CONFIG_GROUP_ADDRESS_GROUP0 + 38,	EN_CONFIG_TYPE_BFLOAT,		2,	0U,	0U,	0U,		0U, 0U, 0U,			0, 0, 0,	-FLT_MAX, FLT_MAX, 1.0f },		// EN_CONFIG_MAX_ABNORMAL
	//{ 27,	CONFIG_GROUP_ADDRESS_GROUP0 + 34,	EN_CONFIG_TYPE_BFLOAT,		2,	0U,	0U,	0U,		0U, 0U, 0U,			0, 0, 0,	-FLT_MAX, FLT_MAX, 0.002f },	// EN_CONFIG_AI_YELLOW_THRESHOLD
	//{ 28,	CONFIG_GROUP_ADDRESS_GROUP0 + 36,	EN_CONFIG_TYPE_BFLOAT,		2,	0U,	0U,	0U,		0U, 0U, 0U,			0, 0, 0,	-FLT_MAX, FLT_MAX, 0.008f },	// EN_CONFIG_AI_RED_THRESHOLD
	//{ 29,	CONFIG_GROUP_ADDRESS_GROUP0 + 38,	EN_CONFIG_TYPE_BFLOAT,		2,	0U,	0U,	0U,		0U, 0U, 0U,			0, 0, 0,	-FLT_MAX, FLT_MAX, 0.012f },		// EN_CONFIG_MAX_ABNORMAL
	{ 30,	CONFIG_GROUP_ADDRESS_GROUP0 + 40,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_WARNING_LATCH
	{ 31,	CONFIG_GROUP_ADDRESS_GROUP0 + 41,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_TOGGLE
	{ 32,	CONFIG_GROUP_ADDRESS_GROUP0 + 42,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_REALTIME_COM
	{ 33,	CONFIG_GROUP_ADDRESS_GROUP0 + 43,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	1U,	0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_BLOCK_COM
	{ 34,	CONFIG_GROUP_ADDRESS_GROUP0 + 44,	EN_CONFIG_TYPE_UINT8_T,		1,	0U,	3U,	0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_SAVE_LOG

	{ 35,	CONFIG_GROUP_ADDRESS_GROUP0 + 0,	EN_CONFIG_TYPE_UNKNOWN,		0,	0U,	0U,	0U,		0U, 0U, 0U,			0, 0, 0,	0.0f, 0.0f, 0.0f },				// EN_CONFIG_END
};

/*############################################################################*/
/*#                                Variable                                  #*/
/*############################################################################*/

static uint8_t group0Buffer[GROUP0_SIZE];	// GROUP0の実体

/*############################################################################*/
/*#                               Prototype                                  #*/
/*############################################################################*/

// 値が範囲内かチェックする
// id		: 値をチェックする設定値のID
// data		: チェック対象の値へのポインタ
// 戻り値	: 範囲内の場合1、範囲外の場合0
static int isValidData(EN_CONFIG id, void* data);

// 指定されたidが有効なid値か
// id		: 調べるid
// 戻り値	: 有効な場合1、そうでない場合0
static int isValidId(EN_CONFIG id);

// 値を設定する共通関数
static CONFIG_DATA_RESULT dataSetCommon(EN_CONFIG id, void* data);

// 値を取得する共通関数
static CONFIG_DATA_RESULT dataGetCommon(EN_CONFIG id, void* data);

// 標準的なstrnlenを自力実装
static int strnlen(const void* src, int num);

/*############################################################################*/
/*#                                  API                                     #*/
/*############################################################################*/

EN_CONFIG_TYPE ConfigDataGetConfigType(EN_CONFIG id)
{
	if(isValidId(id) == 0){ return 	EN_CONFIG_TYPE_UNKNOWN; }
	return ATTRIBUTE_TABLE[id].DataType;
}

// Group0に含まれる設定値を、永続化ストレージからロードする
// 戻り値	: 成功時0、失敗時-1
int ConfigDataLoadConfigGroup0(void)
{
	uint32_t cc;
	FramReadWord(CHECK_CODE_ADDRESS, &cc);
	if(cc != CHECK_CODE_SUCCESS_CODE)
	{
		ConfigDataClear();
		ConfigDataSaveConfigGroup0();
		FramWriteWord(CHECK_CODE_ADDRESS, CHECK_CODE_SUCCESS_CODE);
	}
	else
	{
		FramReadBlock(ATTRIBUTE_TABLE[0].Address, group0Buffer, GROUP0_SIZE);
	}
	return 0;
}

// Group0に含まれる設定値を、永続化ストレージにセーブする
// 戻り値	: 成功時0、失敗時-1
int ConfigDataSaveConfigGroup0(void)
{
	FramWriteBlock(ATTRIBUTE_TABLE[0].Address, group0Buffer, GROUP0_SIZE);
	return 0;
}

// ConfigDataを初期化し、デフォルト値に戻す
void ConfigDataClear(void)
{
	EN_CONFIG_TYPE type;

	for (int id = 0; id < EN_CONFIG_END; id++)
	{
		type = ConfigDataGetConfigType((EN_CONFIG)id);
		switch (type)
		{
		case EN_CONFIG_TYPE_UINT8_T:
			ConfigDataSetUint8Value((EN_CONFIG)id, ATTRIBUTE_TABLE[id].UI8Default);
			break;

		case EN_CONFIG_TYPE_UINT16_T:
			ConfigDataSetUint16Value((EN_CONFIG)id, ATTRIBUTE_TABLE[id].UI16Default);
			break;

		case EN_CONFIG_TYPE_INT16_T:
			ConfigDataSetInt16Value((EN_CONFIG)id, ATTRIBUTE_TABLE[id].I16Default);
			break;

		case EN_CONFIG_TYPE_BFLOAT:
			ConfigDataSetBfloat16Value((EN_CONFIG)id, ATTRIBUTE_TABLE[id].FDefault);
			break;
		case EN_CONFIG_TYPE_UNKNOWN:
			break;
		}
	}	
}

// 設定値の設定
CONFIG_DATA_RESULT ConfigDataSetUint8Value(EN_CONFIG id, uint8_t data)
{
	if(isValidId(id) == 0){ return CONFIG_DATA_RESULT_ID_OUT_OF_RANGE; }
	else if(isValidData(id, &data) == 0){ return CONFIG_DATA_RESULT_VALUE_OUT_OF_RANGE; }
	group0Buffer[ATTRIBUTE_TABLE[id].Address] = data;
	return CONFIG_DATA_RESULT_NORMAL;
}

CONFIG_DATA_RESULT ConfigDataSetUint16Value(EN_CONFIG id, uint16_t data)
{
	return dataSetCommon(id, &data);
}

CONFIG_DATA_RESULT ConfigDataSetInt16Value(EN_CONFIG id, int16_t data)
{
	return dataSetCommon(id, &data);
}

CONFIG_DATA_RESULT ConfigDataSetBfloat16Value(EN_CONFIG id, float data)
{
	bfloat16 bf16 = BfloatUtilityFloatToBfloat16(data);
	return dataSetCommon(id, &bf16);
}

// 設定値の取得
CONFIG_DATA_RESULT ConfigDataGetUint8Value(EN_CONFIG id, uint8_t* data)
{
	if(isValidId(id) == 0){ return CONFIG_DATA_RESULT_ID_OUT_OF_RANGE; }
	*data = group0Buffer[ATTRIBUTE_TABLE[id].Address];
	return CONFIG_DATA_RESULT_NORMAL;
}

CONFIG_DATA_RESULT ConfigDataGetUint16Value(EN_CONFIG id, uint16_t* data)
{
	return dataGetCommon(id, data);
}

CONFIG_DATA_RESULT ConfigDataGetInt16Value(EN_CONFIG id, int16_t* data)
{
	return dataGetCommon(id, data);
}

CONFIG_DATA_RESULT ConfigDataGetBfloat16Value(EN_CONFIG id, bfloat16* data)
{
	return dataGetCommon(id, data);
}
CONFIG_DATA_RESULT ConfigDataGetFloatValue(EN_CONFIG id, float* data)
{
	bfloat16 bf16;
	CONFIG_DATA_RESULT res = dataGetCommon(id, &bf16);
	*data = BfloatUtilityBfloat16ToFloat(bf16);
	return res;
}


// 設定値を10進数文字列で設定する
CONFIG_DATA_RESULT ConfigDataSetUint8ValueString(EN_CONFIG id, const char* str)
{
	unsigned long data;
	char *e;
	int strLength;

	strLength = strnlen(str, CONFIG_DATA_MAX_STRING_LENGTH + 1);
	if(CONFIG_DATA_MAX_STRING_LENGTH < strLength || strLength == 0){ return CONFIG_DATA_RESULT_INVALID_STRING; }
	errno = 0;
	data = strtoul(str, &e, 10);
	if(*e != '\0'){ return CONFIG_DATA_RESULT_INVALID_STRING; }
	else if(errno != 0){ return CONFIG_DATA_RESULT_INVALID_STRING; }
	else if(UINT8_MAX < data){ return CONFIG_DATA_RESULT_INVALID_STRING; }

	return ConfigDataSetUint8Value(id, (uint8_t)data);
}

CONFIG_DATA_RESULT ConfigDataSetUint16ValueString(EN_CONFIG id, const char* str)
{
	unsigned long data;
	char *e;
	int strLength;

	strLength = strnlen(str, CONFIG_DATA_MAX_STRING_LENGTH + 1);
	if(CONFIG_DATA_MAX_STRING_LENGTH < strLength || strLength == 0){ return CONFIG_DATA_RESULT_INVALID_STRING; }
	errno = 0;
	data = strtoul(str, &e, 10);
	if(*e != '\0'){ return CONFIG_DATA_RESULT_INVALID_STRING; }
	else if(errno != 0){ return CONFIG_DATA_RESULT_INVALID_STRING; }
	else if(UINT16_MAX < data){ return CONFIG_DATA_RESULT_INVALID_STRING; }

	return ConfigDataSetUint16Value(id, (uint16_t)data);
}

CONFIG_DATA_RESULT ConfigDataSetInt16ValueString(EN_CONFIG id, const char* str)
{
	long data;
	char *e;
	int strLength;

	strLength = strnlen(str, CONFIG_DATA_MAX_STRING_LENGTH + 1);
	if(CONFIG_DATA_MAX_STRING_LENGTH < strLength || strLength == 0){ return CONFIG_DATA_RESULT_INVALID_STRING; }
	errno = 0;
	data = strtol(str, &e, 10);
	if(*e != '\0'){ return CONFIG_DATA_RESULT_INVALID_STRING; }
	else if(errno != 0){ return CONFIG_DATA_RESULT_INVALID_STRING; }
	else if(data < INT16_MIN || INT16_MAX < data){ return CONFIG_DATA_RESULT_INVALID_STRING; }

	return ConfigDataSetInt16Value(id, (int16_t)data);
}

// 設定値を小数点または指数表記文字列として設定する
CONFIG_DATA_RESULT ConfigDataSetBfloat16ValueString(EN_CONFIG id, const char* str)
{
	float data;
	char *e;
	int strLength;

	strLength = strnlen(str, CONFIG_DATA_MAX_STRING_LENGTH + 1);
	if(CONFIG_DATA_MAX_STRING_LENGTH < strLength || strLength == 0){ return CONFIG_DATA_RESULT_INVALID_STRING; }
	data = strtof(str, &e);
	if(*e != '\0'){ return CONFIG_DATA_RESULT_INVALID_STRING; }
	else if(isfinite(data) == 0){ return CONFIG_DATA_RESULT_INVALID_STRING; }
	return ConfigDataSetBfloat16Value(id, data);
}


// 設定値を10進数文字列として取得する
CONFIG_DATA_RESULT ConfigDataGetUint8ValueString(EN_CONFIG id, char* str, int strSize)
{
	uint8_t data;
	CONFIG_DATA_RESULT res;

	if((res = ConfigDataGetUint8Value(id, &data)) != CONFIG_DATA_RESULT_NORMAL){ return res; }
	else if(strSize < CONFIG_DATA_MAX_STRING_LENGTH + 1){ return CONFIG_DATA_RESULT_SMALL_STRING_BUFFER_SIZE;}

	sprintf(str, "%u", data);
	return CONFIG_DATA_RESULT_NORMAL;
}

// 設定値を10進数文字列として取得する
CONFIG_DATA_RESULT ConfigDataGetUint16ValueString(EN_CONFIG id, char* str, int strSize)
{
	uint16_t data;
	CONFIG_DATA_RESULT res;

	if((res = ConfigDataGetUint16Value(id, &data)) != CONFIG_DATA_RESULT_NORMAL){ return res; }
	else if(strSize < CONFIG_DATA_MAX_STRING_LENGTH + 1){ return CONFIG_DATA_RESULT_SMALL_STRING_BUFFER_SIZE;}

	sprintf(str, "%u", data);
	return CONFIG_DATA_RESULT_NORMAL;
}

// 設定値を10進数文字列として取得する
CONFIG_DATA_RESULT ConfigDataGetInt16ValueString(EN_CONFIG id, char* str, int strSize)
{
	int16_t data;
	CONFIG_DATA_RESULT res;

	if((res = ConfigDataGetInt16Value(id, &data)) != CONFIG_DATA_RESULT_NORMAL){ return res; }
	else if(strSize < CONFIG_DATA_MAX_STRING_LENGTH + 1){ return CONFIG_DATA_RESULT_SMALL_STRING_BUFFER_SIZE;}

	sprintf(str, "%d", data);
	return CONFIG_DATA_RESULT_NORMAL;
}

// 設定値を小数点または指数表記文字列として取得する
CONFIG_DATA_RESULT ConfigDataGetBfloat16ValueString(EN_CONFIG id, char* str, int strSize)
{
	float data;
	CONFIG_DATA_RESULT res;

	if((res = ConfigDataGetFloatValue(id, &data)) != CONFIG_DATA_RESULT_NORMAL){ return res; }
	else if(strSize < CONFIG_DATA_MAX_STRING_LENGTH + 1){ return CONFIG_DATA_RESULT_SMALL_STRING_BUFFER_SIZE;}
	
	sprintf(str, "%g", data);
	return CONFIG_DATA_RESULT_NORMAL;
}


// 単体テスト
int ConfigDataTest(void)
{
	uint8_t ui8;
	uint16_t ui16;
	float f;
	bfloat16 bf;
	char stringBuf[CONFIG_DATA_MAX_STRING_LENGTH + 1];

	// Int16系メソッドは使われないのでテストを省く

	// チェックコードによるリセットを避けるため、一度ロードしておく
	ConfigDataLoadConfigGroup0();

	///////////////////////////
	// 値系GetSetテスト

	// ConfigDataGetConfigType
	if(ConfigDataGetConfigType(EN_CONFIG_HIDDEN_LAYER_NUM) != EN_CONFIG_TYPE_UINT16_T){ return 1; }
	if(ConfigDataGetConfigType(EN_CONFIG_FORGET_RATE) != EN_CONFIG_TYPE_BFLOAT){ return 1; }
	if(ConfigDataGetConfigType(EN_CONFIG_BLOCK_COM) != EN_CONFIG_TYPE_UINT8_T){ return 1; }
	if(ConfigDataGetConfigType(EN_CONFIG_END) != EN_CONFIG_TYPE_UNKNOWN){ return 1; }
	if(ConfigDataGetConfigType((EN_CONFIG)(EN_CONFIG_END + 1)) != EN_CONFIG_TYPE_UNKNOWN){ return 1; }

	// ConfigDataSetUint8Value, ConfigDataGetUint8Value 正常/境界値
	if(ConfigDataSetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, 7) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, &ui8) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui8 != 7){ return 1; }
	if(ConfigDataSetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, 10) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, &ui8) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui8 != 10){ return 1; }
	if(ConfigDataSetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, 15) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, &ui8) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui8 != 15){ return 1; }
	if(ConfigDataSetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, 6) != CONFIG_DATA_RESULT_VALUE_OUT_OF_RANGE){ return 1; }
	if(ConfigDataGetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, &ui8) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui8 != 15){ return 1; }
	if(ConfigDataSetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, 16) != CONFIG_DATA_RESULT_VALUE_OUT_OF_RANGE){ return 1; }
	if(ConfigDataGetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, &ui8) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui8 != 15){ return 1; }

	// ConfigDataSetUint16Value, ConfigDataGetUint16Value 正常/境界値
	if(ConfigDataSetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, 1) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, &ui16) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui16 != 1){ return 1; }
	if(ConfigDataSetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, 256) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, &ui16) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui16 != 256){ return 1; }
	if(ConfigDataSetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, 123) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, &ui16) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui16 != 123){ return 1; }
	if(ConfigDataSetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, 0) != CONFIG_DATA_RESULT_VALUE_OUT_OF_RANGE){ return 1; }
	if(ConfigDataGetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, &ui16) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui16 != 123){ return 1; }
	if(ConfigDataSetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, 257) != CONFIG_DATA_RESULT_VALUE_OUT_OF_RANGE){ return 1; }
	if(ConfigDataGetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, &ui16) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui16 != 123){ return 1; }

	// ConfigDataSetBfloat16Value, ConfigDataGetBfloat16Value, ConfigDataGetFloatValue 正常/境界値
	if(ConfigDataSetBfloat16Value(EN_CONFIG_FORGET_RATE, 0.0f) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetBfloat16Value(EN_CONFIG_FORGET_RATE, &bf) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetFloatValue(EN_CONFIG_FORGET_RATE, &f) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(f != 0.0f){ return 1; }
	if(bf != 0){ return 1; }

	if(ConfigDataSetBfloat16Value(EN_CONFIG_FORGET_RATE, 1.0f) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetBfloat16Value(EN_CONFIG_FORGET_RATE, &bf) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetFloatValue(EN_CONFIG_FORGET_RATE, &f) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(f < (1.0f - 0.01f) || (1.0f + 0.01f) < f){ return 1; }
	if(BfloatUtilityBfloat16ToFloat(bf) < (1.0f - 0.01f) || (1.0f + 0.01f) < BfloatUtilityBfloat16ToFloat(bf)){ return 1; }


	if(ConfigDataSetBfloat16Value(EN_CONFIG_FORGET_RATE, 0.5f) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetBfloat16Value(EN_CONFIG_FORGET_RATE, &bf) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetFloatValue(EN_CONFIG_FORGET_RATE, &f) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(f < (0.5f - 0.005f) || (0.5f + 0.005f) < f){ return 1; }
	if(BfloatUtilityBfloat16ToFloat(bf) < (0.5f - 0.005f) || (0.5f + 0.005f) < BfloatUtilityBfloat16ToFloat(bf)){ return 1; }

	if(ConfigDataSetBfloat16Value(EN_CONFIG_FORGET_RATE, -0.01f) != CONFIG_DATA_RESULT_VALUE_OUT_OF_RANGE){ return 1; }
	if(ConfigDataGetBfloat16Value(EN_CONFIG_FORGET_RATE, &bf) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetFloatValue(EN_CONFIG_FORGET_RATE, &f) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(f < (0.5f - 0.005f) || (0.5f + 0.005f) < f){ return 1; }
	if(BfloatUtilityBfloat16ToFloat(bf) < (0.5f - 0.005f) || (0.5f + 0.005f) < BfloatUtilityBfloat16ToFloat(bf)){ return 1; }

	if(ConfigDataSetBfloat16Value(EN_CONFIG_FORGET_RATE, -1.01f) != CONFIG_DATA_RESULT_VALUE_OUT_OF_RANGE){ return 1; }
	if(ConfigDataGetBfloat16Value(EN_CONFIG_FORGET_RATE, &bf) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetFloatValue(EN_CONFIG_FORGET_RATE, &f) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(f < (0.5f - 0.005f) || (0.5f + 0.005f) < f){ return 1; }
	if(BfloatUtilityBfloat16ToFloat(bf) < (0.5f - 0.005f) || (0.5f + 0.005f) < BfloatUtilityBfloat16ToFloat(bf)){ return 1; }

	// 一度セーブする
	ConfigDataSaveConfigGroup0();

	// ConfigDataClear ここまでのテストで設定した値が初期値に戻っているか
	ConfigDataClear();
	if(ConfigDataGetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, &ui8) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, &ui16) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetFloatValue(EN_CONFIG_FORGET_RATE, &f) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui8 != ATTRIBUTE_TABLE[EN_CONFIG_MEMS_SAMPLING_FREQUENCY].UI8Default){ return 1; }
	if(ui16 != ATTRIBUTE_TABLE[EN_CONFIG_HIDDEN_LAYER_NUM].UI16Default){ return 1; }
	if(f < (ATTRIBUTE_TABLE[EN_CONFIG_FORGET_RATE].FDefault - (ATTRIBUTE_TABLE[EN_CONFIG_FORGET_RATE].FDefault / 10))
		|| (ATTRIBUTE_TABLE[EN_CONFIG_FORGET_RATE].FDefault + (ATTRIBUTE_TABLE[EN_CONFIG_FORGET_RATE].FDefault / 10)) < f)
	{
		return 1;
	}

	// ロードして戻っているか比較
	ConfigDataLoadConfigGroup0();
	if(ConfigDataGetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, &ui8) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui8 != 15){ return 1; }
	if(ConfigDataGetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, &ui16) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui16 != 123){ return 1; }
	if(ConfigDataGetFloatValue(EN_CONFIG_FORGET_RATE, &f) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(f < (0.5f - 0.005f) || (0.5f + 0.005f) < f){ return 1; }


	/////////////////////////////////////////////
	// チェックコードによるリセットが効いているか
	FramWriteWord(CHECK_CODE_ADDRESS, 0);
	ConfigDataLoadConfigGroup0();
	if(ConfigDataGetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, &ui8) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui8 != ATTRIBUTE_TABLE[EN_CONFIG_MEMS_SAMPLING_FREQUENCY].UI8Default){ return 1; }
	if(ConfigDataGetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM, &ui16) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ui16 != ATTRIBUTE_TABLE[EN_CONFIG_HIDDEN_LAYER_NUM].UI16Default){ return 1; }


	///////////////////////////
	// テキスト系GetSetテスト
	// 設定範囲のテストは、内部で値Set系関数を使っているため省く

	// ConfigDataSetUint8ValueString, ConfigDataGetUint8ValueString
	if(ConfigDataSetUint8ValueString(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, "7") != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetUint8ValueString(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, stringBuf, sizeof(stringBuf)) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(strcmp(stringBuf, "7") != 0){ return 1; }
	
	if(ConfigDataSetUint8ValueString(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, "k48") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }	// 不正な文字列
	if(ConfigDataSetUint8ValueString(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, "") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }		// 不正な文字列
	if(ConfigDataSetUint8ValueString(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, "1y") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }	// 不正な文字列
	if(ConfigDataSetUint8ValueString(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, "-10") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }	// 不正な文字列

	if(ConfigDataSetUint8ValueString(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, "00000000000010") != CONFIG_DATA_RESULT_NORMAL){ return 1; }			// 扱う文字数制限
	if(ConfigDataSetUint8ValueString(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, "000000000000010") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }	// 扱う文字数制限

	if(ConfigDataGetUint8ValueString(EN_CONFIG_MEMS_SAMPLING_FREQUENCY, stringBuf, sizeof(stringBuf) - 1) != CONFIG_DATA_RESULT_SMALL_STRING_BUFFER_SIZE){ return 1; }	// バッファのサイズが小さい

	// ConfigDataSetUint16ValueString, ConfigDataGetUint16ValueString
	if(ConfigDataSetUint16ValueString(EN_CONFIG_HIDDEN_LAYER_NUM, "123") != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetUint16ValueString(EN_CONFIG_HIDDEN_LAYER_NUM, stringBuf, sizeof(stringBuf)) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(strcmp(stringBuf, "123") != 0){ return 1; }

	if(ConfigDataSetUint16ValueString(EN_CONFIG_HIDDEN_LAYER_NUM, "k416") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }			// 不正な文字列
	if(ConfigDataSetUint16ValueString(EN_CONFIG_HIDDEN_LAYER_NUM, "") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }				// 不正な文字列
	if(ConfigDataSetUint16ValueString(EN_CONFIG_HIDDEN_LAYER_NUM, "1y") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }				// 不正な文字列
	if(ConfigDataSetUint16ValueString(EN_CONFIG_HIDDEN_LAYER_NUM, "-10") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }	// 不正な文字列

	if(ConfigDataSetUint16ValueString(EN_CONFIG_HIDDEN_LAYER_NUM, "00000000000012") != CONFIG_DATA_RESULT_NORMAL){ return 1; }				// 扱う文字数制限
	if(ConfigDataSetUint16ValueString(EN_CONFIG_HIDDEN_LAYER_NUM, "000000000000123") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }	// 扱う文字数制限

	if(ConfigDataGetUint16ValueString(EN_CONFIG_HIDDEN_LAYER_NUM, stringBuf, sizeof(stringBuf) - 1) != CONFIG_DATA_RESULT_SMALL_STRING_BUFFER_SIZE){ return 1; }	// バッファのサイズが小さい

	// ConfigDataSetBfloat16ValueString, ConfigDataGetBfloat16ValueString
	if(ConfigDataSetBfloat16ValueString(EN_CONFIG_AI_FAR_THRESHOLD, "0.3") != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetBfloat16ValueString(EN_CONFIG_AI_FAR_THRESHOLD, stringBuf, sizeof(stringBuf)) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(strcmp(stringBuf, "0.298828") != 0){ return 1; }

	if(ConfigDataSetBfloat16ValueString(EN_CONFIG_AI_FAR_THRESHOLD, "1.0e+20") != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetBfloat16ValueString(EN_CONFIG_AI_FAR_THRESHOLD, stringBuf, sizeof(stringBuf)) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(strcmp(stringBuf, "9.97277e+19") != 0){ return 1; }

	if(ConfigDataSetBfloat16ValueString(EN_CONFIG_AI_FAR_THRESHOLD, "12345678901234") != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(ConfigDataGetBfloat16ValueString(EN_CONFIG_AI_FAR_THRESHOLD, stringBuf, sizeof(stringBuf)) != CONFIG_DATA_RESULT_NORMAL){ return 1; }
	if(strcmp(stringBuf, "1.23008e+13") != 0){ return 1; }

	if(ConfigDataSetBfloat16ValueString(EN_CONFIG_AI_FAR_THRESHOLD, "k416") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }	// 不正な文字列
	if(ConfigDataSetBfloat16ValueString(EN_CONFIG_AI_FAR_THRESHOLD, "") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }		// 不正な文字列
	if(ConfigDataSetBfloat16ValueString(EN_CONFIG_AI_FAR_THRESHOLD, "1y") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }	// 不正な文字列

	if(ConfigDataSetBfloat16ValueString(EN_CONFIG_AI_FAR_THRESHOLD, "123456789012345") != CONFIG_DATA_RESULT_INVALID_STRING){ return 1; }	// 扱う文字数制限

	if(ConfigDataGetBfloat16ValueString(EN_CONFIG_AI_FAR_THRESHOLD, stringBuf, sizeof(stringBuf) - 1) != CONFIG_DATA_RESULT_SMALL_STRING_BUFFER_SIZE){ return 1; }	// バッファのサイズが小さい
	
	return 0;
}


/*############################################################################*/
/*#                              Subroutine                                  #*/
/*############################################################################*/

static int isValidData(EN_CONFIG id, void* data)
{
	int result = 1;
	EN_CONFIG_TYPE type;
	uint8_t u8;
	uint16_t u16;
	int16_t i16;
	float f;

	type = ConfigDataGetConfigType(id);
	switch (type)
	{
	case EN_CONFIG_TYPE_UINT8_T:
		u8 = *(uint8_t*)data;
		if(u8 < ATTRIBUTE_TABLE[id].UI8Min || ATTRIBUTE_TABLE[id].UI8Max < u8){ result = 0; }
		break;

	case EN_CONFIG_TYPE_UINT16_T:
		u16 = *(uint16_t*)data;
		if(u16 < ATTRIBUTE_TABLE[id].UI16Min || ATTRIBUTE_TABLE[id].UI16Max < u16){ result = 0; }
		break;

	case EN_CONFIG_TYPE_INT16_T:
		i16 = *(int16_t*)data;
		if(i16 < ATTRIBUTE_TABLE[id].I16Min || ATTRIBUTE_TABLE[id].I16Max < i16){ result = 0; }
		break;

	case EN_CONFIG_TYPE_BFLOAT:
		f = BfloatUtilityBfloat16ToFloat(*(bfloat16*)data);
		if(f < ATTRIBUTE_TABLE[id].FMin || ATTRIBUTE_TABLE[id].FMax < f){ result = 0; }
		break;
	default:
		result = 0;
		break;
	}
	return result;
}

static int isValidId(EN_CONFIG id)
{
	if(id < 0 || EN_CONFIG_END <= id){ return 0; }
	return 1;
}

static CONFIG_DATA_RESULT dataSetCommon(EN_CONFIG id, void* data)
{
	if(isValidId(id) == 0){ return CONFIG_DATA_RESULT_ID_OUT_OF_RANGE; }
	else if(isValidData(id, data) == 0){ return CONFIG_DATA_RESULT_VALUE_OUT_OF_RANGE; }
	memcpy(&(group0Buffer[ATTRIBUTE_TABLE[id].Address]), data, ATTRIBUTE_TABLE[id].Size);
	return CONFIG_DATA_RESULT_NORMAL;
}

static CONFIG_DATA_RESULT dataGetCommon(EN_CONFIG id, void* data)
{
	if(isValidId(id) == 0){ return CONFIG_DATA_RESULT_ID_OUT_OF_RANGE; }
	memcpy(data, &(group0Buffer[ATTRIBUTE_TABLE[id].Address]), ATTRIBUTE_TABLE[id].Size);
	return CONFIG_DATA_RESULT_NORMAL;
}

static int strnlen(const void* src, int num)
{
	const uint8_t* d = src;
	for (int i = 0; i < num; i++)
	{
		if(*d == '\0'){ return i;}
		d++;
	}
	return num;
}
