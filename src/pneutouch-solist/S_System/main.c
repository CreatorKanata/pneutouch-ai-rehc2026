/*****************************************************************************
 * File: main.c
 * Title: AIVibrationInference
 * LastUpdated: 2026.03.16
 * Copyright (C) 2025 - 2026 DATA TECNO Co., Ltd.
******************************************************************************/

/* PneuTouch replaces the vibration demo entry point. The original working
   demo is preserved in commit 95a453e. Use the same Debug / Write launches. */
#include "../S_PneuTouch/pressure_app.h"

int main(void)
{
    pressure_app_init();
    for (;;) pressure_app_poll();
}
