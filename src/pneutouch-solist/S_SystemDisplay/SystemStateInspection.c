/*****************************************************************************
 * File: SystemStateInspection.c
 * Title: 検査画面を制御する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file SystemStateInspection.c
 * @brief 検査画面を制御する。
 */
 
#include "SystemStateInspection.h"
#include "SystemState.h"
#include "Lcd.h"
#include "Sw.h"
#include "PhotoCouplerInput.h"
#include "RelayOutput.h"
#include "Regulator5VOutput.h"
#include "Regulator24VOutput.h"
#include "Fram.h"
#include "PowerMonitoringAnalogInput.h"
#include "smpl_common_led.h"
#include "Output.h"
#include <stdio.h>
#include <math.h>

/**
 * @brief 表示用文字列のインデックス
 */
typedef enum
{
	STRING_INDEX_D = 0,																						/**< D */
	STRING_INDEX_CORON,																						/**< ; */
	STRING_INDEX_DIP1,																						/**< DIP1 */	
	STRING_INDEX_DIP2,																						/**< DIP2 */
	STRING_INDEX_DIP3,																						/**< DIP3 */
	STRING_INDEX_DIP4,																						/**< DIP4 */
	STRING_INDEX_EMPTY,																						/**< 空白 */
	STRING_INDEX_VIEW_START																					/**< 何かの表示文字列 */
}STRING_INDEX;

#define WP_VIEW_POSITION							(LCD_START_OF_SECOND_LINE + STRING_INDEX_VIEW_START)	/**< WPの表示場所 */
#define POWER_MONITORING_ANALOG_INPUT_VIEW_POSITION	(LCD_START_OF_SECOND_LINE + STRING_INDEX_VIEW_START)	/**< 電源電圧値の表示場所 */

#define DIP_ON										(1)
#define DIP_OFF										(0)

typedef enum
{
	LED_COUNTER_LED1 = 0,																					/**< LED1 ON */
	LED_COUNTER_LED2,																						/**< LED2 ON */
	LED_COUNTER_LED3,																						/**< LED3 ON */
	LED_COUNTER_NONE																						/**< LED1~3 OFF */
}LED_COUNTER;

#define TIMER_100MS									(10)
volatile static bool timerFlg = false;



void SystemStateInspection(void)
{
	char viewStringBuffer[LCD_MOST_CHARACTERS_ON_A_LINE + 1];
	uint8_t dip1Value;
	uint8_t dip2Value;
	uint8_t dip3Value;
	uint8_t dip4Value;
	double voltageValue;
	static LED_COUNTER ledCounter = LED_COUNTER_LED1;
	
	if(SystemStateCheckStateChange())
	{
		LcdDisplayOnOff(LCD_DISPLAY_ON,LCD_CURSOR_OFF,LCD_CURSOR_BLINK_OFF);
		SystemStateSetShiftToStop(SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED);
		LcdClearDisplay();
		LcdDraw(LCD_START_OF_FIRST_LINE,"ProductCheck");
	}
	else
	{
		//外部入出力端子テスト(フォトカプラ入力、出力)
		if(PhotoCouplerInput0IsEntered())RelayOutputRelay0On();
		else RelayOutputRelay0Off();
		if(PhotoCouplerInput1IsEntered())RelayOutputRelay1On();
		else RelayOutputRelay1Off();
		
		//5V制御テスト(DIP1)
		if(SwIsDsw1Entered())Regulator5VOutputOn();
		else Regulator5VOutputOff();
		//24V制御テスト(DIP2)
		if(SwIsDsw2Entered())Regulator24VOutputOn();
		else Regulator24VOutputOff();
		
		//DIP表示(DIP1~4)
		//電源電圧チェックと表示(PSH2 PSH1と同時押し時は反応なし)
		if(timerFlg)
		{
			if(SwIsDsw1Entered())dip1Value = DIP_ON;
			else dip1Value = DIP_OFF;
			if(SwIsDsw2Entered())dip2Value = DIP_ON;
			else dip2Value = DIP_OFF;
			if(SwIsDsw3Entered())dip3Value = DIP_ON;
			else dip3Value = DIP_OFF;
			if(SwIsDsw4Entered())dip4Value = DIP_ON;
			else dip4Value = DIP_OFF;	
			snprintf(viewStringBuffer,LCD_MOST_CHARACTERS_ON_A_LINE + 1,"D:%d%d%d%d",dip1Value,dip2Value,dip3Value,dip4Value);
			LcdDraw(LCD_START_OF_SECOND_LINE,viewStringBuffer);
			
			if(!SwIsPsw1Entered() && SwIsPsw2Entered())
			{
				SwDisablePsw2UntilNextPress();
				voltageValue = (double)PowerMonitoringAnalogInputGetVoltageValue();
				voltageValue = round(voltageValue * 100) / 100;
				snprintf(viewStringBuffer,LCD_MOST_CHARACTERS_ON_A_LINE + 1,"OUT:%4.2fV",voltageValue);
				LcdDraw(POWER_MONITORING_ANALOG_INPUT_VIEW_POSITION,viewStringBuffer);
			}
			timerFlg = false;
		}

		//WPテストと表示(PSH1 PSH2と同時押し時は反応なし)
		if(SwIsPsw1Entered() && !SwIsPsw2Entered())
		{
			SwDisablePsw1UntilNextPress();
			if(FramInspectWp())
			{
				LcdDraw(WP_VIEW_POSITION,"WP:PASSED");
			}
			else
			{
				LcdDraw(WP_VIEW_POSITION,"WP:FAILED");
			}
		}
		
		//バックライトチェック(PSH3)
		if(SwIsPsw3Entered())LcdBacklightOn();
		else LcdBacklightOff();
		
		//LEDチェック(PSH4)
		if(SwIsPsw4Entered())
		{
			SwDisablePsw4UntilNextPress();
			switch(ledCounter++)
			{
				case LED_COUNTER_LED1:
					smpl_onLED1();
					break;
				case LED_COUNTER_LED2:
					smpl_onLED2();
					break;
				case LED_COUNTER_LED3:
					smpl_onLED3();
					break;
				case LED_COUNTER_NONE:
					smpl_offLED1();
					smpl_offLED2();
					smpl_offLED3();
					ledCounter = LED_COUNTER_LED1;
					break;
			}
		}
	}
}

//1秒おきにLCDに電源電圧値を描画。
void SystemStateInspectionDrawTimer(void)
{
	static uint8_t counter = 0;
	if( counter++ >= TIMER_100MS)
	{
		timerFlg = true;
		counter = 0;
	}
}

int SystemStateInspectionTest(void)
{
	return 0;
}
