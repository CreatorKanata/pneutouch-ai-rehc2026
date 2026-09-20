/*****************************************************************************
 * File: SystemStatePredictWarningLatch.c
 * Title: 警告ラッチ時に表示する推論画面を作成する。
 * LastUpdated: 2025.05.23
******************************************************************************/

/**
 * @file SystemStatePredictWarningLatch.c
 * @brief 警告ラッチ時に表示する推論画面を作成する。
 */

#include "SystemStatePredictWarningLatch.h"
#include "Lcd.h"
#include "ConfigData.h"
#include "smpl_common_led.h"
#include "RelayOutput.h"

#define ANOMALY_RESOLUTION			(16)
#define ANOMALY_DISPLAY_END			(16)

/**
 * @brief  閾値がLcdに表示される場所
 */
typedef struct
{
	uint8_t Yellow;
	uint8_t Red;
}NUMBER;
static NUMBER threshNumber;
typedef struct 
{
	bool Yellow;
	bool Red;
}WARNING_LACTH;
static WARNING_LACTH warningLatchStatus = {false,false};
static void statusInit(void);
static void statusReset(void);
static uint8_t getAbnormalitySymbol(float anomalyValue);
static float getResolution(void);
static uint8_t checkNumber(float value, float resolution);
static NUMBER prepareDraw(void);
static void drawAnomaly(float anomalyValue,ANOMALY anomalyResult);
static void drawAnomalyNormal(uint8_t number, NUMBER configNumber);
static void drawAnomalyYellow(uint8_t number, NUMBER configNumber);
static void drawAnomalyRed(uint8_t number, NUMBER configNumber);


void SystemStatePredictWarningLatchInit(PREDICT_DRAW_FUNC* drawFunc)
{
	drawFunc->Reset = statusReset;
	drawFunc->DrawAnomaly = drawAnomaly;
	statusInit();
}

int SystemStatePredictWarningLatchCheckFunc(PREDICT_DRAW_FUNC* drawFunc)
{
	if(statusReset != *drawFunc->Reset) return -1;
	if(drawAnomaly != *drawFunc->DrawAnomaly) return -2;
	return 0;	
}

static void statusInit(void)
{
	threshNumber = prepareDraw();
}

static void statusReset(void)
{
	warningLatchStatus.Yellow = false;
	warningLatchStatus.Red = false;
	//LED1出力
	smpl_onLED1();
	smpl_offLED2();
	smpl_offLED3();
	//リレー出力off
	RelayOutputRelay0Off();
	RelayOutputRelay1Off();
}

/**
 * @brief LCDに異常度を表示する準備をする(閾値取得)。
 */
static NUMBER prepareDraw(void)
{
	float configYellowThresh;
	float configRedThresh;
	float resolution;
	NUMBER number;
	
	//閾値設定取得
	ConfigDataGetFloatValue(EN_CONFIG_YELLOW_THRESHOLD,&configYellowThresh);
	ConfigDataGetFloatValue(EN_CONFIG_RED_THRESHOLD,&configRedThresh);
	resolution = getResolution();
	
	//番号取得
	number.Yellow = checkNumber(configYellowThresh,resolution);
	number.Red =	checkNumber(configRedThresh,resolution);
	
	return number;
}

/**
 * @brief 異常度をLCDに表示する
 * @param anomalyValue　AIが出力した異常度
 * @param anomalyResult 異常度を閾値に基づいて異常判定した結果 
 */
static void drawAnomaly(float anomalyValue,ANOMALY anomalyResult)
{
	volatile uint8_t currentAnomalyNumber;
	//異常度を表示番号に変換。
	currentAnomalyNumber = getAbnormalitySymbol(anomalyValue);	
		
	//コントロール
	//正常
	if(anomalyResult == ANOMALY_NORMAL)
	{
		drawAnomalyNormal(currentAnomalyNumber,threshNumber);
	}
	//異常(黄色閾値超え赤色閾値以下)
	else if(anomalyResult == ANOMALY_YELLOW)
	{
		drawAnomalyYellow(currentAnomalyNumber,threshNumber);
		//LED2出力
		smpl_onLED2();
		//リレー0出力
		RelayOutputRelay0On();
	}
	//異常(赤色閾値超え)
	else
	{
		drawAnomalyRed(currentAnomalyNumber,threshNumber);
		//LED3出力
		smpl_onLED3();
		//リレー1出力
		RelayOutputRelay1On();
	}
}


