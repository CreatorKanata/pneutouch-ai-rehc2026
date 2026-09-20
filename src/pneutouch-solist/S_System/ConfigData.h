/*****************************************************************************
 * File: ConfigData.h
 * Title: 設定データ
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file ConfigData.h
 * @brief 設定データ
 */

#ifndef CONFIG_DATA_H__
#define CONFIG_DATA_H__

#include <stdint.h>
#include "solistAi.h"	// bfloat16の定義があるため

/**
 * @brief このモジュールが扱う文字列の最大文字数
 * 
 * 文字列を指定する関数は、この文字数以内の文字列を入力する。
 * 文字列を受け取る関数は、この文字数+1以上のサイズを持った配列を指定する必要がある。
 */
#define CONFIG_DATA_MAX_STRING_LENGTH	(14)

/**
 * @brief 有効なIDの最小値
 */
#define MINIMUM_NUMBER_OF_ID			(EN_CONFIG_USE_SENSOR)
/**
 * @brief 有効なIDの最大値
 */
#define MAXIMUM_NUMBER_OF_ID			(EN_CONFIG_END - 1)

/**
 * @brief 設定項目を示す列挙型
 */
typedef enum{
	EN_CONFIG_USE_SENSOR,				/**< 入力ソースの設定。0:アナログ入力 1:MEMSセンサー入力 */

	EN_CONFIG_IEPE_SAMPLING_FREQUENCY,	/**< IEPEセンサーのサンプリング周波数 */
										/**< 7:100 8:200 9:400 10:800 11:1600 12:3200 */
										/**< 13:6400 14:12800 15:25600 */
	EN_CONFIG_IEPE_SAMPLING_NUM,		/**< IEPEセンサーから入力するデータ数 */
	EN_CONFIG_IEPE_GAIN,				/**< bfloat16変換時の固定小数点の位置 0～15 */

	EN_CONFIG_MEMS_DATA_KIND,			/**< MEMSセンサーから入力するデータの種類 */
										/**< 0:x軸、1:y軸、2:z軸 */
	EN_CONFIG_MEMS_SAMPLING_FREQUENCY,	/**< MEMSセンサーのサンプリング周波数 */
										/**< 7:100 8:200 9:400 10:800 11:1600 12:3200 */
										/**< 13:6400 14:12800 15:25600 */
	EN_CONFIG_MEMS_SAMPLING_NUM,		/**< MEMSセンサーから入力するデータ数 */
	EN_CONFIG_MEMS_GAIN,				/**< bfloat16変換時の固定小数点の位置 0～15 */
	EN_CONFIG_MEMS_LPF,					/**< MEMSセンサー内蔵のLPFの設定 0:ODR/9 1:ODR/2 */

	EN_CONFIG_OVERLAP,					/**< オーバーラップするか。センサー共通設定。 0:なし 1:あり */

	EN_CONFIG_FFT_ENABLE,				/**< FFTを行うか 0:行わない 1:行う */
	EN_CONFIG_FFT_SKIP,					/**< FFTした結果をAIに与える時に省くデータ数を設定 0～255*/
	EN_CONFIG_WINDOW,					/**< 窓関数 0:窓関数をかけない 1:ハニング窓をかける */
	EN_CONFIG_HIDDEN_LAYER_NUM,			/**< 隠れ層ノード数 */
	EN_CONFIG_FORGET_RATE,				/**< 忘却率 */
	EN_CONFIG_ACTIVATION_FUNC,			/**< 活性化関数。0:LINEAR 1:SIGMOID 2:RELU */
	EN_CONFIG_LOSS_FUNC,				/**< 損失関数。0:MAE 1:MSE */
	EN_CONFIG_WEIGHT_A_RANDOM_SEED,		/**< 疑似乱数のシード値 */

	EN_CONFIG_AI_MODE,					/**< AIの動作モードの設定 */
										/**< 0:初期学習後推論のみ行う 1:初期学習後逐次学習(学習と推論を繰り返す)を行う */
	EN_CONFIG_AI_LEARN_NUM,				/**< 初期学習の回数 */
	EN_CONFIG_AI_CALC_THRESHOLD,		/**< 閾値計算方法 */
										/**< 0:学習、推論で出力した異常値の最大値 */
										/**< 1:学習、推論で出力した異常値の平均+3σ */
										/**< 2:ユーザーからの入力 */
	EN_CONFIG_AI_FAR_THRESHOLD,			/**< 外れ値の閾値 */
	EN_CONFIG_AVELAGE_ENABLE,			/**< FFT後のスペクトルの平均の設定 0:しない 1:する */
	EN_CONFIG_THINNING,					/**< AIの学習・推論の間引き回数 0:1/1 1:1/2 2:1/3 */
	EN_CONFIG_PREDICT_LAST_ONCE,		/**< 推論ボタンを押している時に、最後に一度だけ推論するかどうか 0:しない 1:する */
	EN_CONFIG_AI_ABNORMAL_AVERAGE,		/**< AIの異常値を平均する回数 */
	EN_CONFIG_ABNORMAL_FORMAT,			/**< 異常度出力形式 0:偏差値出力 1:デジタル値出力 */
	EN_CONFIG_YELLOW_THRESHOLD,			/**< 黄色警告閾値 */
	EN_CONFIG_RED_THRESHOLD,			/**< 赤色警告閾値 */
	EN_CONFIG_MAX_ABNORMAL,				/**< 最大異常度 */
	EN_CONFIG_WARNING_LATCH,			/**< 警告ラッチ 0:しない 1:赤色と黄色警告をラッチする */
	EN_CONFIG_TOGGLE,					/**< トグルを 0:しない 1:する */
	EN_CONFIG_REALTIME_COM,				/**< 高速リアルタイム転送を 0:しない 1:する */
	EN_CONFIG_BLOCK_COM,				/**< 高速ブロック転送を 0:しない 1:する */
	EN_CONFIG_SAVE_LOG,					/**< ログ保存機能 */
										/**< 0:ログを録らない 1:学習・推論終了時にログを録る */
										/**< 2:赤色警告検出時もログを録る 3:黄色警告検出時もログを録る */
	EN_CONFIG_END,						/**< この列挙体の番兵 */
} EN_CONFIG;

