/*****************************************************************************
 * File: Version.c
 * Title: バージョン情報を取得する。
 * LastUpdated: 2025.06.23
******************************************************************************/

/**
 * @file Version.c
 * @brief バージョン情報を取得する。
 */

#include "Version.h"
#include <stdbool.h>

#define MAJOR_VERSION		(1)
#define MINOR_VERSION		(3)
#define RELEASE_DATE		(0x250623)
#define VIEW_NAME			"VER.1.3.25.0623"
#define VIEW_NAME_TAIL		'3'
#define VIEW_NAME_FAILURE	"INCORRECT_VER"
#define VIEW_NAME_MAX		(16)
typedef struct
{
	char ViewName[VIEW_NAME_MAX];
	char ViewNameFailure[VIEW_NAME_MAX];
}VERSION;

static const VERSION version = 
{
	VIEW_NAME,
	VIEW_NAME_FAILURE
};

const char* VersionGetViewName(void)     
{
	//不正なバージョンをチェック
	if(version.ViewName[VIEW_NAME_MAX - 2] != VIEW_NAME_TAIL) return version.ViewNameFailure;
	if(version.ViewName[VIEW_NAME_MAX - 1] != '\0') return version.ViewNameFailure;
	return version.ViewName;
}

static bool getViewNameTest(VERSION versionTest)     
{
	//不正なバージョンをチェック
	if(versionTest.ViewName[VIEW_NAME_MAX - 2] != VIEW_NAME_TAIL) return false;
	if(versionTest.ViewName[VIEW_NAME_MAX - 1] != '\0') return false;
	return true;
}

int VersionTest(void)
{
	VERSION invalidTailVersion = {"VER.0.0.25.0219","INCORRECT_VER"};
	VERSION invalidFinalCharacterVersion = {"VER.0.0.25.02122","INCORRECT_VER"};
	
	if(getViewNameTest(invalidTailVersion) != false) return -1;
	if(getViewNameTest(invalidFinalCharacterVersion) != false) return -1;
	if(getViewNameTest(version) != true) return -1;
	
	return 0;
}

