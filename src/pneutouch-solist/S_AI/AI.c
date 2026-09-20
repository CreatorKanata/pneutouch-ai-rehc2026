/*****************************************************************************
 * File: AI.c
 * Title: AIを制御する。
 * LastUpdated: 2025.05.23
******************************************************************************/
/**
 * @file AI.c
 * @brief AIを制御する。
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "irq.h"
#include "wdt.h"
#include "AI.h"
#include "AIWeight.h"
#include "Sensor.h"
#include "SoftwareInterrupt.h"
#include "ConfigData.h"
#include "HighSpeedCom.h"
#include "SystemError.h"
#include "smpl_common_led.h"
#include "BfloatUtility.h"
#include "smpl_common.h"

/** ブロック読出し1*/
#define BLOCK1							(1)
/** AIContextのバッファ数 */
#define AI_CONTEXT_NUM					(2)
/** センサーの設定 */
#define SENSOR_IEPE						(0)
#define SENSOR_MEMS						(1)
/** センサーのデータ数の最大値 */
#define MAXIMUM_NUMBER_OF_SENSOR_DATA	(512)
/** AIに入力できるデータ数の最大値 */
#define MAXIMUM_NUMBER_OF_AI_INPUT_DATA	(256)
/** DC省くかどうか */
#define SKIP_DC							(1)
#define NOT_SKIP						(0)

/** AIに関わる設定 */
//#pragma pack(push,1)
typedef struct
{
	uint16_t SamplingDataCount;
	uint8_t QFormatGain;
	uint8_t FftEnableFlg;
	uint16_t FftPoints;
	uint16_t FftOutputCount;
	uint8_t SkipFFTDataForInputAI;
	FftWindow Window;
	uint8_t BlockTransmissionFlg;
	float YellowThresh;
	float RedThresh;
	uint16_t AiHiddenSize;
	uint16_t AiOutputSize;
}CONFIG;
//#pragma pack(pop)
/** 設定関連*/
static CONFIG aiConfig;
static void aiParameterInit(void);
static void aiWeightInit(void);

/** AIの学習回数 */
volatile static uint16_t learningCounter = 0;
/** 学習時の異常値と学習回数の文字列 */
static char anomalyValueString[NUMBER_OF_CHARACTERS_OF_AI_LEARN_STRING];
/** chunkNo */
volatile static uint16_t chunkNoCnt = 0;
/** AI関連データバッファ */
static AI_CONTEXT aiContexts[AI_CONTEXT_NUM];
/** 最後に使用したAIContext */
static AI_CONTEXT* latestAiContext = &aiContexts[0];
/** ブロック転送 */
static void sendBlockData(AI_CONTEXT* target);
/** bfloat16変換後のバッファ */
static bfloat16 aiAccDataTemp[AI_CONTEXT_INPUT_SOURCE_SIZE];

/** AIのチャンク番号の管理。 */
inline static void manageChunkNo(void);

/** 推論後のコールバック */
static AIPredictCallBack predictCallBackExe = NULL;
inline static void predictCallBackExecute(void);

/** FFT学習推論*/
typedef void (*AILearnSetting)(void);
typedef void (*AIPredictSetting)(void);
typedef enum
{
	LEARN_PREDICT_CONFIG_SINGLE_SENSOR_WIHTOUT_FFT = 0,
	LEARN_PREDICT_CONFIG_SINGLE_SENSOR_WITH_FFT,
}LEARN_PREDICT_CONFIG;

typedef struct
{
	LEARN_PREDICT_CONFIG Config;
	AILearnSetting Learn;
	AIPredictSetting Predict;
}LEARN_PREDICT_SETTING;

static void aiSetLearnFunc(void);
static void aiSetPredictFunc(void);
static void aiLearnSingleSensorDataWithoutFft(void);
static void aiLearnSingleSensorDataWithFft(void);
static void aiPredictFromSingleSensorWithoutFft(void);
static void aiPredictFromSingleSensorWithFft(void);

