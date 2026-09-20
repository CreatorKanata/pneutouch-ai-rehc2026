/*****************************************************************************
 * File: AIWeight.c
 * Title: AIの重みデータを読み書きする。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file AIWeight.c
 * @brief AIの重みデータを読み書きする。
 */

#include "solistAi.h"
#include "AIWeight.h"
#include "ConfigAiWeight.h"
#include "wdt.h"

#define AI_MODEL_MIN 		(0)
#define AI_MODEL_MAX 		(1)
#define AI_HIDDEN_SIZE_MIN	(1)
#define AI_HIDDEN_SIZE_MAX	(64)
#define AI_OUTPUT_SIZE_MIN	(1)
#define AI_OUTPUT_SIZE_MAX	(256)

//重みベータのデータindex最大
//256×64×2 = 32768
#define BETA_INDEX_MAX		(32768U)

//重みPのデータIndex最大
//64×64×2 = 8192
#define P_INDEX_MAX			(8192U)


/*
AIメモリマップ(推測)
Beta:
　(出力ノード数　×　2B) ×　隠れ層ノード数
e.g)出力ノード数　= 10、隠れ層ノード数 = 5
  0~19,
	20~39,
	40~59,
	60~79,
	80~99
	
P:
　(隠れ層ノード数 × 2B) ×　隠れ層ノード数
e.g)隠れ層ノード数 = 5
  0~9,
	10~19,
	20~29,
	30~39,
	40~49
*/
//#define TEST
//重みデータ格納場所
#define WEIGHT_MAX			(256)
static bfloat16 weightBuffer[WEIGHT_MAX];

//AI重みβ、Pのindex
#define INDEX_MOVE 			(2)
static uint32_t betaIndex = 0;
static uint32_t pIndex = 0;

static void aiWeightInit(void);
static bool framWriteAIWeight(WEIGHT_KIND kind,uint16_t size,uint32_t* index,bfloat16* buffer);
static bool framReadAIWeight(WEIGHT_KIND kind,uint16_t size,uint32_t* index,bfloat16* buffer);
static bool importAllWeightBetaFromFramToAI(uint8_t instance, uint16_t hiddenSize,uint16_t outputSize);
static bool exportAllWeightBetaFromAIToFram(uint8_t instance, uint16_t hiddenSize,uint16_t outputSize);
static bool importAllWeightPFromFramToAI(uint8_t instance, uint16_t hiddenSize);
static bool exportAllWeightPFromAIToFram(uint8_t instance, uint16_t hiddenSize);
static bool importWeightBetaFromFramToAIOnce(uint8_t instance, uint16_t outputSize);
static bool exportWeightBetaFromAIToFramOnce(uint8_t instance, uint16_t outputSize);
static bool importWeightPFromFramToAIOnce(uint8_t instance, uint16_t hiddenSize);
static bool exportWeightPFromAIToFramOnce(uint8_t instance, uint16_t hiddenSize);
static bool checkAIInstance(uint8_t instance);
static bool checkBetaIndexRange(void);
static bool checkPIndexRange(void);
static bool checkOutputSize(uint16_t outputSize);
static bool checkHiddenSize(uint16_t hiddenSize);


//AIの重みをFRAMから読み込みAIに全て書き込む。
bool AIWeightImportWeightBetaAndPFromFramToAI(uint8_t instance, uint16_t hiddenSize,uint16_t outputSize)
{
	if(!checkAIInstance(instance)) return false;
	if(!checkHiddenSize(hiddenSize)) return false;
	if(!checkOutputSize(outputSize)) return false;
	aiWeightInit();
	wdt_clear();
	if(!importAllWeightBetaFromFramToAI(instance,hiddenSize,outputSize))return false;
	wdt_clear();
	if(!importAllWeightPFromFramToAI(instance,hiddenSize))return false;
	//成功
	return true;
}

