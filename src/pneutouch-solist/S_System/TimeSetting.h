/*****************************************************************************
 * File: TimeSetting.h
 * Title: 時刻設定を行う。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file TimeSetting.h
 * @brief 時刻設定を行う。
 */
#ifndef TIME_SETTING_H__
#define TIME_SETTING_H__
#include "RX4111.h"
#include <stdbool.h>

/**
 * @brief 日付時刻の桁
 */
typedef enum
{
	DIGIT_FIRST = 1,	/**< 1桁目 */
	DIGIT_SECOND,		/**< 2桁目 */
}DIGIT;

/**
 * @brief 現在時刻と一つ前の時刻が同じかを分単位で確認する。
 *
 * @return bool 
 */
bool TimeSettingIsCurrentDateTimeSameAsOldDateTime(void);

/**
 * @brief システム上の現在時刻を取得する。
 *
 * @return RX4111_DATE_TIME
 */
RX4111_DATE_TIME TimeSettingGetConfigTime(void);

/**
 * @brief システム上の時刻をRTCに設定する。
 */
void TimeSettingSetConfigTime(void);

/**
 * @brief システム上の時刻をRTCの現在時刻に上書きする。
 */
void TimeSettingMoveCurrentTimetoConfigTime(void);

/**
 * @brief 年を1増やす。
 *
 * @param digit 桁
 */
void TimeSettingAddYear(DIGIT digit);

/**
 * @brief 月を1増やす。
 *
 * @param digit 桁
 */
void TimeSettingAddMonth(DIGIT digit);

/**
 * @brief 日を1増やす。
 *
 * @param digit 桁
 */
void TimeSettingAddDay(DIGIT digit);

/**
 * @brief 時を1増やす。
 *
 * @param digit 桁
 */
void TimeSettingAddHour(DIGIT digit);

/**
 * @brief 分を1増やす。
 *
 * @param digit 桁
 */
void TimeSettingAddMinute(DIGIT digit);

/**
 * @brief 時刻を文字にする。
 *
 * @param dateTime 時刻
 * @return char
 */
char* TimeSettingGetTimeString(RX4111_DATE_TIME dateTime);

/**
 * @brief 現在時刻を文字で取得する。
 *
 * @return char
 */
char* TimeSettingGetCurrentDateTimeString(void);

/**
 * @brief 設定中の時刻を文字で取得する。
 *
 * @return char
 */
char* TimeSettingGetConfigDateTimeString(void);

/**
 * @brief 単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int TimeSettingTest(void);

#endif //TIME_SETTING_H__
