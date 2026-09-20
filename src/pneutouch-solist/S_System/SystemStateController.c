/*****************************************************************************
 * File: SystemStateController.c
 * Title: システムの状態を制御する。
 * LastUpdated: 2025.05.29
******************************************************************************/

/**
 * @file SystemStateController.c
 * @brief システムの状態を制御する。
 */

#include <stdio.h>
#include "SystemStateController.h"
#include "SystemStateStop.h"
#include "SystemStateLearn.h"
#include "SystemStatePredict.h"
#include "SystemStateTime.h"
#include "SystemStateAILearnErase.h"
#include "SystemStateChunkNoClear.h"
#include "SystemStateError.h"
#include "SystemStateInspection.h"
#include "SystemState.h"
#include "AI.h"
#include "ConfigData.h"
#include "irq.h"
#include "SystemError.h"
#include "LogControl.h"
#include "TimeControl.h"
#include "Sw.h"
#include "UartReceiveCommand.h"
#include "HighSpeedCom.h"
#include "HighSpeedComHelper.h"
#include "PowerMonitoringAnalogInput.h"
#include "PowerMonitoringSw.h"
#include "Shutdown.h"
#include "Uart1.h"
#include "UartQueue.h"
#include "UartReceiveCommand.h"
#include "timer0_1.h"
#include "PeriodicHandler10ms.h"
#include "PhotoCouplerInput.h"

#define TIMER_100MS 						(100)
#define SHUTDOWN_VOLTAGE_VALUE				(2.2f)
#define TIME_GET_SHUTDOWN_VOLTAGE_VALUE		(1000)

//移行する状態とその時に呼び出す描画関数
typedef void(*Draw)(void);
typedef struct
{
	SYSTEM_STATE NextState;
	Draw NextDraw;
}NextAction;
typedef void(*Control)(NextAction* next);

//現在の状態とその時に行う制御関数
typedef struct
{
	SYSTEM_STATE CurrentState;
	Control Control;
}Controller;

//SW入力の関数ポインタ
typedef bool(*ConfigEvent)(void);
static ConfigEvent configPsw3Entered = NULL;
static ConfigEvent configPsw4Entered = NULL;
//シャットダウン起動
static bool activateShutdown(void);
static bool isShutDown = false;

//メイン
static NextAction nextAction;
static void decideNextAction(NextAction* next,SYSTEM_STATE state,Draw draw);
static void nextActionInit(NextAction* next);
static void systemInit(NextAction* next);
static void systemStop(NextAction* next);
static void systemLearn(NextAction* next);
static void systemPredict(NextAction* next);
static void systemSetTime(NextAction* next);
static void systemAILearnErase(NextAction* next);
static void systemChunkNoClear(NextAction* next);
static void systemError(NextAction* next);
static void systemInspection(NextAction* next);
static void systemEnd(NextAction* next);

//ユーティリティ
static void utilityStart(void);
static void PeriodicHander10msCallback(void);

//設定とログの通信
static void handleSettingAndLogCommunicaiton(void);
static void enableSettingAndLogCommunicaiton(void);
static void disableSettingAndLogCommunicaiton(void);

//電源電圧監視
volatile static bool isRequiredToMonitorPowerAnalogInput = false;
static void monitorPowerAnalogInput(void);

//状態遷移とその制御用テーブル
static const Controller controllerTable[] =
{
	//INIT
	{SYSTEM_STATE_INIT,systemInit},
	//STOP
	{SYSTEM_STATE_STOP,systemStop},
	//LEARN
	{SYSTEM_STATE_LEARN,systemLearn},
	//PREDICT
	{SYSTEM_STATE_PREDICT,systemPredict},
	//TIME
	{SYSTEM_STATE_TIME,systemSetTime},
	//AI_LEARN_ERASE
	{SYSTEM_STATE_AI_LEARN_ERASE,systemAILearnErase},
	//CHUNK_NO_CLEAR
	{SYSTEM_STATE_CHUNK_NO_CLEAR,systemChunkNoClear},
	//ERROR
	{SYSTEM_STATE_ERROR,systemError},
	//INSPECTION
	{SYSTEM_STATE_INSPECTION,systemInspection},
	//END
	{SYSTEM_STATE_END,systemEnd}
};

//コントローラーの処理
void SystemStateController(void)
{
	uint8_t index = 0;
	uint8_t size  = sizeof(controllerTable) / sizeof(Controller);
	for( ; index < size; index++)
	{
		if(SystemStateGetCurrentState() == controllerTable[index].CurrentState)
		{
			controllerTable[index].Control(&nextAction);
			break;
		}
	}
	//登録していない関数は実行しない
	if(index >= size) return;
	SystemStateSetOldState(controllerTable[index].CurrentState);
	SystemStateSetCurrentState(nextAction.NextState);
	if(nextAction.NextDraw != NULL) nextAction.NextDraw();
}