/**
 * @brief 設定データの型を示す列挙型
 */
typedef enum
{
	EN_CONFIG_TYPE_UINT8_T,		/**< uint8_t型 */
	EN_CONFIG_TYPE_UINT16_T,	/**< uint16_t型 */
	EN_CONFIG_TYPE_INT16_T,		/**< int16_t型 */
	EN_CONFIG_TYPE_BFLOAT,		/**< bfloat型 */
	EN_CONFIG_TYPE_UNKNOWN,		/**< 不明な型 */
} EN_CONFIG_TYPE;

/**
 * @brief 設定データの結果を示す列挙型
 */
typedef enum
{
	CONFIG_DATA_RESULT_NORMAL,						/**< 正常 */
	CONFIG_DATA_RESULT_ID_OUT_OF_RANGE,				/**< IDが範囲外 */
	CONFIG_DATA_RESULT_VALUE_OUT_OF_RANGE,			/**< 設定値が範囲外 */
	CONFIG_DATA_RESULT_INVALID_STRING,				/**< 不正な文字列 */
	CONFIG_DATA_RESULT_SMALL_STRING_BUFFER_SIZE,	/**< 文字列バッファのサイズが小さい */
} CONFIG_DATA_RESULT;

/**
 * @brief 指定したidのデータタイプを取得する
 * 
 * 指定したidが無効な場合は、EN_CONFIG_TYPE_UNKNOWNを返す。
 * 
 * @param id 設定項目を示すID
 * @return EN_CONFIG_TYPE データタイプ
 */
EN_CONFIG_TYPE ConfigDataGetConfigType(EN_CONFIG id);

/**
 * @brief ConfigDataを初期化し、デフォルト値に戻す
 */
void ConfigDataClear(void);

/**
 * @brief Group0に含まれる設定値を、永続化ストレージからロードする
 * 
 * @return int 成功時0、失敗時-1
 */
int ConfigDataLoadConfigGroup0(void);

/**
 * @brief Group0に含まれる設定値を、永続化ストレージにセーブする
 * 
 * @return int 成功時0、失敗時-1
 */
int ConfigDataSaveConfigGroup0(void);