//AIの重みをAIから全て読み込みFRAMへ保存する。
bool AIWeightExportWeightBetaAndPFromAIToFram(uint8_t instance, uint16_t hiddenSize,uint16_t outputSize)
{
	if(!checkAIInstance(instance)) return false;
	if(!checkHiddenSize(hiddenSize)) return false;
	if(!checkOutputSize(outputSize)) return false;
	aiWeightInit();
	ConfigAiWeightClear();
	wdt_clear();
	if(!exportAllWeightBetaFromAIToFram(instance,hiddenSize,outputSize))return false;
	wdt_clear();
	if(!exportAllWeightPFromAIToFram(instance,hiddenSize))return false;
	//成功
	return true;
}


//重みBetaを全てAIに書き込む
static bool importAllWeightBetaFromFramToAI(uint8_t instance, uint16_t hiddenSize,uint16_t outputSize)
{
	aiWeightInit();
	//隠れ層ノード数が不正だと終了
	if(!checkHiddenSize(hiddenSize))return false;
	for(int i = 0; i < hiddenSize; i++)
	{
		if(!importWeightBetaFromFramToAIOnce(instance,outputSize)) 
		{
			return false;
		}
		wdt_clear();
	}
	return true;
}


//重みBetaをAIから全て読み込んでFRAMに書き込む
static bool exportAllWeightBetaFromAIToFram(uint8_t instance, uint16_t hiddenSize,uint16_t outputSize)
{
	aiWeightInit();
	//隠れ層ノード数が不正だと終了
	if(!checkHiddenSize(hiddenSize))return false;
	for(int i = 0; i < hiddenSize; i++)
	{
		if(!exportWeightBetaFromAIToFramOnce(instance,outputSize)) 
		{
			return false;
		}
		wdt_clear();
	}
	return true;
}


//重みPを全てAIに書き込む
static bool importAllWeightPFromFramToAI(uint8_t instance, uint16_t hiddenSize)
{
	aiWeightInit();
	//隠れ層ノード数が不正だと終了
	if(!checkHiddenSize(hiddenSize))return false;
	for(int i = 0; i < hiddenSize; i++)
	{
		if(!importWeightPFromFramToAIOnce(instance,hiddenSize))
		{
			return false;
		}
		wdt_clear();
	}
	return true;
}


//重みPをAIから全て読み込んでFRAMに書き込む
static bool exportAllWeightPFromAIToFram(uint8_t instance, uint16_t hiddenSize)
{
	aiWeightInit();
	//隠れ層ノード数が不正だと終了
	if(!checkHiddenSize(hiddenSize))return false;
	for(int i = 0; i < hiddenSize; i++)
	{
		if(!exportWeightPFromAIToFramOnce(instance,hiddenSize))
		{
			return false;
		}
		wdt_clear();
	}
	return true;
}


//重みBetaを一回分AIに書き込む
static bool importWeightBetaFromFramToAIOnce(uint8_t instance,uint16_t outputSize)
{
	uint32_t offset = betaIndex;
	//AIモデル番号が不正だと終了
	if(!checkAIInstance(instance)) return false;
	//出力層ノード数が不正だと終了
	if(!checkOutputSize(outputSize))return false;
	//AIメモリマップのindex範囲外なら終了
	if(!checkBetaIndexRange())return false;
	//FRAMから読込
	if(!framReadAIWeight(WEIGHT_KIND_BETA,outputSize,&betaIndex,weightBuffer)) return false;
	//AIに書き込み
	ODL_SetWeightBeta(weightBuffer,instance,offset,outputSize*2);
	//成功
	return true;
}


//重みBetaをAIから一回分読み込んでFRAMに書き込む
static bool exportWeightBetaFromAIToFramOnce(uint8_t instance, uint16_t outputSize)
{
	uint32_t offset = betaIndex;
	//AIモデル番号が不正だと終了
	if(!checkAIInstance(instance)) return false;
	//出力層ノード数が不正だと終了
	if(!checkOutputSize(outputSize))return false;
	//AIメモリマップのindex範囲外なら終了
	if(!checkBetaIndexRange())return false;
	//AIから読込
	ODL_GetWeightBeta(weightBuffer,instance,offset,outputSize*2);
	//FRAMに書き込み
	if(!framWriteAIWeight(WEIGHT_KIND_BETA,outputSize,&betaIndex,weightBuffer)) return false;
	//成功
	return true;
}


