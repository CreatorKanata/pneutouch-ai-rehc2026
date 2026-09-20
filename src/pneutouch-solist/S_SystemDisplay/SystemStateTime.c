/*****************************************************************************
 * File: SystemStateTime.c
 * Title: 時刻設定画面を制御する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file SystemStateTime.c
 * @brief 時刻設定画面を制御する。
 */
#include <string.h>
#include "SystemStateTime.h"
#include "SystemState.h"
#include "Lcd.h"
#include "Sw.h"
#include "TimeSetting.h"

/**
 * @brief LCD上の時刻が表示されている位置を示す列挙型
 */
typedef enum
{
	TIME_SECOND_DIGIT_YEAR = 17,
	TIME_FIRST_DIGIT_YEAR = 18,
	TIME_CLOCK_SLASH_FIRST = 19,
	TIME_SECOND_DIGIT_MONTH = 20,
	TIME_FIRST_DIGIT_MONTH = 21,
	TIME_CLOCK_SLASH_SECOND = 22,
	TIME_SECOND_DIGIT_DAY = 23,
	TIME_FIRST_DIGIT_DAY = 24,
	TIME_CLOCK_SPACE_FIRST = 25,
	TIME_SECOND_DIGIT_HOUR = 26,
	TIME_FIRST_DIGIT_HOUR = 27,
	TIME_CLOCK_COLON = 28,
	TIME_SECOND_DIGIT_MINUTE = 29,
	TIME_FIRST_DIGIT_MINUTE = 30,
	TIME_CLOCK_SPACE_SECOND	= 31
}TIME;

/**
 * @brief LCDのカーソルの状態を示す列挙型
 */
typedef enum
{
	LCD_CURSOR_STATE_OFF,
	LCD_CURSOR_STATE_ON
}LCD_CURSOR_STATE;

/** LCDのカーソルの状態 */
volatile static LCD_CURSOR_STATE cursorState = LCD_CURSOR_STATE_OFF;

static char* getTime(void);
static void reloadLcdTime(const char* time, TIME datetime);
static LCD_CURSOR_STATE getCursorState(void);
static void setCursorState(LCD_CURSOR_STATE state);


