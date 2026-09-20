/*****************************************************************************
 * File: Iepe.c
 * Title: IEPEセンサーモジュール
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file Iepe.c
 * @brief IEPEセンサーモジュール
 */

#include <stdio.h>
#include <stdbool.h>
#include "Iepe.h"
#include "smpl_common.h"
#include "irq.h"
#include "SystemError.h"
#include "saAdc0.h"
#include "HighSpeedCom.h"
#include "ConfigData.h"

/*############################################################################*/
/*#                                  Macro                                   #*/
/*############################################################################*/
static const uint16_t TABLE_FREQ_TO_INTERVAL[] = 
{
	65535,	// 不正な設定
	65535,	// 不正な設定
	65535,	// 不正な設定
	65535,	// 不正な設定
	65535,	// 不正な設定
	65535,	// 不正な設定
	65535,	// 不正な設定
	29980,	// 100Hz
	14980,	// 200Hz
	7480,	// 400Hz
	3730,	// 800Hz
	1855,	// 1600Hz
	917,	// 3200Hz
	449,	// 6400Hz
	214,	// 12800Hz
	97,		// 25600Hz
};

#define MIN_FREQ_SETTING	(7)
#define MAX_FREQ_SETTING	(15)

/*############################################################################*/
/*#                                Variable                                  #*/
/*############################################################################*/
// センサーデータバッファ
static AI_CONTEXT* aiContext[2];
// 現在センサーデータを貯めているバッファ
static AI_CONTEXT* currentContext = NULL;
// 前回貯めたセンサーデータのバッファ
static AI_CONTEXT* previousContext = NULL;
// バッファサイズのデータが貯まった時のコールバック
static SensorBufferFullCallbackFunc bufferFullCallback = NULL;
// センサーデータ使用中フラグ
static bool previousContextBusyFlg = false; 
// リアルタイム通信設定フラグ
static uint8_t realtimeFlg = 0;

// 現在センサーデータをためているバッファの、次に入力するIndex
static uint16_t bufferIndex = 0;

/** サンプリングデータ数 */
static uint16_t samplingDataCount = 0;

/*############################################################################*/
/*#                               Prototype                                  #*/
/*############################################################################*/
void SAD_IRQHandler( void );
static bool samplingDataCountTest(void);
/*############################################################################*/
/*#                                  API                                     #*/
/*############################################################################*/

void IepeInitialize(void)
{
	initAdc_t  initAdc;
	enableAdcChannel_t enableChannel;
	uint8_t samplingFreq;

	ConfigDataGetUint8Value(EN_CONFIG_IEPE_SAMPLING_FREQUENCY,&samplingFreq);
	if(samplingFreq < MIN_FREQ_SETTING){ samplingFreq = MIN_FREQ_SETTING; }
	else if(MAX_FREQ_SETTING < samplingFreq){ samplingFreq = MAX_FREQ_SETTING; }

	/* turn on the peripheral */
	smpl_enablePeripheral(SAD0_PERI);

	/* disable interrupt */
	__disable_irq();
	irq_sad0_dis();

	/* set SA-ADC port (AIN0) */
	set_bit(PORT3->P3MOD0, (0 << 16));

	initAdc.discharge          = SAADC_SAINIT_DISCHARGE;
	initAdc.holdTime           = 0x03U;
	initAdc.clock              = SAADC_SACK_OSCLK_DIV16;
	initAdc.mode               = SAADC_SALP_CONTINUOUS;
	initAdc.limitInterrupt     = SAADC_SALMD_INSIDE_LIMIT;
	initAdc.limitMode          = SAADC_SALEN_DISABLE;
	initAdc.ampStabilityTime   = 0x02U;
	initAdc.interruptMode      = SAADC_SADIMD0_ALL_CH;
	initAdc.interruptLimitMode = SAADC_SADIMD1_LIMIT_MATCH;
	initAdc.interval           = TABLE_FREQ_TO_INTERVAL[samplingFreq];
	initAdc.channelSync        = SAADC_SYNC_NORMAL;
	saAdc0_init( &initAdc );
	
	enableChannel.ch0  = SAADC_RUN;
	enableChannel.ch1  = SAADC_OFF;
	enableChannel.ch2  = SAADC_OFF;
	enableChannel.ch3  = SAADC_OFF;
	enableChannel.ch4  = SAADC_OFF;
	enableChannel.ch5  = SAADC_OFF;
	enableChannel.ch6  = SAADC_OFF;
	enableChannel.ch7  = SAADC_OFF;
	enableChannel.ch8  = SAADC_OFF;
	enableChannel.ch9  = SAADC_OFF;
	enableChannel.ch10 = SAADC_OFF;
	enableChannel.ch11 = SAADC_OFF;
	saAdc0_setEnableChannel( &enableChannel );
	
	//サンプリングデータ数の設定
	ConfigDataGetUint16Value(EN_CONFIG_IEPE_SAMPLING_NUM, &samplingDataCount);
	//リアルタイム転送の設定
	ConfigDataGetUint8Value(EN_CONFIG_REALTIME_COM,&realtimeFlg);
	//割り込み優先度
	irq_sad0_setLevel(2);
	/* enable interrupt */
	irq_sad0_clearIRQ();
	irq_sad0_ena();
	__enable_irq();
}

