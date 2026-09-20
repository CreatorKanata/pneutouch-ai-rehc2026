/*****************************************************************************
 * File: Fram.c
 * Title: FRAMモジュールのインターフェース
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file Fram.c
 * @brief FRAMモジュールのインターフェース
 * @note 本モジュールの複数バイトの読み書きはCPUのエンディアンで行う。
 */

#include "Fram.h"
#include "SoftSpi.h"
#include "mcu.h"
#include "rdwr_reg.h"

/*############################################################################*/
/*#                                  Macro                                   #*/
/*############################################################################*/

#define SET_PORT_OUTPUT_WP()	( set_bit( PORT3->P3MOD0, (1 << 1U) ) )
#define WP_OFF()				( set_bit( PORT3->P3DO, (1 << 0U) ) )
#define WP_ON()					( clear_bit( PORT3->P3DO, (1 << 0U) ) )

typedef enum
{
	WP_STATUS_ON = 0,
	WP_STATUS_OFF = 1,
}WP_STATUS;

typedef enum
{
	WP_INSPECTION_RESULT_FAIL = 0,
	WP_INSPECTION_RESULT_PASS = 1,
}WP_INSPECTION_RESULT;

/*############################################################################*/
/*#                                Variable                                  #*/
/*############################################################################*/

/*############################################################################*/
/*#                               Prototype                                  #*/
/*############################################################################*/


/*############################################################################*/
/*#                              Subroutine                                  #*/
/*############################################################################*/

/**
 * @brief ライトイネーブルコマンドを送信する。
 */
static void sendCommandWREN(void);

/**
 * @brief ライトディスエーブルコマンドを送信する。
 */
static void sendCommandWRDI(void);

/**
 * @brief ステータスレジスタが全てアンプロテクトになるようなWRSRコマンドを送信する。
 */
static void sendCommandWrsrUnprotect(void);

/**
 * @brief ステータスレジスタに任意の値を書きこむWRSRコマンドを送信する。
 *
 * @param value コマンド値
 */
static void sendCommandWRSR(const uint8_t value);

/**
 * @brief ステータスレジスタリードを送信しステータスを読み込む。
 *
 * @return unsined char 読み込んだステータスレジスタ
 */
static unsigned char readStatus(void);

/**
 * @brief リードコマンドとアドレスを送信する。
 *
 * @note この関数内ではCSは操作しないのであらかじめCSをEnableする。\n
 *       リードが終わったらDisableする事。
 */
static void sendCommandRead(uint32_t address);

/**
 * @brief ライトコマンドとアドレスを送信する。
 *
 * @note この関数内ではCSは操作しないのであらかじめCSをEnableする。\n
 *       リードが終わったらDisableする事。
 */
static void sendCommandWrite(uint32_t address);

// wp検査プログラムのテスト
static WP_INSPECTION_RESULT checkWp(WP_STATUS wpStatus);
static WP_INSPECTION_RESULT testWp(WP_STATUS wpStatus);
/*############################################################################*/
/*#                                  API                                     #*/
/*############################################################################*/

void FramInit(void)
{
	SET_PORT_OUTPUT_WP();
	WP_OFF();
	
	//volatile char err = 0;				// test
	//volatile unsigned char st =	0;		// test
	//st = readStatus();					// test
	//if((st & 0x02) != 0){ err = 1; }	// test
	sendCommandWREN();
	//st = readStatus();					// test
	//if((st & 0x02) == 0){ err = 1; }	// test
	sendCommandWrsrUnprotect();
	//st = readStatus();					// test
	//if(st != 0x02){ err = 1; }			// test
	sendCommandWRDI();
	//st = readStatus();					// test
	//if(st != 0x00){ err = 1; }			// test
}

// Framから1バイト読み込む
// address	: 読込対象となるFramのアドレス
// byte		: 読込結果の格納先
void FramReadByte( uint32_t address, uint8_t* byte )
{
	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	sendCommandRead(address);
	SoftSpiRead(byte, 1, 0);
	SoftSpiDeviceDisable();
}

// Framから2バイト読み込む
// address	: 読込対象となるFramのアドレス
// halfWord	: 読込結果の格納先
void FramReadHalfWord( uint32_t address, uint16_t* halfWord )
{
	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	sendCommandRead(address);
	SoftSpiRead(halfWord, 2, 0);
	SoftSpiDeviceDisable();
}

// Framから4バイト読み込む
// address	: 読込対象となるFramのアドレス
// word		: 読込結果の格納先
void FramReadWord( uint32_t address, uint32_t* word )
{
	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	sendCommandRead(address);
	SoftSpiRead(word, 4, 0);
	SoftSpiDeviceDisable();
}


// Framから指定バイト数読み込む
// address	: 読込対象となるFramのアドレス
// dst		: 読込結果の格納先
// size		: 読込バイト数
void FramReadBlock( uint32_t address, void* dst, int size)
{
	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	sendCommandRead(address);
	SoftSpiRead(dst, size, 0);
	SoftSpiDeviceDisable();
}


