/*****************************************************************************
 * File: UartCommand.c
 * Title: UARTで受信したコマンドを実行する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file UartCommand.c
 * @brief UARTで受信したコマンドを実行する。
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "uartf1.h"
#include "UartCommand.h"
#include "ConfigData.h"
#include "ConfigAiWeight.h"
#include "AILog.h"
#include "RX4111.h"

/*############################################################################*/
/*#                                  Macro                                   #*/
/*############################################################################*/

// Set parameterコマンドの設定値が始まるトークンのIndex値
#define SETPARAM_VALUE_TOKEN_START_INDEX	(2)

#define MAXIMUM_NUMBER_OF_PARAM_COUNT	(16)	// Get parameterコマンドで送るパラメータ個数の最大
#define MAXIMUM_NUMBER_OF_UART_COMMAND	(6)		// コマンド種類の数
#define MAXIMUM_NUMBER_OF_VALUE_STRING	(CONFIG_DATA_MAX_STRING_LENGTH + 2)

#define WEIGHT_ONE_DATA_CHARA_COUNT		(4)		// 重みデータ1つあたりの文字数
#define MAXIMUM_WEIGHT_DATA_COUNT		(50)	// 一回のコマンドで扱える重みデータ数

/*############################################################################*/
/*#                               Prototype                                  #*/
/*############################################################################*/

typedef void (*UartCommandFunc) (UART_PARSE* rcv);

typedef struct 
{
	const char* Str;
	UartCommandFunc Func;
}UART_COMMAND;

// 受信データから実行するコマンド関数を返す
static UartCommandFunc checkCommand(UART_PARSE* rcv);

// Set parameterコマンド関数
// rcv		: 受信データ
static void setParameter(UART_PARSE* rcv);

// Get parameterコマンド関数
// rcv		: 受信データ
static void getParameter(UART_PARSE* rcv);

// Elase all parameterコマンド関数
// rcv		: 受信データ
static void eraseAllParameter(UART_PARSE* rcv);

// Set Weightコマンド関数
// rcv		: 受信データ
static void setWeight(UART_PARSE* rcv);

// Get Weightコマンド関数
// rcv		: 受信データ
static void getWeight(UART_PARSE* rcv);

// Set DateTimeコマンド関数
// rcv		: 受信データ
static void setDateTime(UART_PARSE* rcv);

// パラメータ番号を取得する
// rcv		: 受信データ
// number	: パラメータ番号の格納先
// 戻り値	: 成功時true
static bool parseParamNumber(UART_PARSE* rcv, uint8_t* number);

// パラメータ番号が正常な物か検証する
// number	: 検証対象
// 戻り値	: 正常な場合true
static bool validationCommandNumber(uint8_t number);

// パラメータ番号に対応する設定値を、数値を表す文字列で設定する
// number		: 設定対象パラメータ番号
// valueString	: 数値を表す設定値文字列
// 戻り値	: 成功時true
static bool setCommandValue(uint8_t number, const char* valueString);

// パラメータ番号に対応する設定値を、数値を表す文字列で設定する
// number	: 検証対象
// 戻り値	: 成功時true
static void createGetParamResponseString(uint8_t number, uint8_t valueCount);

// 重み種類を取得する
// rcv	: 受信データ
// kind	: 重み種類格納先
// 戻り値	: 成功時true
static bool parseWeightKind(UART_PARSE* rcv, WEIGHT_KIND* kind);

// 重みのindex値を取得する
// rcv		: 受信データ
// kind		: index値を取得しようとしている重み種類
// index	: 重みのindex値格納先
// 戻り値	: 成功時true
static bool parseWeightIndex(UART_PARSE* rcv, WEIGHT_KIND kind, uint32_t* index);

// 重みデータ1つ分をパースする
// parseStr	: 連続した重みデータ文字列(4文字以上必要)。内部でバッファとして使用するが、関数から戻る時には元に戻す。
// data		: 重みデータ格納先
// 戻り値	: 成功時true
static bool parseOneWeightData(char* tokenStr, bfloat16* data);

// 10進数文字列を数値にパースし、範囲内の値かチェックする
// str			: パースする文字列
// number		: データ格納先
// validateMin	: 範囲下限
// validateMin	: 範囲上限
// 戻り値		: 成功時true。変換に失敗した、または範囲外の場合false
static bool parseDecimalStringToUint8(char* str, uint8_t* number, uint8_t validateMin, uint8_t validateMax);

/*############################################################################*/
/*#                                Variable                                  #*/
/*############################################################################*/

