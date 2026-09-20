/*****************************************************************************
 * File: SystemStateController.h
 * Title: システムの状態を制御する。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file SystemStateController.h
 * @brief システムの状態を制御する。
 */
#ifndef SYSTEM_STATE_CONTROLLER_H__
#define SYSTEM_STATE_CONTROLLER_H__
#include "Shutdown.h"
/**
 * @brief システムコントローラーの初期化を行う。
 */
void SystemStateControllerInit(void);

/**
 * @brief システムの状態の制御を行う。
 */
void SystemStateController(void);
#endif //SYSTEM_STATE_CONTROLLER_H__