// Framに1バイト書き込む
// address	: 書込対象となるFramのアドレス
// byte		: 書込むデータ
void FramWriteByte( uint32_t address, uint8_t byte )
{
	sendCommandWREN();

	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	sendCommandWrite(address);
	SoftSpiWrite(&byte, 1);
	SoftSpiDeviceDisable();

	sendCommandWRDI();
}

// Framから2バイト書き込む
// address	: 書込対象となるFramのアドレス
// halfWord	: 書込むデータ
void FramWriteHalfWord( uint32_t address, uint16_t halfWord )
{
	sendCommandWREN();

	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	sendCommandWrite(address);
	SoftSpiWrite(&halfWord, 2);
	SoftSpiDeviceDisable();

	sendCommandWRDI();
}


// Framから4バイト書き込む
// address	: 書込対象となるFramのアドレス
// word		: 書込むデータ
void FramWriteWord( uint32_t address, uint32_t word )
{
	sendCommandWREN();
	
	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	sendCommandWrite(address);
	SoftSpiWrite(&word, 4);
	SoftSpiDeviceDisable();

	sendCommandWRDI();
}


// Framから指定バイト数書き込む
// address	: 書込対象となるFramのアドレス
// src		: 書込むデータ群の先頭を示すポインタ
// size		: 書込バイト数
void FramWriteBlock( uint32_t address, const void* src, int size)
{
	sendCommandWREN();

	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	sendCommandWrite(address);
	SoftSpiWrite(src, size);
	SoftSpiDeviceDisable();

	sendCommandWRDI();
}

// 単体テスト
int FramTest( void )
{
	const uint8_t write[10] = {0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E, 0x30, 0x32};
	const uint32_t testAddressTop = 200000; 
	uint8_t readBuf[20];
	uint16_t read16;
	uint32_t read32;

	FramWriteBlock(testAddressTop + 0, write, 10);
	FramWriteByte(testAddressTop + 10, 0x01);
	FramWriteByte(testAddressTop + 11, 0x02);
	FramWriteHalfWord(testAddressTop + 12, 0x0304);
	FramWriteWord(testAddressTop + 14, 0x05060708);
	
	FramReadBlock(testAddressTop + 0, (uint8_t*)&readBuf[0], 20);
	for(int i=0; i<10; i++)
	{
		if(readBuf[i] != write[i]){ return 1; }	// ReadBlockが正しく読めていない
	}
	
	FramReadHalfWord(testAddressTop + 0, &read16);
	if(read16 != 0x2220){ return 2; }		//ReadHalfWordが正しいエンディアンで読まれていない
	FramReadWord(testAddressTop + 0, &read32);
	if(read32 != 0x26242220){ return 3; }	//ReadWordが正しいエンディアンで読まれていない
	
	FramReadByte(testAddressTop + 10, (uint8_t*)&readBuf[0]);
	if(readBuf[0] != 0x01){ return 4; }		//ReadByteが正しく読めていない
	FramReadByte(testAddressTop + 11, (uint8_t*)&readBuf[0]);
	if(readBuf[0] != 0x02){ return 5; }		//ReadByteが正しく読めていない
	FramReadHalfWord(testAddressTop + 12, &read16);
	if(read16 != 0x0304){ return 6; }		//ReadHalfWordが正しいエンディアンで書き込まれていない
	FramReadWord(testAddressTop + 14, &read32);
	if(read32 != 0x05060708){ return 7; }	//ReadWordが正しいエンディアンで書き込まれていない
	
	
	//WPチェック(正常)
	//初期化
	if(testWp(WP_STATUS_ON) != WP_INSPECTION_RESULT_PASS)return -1;
	if(testWp(WP_STATUS_OFF) != WP_INSPECTION_RESULT_PASS)return -1;
	if(FramInspectWp() != true) return -1;
	return 0;
}

bool FramInspectWp(void)
{
	if(checkWp(WP_STATUS_ON) != WP_INSPECTION_RESULT_PASS)return false;
	if(checkWp(WP_STATUS_OFF) != WP_INSPECTION_RESULT_PASS)return false;
	return true;
}



/*############################################################################*/
/*#                                  Local                                   #*/
/*############################################################################*/

static void sendCommandWREN(void)
{
	const uint8_t COMMAND_WREN = 0x06;
	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	SoftSpiWrite(&COMMAND_WREN, 1);
	SoftSpiDeviceDisable();
}


static void sendCommandWRDI(void)
{
	const uint8_t COMMAND_WRDI = 0x04;
	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	SoftSpiWrite(&COMMAND_WRDI, 1);
	SoftSpiDeviceDisable();
}

