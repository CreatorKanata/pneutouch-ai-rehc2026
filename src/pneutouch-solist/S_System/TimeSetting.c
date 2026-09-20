/*****************************************************************************
 * File: TimeSetting.c
 * Title: 時刻設定を行う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file TimeSetting.c
 * @brief 時刻設定を行う。
 */

#include "TimeSetting.h"
#include <string.h>

/** '0' */
#define ASCII_ZERO '0'
#define SET_SEC	(0)

/**
 * @brief 日付データのindex
 */
typedef enum
{
	DATE_TIME_SETTING_SECOND_DIGIT_YEAR = 0,
	DATE_TIME_SETTING_FIRST_DIGIT_YEAR = 1,
	DATE_TIME_SETTING_SECOND_DIGIT_MONTH = 3,
	DATE_TIME_SETTING_FIRST_DIGIT_MONTH = 4,
	DATE_TIME_SETTING_SECOND_DIGIT_DAY = 6,
	DATE_TIME_SETTING_FIRST_DIGIT_DAY = 7,
	DATE_TIME_SETTING_SECOND_DIGIT_HOUR = 9,
	DATE_TIME_SETTING_FIRST_DIGIT_HOUR = 10,
	DATE_TIME_SETTING_SECOND_DIGIT_MINUTE = 12,
	DATE_TIME_SETTING_FIRST_DIGIT_MINUTE = 13
}DATE_TIME_SETTING;

/**
 * @brief 月
 */
typedef enum
{
	MONTH_JAN = 1,
	MONTH_FEB,
	MONTH_MAR,
	MONTH_APR,
	MONTH_MAY,
	MONTH_JUN,
	MONTH_JUL,
	MONTH_AUG,
	MONTH_SEP,
	MONTH_OCT,
	MONTH_NOV,
	MONTH_DEC,
}MONTH;

//RTCモジュールの構造体から日付時刻データを取得する。
static RX4111_DATE_TIME currentDateTime= { 01,01,01,01,01,00};
static RX4111_DATE_TIME oldDateTime= { 01,01,01,01,01,00};
static RX4111_DATE_TIME configDateTime = { 11,11,11,11,11,00};
static char timeString[] =	"00/00/00 00:00";
//RTCの現在時刻を取得する。
static RX4111_DATE_TIME TimeSettingGetCurrentTime(void);


/**
 * @brief RTCの現在時刻を取得する。
 * @return RX4111_DATE_TIME
 */
static RX4111_DATE_TIME TimeSettingGetCurrentTime(void)
{
	memcpy((void*)&oldDateTime, (void*)&currentDateTime, sizeof(RX4111_DATE_TIME));
	RX4111GetTime(&currentDateTime);
	return currentDateTime;
}

bool TimeSettingIsCurrentDateTimeSameAsOldDateTime(void)
{
	if(currentDateTime.Year != oldDateTime.Year) return false;
	if(currentDateTime.Month != oldDateTime.Month) return false;
	if(currentDateTime.Day != oldDateTime.Day) return false;
	if(currentDateTime.Hour != oldDateTime.Hour) return false;
	if(currentDateTime.Minute != oldDateTime.Minute) return false;
	return true;	
}

RX4111_DATE_TIME TimeSettingGetConfigTime(void)
{
	return configDateTime;
}

void TimeSettingSetConfigTime(void)
{
	configDateTime.Sec = SET_SEC;
	RX4111SetTime(&configDateTime);
}

void TimeSettingMoveCurrentTimetoConfigTime(void)
{
	memcpy((void*)&configDateTime, (void*)&currentDateTime, sizeof(RX4111_DATE_TIME));
}


/**
 * @brief 最大値を超えている場合最小値にする。
 * @param min 最小値。
 * @param max 最大値。
 * @param data 対象のデータ。
 */
static void validateMinAndMaxRanges(uint8_t min, uint8_t max, uint8_t* data)
{
	if(*data > max)
	{
		*data = min;
	}
}