//設定用テーブル
static const LEARN_PREDICT_SETTING aiLearnPredictTable[] =
{
	{LEARN_PREDICT_CONFIG_SINGLE_SENSOR_WIHTOUT_FFT,aiLearnSingleSensorDataWithoutFft,aiPredictFromSingleSensorWithoutFft},
	{LEARN_PREDICT_CONFIG_SINGLE_SENSOR_WITH_FFT,aiLearnSingleSensorDataWithFft,aiPredictFromSingleSensorWithFft}
};


inline static void fft(AI_CONTEXT* aiContext,bfloat16* beforeFft,bfloat16** afterFft);
inline static void learn(AI_CONTEXT* aiContext,bfloat16* data);
inline static void predict(AI_CONTEXT* aiContext, bfloat16* data);


/** AIで学習した時の異常値と回数を文字列にする。 */
static void convertAnomalyValueAndCountDuringLeaningToString(bfloat16 value,uint16_t counter);
/** AIの異常値をリセットする*/
static void resetAnomalyValue(void);
/** AIの異常判定結果 */
/** 現在の異常判定結果をリセットする。 */
static void resetAnomalyResult(void);
static ANOMALY currentAnomalyResult = ANOMALY_NORMAL;
static ANOMALY oldAnomalyResult = ANOMALY_NORMAL;
static void determineAnomaly(bfloat16 value);

/** シャットダウン準備状態 */
volatile static bool isShutdownReady = true;
/** シャットダウン処理 */
volatile static bool isShutdownComplete = false;
static void aiShutdownInit(void);
static bool AIIsShutdownReady(void);
static bool AIIsShutdownComplete(void);

/** AIの割り込みハンドラ */
void AI_INT_IRQHandler(void);

void AIInit(void)
{
	__disable_irq();
	irq_ai_dis();	
	smpl_enablePeripheral(AI_PERI);
	
	//ai関連のパラメーターを設定。
	aiParameterInit();
	aiWeightInit();
	//不揮発から閾値等を読み取り設定する。
	ConfigDataGetFloatValue(EN_CONFIG_YELLOW_THRESHOLD,&aiConfig.YellowThresh);
	ConfigDataGetFloatValue(EN_CONFIG_RED_THRESHOLD,&aiConfig.RedThresh);
	// センサー初期化
	SensorInitialize();
	// センサーへのAIContext供給
	SensorSetAiContextForStoring(&aiContexts[0], &aiContexts[1]);
	//ブロック転送設定。
	ConfigDataGetUint8Value(EN_CONFIG_BLOCK_COM,&aiConfig.BlockTransmissionFlg);
	//推論コールバック初期化。
	AISetAIPredictCallBack(NULL);
	//学習回数初期化。
	learningCounter = 0;
	//chunkNo初期化。
	AIChunkNoReset();
	//シャットダウン状態の初期化。
	aiShutdownInit();

	irq_ai_setLevel(2);
	irq_ai_clearIRQ();
	//irq_ai_ena();
	__enable_irq();
}