//重みPを一回分AIに書き込む
static bool importWeightPFromFramToAIOnce(uint8_t instance, uint16_t hiddenSize)
{
	uint32_t offset = pIndex;
	//AIモデル番号が不正だと終了
	if(!checkAIInstance(instance)) return false;
	//隠れ層ノード数が不正だと終了
	if(!checkHiddenSize(hiddenSize))return false;
	//AIメモリマップのindex範囲外なら終了
	if(!checkPIndexRange())return false;
	//FRAMから読込
	if(!framReadAIWeight(WEIGHT_KIND_P,hiddenSize,&pIndex,weightBuffer)) return false;
	//AIに書き込み
	ODL_SetWeightP(weightBuffer,instance,offset,hiddenSize*2);
	//成功
	return true;
}

//重みBetaをAIから一回分読み込んでFRAMに書き込む
static bool exportWeightPFromAIToFramOnce(uint8_t instance, uint16_t hiddenSize)
{
	uint32_t offset = pIndex;
	//AIモデル番号が不正だと終了
	if(!checkAIInstance(instance)) return false;
	//隠れ層ノード数が不正だと終了
	if(!checkHiddenSize(hiddenSize))return false;
	//AIメモリマップのindex範囲外なら終了
	if(!checkPIndexRange())return false;
	//AIから読込
	ODL_GetWeightP(weightBuffer,instance,offset,hiddenSize*2);
	//FRAMに書き込み
	if(!framWriteAIWeight(WEIGHT_KIND_P,hiddenSize,&pIndex,weightBuffer)) return false;
	//成功
	return true;
}


//Indexを初期化
static void aiWeightInit(void)
{
	betaIndex = 0;
	pIndex = 0;
}

//AIモデル数のチェック
static bool checkAIInstance(uint8_t instance)
{
	if( AI_MODEL_MAX < instance ) return false;
	return true;
}

//重みβのindexチェック
static bool checkBetaIndexRange(void)
{
	if( BETA_INDEX_MAX < betaIndex) return false;
	return true;
}

//重みPのindexチェック
static bool checkPIndexRange(void)
{
	if( P_INDEX_MAX < pIndex) return false;
	return true;
}

//出力層ノード数チェック
static bool checkOutputSize(uint16_t outputSize)
{
	if((outputSize < AI_OUTPUT_SIZE_MIN) || (AI_OUTPUT_SIZE_MAX < outputSize)) return false;
	return true;
}

//隠れ層ノード数チェック
static bool checkHiddenSize(uint16_t hiddenSize)
{
	if((hiddenSize < AI_HIDDEN_SIZE_MIN) || (AI_HIDDEN_SIZE_MAX < hiddenSize)) return false;
	return true;
}

//FRAMに重みデータ書き込み
//2Bずつ書き込むのでindex/2
static bool framWriteAIWeight(WEIGHT_KIND kind,uint16_t size,uint32_t* index,bfloat16* buffer)
{
	for(int i = 0; i < size; i++)
	{
		if(ConfigAiWeightWrite(kind,(*index/2),buffer[i])) 
		{
			return false;
		}
		*index += INDEX_MOVE;
	}
	return true;
}

//FRAMから重みデータ読込
//2Bずつ読み込むのでindex/2
static bool framReadAIWeight(WEIGHT_KIND kind,uint16_t size,uint32_t* index,bfloat16* buffer)
{
	//FRAMから読込
	for(int i = 0; i < size; i++)
	{
		if(ConfigAiWeightRead(kind,(*index/2),&buffer[i])) 
		{
			return false;
		}
		*index += INDEX_MOVE;
	}
	return true;
}
