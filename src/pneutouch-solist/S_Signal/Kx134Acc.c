/*****************************************************************************
 * File: Kx134Acc.c
 * Title: MEMS加速度センサーKx134-1211を制御するモジュール
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file Kx134Acc.c
 * @brief MEMS加速度センサーKx134-1211を制御するモジュール
 */

#include <stdio.h>
#include <stdint.h>
#include "ssiof_common.h"
#include "Kx134Spi.h"
#include "ssiof0.h"
#include "wdt.h"
#include "irq.h"
#include "Kx134Acc.h"
#include "smpl_common_led.h"
#include "SoftwareInterrupt.h"
#include "ConfigData.h"
#include "HighSpeedCom.h"
#include "TimeControl.h"
#include "SystemError.h"

/*############################################################################*/
/*#                                  Macro                                   #*/
/*############################################################################*/

/**< ダミーデータ */
#define DUMMY_DATA						(0xff)
/**< Readコマンド */
#define READ_COMMAND					(0x80)
/**< レジスタ */
#define REG_XOUT_L						(0x08)
#define REG_XOUT_H						(0x09)
#define REG_YOUT_L						(0x0A)
#define	REG_YOUT_H						(0x0B)
#define REG_ZOUT_L						(0x0C)
#define REG_ZOUT_H						(0x0D)
/**< モード設定 */
#define REG_CNTL1						(0x1B)
#define STANDBY_MODE					(0x00)
#define OPERATING_MODE					(0x80)
#define LOW_POEWER_MODE					(0x00)
#define HIGH_PEFORMANCE_MODE			(0x40)
#define DATA_REDAY_ENGINE_ENABLE		(0x20)
#define GSEL							(0x00)
#define BEFORE_STARTUP_CNTL1			(STANDBY_MODE | HIGH_PEFORMANCE_MODE  | GSEL)
#define DEFAULT_CNTL1					(BEFORE_STARTUP_CNTL1 | OPERATING_MODE | DATA_REDAY_ENGINE_ENABLE) 
/**< LPFとサンプリング周波数の設定 */
#define REG_ODCNTL						(0x21)
/**< パラメータ設定値を読み込むため、0にする。 */
#define DEFAULT_ODCNTL					(0)
/**< 割り込み設定 */
#define REG_INC1						(0x22)
#define PW1_NO_USE_PULSE				(0xC0)
#define IEN1_INTERRUPT_ENABLED			(0x20)
#define IEA1_ACTIVE_LOW					(0x00)
/**< INT_RELを読み出して割込みステータスクリア */
#define IEL1_READ_CLEAR					(0x00)
#define IEL1_PULSE_CREATE				(0x08)
#define SPI3E_DISABLED					(0x00)
#define DEFAULT_INC1					( PW1_NO_USE_PULSE | IEN1_INTERRUPT_ENABLED | IEA1_ACTIVE_LOW | IEL1_READ_CLEAR | SPI3E_DISABLED )
//#define DEFAULT_INC1					(0x30)

/**< 割り込みソースレジスタのステータスをクリアする。*/
/**< アプリではDRDY割り込みを使っており、その割り込みステータスは軸データ読み出しまたはINTREL読出しでクリアされる。*/
#define REG_INT_REL						(0x1A)

/**< 割り込みの種類の設定 */
#define REG_INC4						(0x25)
#define DATA_READY_INTERRUPT			(0x10)
#define DEFAULT_INC4					(DATA_READY_INTERRUPT)

//ID取得
#define REG_WHO_AM_I					(0x13)
/**< スタートアップタイム */
#define START_UP_TIME_300MS				(300)
#define START_UP_TIME_1000MS			(1000)
/**< パワーアップタイム */
#define POWER_UP_TIME					(50)


/**< バッファリセット */
#define RESET_BUFFER					(0)
/** センサー初期化コマンドサイズ */
#define INITIALIZE_COMMAND_SIZE			(1)
/** 1軸データ送受信用コマンドサイズ */
#define UNIAXIAL_BUFFER_SIZE			(2)
/** 3軸データ送受信用コマンドサイズ */
#define TRIAXIAL_BUFFER_SIZE			(4)
/** MEMSセンサー接続 */
#define MEMS_CONNECTED					(0x46)