static void aiParameterInit(void)
{
	ODL_Parameters aiParameters;
	float addParameter;
	uint8_t sensor;
	uint16_t power = 1;
	
	// AIの入力層データ数、センサーゲイン
	ConfigDataGetUint8Value(EN_CONFIG_USE_SENSOR, &sensor);
	if(sensor == SENSOR_IEPE)
	{
		ConfigDataGetUint16Value(EN_CONFIG_IEPE_SAMPLING_NUM, &aiConfig.SamplingDataCount);
		ConfigDataGetUint8Value(EN_CONFIG_IEPE_GAIN, &aiConfig.QFormatGain);
	}
	else
	{
		ConfigDataGetUint16Value(EN_CONFIG_MEMS_SAMPLING_NUM, &aiConfig.SamplingDataCount);
		ConfigDataGetUint8Value(EN_CONFIG_MEMS_GAIN, &aiConfig.QFormatGain);
	}
	
	//センサーデータ数が最大値より大きい場合は最大値に補正。
	if(aiConfig.SamplingDataCount > MAXIMUM_NUMBER_OF_SENSOR_DATA)
	{
		aiConfig.SamplingDataCount = MAXIMUM_NUMBER_OF_SENSOR_DATA;
	}
	
	//FFTを行うかどうか
	ConfigDataGetUint8Value(EN_CONFIG_FFT_ENABLE, &aiConfig.FftEnableFlg);
	if(aiConfig.FftEnableFlg)
	{
		//センサーデータ数のチェック
		//事前にセンサーデータ数チェックして512以下であることは保証されている。
		while(true)
		{
			power *= 2;
			if(aiConfig.SamplingDataCount <= power)
			{
				aiConfig.SamplingDataCount = power;
				break;
			}
		}
		//センサーデータ数の反映
		if(sensor == SENSOR_IEPE)
		{
			ConfigDataSetUint16Value(EN_CONFIG_IEPE_SAMPLING_NUM, aiConfig.SamplingDataCount);
		}
		else
		{
			ConfigDataSetUint16Value(EN_CONFIG_MEMS_SAMPLING_NUM, aiConfig.SamplingDataCount);
		}
		//FFTポイント数の設定
		aiConfig.FftPoints = aiConfig.SamplingDataCount;
		aiConfig.FftOutputCount = aiConfig.FftPoints / 2;
		//窓関数
		ConfigDataGetUint8Value(EN_CONFIG_WINDOW, &aiConfig.Window);
		//SKIP設定
		ConfigDataGetUint8Value(EN_CONFIG_FFT_SKIP, &aiConfig.SkipFFTDataForInputAI);
		//入力データ数
		aiParameters.inputSize = aiConfig.SamplingDataCount / 2;
		if(aiConfig.SkipFFTDataForInputAI)
		{ 
			//FFT出力データ数 - 1　より　SKIPするデータ数が多い場合は補正
			if(aiConfig.SkipFFTDataForInputAI >= (aiConfig.FftOutputCount - 1))
			{
				aiConfig.SkipFFTDataForInputAI = (uint8_t)(aiConfig.FftOutputCount -1);
				ConfigDataSetUint8Value(EN_CONFIG_FFT_SKIP, aiConfig.SkipFFTDataForInputAI);
			}
			aiParameters.inputSize -= aiConfig.SkipFFTDataForInputAI;
		}
	}
	else
	{	
		//FFTしない場合、センサーデータ数 == AI入力データ数
		//サイズオーバーならセンサーデータ数とAI入力データ数を全て256にする。
		if(aiConfig.SamplingDataCount > MAXIMUM_NUMBER_OF_AI_INPUT_DATA)
		{
			aiConfig.SamplingDataCount = MAXIMUM_NUMBER_OF_AI_INPUT_DATA;
			if(sensor == SENSOR_IEPE)
			{
				ConfigDataSetUint16Value(EN_CONFIG_IEPE_SAMPLING_NUM, aiConfig.SamplingDataCount);
			}
			else
			{
				ConfigDataSetUint16Value(EN_CONFIG_MEMS_SAMPLING_NUM, aiConfig.SamplingDataCount);
			}
		}
		aiParameters.inputSize = aiConfig.SamplingDataCount; 
	}		

	// AIの隠れ層ノード数 
	ConfigDataGetUint16Value(EN_CONFIG_HIDDEN_LAYER_NUM,&aiParameters.hiddenSize);
	aiConfig.AiHiddenSize = aiParameters.hiddenSize;
	// AIの出力層ノード数 
	aiParameters.outputSize = aiParameters.inputSize;
	aiConfig.AiOutputSize = aiParameters.outputSize;
	// AIの忘却率
	ConfigDataGetBfloat16Value(EN_CONFIG_FORGET_RATE,&aiParameters.forgettingFactor);
	// AIの活性化関数 
	ConfigDataGetUint8Value(EN_CONFIG_ACTIVATION_FUNC,&aiParameters.activationFunction);
	// AIの損失関数
	ConfigDataGetUint8Value(EN_CONFIG_LOSS_FUNC,&aiParameters.lossFunction);
	// AIの重みαの乱数シード値
	ConfigDataGetUint16Value(EN_CONFIG_WEIGHT_A_RANDOM_SEED,&aiParameters.seed);
	//AI追加パラメータ
	//scaleAlpha
	addParameter = 34.5388 / (double)aiParameters.inputSize;
	aiParameters.scaleAlpha = BfloatUtilityFloatToBfloat16(addParameter);
	//scaleBeta
	aiParameters.scaleGamma = 0;
	//leakrate
	//aiParameters.leakRate = 1;
	//AIに設定
	ODL_Initialize(AI_MODEL,&aiParameters);
	//fftサイズを設定
	fft_Init(aiConfig.FftPoints,aiConfig.Window);
}