static const char* OK_STRING = "OK";
static const char* NG_STRING = "NG";
static const char* UNKNOWN_STRING = "UN";
static const char* NUMBER_STRING = "NUMBER";
static const char* PARAM_STRING = "PARAM";
static const char* KIND_STRING = "KIND";
static const char* INDEX_STRING = "INDEX";
static const char* ARGUMENT_STRING = "ARG";
static const char* VALUES_COUNT_STRING = "VALUESCOUNT";

static char responseBuffer[MAXIMUM_NUMBER_OF_UART_BUFFER];

static const UART_COMMAND commandTable[MAXIMUM_NUMBER_OF_UART_COMMAND] =
{
	{"#SETPARAM",setParameter},
	{"#GETPARAM",getParameter},
	{"#ERSALL",eraseAllParameter},
	{"#SETWEIGHT",setWeight},
	{"#GETWEIGHT",getWeight},
	{"#SETDATETIME",setDateTime},
};

/*############################################################################*/
/*#                                  API                                     #*/
/*############################################################################*/

char* UartCommandGetResponseString(void)
{
	return responseBuffer;
}


uint32_t UartCommandGetResponseStringLength(void)
{
	uint32_t size = (uint32_t)strlen(responseBuffer);
	return size;
}

void UartCommandExecuteCommand(UART_PARSE* rcv)
{
	UartCommandFunc func = checkCommand(rcv);
	
	if(func == NULL)
	{
		snprintf(responseBuffer,MAXIMUM_NUMBER_OF_UART_BUFFER,"#%s\r",UNKNOWN_STRING);
		return ;
	}
	func(rcv);
}

/*############################################################################*/
/*#                              Subroutine                                  #*/
/*############################################################################*/

static UartCommandFunc checkCommand(UART_PARSE* rcv)
{
	UartCommandFunc funcPointer = NULL;
	const char* cToken = (const char*)rcv->Token[0];
	
	for(int m = 0; m < MAXIMUM_NUMBER_OF_UART_COMMAND; m++)
	{
		if(!strcmp(cToken,commandTable[m].Str))
		{
			funcPointer = commandTable[m].Func;
		}
	}
	return funcPointer;
}

static void setParameter(UART_PARSE* rcv)
{
	uint8_t paramNumber;

	if(!(parseParamNumber(rcv, &paramNumber)))
	{
		snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,%s\r", NG_STRING, NUMBER_STRING);
		return ;
	}

	for(int i = SETPARAM_VALUE_TOKEN_START_INDEX; i < rcv->TokenSize; i++)
	{
		if(!setCommandValue(paramNumber, (const char*)rcv->Token[i]))
		{
			snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER,"#%s,%s\r", NG_STRING, PARAM_STRING);
			return;
		}
		ConfigDataSaveConfigGroup0();
	}
	snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER,"#%s\r",OK_STRING);
}


static void getParameter(UART_PARSE* rcv)
{
	uint8_t paramNumber;
	uint8_t valueCount = 0;

	if(!(parseParamNumber(rcv, &paramNumber)))
	{
		snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,%s\r", NG_STRING, NUMBER_STRING);
		return ;
	}
	
	// 送信可能な最大のパラメータ数まで送る
	for(int i = 0; i < MAXIMUM_NUMBER_OF_PARAM_COUNT ; i++)
	{
		valueCount++;

		// 次のパラメータ番号が範囲外なら終わり
		if(!(validationCommandNumber(paramNumber + valueCount)))
		{
			break;
		}
	}
	createGetParamResponseString(paramNumber, valueCount);
}

static void eraseAllParameter(UART_PARSE* rcv)
{
	ConfigDataClear();
	ConfigDataSaveConfigGroup0();
	AILogAllElase();
	snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s\r", OK_STRING);
}

static void setWeight(UART_PARSE* rcv)
{
	WEIGHT_KIND kind;
	uint32_t index = 0;
	size_t valuesLength = 0;
	char* p = (char*)rcv->Token[3];	// 設定データの文字列
	bfloat16 data;

	if(rcv->TokenSize != 4)
	{
		snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,%s\r", NG_STRING, ARGUMENT_STRING);
		return;
	}
	else if(!parseWeightKind(rcv, &kind))
	{
		snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,%s\r", NG_STRING, KIND_STRING);
		return;
	}
	else if(!parseWeightIndex(rcv, kind, &index))
	{
		snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,%s\r", NG_STRING, INDEX_STRING);
		return;
	}
	
	valuesLength = strlen(p);
	if((valuesLength % WEIGHT_ONE_DATA_CHARA_COUNT != 0) || (MAXIMUM_WEIGHT_DATA_COUNT < valuesLength / 4))
	{
		snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,%s\r", NG_STRING, VALUES_COUNT_STRING);
		return;
	}

	for(; WEIGHT_ONE_DATA_CHARA_COUNT <= valuesLength
		; valuesLength -= WEIGHT_ONE_DATA_CHARA_COUNT, index++, p += WEIGHT_ONE_DATA_CHARA_COUNT)
	{
		if(ConfigAiWeightValidateIndex(kind, index) == -1)
		{
			// indexが範囲外
			snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,%s\r", NG_STRING, INDEX_STRING);
			return;
		}

		if(!parseOneWeightData(p, &data))
		{
			// 発生しない
		}

		ConfigAiWeightWrite(kind, index, data);
	}
	snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER,"#%s\r",OK_STRING);
}


