/*****************************************************************************
 * File: UartQueue.h
 * Title: UARTで取り扱うデータを出し入れするキュー
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file UartQueue.h
 * @brief UARTで取り扱うデータを出し入れするキュー
 */

#ifndef UART_QUEUE_H__
#define UART_QUEUE_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief キューのサイズ
 */
#define NUMBER_OF_BUFFER_MAX				(257)


/**
 * @brief UART通信で使用するキューの構造体
 */
typedef struct
{
	uint8_t Queue[NUMBER_OF_BUFFER_MAX];	/**< キュー*/
	uint8_t Padding[3];						
	uint8_t* Start;							/**< 先頭 */
	uint8_t* End;							/**< 末尾 */
}QUEUE;


/**
 * @brief キューの数。受信用
 */
 #define NUMBER_OF_CH_M						(1)

 /**
 * @brief キューの数。割込み用
 */
 #define NUMBER_OF_CH_I						(1)
 
/**
 * @brief UARTキュー機能を初期化する。
 * 
 * @param ch キューの数。
 * @return bool chが0以外なら初期化失敗でfalseを返す。
 */
bool UartQueueInit(uint8_t ch);

/**
 * @brief データを受信用Queueにエンキューする。
 * 
 * @param data
 * @return bool 受信用Queueがfullならfalseを返す。
 */
bool UartQueueMainEnqueue(uint8_t data);

/**
 * @brief データを割込み用Queueにエンキューする。
 * 
 * @param data
 * @return bool 割込み用Queueがfullならfalseを返す。
 */
bool UartQueueInterruptEnqueue(uint8_t data);

/**
 * @brief 受信用Queueをデキューする。
 * 
 * @param data デキューしたデータを入れる変数
 * @return bool デキューが成功したらtrue
 */
bool UartQueueMainDequeue(uint8_t *data);

/**
 * @brief 割込み用Queueをデキューする。
 * 
 * @param data デキューしたデータを入れる変数
 * @return bool デキューが成功したらtrue
 */
bool UartQueueInterruptDequeue(uint8_t *data);

/**
 * @brief 受信用Queueがエンプティーか確認する。
 * 
 * @return bool 
 */
bool UartQueueIsMainEmpty(void);

/**
 * @brief 割込み用Queueがエンプティーか確認する。
 * 
 * @return bool 
 */
bool UartQueueIsInterruptEmpty(void);

/**
 * @brief 単体テスト
 * 
 * @return int テスト結果。成功時0。
 */
int UartQueueTest(void);
#endif //UART_QUEUE_H__