/**
 * @brief 異常度をLCDに表示する(正常時、ラッチあり)。
 * @param number 異常度の記号
 * @param configNumber 閾値の記号
 */
static void drawAnomalyNormal(uint8_t number, NUMBER configNumber)
{
	uint8_t i = 1; 
	//*
	for( ; i <= number; i++)
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1)),"*");
		if(configNumber.Yellow < i)break;
	}
	//ラッチ処理
	//黄色ラッチ直前まで空白
	for( ; i < configNumber.Yellow; i++)
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1))," ");
	}
	//黄色ラッチならY
	if(warningLatchStatus.Yellow) 
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1)),"Y");
		//赤色ラッチかつ、黄色閾値と赤色閾値が同じマスに表示される場合以外はマスを進める。
		if( ! ( (warningLatchStatus.Red) && (configNumber.Yellow == configNumber.Red)) )
		{
			i++;
		}
	}
	//赤色ラッチ直前まで空白
	for( ; i < configNumber.Red; i++)
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1))," ");
	}
	//赤色ラッチならY
	if(warningLatchStatus.Red) 
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1)),"R");
		i++;
	}
	//行末まで	文字クリア
	for( ; i <= ANOMALY_DISPLAY_END; i++)
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1))," ");
	}
}


/**
 * @brief 異常度をLCDに表示する(黄色閾値超え時、ラッチあり)。
 * @param number 異常度の記号 
 * @param configNumber 閾値の記号
 */
static void drawAnomalyYellow(uint8_t number, NUMBER configNumber)
{
	uint8_t i = 1;
	if(warningLatchStatus.Yellow == false) warningLatchStatus.Yellow = true;
	//*
	for( ; i < configNumber.Yellow; i++)
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1)),"*");
	}
	//Y
	for( ; i <= number; i++)
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1)),"Y");
		if(configNumber.Red < i)break;
	}
	//赤色ラッチ直前まで空白
	for( ; i < configNumber.Red; i++)
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1))," ");
	}
	//赤色ラッチならR Yが出ていても上書き
	if(warningLatchStatus.Red) 
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1)),"R");
		i++;
	}
	//文字クリア
	for( ; i <= ANOMALY_DISPLAY_END; i++)
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1))," ");
	}
}


/**
 * @brief 異常度をLCDに表示する(赤色閾値超え時、ラッチあり)。
 * @param number 異常度の記号 
 * @param configNumber 閾値の記号
 */
static  void drawAnomalyRed(uint8_t number, NUMBER configNumber)
{
	uint8_t i = 1;

	if(warningLatchStatus.Yellow == false) warningLatchStatus.Yellow = true;
	if( warningLatchStatus.Red == false) warningLatchStatus.Red = true;
	
	//*
	for( ; i < configNumber.Yellow; i++)
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1)),"*");
	}
	
	//Y
	for( ; i < configNumber.Red; i++)
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1)),"Y");
	}
	
	//R
	for( ; i <= number; i++)
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1)),"R");
	}
	
	//文字クリア
	for( ; i <= ANOMALY_DISPLAY_END; i++)
	{
		LcdDraw( (LCD_START_OF_SECOND_LINE + (i - 1))," ");
	}
}

/**
 * @brief 異常度に対応したLCDの表示番号を取得する。
 * @param anomalyValue 異常度
 */
static uint8_t getAbnormalitySymbol(float anomalyValue)
{
	uint8_t number;
	float resolution = getResolution();
	
	//番号取得
	number = checkNumber(anomalyValue,resolution);
	
	return number;
}

/**
 * @brief フルスケールに対する分解能を取得する。
 * @return float 分解能
 */
static float getResolution(void)
{
	float configFullScale;
	float resolution;
	
	//閾値設定取得
	ConfigDataGetFloatValue(EN_CONFIG_MAX_ABNORMAL,&configFullScale);
	resolution = configFullScale / ANOMALY_RESOLUTION;
	
	return resolution;
}


/**
 * @brief 異常度に対応したLCDの表示番号を取得する。
 * @param value 異常度
 * @param resolution 分解能
 */