/**
 * @brief その月の日が31日かどうか判定する。。
 * @return bool 31日の場合true、それ以外はfalse。
 */
static bool doesMonthhave31Days(void)
{
	switch(configDateTime.Month)
	{
		case MONTH_JAN:
		case MONTH_MAR:
		case MONTH_MAY:
		case MONTH_JUL:
		case MONTH_AUG:
		case MONTH_OCT:
		case MONTH_DEC:
			return true;
		
		default:
			return false;
	}
}

/**
 * @brief その月月の日が30日かどうか判定する。
 * @return bool 30日の場合true、それ以外はfalse。
 */
static bool doesMonthhave30Days(void)
{
	switch(configDateTime.Month)
	{
		case MONTH_APR:
		case MONTH_JUN:
		case MONTH_SEP:
		case MONTH_NOV:
			return true;
		
		default:
			return false;
	}
}

/**
 * @brief 2月かどうか判定する。
 * @return bool 2月の場合true、それ以外はfalse。
 */
static bool isFebrary(void)
{
	switch(configDateTime.Month)
	{
		case MONTH_FEB:
			return true;
		
		default:
			return false;
	}
}

/**
 * @brief うるう年かどうかを判定する。
 * @return bool うるう年の場合true、それ以外はfalse。
 */
static bool isLeapYear(void)
{
	uint8_t year = configDateTime.Year;
	if (!(year % 4))
	{
		return true;
	}
	else
	{
		return false;
	}
}


void TimeSettingAddYear(DIGIT digit)
{
	switch(digit)
	{
		case DIGIT_SECOND:
			configDateTime.Year += 10;
			break;
		case DIGIT_FIRST:
			configDateTime.Year += 1;
			break;
	}
	validateMinAndMaxRanges(0,99,&configDateTime.Year);
	
	//日付を修正
	if(isFebrary())
	{
		if(isLeapYear())
		{
			validateMinAndMaxRanges(29,29,&configDateTime.Day);
		}
		else
		{
			validateMinAndMaxRanges(28,28,&configDateTime.Day);
		}
	}
}


void TimeSettingAddMonth(DIGIT digit)
{
	switch(digit)
	{
		case DIGIT_SECOND:
			configDateTime.Month += 10;
			break;
		case DIGIT_FIRST:
			configDateTime.Month += 1;
			break;
	}
	validateMinAndMaxRanges(1,12,&configDateTime.Month);
	
	//日付を修正
	switch(configDateTime.Day)
	{
		case 31:
			if(doesMonthhave30Days())
			{
				validateMinAndMaxRanges(30,30,&configDateTime.Day);
			}
			else if(isFebrary())
			{
				if(isLeapYear())
				{
					validateMinAndMaxRanges(29,29,&configDateTime.Day);
				}
				else
				{
					validateMinAndMaxRanges(28,28,&configDateTime.Day);
				}
			}
			break;
		
		case 30:
			if(isFebrary())
			{
				if(isLeapYear())
				{
					validateMinAndMaxRanges(29,29,&configDateTime.Day);
				}
				else
				{
					validateMinAndMaxRanges(28,28,&configDateTime.Day);
				}	
			}
			break;
		
		case 29:
			if(isFebrary())
			{
				if(!isLeapYear())
				{
					validateMinAndMaxRanges(28,28,&configDateTime.Day);
				}
			}
			break;
		}
}


void TimeSettingAddDay(DIGIT digit)
{
	switch(digit)
	{
		case DIGIT_SECOND:
			configDateTime.Day += 10;
			break;
		case DIGIT_FIRST:
			configDateTime.Day += 1;
			break;
	}
	
	if(doesMonthhave31Days())
	{
		validateMinAndMaxRanges(1,31,&configDateTime.Day);
	}
	else if(doesMonthhave30Days())
	{
		validateMinAndMaxRanges(1,30,&configDateTime.Day);
	}
	else if(isFebrary())
	{
		if(isLeapYear())
		{
			validateMinAndMaxRanges(1,29,&configDateTime.Day);
		}
		else
		{
			validateMinAndMaxRanges(1,28,&configDateTime.Day);
		}
	}
}


