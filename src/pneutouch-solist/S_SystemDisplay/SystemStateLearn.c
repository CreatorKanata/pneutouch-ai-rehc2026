/*****************************************************************************
 * File: SystemStateLearn.c
 * Title: 学習画面を表示する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file SystemStateLearn.c
 * @brief 学習画面を表示する。
 */
#include <stdio.h>
#include <string.h>
#include "SystemStateLearn.h"
#include "SystemState.h"
#include "Lcd.h"
#include "smpl_common_led.h"
#include "AI.h"
#define LCD_ANOMALY_VALUE_FULL_SCALE	(16)
#define TIMER_100MS						(10)
volatile static bool timerFlg = false;

void SystemStateLearn(void)
{
	char dstAnomalyValue[NUMBER_OF_CHARACTERS_OF_AI_LEARN_STRING];
	if(SystemStateCheckStateChange())
	{
		LcdDisplayOnOff(LCD_DISPLAY_ON,LCD_CURSOR_OFF,LCD_CURSOR_BLINK_OFF);
		LcdClearDisplay();
		LcdDraw(LCD_START_OF_FIRST_LINE,"Learning");
		smpl_onLED1();
		smpl_offLED2();
		smpl_offLED3();
	}
	else
	{
	}
	if(timerFlg)
	{
		__disable_irq();
		memcpy(dstAnomalyValue,AIGetFloatAnomalyValueString(),NUMBER_OF_CHARACTERS_OF_AI_LEARN_STRING);
		__enable_irq();
		LcdDraw(LCD_START_OF_SECOND_LINE,dstAnomalyValue);
		timerFlg = false;
	}
}

//0.1秒おきにLCDに異常度を描画。
void SystemStateLearnDrawTimer(void)
{
	static short counter = 0;
	if( counter++ >= TIMER_100MS)
	{
		timerFlg = true;
		counter = 0;
	}
}