void SystemStateTime(void)
{
	//時刻取得
	char* localTime = getTime();
	
	volatile static uint8_t cursorAddress = LCD_START_OF_FIRST_LINE;
	
	//別の画面から入った時
	if(SystemStateCheckStateChange())
	{
		LcdDisplayOnOff(LCD_DISPLAY_ON,LCD_CURSOR_OFF,LCD_CURSOR_BLINK_OFF);
		setCursorState(LCD_CURSOR_STATE_OFF);
		SystemStateSetShiftToAILearnErase(SYSTEM_SHIFT_STATE_SHIFT_PEMITTED);
		LcdClearDisplay();
		LcdDraw(LCD_START_OF_FIRST_LINE,"*Clock");
		LcdDraw(LCD_START_OF_SECOND_LINE,localTime);
	}
	else
	{
		//通常動作
		if(getCursorState() == LCD_CURSOR_STATE_OFF)
		{
			//1分おきに時刻表示
			if(!TimeSettingIsCurrentDateTimeSameAsOldDateTime())
			{
				LcdDraw(LCD_START_OF_SECOND_LINE,localTime);
			}
			//時刻変更開始　カーソルON
			if(SwIsPsw4Entered())
			{
				SwDisablePsw4UntilNextPress();
				//初期化
				setCursorState(LCD_CURSOR_STATE_ON);
				SystemStateSetShiftToAILearnErase(SYSTEM_SHIFT_STATE_SHIFT_PROHIBITED);
				LcdSetPositionForDisplay(LCD_START_OF_SECOND_LINE);
				LcdDisplayOnOff(LCD_DISPLAY_ON,LCD_CURSOR_ON,LCD_CURSOR_BLINK_OFF);
				cursorAddress = LCD_START_OF_SECOND_LINE;
				TimeSettingMoveCurrentTimetoConfigTime();
			}
			return;
		}
		
		//時刻設定
		if(getCursorState() == LCD_CURSOR_STATE_ON)
		{
			//カーソル右移動
			if(SwIsPsw2Entered())
			{
				SwDisablePsw2UntilNextPress();
				LcdShiftCursorRight();
				cursorAddress++;
				//次が時刻でないなら2つ先に進む
				if(  (cursorAddress == TIME_CLOCK_SLASH_FIRST) 
					|| (cursorAddress == TIME_CLOCK_SLASH_SECOND) 
					|| (cursorAddress == TIME_CLOCK_SPACE_FIRST) 
					|| (cursorAddress == TIME_CLOCK_COLON)
					)
				{
					LcdShiftCursorRight();
					cursorAddress++;
				}
				//LCDの2行目末尾に来ると先頭に戻る
				else if(cursorAddress >= TIME_CLOCK_SPACE_SECOND)
				{
					LcdSetPositionForDisplay(LCD_START_OF_SECOND_LINE);
					cursorAddress = LCD_START_OF_SECOND_LINE;
				}
			}
			//値インクリメント
			else if(SwIsPsw3Entered())
			{
				SwDisablePsw3UntilNextPress();
				switch(cursorAddress)
				{
					case TIME_SECOND_DIGIT_YEAR:
						TimeSettingAddYear(DIGIT_SECOND);
						break;
					case TIME_FIRST_DIGIT_YEAR:
						TimeSettingAddYear(DIGIT_FIRST);
						break;
					case TIME_SECOND_DIGIT_MONTH:
						TimeSettingAddMonth(DIGIT_SECOND);
						break;
					case TIME_FIRST_DIGIT_MONTH:
						TimeSettingAddMonth(DIGIT_FIRST);
						break;
					case TIME_SECOND_DIGIT_DAY:
						TimeSettingAddDay(DIGIT_SECOND);	
						break;
					case TIME_FIRST_DIGIT_DAY:
						TimeSettingAddDay(DIGIT_FIRST);
						break;
					case TIME_SECOND_DIGIT_HOUR:
						TimeSettingAddHour(DIGIT_SECOND);
						break;
					case TIME_FIRST_DIGIT_HOUR:
						TimeSettingAddHour(DIGIT_FIRST);
						break;
					case TIME_SECOND_DIGIT_MINUTE:
						TimeSettingAddMinute(DIGIT_SECOND);
						break;
					case TIME_FIRST_DIGIT_MINUTE:
						TimeSettingAddMinute(DIGIT_FIRST);
						break;
				}
				//LCDの再描画
				localTime = TimeSettingGetConfigDateTimeString();
				reloadLcdTime(localTime, cursorAddress);
			}
			//時刻設定
			else if(SwIsPsw4Entered())
			{
				SwDisablePsw4UntilNextPress();
				//終了処理
				LcdDisplayOnOff(LCD_DISPLAY_ON,LCD_CURSOR_OFF,LCD_CURSOR_BLINK_OFF);
				setCursorState(LCD_CURSOR_STATE_OFF);
				SystemStateSetShiftToAILearnErase(SYSTEM_SHIFT_STATE_SHIFT_PEMITTED);
				//時刻設定API
				TimeSettingSetConfigTime();
			}
			//キャンセル
			else if(SwIsPsw1Entered())
			{
				//終了処理
				SwDisablePsw1UntilNextPress();
				LcdDisplayOnOff(LCD_DISPLAY_ON,LCD_CURSOR_OFF,LCD_CURSOR_BLINK_OFF);
				setCursorState(LCD_CURSOR_STATE_OFF);
				SystemStateSetShiftToAILearnErase(SYSTEM_SHIFT_STATE_SHIFT_PEMITTED);
			}
		}
	}
}


/**
 * @brief 現在または設定変更中の日付時刻を取得。
 */
inline static char* getTime(void)
{
	if(getCursorState() == LCD_CURSOR_STATE_OFF)
	{
		return TimeSettingGetCurrentDateTimeString();
	}
	else
	{
		return TimeSettingGetConfigDateTimeString();
	}
}


/**
 * @brief 設定変更中の画面操作後にLCDを更新する。
 */
inline static void reloadLcdTime(const char* time, TIME datetime)
{
	LcdDraw(LCD_START_OF_SECOND_LINE,time);
	LcdSetPositionForDisplay(datetime);
}


/**
 * @brief カーソルの状態を返す。
 */
inline static LCD_CURSOR_STATE getCursorState(void)
{
	return cursorState;
}

/**
 * @brief カーソルの状態を決定する。
 *
 * @param state カーソルの状態
 */
inline static void setCursorState(LCD_CURSOR_STATE state)
{
	cursorState = state;
}
