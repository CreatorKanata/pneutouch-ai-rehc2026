/*****************************************************************************
 * File: PowerMonitoringAnalogInput.c
 * Title: 電源電圧監視アナログ入力を使用する。
 * LastUpdated: 2025.06.16
******************************************************************************/

/**
 * @file PowerMonitoringAnalogInput.c
 * @brief 電源電圧監視アナログ入力を使用する。
 */

#include "PowerMonitoringAnalogInput.h"
#include "Output.h"
#include "smpl_common.h"
#include "saAdc_common.h"
#include "saAdc1.h"
#include "irq.h"
#include "wdt.h"
#include "smpl_common_led.h"
#include "float.h"
#include "math.h"
#include "TimeControl.h"
//P31 
/**< IOの設定 */
#define INPUT_CONTROL_PORT_CONFIG			(0x02 << 8)
#define INPUT_CONTROL_PORT_VALUE			(0x02)
#define TEST_INIT_VALUE						(0x000000FF)
#define TEST_VALUE_INPUT_CONTROL_PORT_OFF	(0x00000000)
#define TEST_VALUE_INPUT_CONTROL_PORT_ON	(0x00000002)


/**< ADの設定 */
#define ANALOG_INPUT_CONFIG					(0x00 << 24)
#define ONESHOT_INTERVAL					(0)

/**< TPS22919がONになるまでの時間 */
#define WAIT_2MS							(2)
/**< 1以上の数値で除算 */
#define MONITORING_COUNTS					(3)
/**< 分解能 */
#define NUMBER_OF_DIVISIONS					(4096)
/**< 最大電源電圧値 */
#define POWER_MAX							(6.6f)

volatile static bool isADInterruptOccurred = false;
volatile static float currentVoltageValue = 0.0f;

/**
 * @brief アナログ入力機能を初期化する。
 */
static void analogInit(void);

/**
 * @brief 電源電圧値を計算する。
 *
 * @return float 電源電圧値。
 */
inline static float calculateVoltageValue(void);

/**
 * @brief ADCから値を取得する。
 *
 * @return uint32_t AD変換値。
 */
inline static uint32_t getAnalogVoltageValue(void);

void SAD1_IRQHandler(void);

/**
 * @brief TPS22919をONにする。
 */
static void inputControlPortOn(void);

/**
 * @brief TPS22919をOFFにする。
 */
static void inputControlPortOff(void);


void PowerMonitoringAnalogInputInit(void)
{
	//TPS22919をOFFにする。
	set_bit(PORT3->P3MOD0, INPUT_CONTROL_PORT_CONFIG);
	inputControlPortOff();
	
	//ad初期化　割り込み優先度0
	analogInit();
	//アナログ値をMONITORING_COUNTS分入れる。
	for(int i = 0; i < MONITORING_COUNTS; i++) PowerMonitoringAnalogInputGetVoltageValue();
}

static void analogInit(void)
{
	initAdc_t initStatus;
	enableAdcChannel_t channelStatus;
	
	smpl_enablePeripheral(SAD1_PERI);
	
	__disable_irq();
	irq_sad1_dis();

	set_bit(PORT3->P3MOD0, ANALOG_INPUT_CONFIG);
	
	initStatus.discharge			= SAADC_SAINIT_DISCHARGE;
	initStatus.holdTime				= 0x03;
	initStatus.clock				= SAADC_SACK_OSCLK_DIV16;
	initStatus.mode					= SAADC_SALP_ONESHOT; //連続変換時変更
	initStatus.limitInterrupt		= SAADC_SALMD_INSIDE_LIMIT;
	initStatus.limitMode			= SAADC_SALEN_DISABLE;
	initStatus.ampStabilityTime		= 0x02;
	initStatus.interruptMode		= SAADC_SADIMD0_ALL_CH;
	initStatus.interruptLimitMode	= SAADC_SADIMD1_LIMIT_MATCH;
	initStatus.interval				= ONESHOT_INTERVAL;//連続変換時変更
	initStatus.channelSync			= SAADC_SYNC_NORMAL;
	saAdc1_init(&initStatus);
	
	channelStatus.ch0	= SAADC_OFF;
	channelStatus.ch1	= SAADC_RUN;
	channelStatus.ch2	= SAADC_OFF;
	channelStatus.ch3	= SAADC_OFF;
	channelStatus.ch4	= SAADC_OFF;
	channelStatus.ch5	= SAADC_OFF;
	channelStatus.ch6	= SAADC_OFF;
	channelStatus.ch7	= SAADC_OFF;
	channelStatus.ch8	= SAADC_OFF;
	channelStatus.ch9	= SAADC_OFF;
	channelStatus.ch10	= SAADC_OFF;
	channelStatus.ch11	= SAADC_OFF;
	saAdc1_setEnableChannel(&channelStatus);
	
	isADInterruptOccurred = false;
	currentVoltageValue = 0.0f;
	//割り込み許可
	irq_sad1_setLevel(0);
	irq_sad1_clearIRQ();
	irq_sad1_ena();
	__enable_irq();
}



float PowerMonitoringAnalogInputGetVoltageValue(void)
{
	float voltageValue = 0;
	
	//電圧監視ゲートをOn
	inputControlPortOn();
	
	//2ms待つ
	TimeControlDelayMs(WAIT_2MS);
	
	//電源電圧値を取得
	voltageValue = calculateVoltageValue();
	
	//電圧監視ゲートをoff
	inputControlPortOff();
	
	//現在の値を更新
	currentVoltageValue = voltageValue;
	return voltageValue;
}

float PowerMonitoringAnalogInputCheckCurrentVoltageValue(void)
{
	return currentVoltageValue;
}