/**
 * @brief センサーの軸の列挙型
 */
typedef enum
{
	AXIS_TYPE_X = 0,					/**< X軸 */
	AXIS_TYPE_Y = 1,					/**< Y軸 */
	AXIS_TYPE_Z = 2,					/**< Z軸 */
	AXIS_TYPE_TRIAXIZAL = 3
}AXIS_TYPE;


/*############################################################################*/
/*#                                Variable                                  #*/
/*############################################################################*/

/** 割り込み関数ポインタ */
typedef void (*Kx134AccInterruptEXI)(void);
/** SPI通信受信バッファ */
static uint16_t rxBuffer[TRIAXIAL_BUFFER_SIZE]; 
/** センサーデータバッファ */
static AI_CONTEXT* aiBuffer[2];
/** 現在センサーデータを貯めているバッファ */
static AI_CONTEXT* accCurrentData = NULL;
/** 前回貯めたセンサーデータのバッファ */
static AI_CONTEXT* accPreviousData = NULL;
/** バッファーのインデックス */
static uint16_t bufferIndex = RESET_BUFFER;
/** センサーデータenableフラグ　*/
static bool sensorEnableFlg = false;
/** センサー起動後初回読込かどうか　*/
static bool sensorAfterStartupFlg = true;
/** バッファサイズのデータが貯まった時のコールバック */
static SensorBufferFullCallbackFunc bufferFullCallback = NULL;
/** 割り込みコールバック関数ポインタ */
static Kx134AccInterruptEXI accInterruptCallback = NULL;
/** SPI同期待ち用フラグ */
volatile static bool spiTransferEndFlag = false;
/** センサーデータ使用中フラグ */
static bool previousDataBusyFlg = false; 

/** LPFの設定 */
static uint8_t lpf = 0;
/** サンプリング周波数の設定 */
static uint8_t sampling = 0;
/** リアルタイム通信設定フラグ */
static uint8_t realtimeFlg = 0;
/** サンプリングデータ数 */
static uint16_t samplingDataCount = 0;
 
/** 1軸データ取得時コマンド */
static uint16_t txUniaxialAccData[UNIAXIAL_BUFFER_SIZE] = 
{ 
	( ( ( 0 | READ_COMMAND ) << 8 ) | DUMMY_DATA ),( ( DUMMY_DATA << 8 ) | DUMMY_DATA )
};

/** 3軸データ取得コマンド */
static uint16_t txTriaxialAccData[TRIAXIAL_BUFFER_SIZE] =	
{ 
	( ( ( REG_XOUT_L | READ_COMMAND ) << 8 ) | DUMMY_DATA ),( ( DUMMY_DATA << 8 ) | DUMMY_DATA ),
	( ( DUMMY_DATA << 8 ) | DUMMY_DATA ), 					( ( DUMMY_DATA << 8 ) | DUMMY_DATA )
};

/*############################################################################*/
/*#                               Prototype                                  #*/
/*############################################################################*/
static void sendReceive(uint16_t* txData, uint32_t size, cbfSsiof_t func);
static void applySensorSettings(void);
static void spiTansferEndInterrupt(uint32_t dataCnt, uint16_t errStatus);
static void waitSpiTransferEnd(void);
static void accInterruptInc1Uniaxial(void);
static void accInterruptInc1Triaxial(void);
static void setUniaxialAcc(void);
static void setTriaxialAcc(void);
static void resetBuffer(void);
static void setConfig(void);
static void setAxisConfig(void);
static void initVariable(void);
/*############################################################################*/
/*#                                  API                                     #*/
/*############################################################################*/

void Kx134AccInitialize(void)
{
	Kx134SpiPeripheralInit();
	initVariable();
	//設定読み込み後反映
	setConfig();
	//センサーの起動待ち Max1300ms
	TimeControlInit();
	TimeControlDelayMs(START_UP_TIME_1000MS);
	TimeControlDelayMs(START_UP_TIME_300MS);
}

