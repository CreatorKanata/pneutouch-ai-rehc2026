/*****************************************************************************
 * File: Output.c
 * Title: 汎用出力を使う。
 * LastUpdated: 2025.05.23
 * Copyright (C) 2025 DATA TECNO Co., Ltd.
******************************************************************************/

/**
 * @file Output.c
 * @brief 汎用出力を使う。
 */


#include "Output.h"
#include <stdio.h>
#include "rdwr_reg.h"

//#define TEST
#ifdef TEST
//リトルエンディアン
//組込みにおいてオーバーフロー検出用に配列を用いるのはあまり良くない。
//なぜなら、CPUによってはアライメントでエラーが発生する場合があるから。
//例えば、Cortex-M0+ではuint16_tのデータは偶数、uint32_tは4の倍数のアドレスからしか書き込めない。
//組込みではunionを使う方がいい。

#define OUTPUT_TEST_UINT8_VALUE		(0x00000010)
#define OUTPUT_TEST_UINT16_VALUE	(0x00001010)
#define OUTPUT_TEST_UINT32_VALUE	(0x10101010)
#define OUTPUT_TEST_RESULT_ON		(0x10)
#define OUTPUT_TEST_RESULT_OFF		(0x00)


typedef union
{
	uint8_t Array[4];
	uint8_t UI8test;
	uint16_t UI16test;
	uint32_t UI32test;
}OUTPUT_TEST_UNION;

static OUTPUT_TEST_UNION testUnionData;
//#define TEST_DATA_MAX 			(10)
//static uint8_t outputTestData[TEST_DATA_MAX] = {0,1,2,3,4,5,6,7,8,9};
#endif

OUTPUT_STATUS OutputOnUInt8(volatile void* dst, uint8_t value)
{
	volatile uint8_t* output = (volatile uint8_t *)dst;
	if(output == NULL) return OUTPUT_STATUS_NULL;
	*output |= value;
	return OUTPUT_STATUS_ON;
}

OUTPUT_STATUS OutputOffUInt8(volatile void* dst, uint8_t value)
{
	volatile uint8_t* output = (volatile uint8_t *)dst;
	if(output == NULL) return OUTPUT_STATUS_NULL;
	*output &= ~value;
	return OUTPUT_STATUS_OFF;
}


OUTPUT_STATUS OutputOnUInt16(volatile void* dst, uint16_t value)
{
	volatile uint16_t* output = (volatile uint16_t *)dst;
	if(output == NULL) return OUTPUT_STATUS_NULL;
	*output |= value;
	return OUTPUT_STATUS_ON;
}

OUTPUT_STATUS OutputOffUInt16(volatile void* dst, uint16_t value)
{
	volatile uint16_t* output = (volatile uint16_t *)dst;
	if(output == NULL) return OUTPUT_STATUS_NULL;
	*output &= ~value;
	return OUTPUT_STATUS_OFF;
}


OUTPUT_STATUS OutputOnUInt32(volatile void* dst, uint32_t value)
{
	volatile uint32_t* output = (volatile uint32_t *)dst;
	if(output == NULL) return OUTPUT_STATUS_NULL;
	*output |= value;
	return OUTPUT_STATUS_ON;
}

OUTPUT_STATUS OutputOffUInt32(volatile void* dst, uint32_t value)
{
	volatile uint32_t* output = (volatile uint32_t *)dst;
	if(output == NULL) return OUTPUT_STATUS_NULL;
	*output &= ~value;
	return OUTPUT_STATUS_OFF;
}


#ifdef TEST
static void clearTestData(void)
{
	testUnionData.UI32test = 0;
}

static bool checkParameter( uint8_t  value0, uint8_t  value1, uint8_t  value2, uint8_t  value3)
{
	if( testUnionData.Array[0] != value0 ) return -1;
	if( testUnionData.Array[1] != value1 ) return -1;
	if( testUnionData.Array[2] != value2 ) return -1;
	if( testUnionData.Array[3] != value3 ) return -1;
	return 0;
}
#endif

