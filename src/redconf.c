/* Copyright (C) 2010, Jonas Oberschweiber. All rights reserved.

  This file is part of RedMon.
  
  This program is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the RedMon Free Public Licence 
  (the "Licence") for full details.
  
  Every copy of RedMon must include a copy of the Licence, normally in a 
  plain ASCII text file named LICENCE.  The Licence grants you the right 
  to copy, modify and redistribute RedMon, but only under certain conditions 
  described in the Licence.  Among other things, the Licence requires that 
  the copyright notice and this notice be preserved on all copies.
*/

#include <windows.h>
#include <stdio.h>

#include "portmon.h"

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int cmdShow) {
    char cmd;
    DWORD len = strlen(lpszCmdLine + 1);
    wchar_t out[255];
    LPWSTR wide = (LPWSTR)malloc(sizeof(wchar_t) * (len + 1));
    if (len < 2) {
        return -1;
    }
    cmd = lpszCmdLine[0];
    hdll = hInstance;
    MultiByteToWideChar(CP_ACP, 0, lpszCmdLine + 1, -1, wide, len);
    wide[len] = '\0';
    switch (cmd) {
    case '1':
        rcAddPortUI(NULL, HWND_DESKTOP, wide, out);
        break;
    case '2':
        rcConfigurePortUI(NULL, HWND_DESKTOP, wide);
        break;
    case '3':
        rcDeletePortUI(NULL, HWND_DESKTOP, wide);
        break;
    }
    return 0;
}