static void getWeight(UART_PARSE* rcv)
{
	WEIGHT_KIND kind;
	uint32_t index = 0;
	uint16_t readData;
	int writeLen = 0;
	char* wp = responseBuffer;

	if(rcv->TokenSize != 3)
	{
		snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,%s\r", NG_STRING, ARGUMENT_STRING);
		return;
	}
	else if(!parseWeightKind(rcv, &kind))
	{
		snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,%s\r", NG_STRING, KIND_STRING);
		return;
	}
	else if(!parseWeightIndex(rcv, kind, &index))
	{
		snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,%s\r", NG_STRING, INDEX_STRING);
		return;
	}

	// データまでの応答文字列格納
	writeLen = snprintf(wp, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,", OK_STRING);
	wp += writeLen;
	
	// データ数が規程の数になるか、重みデータが取得できなくなるまでレスポンスにデータ文字を繋ぐ
	for (size_t i = 0; i < MAXIMUM_WEIGHT_DATA_COUNT; i++, index++)
	{
		if(ConfigAiWeightRead(kind, index, (bfloat16*)&readData) == -1){ break; }
		writeLen = snprintf(wp, MAXIMUM_NUMBER_OF_UART_BUFFER, "%04x", readData);
		wp += writeLen;
	}

	*wp = '\r';
	wp++;
	*wp = '\0';
}

// Set DateTimeコマンド関数
// rcv		: 受信データ
static void setDateTime(UART_PARSE* rcv)
{
	RX4111_DATE_TIME dateTime;
	bool condition;

	if(rcv->TokenSize != 7)
	{
		snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,%s\r", NG_STRING, ARGUMENT_STRING);
		return;
	}

	condition = parseDecimalStringToUint8(rcv->Token[1], &dateTime.Year, 0, 99);
	condition &= parseDecimalStringToUint8(rcv->Token[2], &dateTime.Month, 1, 12);
	condition &= parseDecimalStringToUint8(rcv->Token[3], &dateTime.Day, 1, 31);
	condition &= parseDecimalStringToUint8(rcv->Token[4], &dateTime.Hour, 0, 23);
	condition &= parseDecimalStringToUint8(rcv->Token[5], &dateTime.Minute, 0, 59);
	condition &= parseDecimalStringToUint8(rcv->Token[6], &dateTime.Sec, 0, 59);

	if(!condition)
	{
		snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER, "#%s,%s\r", NG_STRING, PARAM_STRING);
		return;
	}

	RX4111SetTime(&dateTime);

	snprintf(responseBuffer, MAXIMUM_NUMBER_OF_UART_BUFFER,"#%s\r",OK_STRING);
}


static bool setCommandValue(uint8_t number, const char* valueString)
{
	switch (ConfigDataGetConfigType(number))
	{
	case EN_CONFIG_TYPE_UINT8_T:
		if(ConfigDataSetUint8ValueString(number, valueString) != CONFIG_DATA_RESULT_NORMAL){ return false; }
		break;
	case EN_CONFIG_TYPE_UINT16_T:
		if(ConfigDataSetUint16ValueString(number, valueString) != CONFIG_DATA_RESULT_NORMAL){ return false; }
		break;
	case EN_CONFIG_TYPE_INT16_T:
		if(ConfigDataSetInt16ValueString(number, valueString) != CONFIG_DATA_RESULT_NORMAL){ return false; }
		break;
	case EN_CONFIG_TYPE_BFLOAT:
		if(ConfigDataSetBfloat16ValueString(number, valueString) != CONFIG_DATA_RESULT_NORMAL){ return false; }
		break;
	case EN_CONFIG_TYPE_UNKNOWN:
		return false;
	}
	return true;
}

