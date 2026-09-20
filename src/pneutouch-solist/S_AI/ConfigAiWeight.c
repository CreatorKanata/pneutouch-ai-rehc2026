/*****************************************************************************
 * File: ConfigAiWeight.c
 * Title: AI重み設定モジュール
 * LastUpdated: 2025.06.17
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file ConfigAiWeight.c
 * @brief AI重み設定モジュール
 */

#include "ConfigAiWeight.h"
#include "Fram.h"
#include "wdt.h"
/*############################################################################*/
/*#                                  Macro                                   #*/
/*############################################################################*/

// 重みベータのデータ個数最大
#define MAX_BETA_COUNT			(32768U / 2)

// 重みPのデータ個数最大
#define MAX_P_COUNT				(8192U / 2)

// 重みベータのFRAMの先頭アドレス
#define FRAM_ADDRES_BETA_TOP	(100U)

// 重みPのFRAMの先頭アドレス
#define FRAM_ADDRES_P_TOP		(FRAM_ADDRES_BETA_TOP + (MAX_BETA_COUNT * 2))


/*############################################################################*/
/*#                                Variable                                  #*/
/*############################################################################*/


/*############################################################################*/
/*#                               Prototype                                  #*/
/*############################################################################*/

// 重みベータを保存する
// index	: 0から始まる保存先インデックス値
// data		: 保存値
// 戻り値	: 成功時0、失敗時-1
static int writeBeta(uint32_t index, bfloat16 data);

// 重みベータを取得する
// index	: 0から始まる読込先インデックス値
// data		: 読込値
// 戻り値	: 成功時0、失敗時-1
static int readBeta(uint32_t index, bfloat16* data);

// 重みPを保存する
// index	: 0から始まる保存先インデックス値
// data		: 保存値
// 戻り値	: 成功時0、失敗時-1
static int writeP(uint32_t index, bfloat16 data);

// 重みPを取得する
// index	: 0から始まる読込先インデックス値
// data		: 読込値
// 戻り値	: 成功時0、失敗時-1
static int readP(uint32_t index, bfloat16* data);

// ベータ index値をFRAMアドレスに変換する
// index	: 変換するindex値
// 戻り値	: FRAMアドレス。不正なindexの場合UINT32_MAX
static uint32_t getFramAddressBeta(uint32_t index);

// P index値をFRAMアドレスに変換する
// index	: 変換するindex値
// 戻り値	: FRAM。不正なindexの場合UINT32_MAX
static uint32_t getFramAddressP(uint32_t index);

/*############################################################################*/
/*#                                  API                                     #*/
/*############################################################################*/

int ConfigAiWeightWrite(WEIGHT_KIND kind, uint32_t index, bfloat16 data)
{
	if(kind != WEIGHT_KIND_BETA && kind != WEIGHT_KIND_P)
	{
		return -1;
	}

	if(kind == WEIGHT_KIND_BETA)
	{
		return writeBeta(index, data);
	}
	else
	{
		return writeP(index, data);
	}
}

int ConfigAiWeightRead(WEIGHT_KIND kind, uint32_t index, bfloat16* data)
{
	if(kind == WEIGHT_KIND_BETA)
	{
		return readBeta(index, data);
	}
	else
	{
		return readP(index, data);
	}
}

int ConfigAiWeightValidateIndex(WEIGHT_KIND kind, uint32_t index)
{
	if (kind == WEIGHT_KIND_BETA)
	{
		return (index < MAX_BETA_COUNT) ? 0 : -1;
	}
	else
	{
		return (index < MAX_P_COUNT) ? 0 : -1;
	}
}

int ConfigAiWeightClear(void)
{
	const bfloat16 CLEAR = 0;

	for(uint32_t betaIndex = 0; betaIndex < MAX_BETA_COUNT; betaIndex++)
	{
		if(ConfigAiWeightWrite(WEIGHT_KIND_BETA,betaIndex,CLEAR)) return -1;
		wdt_clear();
	}
	for(uint32_t pIndex = 0; pIndex < MAX_P_COUNT; pIndex++)
	{
		if(ConfigAiWeightWrite(WEIGHT_KIND_P,pIndex,CLEAR)) return -1;
		wdt_clear();
	}
	return 0;
}

