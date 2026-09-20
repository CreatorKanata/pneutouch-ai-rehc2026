/*****************************************************************************
 * File: main.c
 * Title: PneutouchAI
 * LastUpdated: 2026.09.20
 * Copyright (C) 2026 Kanata the Kid Creator
 ******************************************************************************/
/* Live pressure -> 12 features -> Solist-AI -> LCD for 3 seconds. */
#include "../S_PneuTouch/pressure_app.h"

int main(void)
{
    pressure_app_init();
    for (;;) pressure_app_poll();
}