void IepeSetAiContextForStoring(AI_CONTEXT* con1, AI_CONTEXT* con2)
{
	aiContext[0] = con1;
	aiContext[1] = con2;
	currentContext = con1;
}

// センサーによってデータが格納されたAiContextを取得する
AI_CONTEXT* IepeGetAiContextDataStored(void)
{
	previousContextBusyFlg = true;
	return previousContext;
}

// IepeGetAiContextDataStoredによって取得したAiContextを
// 使い終わった事をこのモジュールに知らせる
void IepeCompletedUsingAiContext(void)
{
	previousContextBusyFlg = false;
}

// バッファサイズ分のデータが貯まった時に呼ばれるコールバック関数を設定する
void IepeSetBufferFullCallback(SensorBufferFullCallbackFunc func)
{
	bufferFullCallback = func;
}

// センサー入力を開始する
void IepeStart(void)
{
	bufferIndex = 0;
	saAdc0_start();
}

// センサー入力を停止する
void IepeStop(void)
{
	saAdc0_stop();
}

// センサー入力中か
bool IepeIsRunning(void)
{
	return saAdc0_getRunning();
}

// 割込みハンドラ
void SAD_IRQHandler( void )
{
	static bool toggleFlg = true;
	volatile uint16_t ui16Data = 0;
	volatile int16_t i16Data = 0;
	
	ui16Data = (uint16_t)(saAdc0_getResult0() & 0xFFF0);	// 左詰め12bitのまま使ってフルスケール16bitとして扱う
	i16Data = (int16_t)(0x7FFF - ui16Data);		// ±反転と符号付きに変換
	if(realtimeFlg)HighSpeedComInputRtData((uint16_t)i16Data);
	currentContext->LogInfo.InputData[bufferIndex] = i16Data;
	
	if(bufferIndex >= (samplingDataCount - 1))
	{
		bufferIndex = 0;
		//E01エラー登録。
		if(previousContextBusyFlg)
		{
			SystemErrorInsert(SYSTEM_ERROR_E01);
		}
		if(toggleFlg == true)
		{
			currentContext = aiContext[1];
			previousContext = aiContext[0];
			toggleFlg = false;
		}
		else if(toggleFlg == false)
		{
			currentContext = aiContext[0];
			previousContext = aiContext[1];
			toggleFlg = true;
		}
		if(bufferFullCallback != NULL) bufferFullCallback();
	}
	else
	{
		bufferIndex++;
	}
}



int IepeTest(void)
{
	bool countFlg = false;
	uint8_t configRealtime = 1;
	if(realtimeFlg != 0)return -1;
	//初期化が正しいか
	ConfigDataSetUint8Value(EN_CONFIG_REALTIME_COM,configRealtime);
	IepeInitialize();
	if(realtimeFlg != configRealtime) return -1;
	
	//サンプリングデータ数の処理が正しいか
	ConfigDataSetUint16Value(EN_CONFIG_IEPE_SAMPLING_NUM, 256);
	ConfigDataGetUint16Value(EN_CONFIG_IEPE_SAMPLING_NUM, &samplingDataCount);
	while(!countFlg) 
	{
		countFlg = samplingDataCountTest();
	}
	if(bufferIndex != (samplingDataCount -1)) return -1;
	return 0;
}

/*############################################################################*/
/*#                              Subroutine                                  #*/
/*############################################################################*/
static bool samplingDataCountTest(void)
{
	if(bufferIndex >= (samplingDataCount - 1))
	{
		return true;
	}
	else 
	{
		bufferIndex++;
		return false;
	}
}