void TimeSettingAddHour(DIGIT digit)
{
	switch(digit)
	{
		case DIGIT_SECOND:
			configDateTime.Hour += 10;
			break;
		case DIGIT_FIRST:
			configDateTime.Hour += 1;
			break;
	}
	validateMinAndMaxRanges(0,23,&configDateTime.Hour);
}


void TimeSettingAddMinute(DIGIT digit)
{
	switch(digit)
	{
		case DIGIT_SECOND:
			configDateTime.Minute += 10;
			break;
		case DIGIT_FIRST:
			configDateTime.Minute += 1;
			break;
	}
	validateMinAndMaxRanges(0,59,&configDateTime.Minute);
}


char* TimeSettingGetTimeString(RX4111_DATE_TIME dateTime)
{
	//年
	timeString[DATE_TIME_SETTING_SECOND_DIGIT_YEAR] = dateTime.Year / 10 + ASCII_ZERO;
	timeString[DATE_TIME_SETTING_FIRST_DIGIT_YEAR] = dateTime.Year % 10 + ASCII_ZERO;
	//月
	timeString[DATE_TIME_SETTING_SECOND_DIGIT_MONTH] = dateTime.Month / 10 + ASCII_ZERO;
	timeString[DATE_TIME_SETTING_FIRST_DIGIT_MONTH] = dateTime.Month % 10 + ASCII_ZERO;
	//日
	timeString[DATE_TIME_SETTING_SECOND_DIGIT_DAY] = dateTime.Day / 10 + ASCII_ZERO;
	timeString[DATE_TIME_SETTING_FIRST_DIGIT_DAY] = dateTime.Day % 10 + ASCII_ZERO;
	//時間
	timeString[DATE_TIME_SETTING_SECOND_DIGIT_HOUR] = dateTime.Hour / 10 + ASCII_ZERO;
	timeString[DATE_TIME_SETTING_FIRST_DIGIT_HOUR] = dateTime.Hour % 10 + ASCII_ZERO;
	//分
	timeString[DATE_TIME_SETTING_SECOND_DIGIT_MINUTE] = dateTime.Minute / 10 + ASCII_ZERO;
	timeString[DATE_TIME_SETTING_FIRST_DIGIT_MINUTE] = dateTime.Minute % 10 + ASCII_ZERO;
	
	return timeString;
}


char* TimeSettingGetCurrentDateTimeString(void)
{
	TimeSettingGetCurrentTime();
	return TimeSettingGetTimeString(currentDateTime);
}


char* TimeSettingGetConfigDateTimeString(void)
{
	return TimeSettingGetTimeString(configDateTime);
}

