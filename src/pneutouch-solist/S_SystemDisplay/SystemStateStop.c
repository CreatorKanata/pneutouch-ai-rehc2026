/*****************************************************************************
 * File: SystemStateStop.c
 * Title: 停止画面を制御する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file SystemStateStop.c
 * @brief 停止画面を制御する。
 */
#include "SystemStateStop.h"
#include "SystemState.h"
#include "Lcd.h"
#include "smpl_common_led.h"
#include "SystemError.h"
#include "ConfigData.h"
#include "AI.h"
#include "TimeSetting.h"
#include "AILog.h"

#define LCD_ERROR	(14)

typedef enum
{
	LOG_CONFIG_UNUSE = 0,
	LOG_CONFIG_END,
	LOG_CONFIG_WARNING_RED,
	LOG_CONFIG_WARNING_YELLOW,
}LOG_CONFIG;


void SystemStateStop(void)
{
	//状態変化していないなら何もしない。
	if(SystemStateCheckStateChange())
	{
		//学習推論終了時
		if( ( SystemStateGetOldState() == SYSTEM_STATE_LEARN ) || ( SystemStateGetOldState() == SYSTEM_STATE_PREDICT ) )
		{
			LcdDraw(LCD_START_OF_FIRST_LINE,"                ");
			LcdDraw(LCD_START_OF_FIRST_LINE,"Stop");
			smpl_offLED1();
		}
		else
		{
			LcdDisplayOnOff(LCD_DISPLAY_ON,LCD_CURSOR_OFF,LCD_CURSOR_BLINK_OFF);
			LcdClearDisplay();
			LcdDraw(LCD_START_OF_FIRST_LINE,"Stop");
		}
		LcdDraw(LCD_ERROR,SystemErrorGetStringError());
	}
	//エラー表示 
	if(SystemErrorNotifyErrorChanged())
	{
		//E00なら何もしない。エラー管理画面にも行かない。
		if(SYSTEM_ERROR_E00 == SystemErrorGetError())
		{
			SystemStateSetShiftToError(SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED);
			return;
		}
		//エラーが残っているならエラークリア画面を巡回に追加。
		LcdDraw(LCD_ERROR,SystemErrorGetStringError());
		SystemStateSetShiftToError(SYSTEM_SHIFT_STATE_SHIFT_PEMITTED);
	}
}
