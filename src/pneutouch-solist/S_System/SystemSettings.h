/*****************************************************************************
 * File: SystemSettings.h
 * Title: 設定を読み込んで適切な機能を注入する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file SystemSettings.h
 * @brief 設定を読み込んで適切な機能を注入する。
 */
#ifndef SYSTEM_SETTINGS_H__
#define SYSTEM_SETTINGS_H__

/**
 * @brief 機能を注入するための列挙型
 */
typedef enum
{
	SYSTEM_SETTINGS_PREDICT = 0,	/**< 推論画面 */
	SYSTEM_SETTINGS_LOG				/**< ログ */
}SYSTEM_SETTINGS;

/**
 * @brief 列挙型の機能を全て初期化する。
 */
void SystemSettingsInit(void);

/**
 * @brief 対象モジュールに対して適切な機能を登録、注入する。
 *
 * @param setting 登録するモジュールのindex
 */
void SystemSettingsRegister(SYSTEM_SETTINGS setting);

/**
 * @brief 単体テスト
 * 
 * @return int 成功時0。失敗時は0以外。
 */
int SystemSettingsTest(void);
#endif //SYSTEM_SETTINGS_H__