//テスト
int TimeSettingTest(void)
{
	//*
	RX4111_DATE_TIME testTime = {0,0,0,0,0,0};
	char* testTimeString = NULL;
	bool sameFlg = false;
	
	//RTCから時刻取得
	testTime = TimeSettingGetCurrentTime();
	sameFlg = TimeSettingIsCurrentDateTimeSameAsOldDateTime();
	if(sameFlg) return 84;
	
	TimeSettingGetCurrentTime();
	sameFlg = TimeSettingIsCurrentDateTimeSameAsOldDateTime();
	//configDateTimeにコピー
	TimeSettingMoveCurrentTimetoConfigTime();
	//Configの時刻を確認。
	testTime = TimeSettingGetConfigTime();
	//文字列にする。
	testTimeString = TimeSettingGetTimeString(configDateTime);
	//secが0秒になっているか確認。
	TimeSettingSetConfigTime();
	if(configDateTime.Sec != SET_SEC) return 84;
	
	//年
	configDateTime.Year = 1;
 	TimeSettingAddYear(DIGIT_SECOND);
	TimeSettingAddYear(DIGIT_FIRST);
	if(configDateTime.Year != 12) return 84;

	//年　境界値
	configDateTime.Year = 99;
	TimeSettingAddYear(DIGIT_FIRST);
	if(configDateTime.Year != 0) return 84;
	
	//年　異常値
	configDateTime.Year = 4;
	configDateTime.Month = 2;
	configDateTime.Day = 29;
	TimeSettingAddYear(DIGIT_FIRST);
	if(configDateTime.Day != 28) return 84;
	
	//月
	configDateTime.Month = 1;
	configDateTime.Day = 1;
	TimeSettingAddMonth(DIGIT_SECOND);
	TimeSettingAddMonth(DIGIT_FIRST);
	if(configDateTime.Month != 12) return 84;
	
	//月 異常値
	configDateTime.Month = 3;
	configDateTime.Day = 31;
	TimeSettingAddMonth(DIGIT_FIRST);
	if(configDateTime.Day != 30) return 84;
	
	configDateTime.Year = 4;
	configDateTime.Month = 1;
	configDateTime.Day = 31;
	TimeSettingAddMonth(DIGIT_FIRST);
	if(configDateTime.Day != 29) return 84;
	
	configDateTime.Year = 4;
	configDateTime.Month = 1;
	configDateTime.Day = 30;
	TimeSettingAddMonth(DIGIT_FIRST);
	if(configDateTime.Day != 29) return 84;
	
 	configDateTime.Year = 5;
	configDateTime.Month = 1;
	configDateTime.Day = 31;
	TimeSettingAddMonth(DIGIT_FIRST);
	if(configDateTime.Day != 28) return 84;
	
	configDateTime.Year = 5;
	configDateTime.Month = 1;
	configDateTime.Day = 30;
	TimeSettingAddMonth(DIGIT_FIRST);
	if(configDateTime.Day != 28) return 84;
	
	configDateTime.Year = 5;
	configDateTime.Month = 1;
	configDateTime.Day = 29;
	TimeSettingAddMonth(DIGIT_FIRST);
	if(configDateTime.Day != 28) return 84;
	
	//日
	configDateTime.Day = 1;
 	TimeSettingAddDay(DIGIT_SECOND);
	TimeSettingAddDay(DIGIT_FIRST);
	if(configDateTime.Day != 12) return 84;
	
	//日　境界値
	configDateTime.Month = 1;
	configDateTime.Day = 31;
	TimeSettingAddDay(DIGIT_FIRST);
	if(configDateTime.Day != 1) return 84;
	
	configDateTime.Month = 4;
	configDateTime.Day = 30;
	TimeSettingAddDay(DIGIT_FIRST);
	if(configDateTime.Day != 1) return 84;
	
	configDateTime.Year = 4;
	configDateTime.Month = 2;
	configDateTime.Day = 29;
	TimeSettingAddDay(DIGIT_FIRST);
	if(configDateTime.Day != 1) return 84;
	
	configDateTime.Year = 5;
	configDateTime.Month = 2;
	configDateTime.Day = 28;
	TimeSettingAddDay(DIGIT_FIRST);
	if(configDateTime.Day != 1) return 84;
	
	//時
	configDateTime.Hour = 1;
	TimeSettingAddHour(DIGIT_SECOND);
	TimeSettingAddHour(DIGIT_FIRST);
	if(configDateTime.Hour != 12) return 84;
	
	//時 境界値
	configDateTime.Hour = 23;
	TimeSettingAddHour(DIGIT_FIRST);
	if(configDateTime.Hour != 0) return 84;
	
	//分
	configDateTime.Minute = 1;
	TimeSettingAddMinute(DIGIT_SECOND);
	TimeSettingAddMinute(DIGIT_FIRST);
	if(configDateTime.Minute != 12) return 84;
	
	//分 境界値
	configDateTime.Minute = 59;
	TimeSettingAddMinute(DIGIT_FIRST);
	if(configDateTime.Minute != 0) return 84;
	
	//*/
	return 1;
}