void Kx134AccSetBufferFullCallback(SensorBufferFullCallbackFunc exe)
{
	bufferFullCallback = exe;
}

void Kx134AccStart(void)
{
	//割り込み許可
	Kx134SpiSensorDataReadyInterruptUnuse();
	Kx134SpiInterruptUnuse();
	Kx134SpiInterruptUse();
	
	applySensorSettings();
	
	resetBuffer();
	//センサー有効化
	sensorEnableFlg = true;
	sensorAfterStartupFlg = true;
	//センサーデータ読み取り可能割り込み開始(割込み不使用の場合はここでそのAPIを呼び出す)
	Kx134SpiInterruptUnuse();
	Kx134SpiSensorDataReadyInterruptUse();

}

void Kx134AccStop(void)
{
	sensorEnableFlg = false;
	Kx134SpiInterruptUnuse();
	Kx134SpiSensorDataReadyInterruptUnuse();
}

bool Kx134AccIsRunning(void)
{
	return sensorEnableFlg;
}

void Kx134AccSetAiContextForStoring(AI_CONTEXT* con1, AI_CONTEXT* con2)
{
	aiBuffer[0] = con1;
	aiBuffer[1] = con2;
	accCurrentData = con1;
}

AI_CONTEXT* Kx134AccGetAiContextDataStored(void)
{
	previousDataBusyFlg = true;
	return accPreviousData;
}

void Kx134AccCompletedUsingAiContext(void)
{
	previousDataBusyFlg = false;
}
 

/*############################################################################*/
/*#                              Subroutine                                  #*/
/*############################################################################*/

/**
 * @brief SPI送受信
 *
 * @param txData 送信データ
 * @param size 送信データのサイズ
 * @param func spi通信後の転送終了割り込みで行うコールバック。無い場合はNULL
 */
inline static void sendReceive(uint16_t* txData,uint32_t size,cbfSsiof_t func)
{
	//clr transfer_end_flg
	spiTransferEndFlag = false;
	//fifo clear
	Kx134SpiClearFifo();
	Kx134SpiStart(rxBuffer, txData, size, func);
}
 
/**
 * @brief 転送終了割り込み時コールバック
 */
static void spiTansferEndInterrupt(uint32_t dataCnt, uint16_t errStatus)
{
	spiTransferEndFlag = true;
}
 
/**
 * @brief SPI送受信同期待ち
 *
 * @note Kx134センサー初期化時のみ使用
 */
static void waitSpiTransferEnd(void)
{
	while(spiTransferEndFlag == false) 
	{
		wdt_clear();
	}
}

/**
 * @brief センサーを初期化する。
 */