static uint8_t checkNumber(float value, float resolution)
{
	uint8_t number = 1;
	float thresh = resolution;

	while( thresh < value)
	{
		thresh += resolution;
		number++;
		//16で打ち止め
		if( number >= ANOMALY_DISPLAY_END) break;
	}
	return number;
}


//テスト用。ラッチ状態チェック。
static bool checkLatch(bool yellow, bool red)
{
	if(warningLatchStatus.Yellow != yellow) return -1;
	if(warningLatchStatus.Red != red) return -1;
	return 0;
}

//単体テスト
int SystemStatePredictWarningLatchTest(void)
{
	volatile NUMBER number;
	volatile uint8_t abnormalitySymbol;
	ANOMALY anomalyResult;
	
	
	//初期状態
	ConfigDataSetBfloat16Value(EN_CONFIG_YELLOW_THRESHOLD,0.3f);
	ConfigDataSetBfloat16Value(EN_CONFIG_RED_THRESHOLD,0.7f);
	statusInit();
	statusReset();
	//configは黄色閾値0.3f、赤色閾値0.7f、フルスケール1.0fを設定。
	//閾値の番号確認。
	//yellow = 0.3f(4.8/16⇒5) red = 0.7f(11.2/16⇒12)
	if(threshNumber.Yellow != 5) return -1;
	if(threshNumber.Red != 12) return -1;
	
	//全体をざっとテスト
	//フルスケール1.0f、LCD表示マス16: 一マス1/16。　0から0.0625増えるごとに一マス進む。
	//正常 3.2/16 ⇒ 4
	//****
	//LED、リレー出力無
	anomalyResult = ANOMALY_NORMAL;
	drawAnomaly(0.2f,anomalyResult);
	//黄色閾値超え 8/16 ⇒　8
	//****YYYY
	//LED2、リレー0出力
	anomalyResult = ANOMALY_YELLOW;
	drawAnomaly(0.5f,anomalyResult);
	//正常 3.2/16 ⇒ 4
	//****
	//LED2、ラッチ確認(リレー0出力)
	anomalyResult = ANOMALY_NORMAL;
	drawAnomaly(0.2f,anomalyResult);
	//赤色閾値超え 12.8/16 ⇒　13
	//****YYYYYYYRR
	//LED23、リレー01出力
	anomalyResult = ANOMALY_RED;
	drawAnomaly(0.8f,anomalyResult);		
	//正常 3.2/16 ⇒ 4
	//****
	//LED23、ラッチ確認(リレー01出力)
	anomalyResult = ANOMALY_NORMAL;
	drawAnomaly(0.2f,anomalyResult);
	
	//各描画関数についてテスト 
	//閾値を変更するためローカル変数に代入
	number = prepareDraw();
	if(!((number.Yellow == 5) && (number.Red == 12))) return -1;
	
	//ラッチ状態リセット
	statusReset();
	if(checkLatch(false,false))return -1;
	
	//正常⇒黄色⇒赤色⇒黄色⇒正常の順で異常判定された場合
	//正常 3.2/16 ⇒ 4
	//****
	abnormalitySymbol = getAbnormalitySymbol(0.2f);
	if(abnormalitySymbol != 4) return -1;
	drawAnomalyNormal(abnormalitySymbol,number);
	if(checkLatch(false,false))return -1;
	
	//黄色閾値超え 8/16 ⇒ 8
	//****YYYY
	abnormalitySymbol = getAbnormalitySymbol(0.5f);
	if(abnormalitySymbol != 8) return -1;
	drawAnomalyYellow(abnormalitySymbol,number);
	if(checkLatch(true,false))return -1;
	
	//赤色閾値超え 12.8/16 ⇒ 13
	//****YYYYYYYRR
	abnormalitySymbol = getAbnormalitySymbol(0.8f);
	if(abnormalitySymbol != 13) return -1;
	drawAnomalyRed(abnormalitySymbol,number);
	if(checkLatch(true,true))return -1;
	
	//変数上でラッチ保持しているか確認。
	drawAnomalyRed(abnormalitySymbol,number);
	if(checkLatch(true,true))return -1;
	
	//Rラッチ確認 12マス目にRが残るはず。
	//黄色閾値超え 8/16 ⇒ 8
	//****YYYY@@@R(@は空白を指す)
	abnormalitySymbol = getAbnormalitySymbol(0.5f);
	if(abnormalitySymbol != 8) return -1;
	drawAnomalyYellow(abnormalitySymbol,number);
	if(checkLatch(true,true))return -1;
	
	//Y Rラッチ確認(RラッチかつY!=R)
	//5マス目にY、12マス目にRが残るはず。
	//正常 3.2/16 ⇒ 4
	//****Y@@@@@@R(@は空白を指す)
	abnormalitySymbol = getAbnormalitySymbol(0.2f);
	if(abnormalitySymbol != 4) return -1;
	drawAnomalyNormal(abnormalitySymbol,number);
	if(checkLatch(true,true))return -1;
	
	//正常⇒赤色⇒正常の順で異常判定された場合。
	//ラッチ状態リセット
	statusReset();
	if(checkLatch(false,false))return -1;
	//正常 3.2/16 ⇒ 4
	//****
	abnormalitySymbol = getAbnormalitySymbol(0.2f);
	if(abnormalitySymbol != 4) return -1;
	drawAnomalyNormal(abnormalitySymbol,number);
	if(checkLatch(false,false))return -1;
	//赤色閾値超え 12.8/16 ⇒ 13
	//****YYYYYYYRR
	abnormalitySymbol = getAbnormalitySymbol(0.8f);
	if(abnormalitySymbol != 13) return -1;
	drawAnomalyRed(abnormalitySymbol,number);
	if(checkLatch(true,true))return -1;
	//正常 3.2/16 ⇒ 4
	//****Y@@@@@@R(@は空白を指す)
	abnormalitySymbol = getAbnormalitySymbol(0.2f);
	if(abnormalitySymbol != 4) return -1;
	drawAnomalyNormal(abnormalitySymbol,number);
	if(checkLatch(true,true))return -1;
	
	
	//正常⇒黄色⇒正常の順で異常判定された場合。
	//ラッチ状態リセット
	statusReset();
	if(checkLatch(false,false))return -1;
	//正常 3.2/16 ⇒ 4
	//****
	abnormalitySymbol = getAbnormalitySymbol(0.2f);
	if(abnormalitySymbol != 4) return -1;
	drawAnomalyNormal(abnormalitySymbol,number);
	if(checkLatch(false,false))return -1;
	//黄色閾値超え 8/16 ⇒ 8
	//****YYYY
	abnormalitySymbol = getAbnormalitySymbol(0.5f);
	if(abnormalitySymbol != 8) return -1;
	drawAnomalyYellow(abnormalitySymbol,number);
	if(checkLatch(true,false))return -1;
	//正常 3.2/16 ⇒ 4
	//****Y
	abnormalitySymbol = getAbnormalitySymbol(0.2f);
	if(abnormalitySymbol != 4) return -1;
	drawAnomalyNormal(abnormalitySymbol,number);
	if(checkLatch(true,false))return -1;
	
	//ラッチ状態リセット
	statusReset();
	//Y Rラッチ確認(RラッチなしかつY!=R) 
	//正常 3.2/16 ⇒ 4
	//****
	abnormalitySymbol = getAbnormalitySymbol(0.2f);
	if(abnormalitySymbol != 4) return -1;
	drawAnomalyNormal(abnormalitySymbol,number);
	if(checkLatch(false,false))return -1;
	
	//ラッチ状態リセット
	statusReset();
	//Y Rラッチ確認(RラッチありかつY!=R)
	//赤色閾値超え 12.8/16 ⇒ 13
	//****YYYYYYYRR
	abnormalitySymbol = getAbnormalitySymbol(0.8f);
	if(abnormalitySymbol != 13) return -1;
	drawAnomalyRed(abnormalitySymbol,number);
	if(checkLatch(true,true))return -1;
	//正常 3.2/16 ⇒ 4
	//****Y@@@@@@R(@は空白を指す)
	abnormalitySymbol = getAbnormalitySymbol(0.2f);
	if(abnormalitySymbol != 4) return -1;
	drawAnomalyNormal(abnormalitySymbol,number);
	if(checkLatch(true,true))return -1;
	
	
	//ラッチ状態リセット
	statusReset();
	//Y Rラッチ確認(RラッチなしかつY==R)
	//12.8/16⇒13
	number.Yellow = checkNumber(0.8f,getResolution());
	number.Red =	checkNumber(0.8f,getResolution());
	//正常　11.2/16 ⇒ 12
	//************
	abnormalitySymbol = getAbnormalitySymbol(0.7f);
	drawAnomalyNormal(abnormalitySymbol,number);

	//ラッチ状態リセット
	statusReset();
	//Y Rラッチ確認(RラッチありかつY==R)
	//赤色閾値超え 14.4/16 ⇒ 15
	//************RRR
	abnormalitySymbol = getAbnormalitySymbol(0.9f);
	drawAnomalyRed(abnormalitySymbol,number);
	if(checkLatch(true,true))return -1;
	//正常　8/16 ⇒ 8
	//********@@@@R
	abnormalitySymbol = getAbnormalitySymbol(0.5f);
	drawAnomalyNormal(abnormalitySymbol,number);
	if(checkLatch(true,true))return -1;
	
	
	//閾値番号の再設定。
	//通常動作
	number = prepareDraw();
	//ラッチ初期化
	statusReset();
	//閾値の境界値 分解能0.0625　×　4 = 0.025  4/16　⇒ 4
	//****
	abnormalitySymbol = getAbnormalitySymbol(0.25f);
	if(abnormalitySymbol != 4) return -1;
	drawAnomalyNormal(abnormalitySymbol,number);
	//0.2499  3.9984/16 ⇒ 4
	//****
	abnormalitySymbol = getAbnormalitySymbol(0.2499f);
	if(abnormalitySymbol != 4) return -1;
	drawAnomalyNormal(abnormalitySymbol,number);
	//0.2501  4.0016/16 ⇒　5
	//*****
	abnormalitySymbol = getAbnormalitySymbol(0.2501f);
	if(abnormalitySymbol != 5) return -1;
	drawAnomalyNormal(abnormalitySymbol,number);
	
	//ラッチ初期化
	statusReset();

	//黄色閾値超える前と後で黄色閾値の表示場所が*⇒Yに変化
	//*****
	abnormalitySymbol = getAbnormalitySymbol(0.299f);
	drawAnomalyNormal(abnormalitySymbol,number);
	//****Y
	abnormalitySymbol = getAbnormalitySymbol(0.301f);
	drawAnomalyYellow(abnormalitySymbol,number);
	
	//赤色閾値超える前と後で赤色閾値の表示場所がY⇒Rに変化
	//****YYYYYYY
	abnormalitySymbol = getAbnormalitySymbol(0.699f);
	drawAnomalyYellow(abnormalitySymbol,number);
	//****YYYYYYR
	abnormalitySymbol = getAbnormalitySymbol(0.701f);
	drawAnomalyRed(abnormalitySymbol,number);
	
	//ラッチ初期化
	statusReset();
	//フルスケール超え　5で打ち止め
	//*****
	abnormalitySymbol = getAbnormalitySymbol(12.0f);
	drawAnomalyNormal(abnormalitySymbol,number);
	if(checkLatch(false,false))return -1;
	//フルスケール超え　12で打ち止め
	//****YYYYYYYY
	abnormalitySymbol = getAbnormalitySymbol(12.0f);
	drawAnomalyYellow(abnormalitySymbol,number);
	if(checkLatch(true,false))return -1;
	//フルスケール超え 16で打ち止め
	//*****YYYYYYRRRRR
	abnormalitySymbol = getAbnormalitySymbol(12.0f);
	drawAnomalyRed(abnormalitySymbol,number);
	if(checkLatch(true,true))return -1;

	//黄色閾値 == 0.0f　の時の表示確認
	//8/16 ⇒　8までY
	statusReset();
	number.Yellow = 0.0f;
	abnormalitySymbol = getAbnormalitySymbol(0.5f);
	drawAnomalyYellow(abnormalitySymbol,number);
	//黄色、赤色閾値 == 0.0f の時の表示確認
	//12.8/16 ⇒ 13までR
	number.Yellow = 0.0f;
	number.Red = 0.0f;
	abnormalitySymbol = getAbnormalitySymbol(0.8f);
	drawAnomalyRed(abnormalitySymbol,number);
	
	return 0;
}
