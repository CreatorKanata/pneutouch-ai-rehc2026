/*****************************************************************************
 * File: HighSpeedCom.h
 * Title: 高速通信
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file HighSpeedCom.h
 * @brief 高速通信
 * @details 
 * センサー波形をリアルタイムに観察するなど、高速性を要求される通信に使用するモジュールです。
 */

#ifndef HIGH_SPEED_COM_H__
#define HIGH_SPEED_COM_H__

#include <stdint.h>

/**
 * @brief 高速通信の結果を示す列挙型
 */
typedef enum
{
	HIGH_SPEED_COM_RESULT_NORMAL = 0,		/**< 正常 */
	HIGH_SPEED_COM_RESULT_OVERFLOW,			/**< オーバーフロー状態 */
	HIGH_SPEED_COM_RESULT_INVALID_BLOCK_NO,	/**< 不正なブロック番号の指定 */
	HIGH_SPEED_COM_RESULT_BUSY,				/**< ビジー状態 */
} HIGH_SPEED_COM_RESULT;

/**
 * @brief ログデータ要求のコールバック関数
 * 
 * @param logIndex 要求するログのインデックス値
 * @param data 取得できたログデータへのポインタ格納先
 * @param dataSize 取得できたログデータのサイズ格納先（バイト単位）
 * @return int ログデータ取得成功時は0、失敗時は-1を返す
 */
typedef int (*HighSpeedComRequestLogDataCallback) (uint16_t logIndex, void** data, int* dataSize);

/**
 * @brief ログデータ要求のコールバック関数を設定する
 * 
 * @param func 設定するコールバック関数
 */
void HighSpeedComSetCallbackRequestLogData(HighSpeedComRequestLogDataCallback func);

/**
 * @brief 高速通信で使用するペリフェラルを初期化する
 */
void HighSpeedComPeripheralInit(void);

/**
 * @brief メインループで行う処理
 */
void HighSpeedComMain(void);

/**
 * @brief 高速通信モジュールを開始する
 */
void HighSpeedComStart(void);

/**
 * @brief リアルタイムデータを追加する
 * 
 * @param data 追加するデータ
 * @return HIGH_SPEED_COM_RESULT NORMAL, OVERFLOW のいずれかが返る
 */
HIGH_SPEED_COM_RESULT HighSpeedComInputRtData(uint16_t data);

/**
 * @brief リアルタイムデータバッファがオーバーフロー状態か取得する
 * 
 * @return HIGH_SPEED_COM_RESULT NORMAL, OVERFLOW のいずれかが返る
 */
HIGH_SPEED_COM_RESULT HighSpeedComRtOverflow(void);

/**
 * @brief ブロックデータの送信を要求する
 * 
 * @param src 送信対象データの先頭へのポインタ
 * @param wordCount 送信データ数(Word単位)
 * @param blockNo 1から始まるブロック番号
 * @return HIGH_SPEED_COM_RESULT NORMAL, INVALID_BLOCK_NO, BUSY のいずれかが返る
 */
HIGH_SPEED_COM_RESULT HighSpeedComSendBlock(void* src, int wordCount, int blockNo);

/**
 * @brief 指定ブロックのブロック転送が行われている最中か取得する
 * 
 * @param blockNo 1から始まるブロック番号
 * @return HIGH_SPEED_COM_RESULT NORMAL, INVALID_BLOCK_NO, BUSY のいずれかが返る
 */
HIGH_SPEED_COM_RESULT HighSpeedComBlockIsBusy(int blockNo);

#endif // HIGH_SPEED_COM_H__