int OutputTest(void)
{
#ifdef TEST
	//8bit 
	//初期化
	clearTestData();
	if(checkParameter(0,0,0,0)) return -1;
	//On
	OutputOnUInt8(&testUnionData,OUTPUT_TEST_UINT8_VALUE);
	if(checkParameter(OUTPUT_TEST_RESULT_ON,0,0,0)) return -1;

	//On
	OutputOnUInt8(&testUnionData,OUTPUT_TEST_UINT8_VALUE);
	if(checkParameter(OUTPUT_TEST_RESULT_ON,0,0,0)) return -1;
	
	//Off
	OutputOffUInt8(&testUnionData,OUTPUT_TEST_UINT8_VALUE);
	if(checkParameter(OUTPUT_TEST_RESULT_OFF,0,0,0)) return -1;

	//Off
	OutputOffUInt8(&testUnionData,OUTPUT_TEST_UINT8_VALUE);
	if(checkParameter(OUTPUT_TEST_RESULT_OFF,0,0,0)) return -1;
	
	//16bit
	clearTestData();
	if(checkParameter(0,0,0,0)) return -1;
	//On
	OutputOnUInt16(&testUnionData,OUTPUT_TEST_UINT16_VALUE);
	if(checkParameter( 0 + OUTPUT_TEST_RESULT_ON, 0 + OUTPUT_TEST_RESULT_ON,0,0)) return -1;

	//On
	OutputOnUInt16(&testUnionData,OUTPUT_TEST_UINT16_VALUE);
	if(checkParameter( 0 + OUTPUT_TEST_RESULT_ON, 0 + OUTPUT_TEST_RESULT_ON,0,0)) return -1;
	
	//Off
	OutputOffUInt16(&testUnionData,OUTPUT_TEST_UINT16_VALUE);
	if(checkParameter( 0 + OUTPUT_TEST_RESULT_OFF, 0 + OUTPUT_TEST_RESULT_OFF,0,0)) return -1;

	//Off
	OutputOffUInt16(&testUnionData,OUTPUT_TEST_UINT16_VALUE);
	if(checkParameter( 0 + OUTPUT_TEST_RESULT_OFF, 0 + OUTPUT_TEST_RESULT_OFF,0,0)) return -1;
	
	
	//32bit 
	//初期化
	clearTestData();
	if(checkParameter(0,0,0,0)) return -1;
	//On
	OutputOnUInt32(&testUnionData,OUTPUT_TEST_UINT32_VALUE);
	if(checkParameter( 0 + OUTPUT_TEST_RESULT_ON, 0 + OUTPUT_TEST_RESULT_ON, 0 + OUTPUT_TEST_RESULT_ON, 0 + OUTPUT_TEST_RESULT_ON)) return -1;

	//On
	OutputOnUInt32(&testUnionData,OUTPUT_TEST_UINT32_VALUE);
	if(checkParameter( 0 + OUTPUT_TEST_RESULT_ON, 0 + OUTPUT_TEST_RESULT_ON, 0 + OUTPUT_TEST_RESULT_ON, 0 + OUTPUT_TEST_RESULT_ON)) return -1;
	
	//Off
	OutputOffUInt32(&testUnionData,OUTPUT_TEST_UINT32_VALUE);
	if(checkParameter( 0 + OUTPUT_TEST_RESULT_OFF,0 + OUTPUT_TEST_RESULT_OFF, 0 + OUTPUT_TEST_RESULT_OFF, 0 + OUTPUT_TEST_RESULT_OFF)) return -1;

	//Off
	OutputOffUInt32(&testUnionData,OUTPUT_TEST_UINT32_VALUE);
	if(checkParameter( 0 + OUTPUT_TEST_RESULT_OFF,0 + OUTPUT_TEST_RESULT_OFF, 0 + OUTPUT_TEST_RESULT_OFF, 0 + OUTPUT_TEST_RESULT_OFF)) return -1;
#endif
	return 0;
}
