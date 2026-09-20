/*****************************************************************************
 * File: SensorCommon.h
 * Title: センサー共通の定義を含むヘッダファイル
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file SensorCommon.h
 * @brief センサー共通の定義を含むヘッダファイル
 */

#ifndef SENSOR_COMMON_H__
#define SENSOR_COMMON_H__

/**
 * @brief バッファサイズ分のデータが貯まった時に呼ばれるコールバック関数型
 */
typedef void (*SensorBufferFullCallbackFunc)(void);

#endif // SENSOR_COMMON_H__