// 単体テスト
int ConfigAiWeightTest(void)
{
	bfloat16 read = 55;
	uint16_t read16 = 55;

	///////////////////////////
	// index範囲
	if(ConfigAiWeightWrite(WEIGHT_KIND_BETA, 0, (bfloat16)0) != 0){ return 1; }
	if(ConfigAiWeightWrite(WEIGHT_KIND_BETA, 1, (bfloat16)1) != 0){ return 1; }
	if(ConfigAiWeightWrite(WEIGHT_KIND_BETA, 16383, (bfloat16)2) != 0){ return 1; }
	if(ConfigAiWeightWrite(WEIGHT_KIND_BETA, 16384, (bfloat16)3) != -1){ return 1; }

	if(ConfigAiWeightWrite(WEIGHT_KIND_P, 0, (bfloat16)4) != 0){ return 1; }
	if(ConfigAiWeightWrite(WEIGHT_KIND_P, 1, (bfloat16)5) != 0){ return 1; }
	if(ConfigAiWeightWrite(WEIGHT_KIND_P, 4095, (bfloat16)6) != 0){ return 1; }
	if(ConfigAiWeightWrite(WEIGHT_KIND_P, 4096, (bfloat16)7) != -1){ return 1; }

	/////////////////////////////////////
	// 前のテスト項目で書き込んだ値の読込
	if(ConfigAiWeightRead(WEIGHT_KIND_BETA, 0, &read) != 0){ return 1; }
	if((uint16_t)read != 0){ return 1; }
	if(ConfigAiWeightRead(WEIGHT_KIND_BETA, 1, &read) != 0){ return 1; }
	if((uint16_t)read != 1){ return 1; }
	if(ConfigAiWeightRead(WEIGHT_KIND_BETA, 16383, &read) != 0){ return 1; }
	if((uint16_t)read != 2){ return 1; }
	if(ConfigAiWeightRead(WEIGHT_KIND_BETA, 16384, &read) != -1){ return 1; }

	if(ConfigAiWeightRead(WEIGHT_KIND_P, 0, &read) != 0){ return 1; }
	if((uint16_t)read != 4){ return 1; }
	if(ConfigAiWeightRead(WEIGHT_KIND_P, 1, &read) != 0){ return 1; }
	if((uint16_t)read != 5){ return 1; }
	if(ConfigAiWeightRead(WEIGHT_KIND_P, 4095, &read) != 0){ return 1; }
	if((uint16_t)read != 6){ return 1; }
	if(ConfigAiWeightRead(WEIGHT_KIND_P, 4096, &read) != -1){ return 1; }

	/////////////////////////////////////////////////////
	// 前のテスト項目で書き込んだ値が狙ったFRAM位置にあるか
	FramReadHalfWord(100, &read16);
	if(read16 != 0){ return 1; }
	FramReadHalfWord(102, &read16);
	if(read16 != 1){ return 1; }

	FramReadHalfWord(32868U, &read16);
	if(read16 != 4){ return 1; }
	FramReadHalfWord(32870U, &read16);
	if(read16 != 5){ return 1; }

	//////////////////////////////////
	// kindの範囲チェック
	if(ConfigAiWeightWrite((WEIGHT_KIND) 0, 0, 0) != 0){ return 1; }
	if(ConfigAiWeightWrite((WEIGHT_KIND) 1, 0, 0) != 0){ return 1; }
	if(ConfigAiWeightWrite((WEIGHT_KIND) 2, 0, 0) != -1){ return 1; }
	
	//重みのクリア
	if(ConfigAiWeightClear()) return -2;

	return 0;
}


/*############################################################################*/
/*#                              Subroutine                                  #*/
/*############################################################################*/

int writeBeta(uint32_t index, bfloat16 data)
{
	uint32_t adr = getFramAddressBeta(index);
	if(adr == UINT32_MAX){ return -1; }
	FramWriteHalfWord( adr, (uint16_t)data );
	return 0;
}

int readBeta(uint32_t index, bfloat16* data)
{
	uint32_t adr = getFramAddressBeta(index);
	if(adr == UINT32_MAX){ return -1; }
	FramReadHalfWord(adr, (uint16_t*)data);
	return 0;
}

int writeP(uint32_t index, bfloat16 data)
{
	uint32_t adr = getFramAddressP(index);
	if(adr == UINT32_MAX){ return -1; }
	FramWriteHalfWord( adr, (uint16_t)data );
	return 0;
}

int readP(uint32_t index, bfloat16* data)
{
	uint32_t adr = getFramAddressP(index);
	if(adr == UINT32_MAX){ return -1; }
	FramReadHalfWord(adr, (uint16_t*)data);
	return 0;
}


static uint32_t getFramAddressBeta(uint32_t index)
{
	if(MAX_BETA_COUNT <= index){ return UINT32_MAX; }
	return (index * 2) + FRAM_ADDRES_BETA_TOP;
}

static uint32_t getFramAddressP(uint32_t index)
{
	if(MAX_P_COUNT <= index){ return UINT32_MAX; }
	return (index * 2) + FRAM_ADDRES_P_TOP;
}