static void applySensorSettings(void)
{
	//read device_id
	uint16_t txDataWhoAmI = ( ( ( REG_WHO_AM_I | READ_COMMAND ) << 8 ) | DUMMY_DATA );
	//first
	uint16_t txDataCntl1First = ( ( REG_CNTL1 << 8 ) | 0); 
	uint16_t txDataInc1 = ( ( REG_INC1 << 8 ) | DEFAULT_INC1 );
	uint16_t txDataInc4 = ( ( REG_INC4 << 8) | DEFAULT_INC4 );
	uint16_t txDataOdcntl = ( ( REG_ODCNTL << 8 ) | DEFAULT_ODCNTL );
	//last
	uint16_t txDataCntl1Second = ( ( REG_CNTL1 << 8) | DEFAULT_CNTL1 );
	
	//LPFとサンプリング周波数の設定を反映。
	txDataOdcntl |=  ( ( lpf << 6 ) | sampling);
		
	//Confrim WHO_AM_I
	sendReceive(&txDataWhoAmI,INITIALIZE_COMMAND_SIZE,spiTansferEndInterrupt);
	waitSpiTransferEnd();
	
	//最初にSPIでデバイスIDを読み取ると、0x81というデータが返ってくる。
	//本来のデバイスIDは0x46。二回目の読み取りでは0x46が返ってくる。
	sendReceive(&txDataWhoAmI,INITIALIZE_COMMAND_SIZE,spiTansferEndInterrupt);
	waitSpiTransferEnd();
	
	//E03エラー登録。memsセンサー未接続ならエラー
	if(rxBuffer[0] == MEMS_CONNECTED)
	{
		SystemErrorRemoveAt(SYSTEM_ERROR_E03);
	}
	else
	{
		SystemErrorInsert(SYSTEM_ERROR_E03);
	}
	
	//Set CNTL1 first
	sendReceive(&txDataCntl1First,INITIALIZE_COMMAND_SIZE,spiTansferEndInterrupt);
	waitSpiTransferEnd();

	//INC1
	sendReceive(&txDataInc1,INITIALIZE_COMMAND_SIZE,spiTansferEndInterrupt);
	waitSpiTransferEnd();
	
	//INC4
	sendReceive(&txDataInc4,INITIALIZE_COMMAND_SIZE,spiTansferEndInterrupt);
	waitSpiTransferEnd();
	
	//ODCTL
	sendReceive(&txDataOdcntl,INITIALIZE_COMMAND_SIZE,spiTansferEndInterrupt);
	waitSpiTransferEnd();

	//CNTL1 last
	sendReceive(&txDataCntl1Second,INITIALIZE_COMMAND_SIZE,spiTansferEndInterrupt);
	waitSpiTransferEnd();

	//設定反映待ち Max50ms
	TimeControlDelayMs(POWER_UP_TIME);
}

/**
 * @brief 割り込みコールバック(1軸)
 *
 * @note 処理高速化のため割り込みを使わない。
 */
static void accInterruptInc1Uniaxial(void)
{
	if(!sensorAfterStartupFlg)
	{
		Kx134SpiReadFifoFourByte();
		sendReceive(txUniaxialAccData,UNIAXIAL_BUFFER_SIZE,NULL);
		setUniaxialAcc();
	}
	else
	{
		//センサー起動後最初の割り込みではデータを取りに行くのみ
		sendReceive(txUniaxialAccData,UNIAXIAL_BUFFER_SIZE,NULL);
		sensorAfterStartupFlg = false;
	}
}
 
/**
 * @brief 割り込みコールバック(3軸)
 *
 * @note 処理高速化のため割り込みを使わない。
 */
static void accInterruptInc1Triaxial(void)
{
	if(!sensorAfterStartupFlg)
	{
		Kx134SpiReadFifoEightByte();
		sendReceive(txTriaxialAccData,TRIAXIAL_BUFFER_SIZE,NULL);
		setTriaxialAcc();
	}
	else
	{
		sendReceive(txTriaxialAccData,TRIAXIAL_BUFFER_SIZE,NULL);
		sensorAfterStartupFlg = false;
	}
}
 

/**
 * @brief 1軸データセット
 */
inline static void setUniaxialAcc(void)
{
	static bool toggleFlg = true;
	volatile uint16_t ui16Data = 0;
	volatile int16_t i16Data = 0;
	
	ui16Data = ( (rxBuffer[1] & 0xff00 ) | ( rxBuffer[0] & 0x00ff ) );
	if(realtimeFlg)HighSpeedComInputRtData((uint16_t)ui16Data);
	i16Data = (int16_t)ui16Data;
	accCurrentData->LogInfo.InputData[bufferIndex] = i16Data;
	
	if(bufferIndex >= (samplingDataCount - 1))
	{
		bufferIndex = RESET_BUFFER;
		//E01エラー登録。
		if(previousDataBusyFlg)
		{
			SystemErrorInsert(SYSTEM_ERROR_E01);
		}
		if(toggleFlg == true)
		{
			accCurrentData = aiBuffer[1];
			accPreviousData = aiBuffer[0];
			toggleFlg = false;
		}
		else if(toggleFlg == false)
		{
			accCurrentData = aiBuffer[0];
			accPreviousData = aiBuffer[1];
			toggleFlg = true;
		}
		if(bufferFullCallback != NULL) bufferFullCallback();
	}
	else
	{
		bufferIndex++;
	}
}


