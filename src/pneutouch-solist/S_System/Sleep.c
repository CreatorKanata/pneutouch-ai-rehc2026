/*****************************************************************************
 * File: Sleep.c
 * Title: Sleep機能を使用する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file Sleep.c
 * @brief Sleep機能を使用する。
 */

#include "Sleep.h"
#include "lp_manage.h"
void SleepChangetoHaltMode(void)
{
	lp_setHaltMode();
}