static void aiWeightInit(void)
{
	//重みのリセット
	ODL_Reset(AI_MODEL);
	//重みデータ読込
	AIWeightImportWeightBetaAndPFromFramToAI(AI_MODEL,aiConfig.AiHiddenSize,aiConfig.AiOutputSize);
	wdt_clear();
}

static void aiShutdownInit(void)
{
	isShutdownReady = true;
	isShutdownComplete = false;
	ShutdownAdd(AIIsShutdownReady,AIIsShutdownComplete);
}

float AIGetFloatAnomalyValue(void)
{
	return BfloatUtilityBfloat16ToFloat(latestAiContext->LogInfo.Predict.Anomaly);
}

static void resetAnomalyValue(void)
{
	latestAiContext->LogInfo.Predict.Anomaly = 0;
}

static void determineAnomaly(bfloat16 value)
{
	float anomalyValue = BfloatUtilityBfloat16ToFloat(value);
	oldAnomalyResult = currentAnomalyResult;
 	if( anomalyValue <= aiConfig.YellowThresh)
	{
		currentAnomalyResult = ANOMALY_NORMAL;
	}
	else if( anomalyValue <= aiConfig.RedThresh)
	{
		currentAnomalyResult = ANOMALY_YELLOW;
	}
	else
	{
		currentAnomalyResult = ANOMALY_RED;
	}
}

ANOMALY AIGetCurrentAnomalyResult(void)
{
	return currentAnomalyResult;	
}

ANOMALY AIGetOldAnomalyResult(void)
{
	return oldAnomalyResult;
}

static void resetAnomalyResult(void)
{
	currentAnomalyResult = ANOMALY_NORMAL;
	oldAnomalyResult = ANOMALY_NORMAL;
}

void AIModelWeightReset(void)
{
	ODL_Reset(AI_MODEL);
}

void AILearningCounterReset(void)
{
	learningCounter = 0;
	snprintf(
		anomalyValueString,
		NUMBER_OF_CHARACTERS_OF_AI_LEARN_STRING,
		"                ");
}

char* AIGetFloatAnomalyValueString(void)
{
	return anomalyValueString;
}

void AIChunkNoReset(void)
{
	chunkNoCnt = 0;
}

inline static void manageChunkNo(void)
{
	if(chunkNoCnt < UINT16_MAX)chunkNoCnt++;
	else chunkNoCnt = 0;
}

static void sendBlockData(AI_CONTEXT* target)
{
	target->LogInfo.Predict.ChunkNo = chunkNoCnt;
	//E02エラー登録。
	if(HIGH_SPEED_COM_RESULT_BUSY == HighSpeedComBlockIsBusy(BLOCK1))
	{
		SystemErrorInsert(SYSTEM_ERROR_E02);
	}	
	HighSpeedComSendBlock(&target->LogInfo.Predict, sizeof(target->LogInfo.Predict) / 2, BLOCK1);
	manageChunkNo();
}


void AISetAIPredictCallBack(AIPredictCallBack predictCallBack)
{
	predictCallBackExe = predictCallBack;
}


inline static void predictCallBackExecute(void)
{
	if(predictCallBackExe != NULL) predictCallBackExe();
}

AI_CONTEXT* AIGetLatestAIContext(void)
{
	return latestAiContext;
}

AI_CONTEXT* AIGetFirstAIContext(void)
{
	return &aiContexts[0];
}

AI_CONTEXT* AIGetSecondAIContext(void)
{
	return &aiContexts[1];
}

