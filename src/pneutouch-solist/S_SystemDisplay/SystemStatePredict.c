/*****************************************************************************
 * File: SystemStatePredict.c
 * Title: 推論画面を表示する。
 * LastUpdated: 2025.05.30
******************************************************************************/

/**
 * @file SystemStatePredict.c
 * @brief 推論画面を表示する。
 */

#include "SystemStatePredict.h"
#include "SystemStatePredictDraw.h"
#include "SystemState.h"
#include "Lcd.h"
#include "AI.h"
#include "irq.h"

/** 0.14秒を指すカウンタ値。10msハンドラでカウントするためこの値になる */
#define TIMER_140MS			(14)
volatile static bool timerFlg = false;


void SystemStatePredict(void)
{
	volatile float anomalyValue; 
	volatile ANOMALY anomalyResult;
	
	if(SystemStateCheckStateChange())
	{
		LcdDisplayOnOff(LCD_DISPLAY_ON,LCD_CURSOR_OFF,LCD_CURSOR_BLINK_OFF);
		LcdClearDisplay();
		LcdDraw(LCD_START_OF_FIRST_LINE,"Inference");
		
		//前回の情報削除
		SystemStatePredictDrawReset();
	}
	else
	{

	}
	//一定時間に一度以下の処理をする
	if(timerFlg)
	{
		timerFlg = false;
		
		//異常度、異常判定を取得
		__disable_irq();
		anomalyValue = AIGetFloatAnomalyValue();	
		anomalyResult = AIGetCurrentAnomalyResult();
		__enable_irq();
		
		//描画
		SystemStatePredictDrawDraw(anomalyValue,anomalyResult);
	}
}


void SystemStatePredictDrawTimer(void)
{
	static short counter = 0;
	if(counter++ >= TIMER_140MS)
	{
		timerFlg = true;
		counter = 0;
	}
}
