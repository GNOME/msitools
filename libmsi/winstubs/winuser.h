#ifndef _WINUSER_
#define _WINUSER_

#include <_mingw_unicode.h>

#define wvsprintf __MINGW_NAME_AW(wvsprintf)
#define wsprintf __MINGW_NAME_AW(wsprintf)

int WINAPI wvsprintfA(LPSTR,LPCSTR,va_list arglist);
int WINAPI wvsprintfW(LPWSTR,LPCWSTR,va_list arglist);
int WINAPIV wsprintfA(LPSTR,LPCSTR,...);
int WINAPIV wsprintfW(LPWSTR,LPCWSTR,...);

#endif