static void convertAnomalyValueAndCountDuringLeaningToString(bfloat16 value,uint16_t counter)
{
	uint8_t size = 0;
	const uint8_t EMPTY = 0x20;
	memset(anomalyValueString,0,sizeof(anomalyValueString));
	snprintf(
		anomalyValueString,
		NUMBER_OF_CHARACTERS_OF_AI_LEARN_STRING,
		"%.2e %5d",
		(double)BfloatUtilityBfloat16ToFloat(value),
		counter);
	size = (uint8_t)strlen(anomalyValueString);
	while(!( size >= (NUMBER_OF_CHARACTERS_OF_AI_LEARN_STRING - 1)))
	{
		anomalyValueString[size] = EMPTY;
		size++;
	}
}

static bool AIIsShutdownReady(void)
{
	__disable_irq();
	if(isShutdownReady != true) 
	{
		__enable_irq();
		return false;
	}
	//シャットダウン準備
	//ソフトウェア割り込み禁止はできないので、NULLを入れてAI処理を実行させないようにする。
	SoftwareInterruptSetCallback(NULL);
	__enable_irq();
	//シャットダウン処理可へ
	isShutdownComplete = true;
	return true;
}

static bool AIIsShutdownComplete(void)
{
	if(isShutdownComplete != true) return false;
	//シャットダウン準備処理(同期的)
	//重みデータ保存
	AIWeightExportWeightBetaAndPFromAIToFram(AI_MODEL,aiConfig.AiHiddenSize,aiConfig.AiOutputSize);
	wdt_clear();
	//シャットダウン処理可へ
	return true;
}

void AILearnStart(void)
{
	aiSetLearnFunc();
	SensorStart();
}

void AILearnStop(void)
{
	__disable_irq();
	SensorStop();
	SoftwareInterruptSetCallback(NULL);
	__enable_irq();
}

void AIPredictStart(void)
{
	resetAnomalyValue();
	resetAnomalyResult();
	aiSetPredictFunc();
	SensorStart();
}

void AIPredictStop(void)
{
	__disable_irq();
	SensorStop();
	SoftwareInterruptSetCallback(NULL);
	__enable_irq();
}


static void aiSetLearnFunc(void)
{
	if(aiConfig.FftEnableFlg)
	{
		SoftwareInterruptSetCallback(aiLearnPredictTable[LEARN_PREDICT_CONFIG_SINGLE_SENSOR_WITH_FFT].Learn);
	}
	else
	{
		SoftwareInterruptSetCallback(aiLearnPredictTable[LEARN_PREDICT_CONFIG_SINGLE_SENSOR_WIHTOUT_FFT].Learn);
	}
}

static void aiSetPredictFunc(void)
{
	if(aiConfig.FftEnableFlg)
	{
		SoftwareInterruptSetCallback(aiLearnPredictTable[LEARN_PREDICT_CONFIG_SINGLE_SENSOR_WITH_FFT].Predict);	
	}
	else
	{
		SoftwareInterruptSetCallback(aiLearnPredictTable[LEARN_PREDICT_CONFIG_SINGLE_SENSOR_WIHTOUT_FFT].Predict);
	}	
}

static void aiLearnSingleSensorDataWithoutFft(void)
{
	AI_CONTEXT* aiContext = SensorGetAiContextDataStored();
	
	isShutdownReady = false;
	
	ODL_ToBfloat16(aiAccDataTemp,aiContext->LogInfo.InputData,aiConfig.QFormatGain,aiConfig.SamplingDataCount);
	learn(aiContext,aiAccDataTemp);
	
	SensorCompletedUsingAiContext();
	isShutdownReady = true;
}

static void aiLearnSingleSensorDataWithFft(void)
{
	AI_CONTEXT* aiContext = SensorGetAiContextDataStored();
	bfloat16* fftData;
	
	isShutdownReady = false;
	
	ODL_ToBfloat16(aiAccDataTemp,aiContext->LogInfo.InputData,aiConfig.QFormatGain,aiConfig.SamplingDataCount);
	fft(aiContext,aiAccDataTemp,&fftData);
	learn(aiContext,fftData);
	
	SensorCompletedUsingAiContext();
	isShutdownReady = true;
}

