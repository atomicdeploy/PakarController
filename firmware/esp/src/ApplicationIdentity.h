#pragma once
#include <DeviceContext.h>
#include "ApplicationArtwork.h"
#include "BoardArtwork.h"
// The deployed firmware owns its application identity. Board display/DNS names
// are user settings; renaming a board must not pretend another program is running.
#ifndef APP_NAME
#if PAKAR_FIRST_WINS_ENABLED
#define APP_NAME "Pakar Keys"
#elif PAKAR_TABLE_ENABLED
#define APP_NAME "Pakar Table"
#else
#define APP_NAME "Pakar Controller"
#endif
#endif
#ifndef APP_TITLE
#define APP_TITLE APP_NAME
#endif
#ifndef APP_ID
#define APP_ID "atomicdeploy.pakarcontroller"
#endif
#ifndef APP_ICON_PNG_BASE64
#if PAKAR_FIRST_WINS_ENABLED
#define APP_ICON_PNG_BASE64 kBoardIcon_keys
#elif PAKAR_TABLE_ENABLED
#define APP_ICON_PNG_BASE64 kBoardIcon_table
#else
#define APP_ICON_PNG_BASE64 kApplicationIconPngBase64
#endif
#endif
static_assert(sizeof(APP_NAME) <= 97 && sizeof(APP_TITLE) <= 161 && sizeof(APP_ID) <= 97,
              "Application identity exceeds client bounds");