static void sendCommandWrsrUnprotect(void)
{
	const uint8_t COMMAND_WRSR_UNPROTECTED[2] = {0x01, 0x00};
	
	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	SoftSpiWrite(COMMAND_WRSR_UNPROTECTED, 2);
	SoftSpiDeviceDisable();
}

static void sendCommandWRSR(const uint8_t value)
{
	const uint8_t COMMAND_WRSR[2] = {0x01, value};
	
	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	SoftSpiWrite(COMMAND_WRSR, 2);
	SoftSpiDeviceDisable();
}

static unsigned char readStatus(void)
{
	const uint8_t COMMAND = 0x05;
	unsigned char dst;
	SoftSpiDeviceEnable(SOFT_SPI_DEVICE_FRAM);
	SoftSpiWrite(&COMMAND, 1);
	SoftSpiRead(&dst, 1, 0);
	SoftSpiDeviceDisable();
	return dst;
}

static void sendCommandRead(uint32_t address)
{
	const uint8_t COMMAND_READ = 0x03;
	uint8_t sendData[4] =
	{
		COMMAND_READ,
		(address >> 16) & 0xFF,
		(address >> 8) & 0xFF,
		address & 0xFF
	};
	
	SoftSpiWrite(sendData, sizeof(sendData));
}

static void sendCommandWrite(uint32_t address)
{
	const uint8_t COMMAND_WRITE = 0x02;
	uint8_t sendData[4] =
	{
		COMMAND_WRITE,
		(address >> 16) & 0xFF,
		(address >> 8) & 0xFF,
		address & 0xFF
	};
	
	SoftSpiWrite(sendData, sizeof(sendData));
}

//wp確認
//WPをOFFにしているなら0x02でpass
//WPをONにしているなら0x82でpass
static WP_INSPECTION_RESULT checkWp(WP_STATUS wpStatus)
{
	//初期化
	volatile unsigned char statusRegisterValue = 0;
	WP_INSPECTION_RESULT result;
	const uint8_t COMMAND_WRSR_WPEN_ON = 0x80;
	const uint8_t COMMAND_WRSR_WPEN_OFF = 0x00;
	const uint8_t OK = 0x02;
	const uint8_t NG = 0x82;
	volatile uint8_t target;
	//WP = 1、statusRegister: 0x00
	FramInit();
	//WPの状態を決定。OFF(1)、ON(0)
	if(wpStatus == WP_STATUS_OFF)
	{
		WP_OFF();
		target = OK;
	}
	else 
	{
		WP_ON();
		target = NG;
	}

	//検査
	//WEL = 1
	//statusRegister: 0x02 
	sendCommandWREN();
	//WPEN = 1
	//statusRegister: 0x82
	sendCommandWRSR(COMMAND_WRSR_WPEN_ON);
	//WPEN = 0
	//statusRegister: 
	sendCommandWRSR(COMMAND_WRSR_WPEN_OFF);
	statusRegisterValue = readStatus();
	if(statusRegisterValue == target) result = WP_INSPECTION_RESULT_PASS;
	else result = WP_INSPECTION_RESULT_FAIL;

	//ラッチ解除
	sendCommandWRDI();
	return result;
}


//wp検査プログラムデバッグ用
//WPをOFFにしているなら0x02でpass
//WPをONにしているなら0x82でpass
static WP_INSPECTION_RESULT testWp(WP_STATUS wpStatus)
{
	//初期化
	volatile unsigned char statusRegisterValue = 0;
	WP_INSPECTION_RESULT result;
	const uint8_t COMMAND_WRSR_WPEN_ON = 0x80;
	const uint8_t COMMAND_WRSR_WPEN_OFF = 0x00;
	const uint8_t OK = 0x02;
	const uint8_t NG = 0x82;
	volatile uint8_t target;
	//WP = 1、statusRegister: 0x00
	FramInit();
	statusRegisterValue = readStatus();
	//WPの状態を決定。OFF(1)、ON(0)
	if(wpStatus == WP_STATUS_OFF)
	{
		WP_OFF();
		target = OK;
	}
	else 
	{
		WP_ON();
		target = NG;
	}

	//検査
	//WEL = 1
	//statusRegister: 0x02 
	sendCommandWREN();
	statusRegisterValue = readStatus();
	//WPEN = 1
	//statusRegister: 0x82
	sendCommandWRSR(COMMAND_WRSR_WPEN_ON);
	statusRegisterValue = readStatus();
	//WPEN = 0
	//statusRegister: 0x02ならWP == 1、0x82ならWP == 0
	sendCommandWRSR(COMMAND_WRSR_WPEN_OFF);
	statusRegisterValue = readStatus();
	if(statusRegisterValue == target) result = WP_INSPECTION_RESULT_PASS;
	else result = WP_INSPECTION_RESULT_FAIL;
	//ラッチ解除
	sendCommandWRDI();
	statusRegisterValue = readStatus();
	return result;
}
