/* Copyright (C) 1997-1998, Ghostgum Software Pty Ltd.  All rights reserved.
  
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

/* RedMon uninstall program */
#include <windows.h>
#include "redmon.h"
#include "unredmon.h"

char sysdir[256];
char buffer[4096];
DWORD needed, returned;
int rc;
unsigned int i;
PORT_INFO_2 *pi2;
BOOL silent = FALSE;
HKEY hkey;
TCHAR copyright[] = COPYRIGHT;
BOOL is_winnt = FALSE;
BOOL is_win4 = FALSE;
HINSTANCE phInstance;
TCHAR title[256];
TCHAR monitorname[64];

int 
message(int id)
{
    if (!silent) {
	LPVOID lpMessageBuffer = NULL;
	TCHAR buf[256];
	TCHAR mess[256];
	DWORD error = GetLastError();
	LoadString(phInstance, id, mess, sizeof(mess)/sizeof(TCHAR)-1);
	wsprintf(buf, mess, error);

	if (error)
	    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, error, /* user default language */
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMessageBuffer, 0, NULL);
	if (lpMessageBuffer) {
	    lstrcat(buf, "\n");
	    lstrcat(buf, lpMessageBuffer);
	    LocalFree(LocalHandle(lpMessageBuffer));
	}

        MessageBox(HWND_DESKTOP, buf, title, MB_OK);
    }
    return 1; 
}



int PASCAL 
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int cmdShow)
{
DWORD version = GetVersion();
   phInstance = hInstance;
   if ((HIWORD(version) & 0x8000)==0)
      is_winnt = TRUE;
   if (LOBYTE(LOWORD(version)) >= 4)
      is_win4 = TRUE;

   if (lstrlen(lpszCmdLine))
       silent = TRUE;

   LoadString(phInstance, IDS_TITLE, title, sizeof(title)/sizeof(TCHAR)-1);
   LoadString(phInstance, IDS_MONITORNAME, monitorname, 
	sizeof(monitorname)/sizeof(TCHAR)-1);

   /* Check that it really is installed */
   if (EnumMonitors(NULL, 1, (LPBYTE)buffer, sizeof(buffer), 
        &needed, &returned)) {
      MONITOR_INFO_1 *mi;
      mi = (MONITOR_INFO_1 *)buffer;
      for (i=0; i<returned; i++) {
	 if (lstrcmp(mi[i].pName, monitorname) == 0)
	    break;
      }
   }
   else
      return message(IDS_ENUMMONITORS_FAILED);

   if (i == returned)
	return message(IDS_NOT_INSTALLED);

   /* Warn user about what we are about to do */
   if (!silent) {
         TCHAR buf[256];
	 LoadString(phInstance, IDS_INTRO, buf, sizeof(buf)/sizeof(TCHAR)-1);
         if (MessageBox(HWND_DESKTOP, buf, title, MB_YESNO) != IDYES)
	     return 0;
   }

   /* Check if monitor is still in use */
   rc = EnumPorts(NULL, 2, (LPBYTE)buffer, sizeof(buffer), 
      &needed, &returned);
   pi2 = (PORT_INFO_2 *)buffer;
   if (rc) {
      for (i=0; i<returned; i++) {
	 if (lstrcmp(pi2[i].pMonitorName, monitorname) == 0) {
            TCHAR buf[256];
	    LoadString(phInstance, IDS_INUSE, buf, sizeof(buf)/sizeof(TCHAR)-1);
	    wsprintf(sysdir, buf, pi2[i].pPortName);
            MessageBox(HWND_DESKTOP, sysdir, title, MB_OK);
	    return 1;
	 }
      }
   }
   else
      return message(IDS_ENUMPORTS_FAILED);

   /* Try to delete the monitor */
   if (!DeleteMonitor(NULL,  
      NULL /* is_winnt ? MONITORENVNT : MONITORENV95 */, 
      monitorname))
	return message(IDS_DELETEMONITOR_FAILED);

   /* Delete the monitor files */
   if (!GetSystemDirectory(sysdir, sizeof(sysdir)))
	return message(IDS_NOSYSDIR);
   lstrcpy(buffer, sysdir);
   lstrcat(buffer, "\\");
   lstrcat(buffer, is_winnt ? MONITORDLLNT : MONITORDLL95);
   if (!DeleteFile(buffer))
	return message(IDS_ERROR_DELETE_DLL);
   lstrcpy(buffer, sysdir);
   lstrcat(buffer, "\\");
   lstrcat(buffer, MONITORHLP);
   if (!DeleteFile(buffer))
	return message(IDS_ERROR_DELETE_HELP);

   /* delete registry entries for uninstall */
   if ((rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, UNINSTALLKEY, 0, 
	KEY_ALL_ACCESS, &hkey)) == ERROR_SUCCESS) {
	RegDeleteKey(hkey, MONITORKEY);
	RegCloseKey(hkey);
   }

   /* Delete this program, but we can't do it while we are running.
    * Defer deletion until next reboot
    */
   wsprintf(buffer, TEXT("%s\\%s"), sysdir, UNINSTALLPROG);
   if (is_winnt) {
        MoveFileEx(buffer, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
   }
   else {
	char ininame[256];
	GetWindowsDirectory(ininame, sizeof(ininame));
	lstrcat(ininame, "\\wininit.ini");
	/* This method is dodgy, because multiple applications
         * using this method to delete files will overwrite
         * earlier delete instructions.
         */
	WritePrivateProfileString("Rename", "NUL", buffer, ininame);
	SetLastError(0);
   }

#ifdef UNUSED
   /* We should delete this program, but we can't do it
    * while we are running.
    * I think there is a registry key we can create which says
    * "Delete these files on next reboot", but I can't find the 
    * documentation about it.
    * Instead, run a DOS window once to delete the file
    */
    if ((rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
	"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
	0, KEY_ALL_ACCESS, &hkey)) == ERROR_SUCCESS) {
	wsprintf(buffer, "command /c del %s\\%s", sysdir, UNINSTALLPROG);
	RegSetValueEx(hkey, MONITORKEY, 0, REG_SZ,
	    (CONST BYTE *)buffer, lstrlen(buffer)+1);
    }
#endif
   
   
   message(IDS_UNINSTALLED);

   return 0;
}