inline static float calculateVoltageValue(void)
{
	static uint32_t powerAnalogInputBuffer[MONITORING_COUNTS];
	static uint8_t bufferCounter = 0;
	uint32_t adcValue = 0;
	uint32_t adcSum = 0;
	float voltageValue = 0;
	
	//AD × MONITORING_COUNTS
	for(int i = 0; i < MONITORING_COUNTS; i++) 
	{
		adcValue += getAnalogVoltageValue();
	}

	if(bufferCounter < MONITORING_COUNTS) 
	{
		powerAnalogInputBuffer[bufferCounter] = adcValue / MONITORING_COUNTS;
	}
	
	if(++bufferCounter >= MONITORING_COUNTS) 
	{
		bufferCounter = 0;
	}
	
	//add × MONITORING_COUNTS
	for(int i = 0; i < MONITORING_COUNTS; i++) 
	{
		adcSum += powerAnalogInputBuffer[i];
	}
	
	//平均して電圧変換
	voltageValue = (float)adcSum * (POWER_MAX / (NUMBER_OF_DIVISIONS * MONITORING_COUNTS));

	return voltageValue;
}

inline static uint32_t getAnalogVoltageValue(void)
{
	uint32_t value;
	//AD変換開始
	saAdc1_start();
	while(!isADInterruptOccurred) wdt_clear();
	//AD終了
	saAdc1_stop();
	isADInterruptOccurred = false;
	//12bitAD右詰 左20bit不使用
	value = (saAdc1_getResult1() >> 4);
	return value;
}

void SAD1_IRQHandler(void)
{
	isADInterruptOccurred = true;
}

static void inputControlPortOn(void)
{
	OutputOnUInt32(&(PORT3->P3DO),INPUT_CONTROL_PORT_VALUE);
}

static void inputControlPortOff(void)
{
	OutputOffUInt32(&(PORT3->P3DO),INPUT_CONTROL_PORT_VALUE);
}


int PowerMonitoringAnalogInputTest(void)
{
	volatile uint32_t adTestValue = 0;
	volatile float powerTestValue = 0;
	
	//初期化
	PowerMonitoringAnalogInputInit();
	if((read_reg32(PORT3->P3MOD0) & 0x0000ff00) != INPUT_CONTROL_PORT_CONFIG) return -1;
	if((read_reg32(PORT3->P3MOD0) & 0xff000000) != ANALOG_INPUT_CONFIG) return -1;
	if(isADInterruptOccurred != false) return -1;
	if(fabsf(currentVoltageValue - 0.0f) < FLT_EPSILON) return -1;
	//制御IO
	OutputOffUInt32(&(PORT3->P3DO),TEST_INIT_VALUE);
	if(read_reg32(PORT3->P3DO) != TEST_VALUE_INPUT_CONTROL_PORT_OFF) return -1;
	inputControlPortOn();
	if(read_reg32(PORT3->P3DO) != TEST_VALUE_INPUT_CONTROL_PORT_ON) return -1;
	inputControlPortOn();
	if(read_reg32(PORT3->P3DO) != TEST_VALUE_INPUT_CONTROL_PORT_ON) return -1;
	inputControlPortOff();
	if(read_reg32(PORT3->P3DO) != TEST_VALUE_INPUT_CONTROL_PORT_OFF) return -1;
	inputControlPortOff();
	if(read_reg32(PORT3->P3DO) != TEST_VALUE_INPUT_CONTROL_PORT_OFF) return -1;
	
	//アナログ入力
	smpl_onLED1();
	TimeControlDelayMs(WAIT_2MS);
	smpl_offLED1();
	smpl_onLED1();
	smpl_offLED1();
	//AD値
	//制御ゲートオフでアナログ入力0あたりになっているか確認
	adTestValue = getAnalogVoltageValue();
	if(adTestValue > 10 ) return -1;
	//電源電圧監視アナログ入力確認
	//テスターでアナログ値を測る時はゲートON状態で
	powerTestValue = PowerMonitoringAnalogInputGetVoltageValue();
	if( ( powerTestValue < 4.5f) || (5.5f < powerTestValue) ) return -1;
	powerTestValue = PowerMonitoringAnalogInputGetVoltageValue();
	if(fabsf(PowerMonitoringAnalogInputCheckCurrentVoltageValue() - powerTestValue) > FLT_EPSILON) return -1;
	powerTestValue = PowerMonitoringAnalogInputGetVoltageValue();
	if(fabsf(PowerMonitoringAnalogInputCheckCurrentVoltageValue() - powerTestValue) > FLT_EPSILON) return -1;
	powerTestValue = PowerMonitoringAnalogInputGetVoltageValue();
	if(fabsf(PowerMonitoringAnalogInputCheckCurrentVoltageValue() - powerTestValue) > FLT_EPSILON) return -1;
	powerTestValue = PowerMonitoringAnalogInputGetVoltageValue();
	if(fabsf(PowerMonitoringAnalogInputCheckCurrentVoltageValue() - powerTestValue) > FLT_EPSILON) return -1;
	powerTestValue = PowerMonitoringAnalogInputGetVoltageValue();
	if(fabsf(PowerMonitoringAnalogInputCheckCurrentVoltageValue() - powerTestValue) > FLT_EPSILON) return -1;
	powerTestValue = PowerMonitoringAnalogInputGetVoltageValue();
	if(fabsf(PowerMonitoringAnalogInputCheckCurrentVoltageValue() - powerTestValue) > FLT_EPSILON) return -1;
	powerTestValue = PowerMonitoringAnalogInputGetVoltageValue();
	if( ( powerTestValue < 4.5f) || (5.5f < powerTestValue) ) return -1;
	return 0;
}