//次の状態と描画関数を決定
static void decideNextAction(NextAction* next,SYSTEM_STATE state,Draw draw)
{
	next->NextState = state;
	next->NextDraw = draw;
}

//nextActionの初期化処理
static void nextActionInit(NextAction* next)
{
	decideNextAction(next,SYSTEM_STATE_INIT,NULL);
}


//初期化処理(設定値読込、state初期化)
void SystemStateControllerInit(void)
{
	uint8_t configToggleData;
	ConfigDataGetUint8Value(EN_CONFIG_TOGGLE,&configToggleData);
	//トグル
	if(configToggleData)
	{
		configPsw3Entered = SwIsTogglePsw3Valid;
		configPsw4Entered = SwIsTogglePsw4Valid;
	}
	//通常
	else
	{
		configPsw3Entered = SwIsPsw3Entered;
		configPsw4Entered = SwIsPsw4Entered;
	}
	SystemStateInit();
	//シャットダウン初期化
	ShutdownInit();
	isShutDown = false;
	nextActionInit(&nextAction);
}

//SYSTEM_STATE_INIT状態の処理
static void systemInit(NextAction* next)
{
	//ユーティティ起動
	utilityStart();
	//動作モード決定
	TimeControlDelayMs(TIMER_100MS);
	if(SwIsPsw1Entered() && SwIsPsw2Entered())
	{
		decideNextAction(next,SYSTEM_STATE_INSPECTION,SystemStateInspection);
	}
	else 
	{
		decideNextAction(next,SYSTEM_STATE_STOP,SystemStateStop);
	}
	SwDisableAllPswUntilNextPress();
}

//SYSTEM_STATE_STOP状態の処理
static void systemStop(NextAction* next)
{
	if(activateShutdown())return;
	monitorPowerAnalogInput();
	handleSettingAndLogCommunicaiton();
	if(SwIsPsw2Entered())
	{
		SwDisableAllPswUntilNextPress();
		decideNextAction(next,SYSTEM_STATE_AI_LEARN_ERASE,SystemStateAILearnErase);
	}
	else if(configPsw3Entered())
	{
		AILearnStart();
		disableSettingAndLogCommunicaiton();
		decideNextAction(next,SYSTEM_STATE_LEARN,SystemStateLearn);
	}
	else if(configPsw4Entered())
	{
		AIPredictStart();
		disableSettingAndLogCommunicaiton();
		LogControlReset();
		decideNextAction(next,SYSTEM_STATE_PREDICT,SystemStatePredict);
	}
	else if(SwIsPsw1Entered())
	{
		SwDisableAllPswUntilNextPress();
		decideNextAction(next,SYSTEM_STATE_TIME,SystemStateTime);
	}
}


//SYSTEM_STATE_LEARN状態の処理
static void systemLearn(NextAction* next)
{
	if(activateShutdown())return;
	monitorPowerAnalogInput();
	if(configPsw3Entered == NULL) return;
	if(!configPsw3Entered())
	{
		SwDisableAllPswUntilNextPress();
		AILearnStop();
		LogControlSaveEndLog();
		enableSettingAndLogCommunicaiton();
		decideNextAction(next,SYSTEM_STATE_STOP,SystemStateStop);		
	}
}

//SYSTEM_STATE_PREDICT状態の処理
static void systemPredict(NextAction* next)
{
	if(activateShutdown())return;
	monitorPowerAnalogInput();
	if(configPsw4Entered == NULL) return;
	if(!configPsw4Entered())
	{
		SwDisableAllPswUntilNextPress();
		AIPredictStop();
		LogControlSaveEndLog();
		enableSettingAndLogCommunicaiton();
		decideNextAction(next,SYSTEM_STATE_STOP,SystemStateStop);	
	}
	else
	{
		if(LogControlIsPredictLogRequiredSave())
		{
			LogControlSavePredictLog();
		}
	}
}

//LCD_STATE_TIME状態の処理
static void systemSetTime(NextAction* next)
{
	if(activateShutdown())return;
	monitorPowerAnalogInput();
	handleSettingAndLogCommunicaiton();
	if(SwIsPsw1Entered() && SystemStateGetShiftToAILearnErase())
	{
		SwDisableAllPswUntilNextPress();
		decideNextAction(next,SYSTEM_STATE_AI_LEARN_ERASE,SystemStateAILearnErase);
	}
}

//SYSTEM_STATE_AI_LEARN_ERASE状態の処理
static void systemAILearnErase(NextAction* next)
{
	if(activateShutdown())return;
	monitorPowerAnalogInput();
	handleSettingAndLogCommunicaiton();
	if(SwIsPsw1Entered())
	{
		SwDisableAllPswUntilNextPress();
		decideNextAction(next,SYSTEM_STATE_CHUNK_NO_CLEAR,SystemStateChunkNoClear);
	}
	else
	{
		if(SystemStateGetShiftToStop())
		{
			SwDisableAllPswUntilNextPress();
			decideNextAction(next,SYSTEM_STATE_STOP,SystemStateStop);
		}
	}
}

