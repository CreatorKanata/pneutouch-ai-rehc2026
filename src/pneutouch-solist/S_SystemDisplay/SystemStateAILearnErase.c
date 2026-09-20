/*****************************************************************************
 * File: SystemStateAILearnErase.c
 * Title: 学習した重みデータの消去画面を制御する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file SystemStateAILearnErase.c
 * @brief 学習した重みデータの消去画面を制御する。
 */

#include "SystemStateAILearnErase.h"
#include "SystemState.h"
#include "Lcd.h"
#include "Sw.h"
#include "AI.h"
#define YES	(17)
#define NO	(19)


void SystemStateAILearnErase(void)
{
	static uint8_t cursorAddress = LCD_START_OF_FIRST_LINE;
	if(SystemStateCheckStateChange())
	{
		LcdDisplayOnOff(LCD_DISPLAY_ON,LCD_CURSOR_OFF,LCD_CURSOR_BLINK_OFF);
		SystemStateSetShiftToStop(SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED);
		LcdClearDisplay();
		LcdDraw(LCD_START_OF_FIRST_LINE,"*Clear");
		LcdDraw(LCD_START_OF_SECOND_LINE,"Y/N");
		LcdSetPositionForDisplay(NO);
		cursorAddress = NO;
		LcdDisplayOnOff(LCD_DISPLAY_ON,LCD_CURSOR_ON,LCD_CURSOR_BLINK_OFF);
	}
	else
	{
		//カーソル右移動
		if(SwIsPsw2Entered())
		{
			SwDisablePsw2UntilNextPress();
			if(cursorAddress == YES)
			{
				cursorAddress = NO;
				LcdSetPositionForDisplay(NO);
			}
			else if(cursorAddress == NO)
			{
				cursorAddress = YES;
				LcdSetPositionForDisplay(YES);
			}
		}
		else if(SwIsPsw4Entered())
		{
			SwDisablePsw4UntilNextPress();
			if(cursorAddress == YES)
			{
				AIModelWeightReset();
				AILearningCounterReset();
				SystemStateSetShiftToStop(SYSTEM_SHIFT_STATE_SHIFT_PEMITTED);
			}
			else if(cursorAddress == NO)
			{
				//画面から抜ける
				SystemStateSetShiftToStop(SYSTEM_SHIFT_STATE_SHIFT_PEMITTED);
			}
		}
	}
}