/**
 * @brief 設定値を設定する。データタイプが EN_CONFIG_TYPE_UINT8_T に限る
 * 
 * @param id 設定する項目を示すID
 * @param data 設定する値
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataSetUint8Value(EN_CONFIG id, uint8_t data);

/**
 * @brief 設定値を設定する。データタイプが EN_CONFIG_TYPE_UINT16_T に限る
 * 
 * @param id 設定する項目を示すID
 * @param data 設定する値
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataSetUint16Value(EN_CONFIG id, uint16_t data);

/**
 * @brief 設定値を設定する。データタイプが EN_CONFIG_TYPE_INT16_T に限る
 * 
 * @param id 設定する項目を示すID
 * @param data 設定する値
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataSetInt16Value(EN_CONFIG id, int16_t data);

/**
 * @brief 設定値を設定する。データタイプが EN_CONFIG_TYPE_BFLOAT に限る
 * 
 * @param id 設定する項目を示すID
 * @param data 設定する値
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataSetBfloat16Value(EN_CONFIG id, float data);

/**
 * @brief 設定値を取得する。データタイプが EN_CONFIG_TYPE_UINT8_T に限る
 * 
 * @param id 取得する項目を示すID
 * @param data 設定値の格納先
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataGetUint8Value(EN_CONFIG id, uint8_t* data);

/**
 * @brief 設定値を取得する。データタイプが EN_CONFIG_TYPE_UINT16_T に限る
 * 
 * @param id 取得する項目を示すID
 * @param data 設定値の格納先
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataGetUint16Value(EN_CONFIG id, uint16_t* data);

/**
 * @brief 設定値を取得する。データタイプが EN_CONFIG_TYPE_INT16_T に限る
 * 
 * @param id 取得する項目を示すID
 * @param data 設定値の格納先
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataGetInt16Value(EN_CONFIG id, int16_t* data);

/**
 * @brief 設定値を取得する。データタイプが EN_CONFIG_TYPE_BFLOAT に限る
 * 
 * @param id 取得する項目を示すID
 * @param data 設定値の格納先
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataGetBfloat16Value(EN_CONFIG id, bfloat16* data);

/**
 * @brief 設定値を取得する。データタイプが EN_CONFIG_TYPE_BFLOAT に限る
 * 
 * @param id 取得する項目を示すID
 * @param data 設定値の格納先
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataGetFloatValue(EN_CONFIG id, float* data);

////////////////////////
// 文字列系アクセサ

/**
 * @brief 設定値を10進数文字列で設定する
 * 
 * @param id 設定する項目を示すID
 * @param str 設定する値を示す文字列
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataSetUint8ValueString(EN_CONFIG id, const char* str);

/**
 * @brief 設定値を10進数文字列で設定する
 * 
 * @param id 設定する項目を示すID
 * @param str 設定する値を示す文字列
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataSetUint16ValueString(EN_CONFIG id, const char* str);

/**
 * @brief 設定値を10進数文字列で設定する
 * 
 * @param id 設定する項目を示すID
 * @param str 設定する値を示す文字列
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataSetInt16ValueString(EN_CONFIG id, const char* str);

/**
 * @brief 設定値を小数点または指数表記文字列として設定する
 * 
 * @param id 設定する項目を示すID
 * @param str 設定する値を示す文字列
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataSetBfloat16ValueString(EN_CONFIG id, const char* str);

/**
 * @brief 設定値を10進数文字列として取得する
 * 
 * @param id 取得する項目を示すID
 * @param str 取得した設定値を示す文字列
 * @param strSize 格納先となるstrのサイズ
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataGetUint8ValueString(EN_CONFIG id, char* str, int strSize);

/**
 * @brief 設定値を10進数文字列として取得する
 * 
 * @param id 取得する項目を示すID
 * @param str 取得した設定値を示す文字列
 * @param strSize 格納先となるstrのサイズ
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataGetUint16ValueString(EN_CONFIG id, char* str, int strSize);

/**
 * @brief 設定値を10進数文字列として取得する
 * 
 * @param id 取得する項目を示すID
 * @param str 取得した設定値を示す文字列
 * @param strSize 格納先となるstrのサイズ
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataGetInt16ValueString(EN_CONFIG id, char* str, int strSize);

/**
 * @brief 設定値を小数点または指数表記文字列として取得する
 * 
 * @param id 取得する項目を示すID
 * @param str 取得した設定値を示す文字列
 * @param strSize 格納先となるstrのサイズ
 * @return CONFIG_DATA_RESULT 実行結果
 */
CONFIG_DATA_RESULT ConfigDataGetBfloat16ValueString(EN_CONFIG id, char* str, int strSize);

/**
 * @brief 単体テスト
 * 
 * @return int テスト結果。成功時0
 */
int ConfigDataTest(void);

#endif // CONFIG_DATA_H__