//SYSTEM_STATE_CHUNK_NO_CLEAR状態の処理
static void systemChunkNoClear(NextAction* next)
{
	if(activateShutdown())return;
	monitorPowerAnalogInput();
	handleSettingAndLogCommunicaiton();
	if(SwIsPsw1Entered())
	{
		SwDisableAllPswUntilNextPress();
		if(SystemStateGetShiftToError())
		{
			decideNextAction(next,SYSTEM_STATE_ERROR,SystemStateError);
		}
		else
		{
			decideNextAction(next,SYSTEM_STATE_STOP,SystemStateStop);
		}
	}
	else
	{
		if(SystemStateGetShiftToStop())
		{
			SwDisableAllPswUntilNextPress();
			decideNextAction(next,SYSTEM_STATE_STOP,SystemStateStop);
		}
	}
}

//SYSTEM_STATE_ERROR状態の処理
static void systemError(NextAction* next)
{
	if(activateShutdown())return;
	monitorPowerAnalogInput();
	handleSettingAndLogCommunicaiton();
	if((SwIsPsw1Entered() || SystemStateGetShiftToStop()))
	{
		SwDisableAllPswUntilNextPress();
		decideNextAction(next,SYSTEM_STATE_STOP,SystemStateStop);
	}
}

static void systemInspection(NextAction* next)
{
	if(activateShutdown())return;
	monitorPowerAnalogInput();
}

//SYSTEM_STATE_END状態の処理
static void systemEnd(NextAction* next)
{
	if(!isShutDown)
	{
		SwDisableAllPswUntilNextPress();
		if(SystemStateGetCurrentState() == SYSTEM_STATE_LEARN) AILearnStop();
		else if(SystemStateGetCurrentState() == SYSTEM_STATE_PREDICT) AIPredictStop();
		//状態更新
		SystemStateSetOldState(SystemStateGetCurrentState());
		SystemStateSetCurrentState(SYSTEM_STATE_END);
		decideNextAction(next,SYSTEM_STATE_END,NULL);
		isShutDown = true;
	}
	else ShutdownExecute();
}

static void utilityStart(void)
{
	//電源監視機能用フラグ初期化
	isRequiredToMonitorPowerAnalogInput = false;
	//設定(UART)とログの通信を許可
	enableSettingAndLogCommunicaiton();
	//10msタイマー開始
	PeriodicHandler10msSetCallBack(PeriodicHander10msCallback);
	timer0_start();
}

//10ms割り込みハンドラコールバック
//各種入力のポーリング、学習推論画面の描画タイマー、電源電圧監視タイマー。
inline static void PeriodicHander10msCallback(void)
{
	volatile static uint16_t shutdownVolatgeCnt = 0;
	if(TIME_GET_SHUTDOWN_VOLTAGE_VALUE <= ++shutdownVolatgeCnt) 
	{
		isRequiredToMonitorPowerAnalogInput = true;
		shutdownVolatgeCnt = 0;
	}
	SwPolling();
	PowerMonitoringSwPolling();
	if(SystemStateGetCurrentState() == SYSTEM_STATE_INSPECTION)
	{
		PhotoCouplerInputPolling();
		SystemStateInspectionDrawTimer();
	}
	SystemStateLearnDrawTimer();
	SystemStatePredictDrawTimer();
}

//シャットダウン起動
static bool activateShutdown(void)
{
	if(PowerMonitoringSwIsPressed() || (SHUTDOWN_VOLTAGE_VALUE >= PowerMonitoringAnalogInputCheckCurrentVoltageValue())) 
	{
		systemEnd(&nextAction);
		return true;
	}
	else
	{
		return false;
	}
}

//電源電圧監視
static void monitorPowerAnalogInput(void)
{
	if(isRequiredToMonitorPowerAnalogInput)
	{
		PowerMonitoringAnalogInputGetVoltageValue();
		isRequiredToMonitorPowerAnalogInput = false;
	}
}

//設定とログの通信を処理
static void handleSettingAndLogCommunicaiton(void)
{
	UartReceiveCommandReceiveBlock();
	UartReceiveCommandReceiveAnalizer();
	HighSpeedComMain();
}

//設定とログの通信を許可
static void enableSettingAndLogCommunicaiton(void)
{
	Uart1StartReadByte(UartReceiveCommandUart1InterruptReadCallback);
	HighSpeedComHelperLogEnable();
}

//設定とログの通信を禁止
static void disableSettingAndLogCommunicaiton(void)
{
	Uart1StopReadByte();
	HighSpeedComHelperLogDisable();
}
