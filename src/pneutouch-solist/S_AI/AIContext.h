/*****************************************************************************
 * File: AIContext.h
 * Title: AI関連のデータコンテキスト
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file AIContext.h
 * @brief AI関連のデータコンテキスト
 * @details 
 * AIに関する生成データをまとめた構造体が定義されており、この構造体を通じてデータのやりとりを行う事で
 * コピーレスなやり取りを行う事を目的としています。
 */

#ifndef AI_CONTEXT_H__
#define	AI_CONTEXT_H__

#include "solistAi.h"

/**
 * @brief 入力データ数
 */
#define AI_CONTEXT_INPUT_SOURCE_SIZE (512)

/**
 * @brief FFT出力データ数
 */
#define AI_CONTEXT_FFT_OUTPUT_SIZE (256)

/**
 * @brief ログ要因
 */
typedef enum
{
	LOG_FACTOR_UNUSE = 0,			/**< ログが未使用 */
	LOG_FACTOR_END = 1,				/**< 学習または推論が終了した時 */
	LOG_FACTOR_WARNING_RED = 2,		/**< 赤色警告が発生した時 */
	LOG_FACTOR_WARNING_YELLOW = 3,	/**< 黄色警告が発生した時 */
	LOG_FACTOR_SENTINEL = 0xFFFF	/**< 番兵。このenumを2バイト型にするため0xFFFFを割り当てている */
} LOG_FACTOR;

/**
 * @brief ログ保存したかどうか
 */
typedef enum
{
	LOG_SAVE_NO = 0,				/**< 未保存 */
	LOG_SAVE_YES = 1,				/**< 保存済み */
	LOG_SAVE_SENTINEL = 0xFFFF		/**< 番兵。このenumを2バイト型にするため0xFFFFを割り当てている */
} LOG_SAVE;

/**
 * @brief 月、日、時間をビット単位で割り付けた値
 */
typedef struct
{
	uint16_t Hour : 5;				/**< 時間 */
	uint16_t Day : 5;				/**< 日 */
	uint16_t Month : 5;				/**< 月 */
	uint16_t : 1;					/**< 予約 */
} AI_CONTEXT_MONTH_DAY_HOUR;

/**
 * @brief 分、秒をビット単位で割り付けた値
 */
typedef struct
{
	uint16_t Second : 6;			/**< 秒 */
	uint16_t Minute : 6;			/**< 分 */
	uint16_t : 4;					/**< 予約 */
} AI_CONTEXT_MINUTE_SECOND;

/**
 * @brief 推論バッファ
 */
typedef struct
{
	uint16_t ChunkNo;							/**< チャンク番号 */
	uint16_t Reserve1;							/**< 予約 */
	uint16_t Reserve2;							/**< 予約 */
	uint16_t Reserve3;							/**< 予約 */
	bfloat16 Fft[AI_CONTEXT_FFT_OUTPUT_SIZE];	/**< FFT出力データ。先頭はDC成分を表す */
	bfloat16 Anomaly;							/**< 異常値 */
} PREDICT_BUFFER;

/**
 * @brief ログ情報
 */
#pragma pack(push, 2)
typedef struct
{
	LOG_FACTOR Factor;									/**< ログ要因 */
	uint16_t Year;										/**< 年 */
	AI_CONTEXT_MONTH_DAY_HOUR MonthDayHour; 			/**< 月、日、時間 */
	AI_CONTEXT_MINUTE_SECOND MinuteSecond;				/**< 分、秒 */
	int16_t InputData[AI_CONTEXT_INPUT_SOURCE_SIZE];	/**< 入力データ */
	PREDICT_BUFFER Predict;								/**< 推論バッファ */
} LOG_INFO;
#pragma pack(pop)

/**
 * @brief AI周りのデータをやり取りするコンテキスト
 */
typedef struct
{
	LOG_INFO LogInfo;	/**< ログ情報 */
	LOG_SAVE LogSave;	/**< ログ保存状態 */
} AI_CONTEXT;

#endif //AI_CONTEXT_H__