static void aiPredictFromSingleSensorWithoutFft(void)
{
	AI_CONTEXT* aiContext = SensorGetAiContextDataStored();

	isShutdownReady = false;
	
	ODL_ToBfloat16(aiAccDataTemp,aiContext->LogInfo.InputData,aiConfig.QFormatGain,aiConfig.SamplingDataCount);
	predict(aiContext,aiAccDataTemp);

	SensorCompletedUsingAiContext();
	isShutdownReady = true;	
}

static void aiPredictFromSingleSensorWithFft(void)
{
	//データ取得
	AI_CONTEXT* aiContext = SensorGetAiContextDataStored();
	bfloat16* fftData;
	
	isShutdownReady = false;

	ODL_ToBfloat16(aiAccDataTemp,aiContext->LogInfo.InputData,aiConfig.QFormatGain,aiConfig.SamplingDataCount);
	fft(aiContext,aiAccDataTemp,&fftData);
	predict(aiContext,fftData);
	
	SensorCompletedUsingAiContext();
	isShutdownReady = true;
}

/**
 * @brief fft設定、実行、結果取得
 *
 * @param aiContext バッファ
 * @param beforeFft fft処理したデータのバッファ
 * @param afterFft　fft処理したデータを格納するバッファのポインタ
 */
inline static void fft(AI_CONTEXT* aiContext,bfloat16* beforeFft,bfloat16** afterFft)
{
	fft_Start(beforeFft,aiConfig.FftPoints);
	while(fft_IsBusy())
	{
		wdt_clear();
	}
	*afterFft = aiContext->LogInfo.Predict.Fft;			
	fft_GetResult(*afterFft,aiConfig.FftOutputCount);
}

/**
 * @brief 学習
 *
 * @param aiContext バッファ
 * @param data 学習用データ
 */
inline static void learn(AI_CONTEXT* aiContext,bfloat16* data)
{
	ODL_StartTrain(AI_MODEL,&data[aiConfig.SkipFFTDataForInputAI],&data[aiConfig.SkipFFTDataForInputAI]);
	if(learningCounter < UINT16_MAX)learningCounter++;
	else learningCounter = 0;
	
	while(ODL_IsBusy())
	{
		wdt_clear();
	}
	ODL_StartPredict(AI_MODEL,&data[aiConfig.SkipFFTDataForInputAI],&data[aiConfig.SkipFFTDataForInputAI]);
	while(ODL_IsBusy())
	{
		wdt_clear();
	}
	aiContext->LogInfo.Predict.Anomaly = ODL_GetLoss(AI_MODEL);
	aiContext->LogSave = LOG_SAVE_NO;
	convertAnomalyValueAndCountDuringLeaningToString(aiContext->LogInfo.Predict.Anomaly,learningCounter);
	if(aiConfig.BlockTransmissionFlg)sendBlockData(aiContext);
	//context切替
	latestAiContext = aiContext;
}

/**
 * @brief 推論
 *
 * @param aiContext バッファ
 * @param data 推論用データ
 */
inline static void predict(AI_CONTEXT* aiContext, bfloat16* data)
{
	ODL_StartPredict(AI_MODEL,&data[aiConfig.SkipFFTDataForInputAI],&data[aiConfig.SkipFFTDataForInputAI]);
	while(ODL_IsBusy())
	{
		wdt_clear();
	}	
	aiContext->LogInfo.Predict.Anomaly = ODL_GetLoss(AI_MODEL);
	//異常判定
	determineAnomaly(aiContext->LogInfo.Predict.Anomaly);

	aiContext->LogSave = LOG_SAVE_NO;
	if(aiConfig.BlockTransmissionFlg)sendBlockData(aiContext);
	//context切替
	latestAiContext = aiContext;
	predictCallBackExecute();
}

void AI_INT_IRQHandler(void)
{
	irq_ai_clearIRQ();
}
