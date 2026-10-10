#define PROGMEM
#ifdef PAKAR_IDENTITY_CUSTOM
#define APP_NAME "Pakar Keys"
#define APP_TITLE "Keypad controller"
#define APP_ID "atomicdeploy.pakar.keys"
#endif
#include "ApplicationIdentity.h"
#include <assert.h>
#include <string.h>
int main() {
#ifdef PAKAR_IDENTITY_CUSTOM
  assert(strcmp(APP_NAME,"Pakar Keys")==0 && strcmp(APP_TITLE,"Keypad controller")==0);
  assert(strcmp(APP_ID,"atomicdeploy.pakar.keys")==0);
#else
  assert(strcmp(APP_NAME,PAKAR_TABLE_ENABLED ? "Pakar Table" : "Pakar Controller")==0);
  assert(strcmp(APP_TITLE,APP_NAME)==0);
#endif
  assert(strncmp(APP_ICON_PNG_BASE64,"iVBOR",5)==0);
  assert(strlen(APP_ICON_PNG_BASE64)<4096);
}
