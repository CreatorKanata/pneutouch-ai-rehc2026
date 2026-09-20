/*****************************************************************************
 * File: main.c
 * Title: AIVibrationInference
 * LastUpdated: 2026.03.16
 * Copyright (C) 2025 - 2026 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file main.c
 * @brief AIVibrationInference
 */

#include "mcu.h"
#include "main.h"
#include "clock.h"
#include "smpl_common.h"
#include "wdt.h"
#include "irq.h"
#include "Sleep.h"
#include "Lcd.h"
#include "SystemSettings.h"
#include "SystemStateController.h"
#include "AI.h"
#include "SoftwareInterrupt.h"
#include "HighSpeedCom.h"
#include "HighSpeedComHelper.h"
#include "SoftSpi.h"
#include "RX4111.h"
#include "Fram.h"
#include "ConfigData.h"
#include "AILog.h"
#include <stdio.h>
#include "smpl_common_led.h"
#include "Sw.h"
#include "Kx134Acc.h"
#include "TimeControl.h"
#include "Sensor.h"
#include "RelayOutput.h"
#include "SystemPowerControl.h"
#include "Output.h"
#include "Input.h"
#include "TimeSetting.h"
#include "Version.h"
#include "PhotoCouplerInput.h"
#include "Regulator5VOutput.h"
#include "Regulator24VOutput.h"
#include "PowerMonitoringAnalogInput.h"
#include "LogControl.h"
#include "SystemError.h"
#include "PeriodicHandler10ms.h"
#include "Uart1.h"
#include "UartQueue.h"
#include "UartReceiveCommand.h"
#define WAIT_FOR_DISPLAY					(1000)

static void coreInit(void);
static void utilityInit(void);
static void settingInit(void);
static void controllerInit(void);
static void customizableUtilityInit(void);


int32_t main( void )
{
	//必要機能初期化
	coreInit();
	//各種初期化
	utilityInit();
	//設定読み込み
	settingInit();
	//コントローラー初期化
	controllerInit();
	//各種初期化(設定あり)
	customizableUtilityInit();
	for(;;) 
	{
		SystemStateController();
		wdt_clear();
		SleepChangetoHaltMode();
	}
}

static void coreInit(void)
{
	__disable_irq();
	//WDT初期化
	wdt_init(WDT_2S);
	wdt_clear();
	//クロック初期化
	smpl_setLsCrystal32Khz();
	smpl_setHsPll48Mhz(CLK_XSPEN_DIS,CLK_HXSPEN_DIS);
	__enable_irq();
	
	//POWER初期化
	SystemPowerControlInit();
	//ソフトウェア割り込み初期化
	SoftwareInterruptInit();
	//softspi初期化
	SoftSpiPeripheralInit();
	//ディレイ用タイマー初期化
	TimeControlInit();
	//10msタイマー初期化
	PeriodicHandler10msInit();
	wdt_clear();
}

static void utilityInit(void)
{
	//Input初期化
	SwInit();
	PhotoCouplerInputInit();

	//output初期化
	RelayOutputInit();
	smpl_initLED1( LED_INACTIVE );
	smpl_initLED2( LED_INACTIVE );
	smpl_initLED3( LED_INACTIVE );
	Regulator5VOutputInit();
	Regulator24VOutputInit();
	
	//UART初期化
	Uart1PeripheralInit();
	UartQueueInit(0);
	UartReceiveCommandInit();

	//LCD初期化
	LcdPeripheralInit();
	LcdInit();
	
	//RTC初期化
	RX4111Init();
	
	//高速通信機能(リアルタイム転送、ブロック転送)初期化
	HighSpeedComPeripheralInit();
	
	//電源監視機能初期化
	PowerMonitoringAnalogInputInit();
	wdt_clear();
}

static void settingInit(void)
{
	//Fram初期化
	FramInit();
	//AILog初期化
	AILogInitialize();
	ConfigDataLoadConfigGroup0();
	wdt_clear();
}

static void controllerInit(void)
{
	//システムコントローラー初期化
	SystemStateControllerInit();
	//バージョン表示
	//VersionTest();
	LcdDraw(LCD_START_OF_FIRST_LINE,VersionGetViewName());
	LcdDisplayOnOff(LCD_DISPLAY_ON,LCD_CURSOR_OFF,LCD_CURSOR_BLINK_OFF);
	TimeControlDelayMs(WAIT_FOR_DISPLAY);
	wdt_clear();
}

static void customizableUtilityInit(void)
{
	uint8_t data;	
	
	//高速通信機能(リアルタイム転送、ブロック転送)スタート。常時通信を受け付ける。
	ConfigDataGetUint8Value(EN_CONFIG_REALTIME_COM,&data);
	if(data)
	{
		HighSpeedComStart();
	}
	else
	{
		ConfigDataGetUint8Value(EN_CONFIG_BLOCK_COM,&data);
		if(data) HighSpeedComStart();
	}
	
	//AITestSolistAiLibrary2Model();

	//AI初期化
	AIInit();

	SystemErrorInit();
	SystemSettingsInit();
	//推論画面
	SystemSettingsRegister(SYSTEM_SETTINGS_PREDICT);
	//ログ
	SystemSettingsRegister(SYSTEM_SETTINGS_LOG);
	wdt_clear();
}
