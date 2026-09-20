/*****************************************************************************
 * File: Sensor.c
 * Title: 複数あるセンサーのどれを使うかを、設定によって切り替えるためのラッパーモジュール
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file Sensor.c
 * @brief 複数あるセンサーのどれを使うかを、設定によって切り替えるためのラッパーモジュール
 * 
 * システムで使用するセンサーは全てこのモジュールを介してアクセスする
 */
 
#include <stdio.h>
#include <stdbool.h>
#include "Sensor.h"
#include "Kx134Acc.h"
#include "Iepe.h"
#include "ConfigData.h"
#include "SoftwareInterrupt.h"
/*############################################################################*/
/*#                                  Macro                                   #*/
/*############################################################################*/

/*############################################################################*/
/*#                                Variable                                  #*/
/*############################################################################*/

static uint8_t useMems;

/*############################################################################*/
/*#                               Prototype                                  #*/
/*############################################################################*/
static void setPendSV(void);
/*############################################################################*/
/*#                                  API                                     #*/
/*############################################################################*/

void SensorInitialize(void)
{
	ConfigDataGetUint8Value(EN_CONFIG_USE_SENSOR, &useMems);
	
	if(useMems)
	{
		Kx134AccInitialize();
	}
	else
	{
		IepeInitialize();
	}
	//センサーデータフル取得時に行わせたい処理をセット。
	SensorSetBufferFullCallback(setPendSV);
}

//センサーデータ取得時に行わせたい処理
static void setPendSV(void)
{
	//ソフトウェア割り込み起動
	SoftwareInterruptPendSVActivate();
}

void SensorSetAiContextForStoring(AI_CONTEXT* con1, AI_CONTEXT* con2)
{
	if(useMems)
	{
		Kx134AccSetAiContextForStoring(con1,con2);
	}
	else
	{
		IepeSetAiContextForStoring(con1,con2);
	}
}

// センサーによってデータが格納されたAiContextを取得する
AI_CONTEXT* SensorGetAiContextDataStored(void)
{
	AI_CONTEXT* ret;
	if(useMems)
	{
		ret = Kx134AccGetAiContextDataStored();
	}
	else
	{
		ret = IepeGetAiContextDataStored();
	}
	return ret;
}

// SensorGetAiContextDataStoredによって取得したAiContextを
// 使い終わった事をこのモジュールに知らせる
void SensorCompletedUsingAiContext(void)
{
	if(useMems)
	{
		Kx134AccCompletedUsingAiContext();
	}
	else
	{
		IepeCompletedUsingAiContext();
	}
}

// バッファサイズ分のデータが貯まった時に呼ばれるコールバック関数を設定する
void SensorSetBufferFullCallback(SensorBufferFullCallbackFunc func)
{

	if(useMems)
	{
		Kx134AccSetBufferFullCallback(func);
	}
	else
	{
		IepeSetBufferFullCallback(func);
	}
}

// センサー入力を開始する
void SensorStart(void)
{
	if(useMems)
	{
		Kx134AccStart();
	}
	else
	{
		IepeStart();
	}
}

// センサー入力を停止する
void SensorStop(void)
{
	if(useMems)
	{
		Kx134AccStop();
	}
	else
	{
		IepeStop();
	}
}

// センサー入力中か
bool SensorIsRunning(void)
{
	bool ret;
	if(useMems)
	{
		ret = Kx134AccIsRunning();
	}
	else
	{
		ret = IepeIsRunning();
	}
	return ret;
}

int SensorTest(void)
{
	if(IepeTest())return -1;
	return 0;
}

/*############################################################################*/
/*#                              Subroutine                                  #*/
/*############################################################################*/