/**
 * @brief 3軸データセット
 */
inline static void setTriaxialAcc(void)
{
	//static bool toggleFlg = true;
	static int16_t data[3] = {0, 0, 0};
	
	data[0] = (int16_t) ( (rxBuffer[1] & 0xff00 ) | ( rxBuffer[0] & 0x00ff ) );
	data[1] = (int16_t) ( (rxBuffer[2] & 0xff00 ) | ( rxBuffer[1] & 0x00ff ) );
	data[2] = (int16_t) ( (rxBuffer[3] & 0xff00 ) | ( rxBuffer[2] & 0x00ff ) );

	/* 未使用
	accCurrentData->LogInfo.InputData[bufferIndex] = data[0];
	if(realtimeFlg)HighSpeedComInputRtData((uint16_t)accCurrentData->LogInfo.InputData[bufferIndex]);
	
	if(bufferIndex >= (samplingDataCount - 1))
	{
		bufferIndex = RESET_BUFFER;
		//E01エラー登録。
		if(previousDataBusyFlg)
		{
			SystemErrorInsert(SYSTEM_ERROR_E01);
		}
		if(toggleFlg == true)
		{
			accCurrentData = aiBuffer[1];
			accPreviousData = aiBuffer[0];
			toggleFlg = false;
		}
		else if(toggleFlg == false)
		{
			accCurrentData = aiBuffer[0];
			accPreviousData = aiBuffer[1];
			toggleFlg = true;
		}
		if(bufferFullCallback != NULL) bufferFullCallback();
	}
	else
	{
		bufferIndex++;
	}
	*/
}

/**
 * @brief センサーデータのバッファをクリア。
 */
static void resetBuffer(void)
{
	bufferIndex = RESET_BUFFER;
}

/**
 * @brief このモジュールに関する設定を行う。
 */
static void setConfig(void)
{
	//lpf
	ConfigDataGetUint8Value(EN_CONFIG_MEMS_LPF,&lpf);
	//サンプリング周波数
	ConfigDataGetUint8Value(EN_CONFIG_MEMS_SAMPLING_FREQUENCY,&sampling);
	//軸と使用する関数
	setAxisConfig();
	//サンプリングデータ数
	ConfigDataGetUint16Value(EN_CONFIG_MEMS_SAMPLING_NUM, &samplingDataCount);
	//リアルタイム転送の設定
	ConfigDataGetUint8Value(EN_CONFIG_REALTIME_COM,&realtimeFlg);
}

/**
 * @brief 初期化(軸の設定)。
 */
static void setAxisConfig(void)
{
	uint8_t axis;
	ConfigDataGetUint8Value(EN_CONFIG_MEMS_DATA_KIND,&axis);
	//センサーデータ取得割り込み(irq)で実行する関数の設定
	switch(axis)
	{
		case AXIS_TYPE_X:
			txUniaxialAccData[0] |= ( REG_XOUT_L << 8 );
			accInterruptCallback = accInterruptInc1Uniaxial;
			break;
		case AXIS_TYPE_Y:
			txUniaxialAccData[0] |= ( REG_YOUT_L << 8 );
			accInterruptCallback = accInterruptInc1Uniaxial;
			break;
		case AXIS_TYPE_Z:
			txUniaxialAccData[0] |= ( REG_ZOUT_L << 8 );
			accInterruptCallback = accInterruptInc1Uniaxial;
			break;
		case AXIS_TYPE_TRIAXIZAL:
			accInterruptCallback = accInterruptInc1Triaxial;
			break;
	}
	//irq割り込みの設定
	Kx134SpiSetSensorDataReadyInterrupt(accInterruptCallback);
}

/**
 * @brief 変数の初期化
 */
static void initVariable(void)
{
	accCurrentData = NULL;
	accPreviousData = NULL;
	bufferFullCallback = NULL;
	accInterruptCallback = NULL;
	
	bufferIndex = RESET_BUFFER;
	sensorEnableFlg = false;
	sensorAfterStartupFlg = true;
	spiTransferEndFlag = false;
	previousDataBusyFlg = false; 
}
