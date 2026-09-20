/*****************************************************************************
 * File: UartQueue.c
 * Title: UARTで取り扱うデータを出し入れするキュー
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file UartQueue.c
 * @brief UARTで取り扱うデータを出し入れするキュー
 */

#include "UartQueue.h"

#define NUMBER_OF_QUE_MAX	(256)

static QUEUE mainRcv[NUMBER_OF_CH_M];
static QUEUE interrupRcv[NUMBER_OF_CH_I];
static uint8_t chIndex;

static bool isMFull(void);
static bool isIFull(void);

bool UartQueueInit(uint8_t ch)
{
	if(ch != (NUMBER_OF_CH_M - 1)) return false;
	chIndex = ch;
	mainRcv[chIndex].Start = mainRcv[chIndex].Queue;
	mainRcv[chIndex].End = mainRcv[chIndex].Queue;
	interrupRcv[chIndex].Start = interrupRcv[chIndex].Queue;
	interrupRcv[chIndex].End = interrupRcv[chIndex].Queue;
	return true;
}

bool UartQueueIsMainEmpty(void)
{
	if(mainRcv[chIndex].Start == mainRcv[chIndex].End)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool UartQueueIsInterruptEmpty(void)
{
	if(interrupRcv[chIndex].Start == interrupRcv[chIndex].End)
	{
		return true;
	}
	else
	{
		return false;
	}
}

static bool isMFull(void)
{
	uint8_t* queueTerminal = mainRcv[chIndex].Queue;
	queueTerminal += NUMBER_OF_QUE_MAX;

	if(mainRcv[chIndex].Start == mainRcv[chIndex].Queue)
	{
		if(mainRcv[chIndex].End == (queueTerminal))
		{
			return true;
		}
	}
	else if((mainRcv[chIndex].Start - mainRcv[chIndex].End) == 1)
	{
		return true;
	}
	return false;
}

static bool isIFull(void)
{
	uint8_t* queueTerminal = interrupRcv[chIndex].Queue;
	queueTerminal += NUMBER_OF_QUE_MAX;

	if(interrupRcv[chIndex].Start == interrupRcv[chIndex].Queue)
	{
		if (interrupRcv[chIndex].End == (queueTerminal))
		{
			return true;
		}
	}
	else if((interrupRcv[chIndex].Start - interrupRcv[chIndex].End) == 1)
	{
		return true;
	}
	return false;
}

bool UartQueueMainEnqueue(uint8_t data)
{
	uint8_t* queueTerminal = mainRcv[chIndex].Queue;
	queueTerminal += NUMBER_OF_BUFFER_MAX;

	if(isMFull())
	{
		return false;
	}

	*(mainRcv[chIndex].End) = data;
	mainRcv[chIndex].End++;
	
	if(mainRcv[chIndex].End >= queueTerminal)
	{
		mainRcv[chIndex].End = mainRcv[chIndex].Queue;
	}
	return true;
}

bool UartQueueInterruptEnqueue(uint8_t data)
{
	uint8_t* queueTerminal = interrupRcv[chIndex].Queue;
	queueTerminal += NUMBER_OF_BUFFER_MAX;

	if(isIFull())
	{
		return false;
	}

	*(interrupRcv[chIndex].End) = data;
	interrupRcv[chIndex].End++;
	
	if(interrupRcv[chIndex].End >= queueTerminal)
	{
		interrupRcv[chIndex].End = interrupRcv[chIndex].Queue;
	}
	return true;
}


bool UartQueueMainDequeue(uint8_t *data)
{
	uint8_t* queueTerminal = mainRcv[chIndex].Queue;
	queueTerminal += NUMBER_OF_BUFFER_MAX;
	
	if (UartQueueIsMainEmpty())
	{
		return false;
	}
	
	*data = *(mainRcv[chIndex].Start);
	mainRcv[chIndex].Start++;
	if (mainRcv[chIndex].Start >= queueTerminal )
	{
		mainRcv[chIndex].Start = mainRcv[chIndex].Queue;
	}
	return true;
}

bool UartQueueInterruptDequeue(uint8_t *data)
{
	uint8_t* queueTerminal = interrupRcv[chIndex].Queue;
	queueTerminal += NUMBER_OF_BUFFER_MAX;
	
	if (UartQueueIsInterruptEmpty())
	{
		return false;
	}

	*data = *(interrupRcv[chIndex].Start);
	interrupRcv[chIndex].Start++;
	
	if (interrupRcv[chIndex].Start >= queueTerminal )
	{
		interrupRcv[chIndex].Start = interrupRcv[chIndex].Queue;
	}
	return true;
}

int UartQueueTest(void)
{
	//初期化
	uint8_t mainData = 0;
	uint8_t interruptData = 0;
	if(UartQueueInit(1) != false) return -1;
	if(UartQueueInit(255) != false) return -1;
	if(UartQueueInit(0) != true) return -1;
	if(UartQueueIsMainEmpty() != true) return -1;
	if(UartQueueIsInterruptEmpty() != true) return -1;
	if(isMFull() != false) return -1;
	if(isIFull() != false) return -1;
	if(UartQueueMainDequeue(&mainData) != false) return -1;
	if(UartQueueInterruptDequeue(&interruptData) != false) return -1;
	
	//エンキュー
	for(int i = 0; i < (NUMBER_OF_BUFFER_MAX - 1); i++)
	{
		if(UartQueueMainEnqueue(mainData++) != true) return -1;
		if(UartQueueInterruptEnqueue(interruptData++) != true) return -1;
	}
	//フル
	if(UartQueueMainEnqueue(mainData) != false) return -1;
	if(UartQueueInterruptEnqueue(interruptData) != false) return -1;
	
	//デキュー
	for(int i = 0; i < (NUMBER_OF_BUFFER_MAX - 1); i++)
	{
		if(UartQueueMainDequeue(&mainData) != true) return -1;
		if(UartQueueInterruptDequeue(&interruptData) != true) return -1;
	}
	//エンプティー
	if(UartQueueMainDequeue(&mainData) != false) return -1;
	if(UartQueueInterruptDequeue(&interruptData) != false) return -1;
	
	return 0;
}