static void createGetParamResponseString(uint8_t number, uint8_t valueCount)
{
	static char valueStr[MAXIMUM_NUMBER_OF_VALUE_STRING] = "";
	static char tempBuf[CONFIG_DATA_MAX_STRING_LENGTH + 1];
	CONFIG_DATA_RESULT res;
	EN_CONFIG configId;
	
	snprintf(responseBuffer,MAXIMUM_NUMBER_OF_UART_BUFFER,"#%s",OK_STRING);
	
	for(unsigned int i = 0; i < valueCount; i++)
	{
		configId = (EN_CONFIG)(number + i);
		switch (ConfigDataGetConfigType(configId))
		{
		case EN_CONFIG_TYPE_UINT8_T:
			res = ConfigDataGetUint8ValueString(configId, tempBuf, sizeof(tempBuf));
			break;
		case EN_CONFIG_TYPE_UINT16_T:
			res = ConfigDataGetUint16ValueString(configId, tempBuf, sizeof(tempBuf));
			break;
		case EN_CONFIG_TYPE_INT16_T:
			res = ConfigDataGetInt16ValueString(configId, tempBuf, sizeof(tempBuf));
			break;
		case EN_CONFIG_TYPE_BFLOAT:
			res = ConfigDataGetBfloat16ValueString(configId, tempBuf, sizeof(tempBuf));
			break;
		case EN_CONFIG_TYPE_UNKNOWN:
			res = CONFIG_DATA_RESULT_ID_OUT_OF_RANGE;
			break;
		}
		if(res == CONFIG_DATA_RESULT_NORMAL)
		{
			snprintf(valueStr, MAXIMUM_NUMBER_OF_VALUE_STRING, ",%s" ,tempBuf);
			strncat(responseBuffer, valueStr, MAXIMUM_NUMBER_OF_VALUE_STRING);
		}
	}
	strncat(responseBuffer ,"\r", 1);
}

static bool parseParamNumber(UART_PARSE* rcv, uint8_t* number)
{
	char* errorChar;
	*number = (uint8_t)strtol( (char*)(rcv->Token[1]), &errorChar, 10);
	
	if(*errorChar != '\0')
	{
		return false;
	}
	
	return validationCommandNumber(*number);
}

static bool validationCommandNumber(uint8_t number)
{
	if((number < (uint8_t)MINIMUM_NUMBER_OF_ID) || ((uint8_t)MAXIMUM_NUMBER_OF_ID < number)) 
	{
		return false;
	}
	return true;
}

static bool parseWeightKind(UART_PARSE* rcv, WEIGHT_KIND* kind)
{
	char* errorChar;
	*kind = (uint8_t)strtol( (char*)(rcv->Token[1]), &errorChar, 10);
	
	if(*errorChar != '\0')
	{
		return false;
	}
	
	return (*kind == WEIGHT_KIND_BETA) || (*kind == WEIGHT_KIND_P);
}

static bool parseWeightIndex(UART_PARSE* rcv, WEIGHT_KIND kind, uint32_t* index)
{
	char* errorChar;
	*index = (uint32_t)strtoul( (char*)(rcv->Token[2]), &errorChar, 10);
	
	if(*errorChar != '\0')
	{
		return false;
	}

	return ConfigAiWeightValidateIndex(kind, *index) == 0;
}

static bool parseOneWeightData(char* tokenStr, bfloat16* data)
{
	char reserveEos;		// 文字列処理するために、一時的に文字列終端を置くための取り置きバッファ
	char* errorChar;
	unsigned long temp;

	// 文字列として扱うために終端文字置き換え
	reserveEos = tokenStr[WEIGHT_ONE_DATA_CHARA_COUNT];
	tokenStr[WEIGHT_ONE_DATA_CHARA_COUNT] = '\0';

	temp = strtoul( tokenStr, &errorChar, 16);
	
	// この判定の前に元に戻すと、終了位置が\0にならないのでエラーと認識されてしまう事がある
	if(*errorChar != '\0')
	{
		tokenStr[WEIGHT_ONE_DATA_CHARA_COUNT] = reserveEos;	// 元に戻す
		return false;
	}

	tokenStr[WEIGHT_ONE_DATA_CHARA_COUNT] = reserveEos;	// 元に戻す

	*data = (bfloat16)temp;
	return true;
}

static bool parseDecimalStringToUint8(char* str, uint8_t* number, uint8_t validateMin, uint8_t validateMax)
{
	char* errorChar;
	*number = (uint8_t)strtol( str, &errorChar, 10);
	
	if(*errorChar != '\0')
	{
		return false;
	}
	
	return (validateMin <= *number) && (*number <= validateMax);
}
