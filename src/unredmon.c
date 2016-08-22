/* Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.
  
  This file is part of RedMon.
  
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  This software is distributed under licence and may not be copied, modified
  or distributed except as expressly authorised under the terms of the
  LICENCE.

*/

/* RedMon uninstall program */
#include <windows.h>
#include "redmon.h"
#include "unredmon.h"

#define MAXSTR 512
char sysdir[MAXSTR];
char buffer[65536];
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

/* Attempt to disconnect port from all printers, and then delete port.
 * This will only succeed if the redirected port name (e.g. RPT1:)
 * has the same or longer length as FILE:
 */
void
disconnect_port(LPTSTR port_name)
{
TCHAR fileport[] = TEXT("FILE:");
PRINTER_INFO_2 *pri2;
PRINTER_INFO_2 *printer_info;
PRINTER_DEFAULTS pd;
HANDLE hPrinter;
DWORD needed, returned;
char enumbuf[65536];
char printerbuf[65536];
int rc;
int len;
    /* Find printer that uses this port */
    rc = EnumPrinters(PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL, 
	  NULL, 2, (LPBYTE)enumbuf, sizeof(enumbuf), 
      &needed, &returned);
    pri2 = (PRINTER_INFO_2 *)enumbuf;
    if (rc) {
        for (i=0; i<returned; i++) {
	    if (lstrcmp(pri2[i].pPortName, port_name) == 0) {
		pd.pDatatype = NULL;
		pd.pDevMode = NULL;
		pd.DesiredAccess = PRINTER_ALL_ACCESS;
		if (OpenPrinter(pri2[i].pPrinterName, &hPrinter, &pd)) {
		    if (GetPrinter(hPrinter, 2, (LPBYTE)printerbuf, 
			    sizeof(printerbuf), &needed)) {
			printer_info = (PRINTER_INFO_2 *)printerbuf;
			/* Replace port name with FILE: */
			len = lstrlen(fileport);
			if (lstrlen(printer_info->pPortName) >= len) {
			    memcpy(printer_info->pPortName, fileport, 
				(len+1) * sizeof(TCHAR));
			}
			SetPrinter(hPrinter, 2, printerbuf, 0);
		    } 
		    ClosePrinter(hPrinter);
		}
	    }
        }
     }
     /* This may not be silent, but probably is */
     DeletePort(NULL, HWND_DESKTOP, port_name);
}


#ifndef _WIN64
typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) 
  (HANDLE hProcess,PBOOL Wow64Process);
LPFN_ISWOW64PROCESS fnIsWow64Process;

/* This is used to exec the 64-bit setup program 
 * if the 32-bit setup is called on Win64.
 */
int
reexec(LPCTSTR prog)
{
    STARTUPINFO siStartInfo;
    PROCESS_INFORMATION piProcInfo;
    LPCTSTR cmdline = GetCommandLine();
    TCHAR szExeName[MAXSTR];
    TCHAR szCommandLine[MAXSTR+MAXSTR];
    LPCTSTR args;
    DWORD len;
    DWORD exit_code = 0;

    /* Get full path of prog in same diretory as setup.exe */
    memset(szExeName, 0, sizeof(szExeName));
    len = GetModuleFileName(NULL, szExeName, sizeof(szExeName)/sizeof(TCHAR)-1);
    for (i=len-1; i>0; i--) {
	if (szExeName[i] == '\\') {
	    szExeName[++i] = '\0';
	    break;
	}
    }	
    lstrcpyn(szExeName+i, prog, sizeof(szExeName)/sizeof(TCHAR)-i-1);

    /* Find args of this setup program */
    args = cmdline;
    if (*args == '\042') {
	/* skip until closing " */
	args++;
	while (*args && (*args != '\042'))
	    args++;
	if (*args)
	    args++;
    }
    else {
	/* skip until space */
	while (*args && (*args != ' '))
	    args++;
    }
    while (*args && (*args == ' '))
	args++;

    memset(szCommandLine, 0, sizeof(szCommandLine));
    lstrcpyn(szCommandLine, TEXT("\042"), 
       sizeof(szCommandLine)/sizeof(TCHAR)-1);
    len = lstrlen(szCommandLine);
    lstrcpyn(szCommandLine+len, szExeName, 
        sizeof(szCommandLine)/sizeof(TCHAR)-len-1);
    len = lstrlen(szCommandLine);
    lstrcpyn(szCommandLine+len, TEXT("\042"), 
        sizeof(szCommandLine)/sizeof(TCHAR)-len-1);
    if (*args) {
	len = lstrlen(szCommandLine);
	lstrcpyn(szCommandLine+len, TEXT(" "), 
	    sizeof(szCommandLine)/sizeof(TCHAR)-len-1);
	len = lstrlen(szCommandLine);
	lstrcpyn(szCommandLine+len, args, 
	    sizeof(szExeName)/sizeof(TCHAR)-len-1);
    }

    memset(&siStartInfo, 0, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.wShowWindow = SW_SHOWNORMAL;            /* ignored */

    /* Create the child process. */

    if (!CreateProcess(NULL,
        szCommandLine, /* command line                       */
        NULL, NULL, FALSE, 0, NULL, NULL,
        &siStartInfo,  /* STARTUPINFO pointer                */
        &piProcInfo))  /* receives PROCESS_INFORMATION  */
    {
	if (!silent)
            MessageBox(HWND_DESKTOP, szCommandLine, 
		TEXT("RedMon uninstaller can't start"), MB_OK);
        return 1;
    }

    /* Wait until prog finishes */

    while (GetExitCodeProcess(piProcInfo.hProcess, &exit_code)) {
	if (exit_code != STILL_ACTIVE)
	    break;
	else
            WaitForSingleObject(piProcInfo.hProcess, 10000);
    }

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    return exit_code;
}
#endif



int PASCAL 
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int cmdShow)
{
   DWORD version = GetVersion();
   BOOL bIsWow64 = FALSE;
   phInstance = hInstance;
   if ((HIWORD(version) & 0x8000)==0)
      is_winnt = TRUE;
   if (LOBYTE(LOWORD(version)) >= 4)
      is_win4 = TRUE;

#ifndef _WIN64
    fnIsWow64Process = (LPFN_ISWOW64PROCESS)
        GetProcAddress(GetModuleHandle("kernel32"),"IsWow64Process");
    if (fnIsWow64Process != NULL)
        fnIsWow64Process(GetCurrentProcess(),&bIsWow64);
    if (bIsWow64)
	return reexec(TEXT("unredmon64.exe"));
#endif

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
	    /* A RedMon port still exists, and may be used by a printer */
	    /* Attempt to disconnect and delete port */
 	    disconnect_port(pi2[i].pPortName);
	 }
      }
   }

   /* Try again, hoping that we succeeded in deleting all ports */
   rc = EnumPorts(NULL, 2, (LPBYTE)buffer, sizeof(buffer), 
      &needed, &returned);
   pi2 = (PORT_INFO_2 *)buffer;
   if (rc) {
      for (i=0; i<returned; i++) {
	 if (lstrcmp(pi2[i].pMonitorName, monitorname) == 0) {
	    /* A RedMon port still exists, and may be used by a printer */
	    /* Refuse to uninstall RedMon until the port is deleted */
            TCHAR buf[256];
	    LoadString(phInstance, IDS_INUSE, buf, sizeof(buf)/sizeof(TCHAR)-1);
	    wsprintf(sysdir, buf, pi2[i].pPortName);
            if (!silent)
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



   /* On Window7 x64, the monitor DLL was still in use after the DeleteMonitor
    * call above.  We need to wait until the spooler reports that the monitor
    * is no longer available before we try to delete the DLL.
   { 
        int loaded = 1;
        int tries = 0;
	int delay = 0;

        while (loaded && (tries < 10))  {
	   loaded = 0;
	   if (EnumMonitors(NULL, 1, (LPBYTE)buffer, sizeof(buffer), 
		&needed, &returned)) {
	      MONITOR_INFO_1 *mi;
	      mi = (MONITOR_INFO_1 *)buffer;
	      for (i=0; i<returned; i++) {
		 if (lstrcmp(mi[i].pName, monitorname) == 0) {
		    loaded = 1;
		    break;
		 }
	      }
	   }
	   if (loaded) {
		tries++;
		delay = delay + delay + 50;
		Sleep(delay);
	   }
	 }
   }


   /* Delete the monitor files */
   if (!GetSystemDirectory(sysdir, sizeof(sysdir)))
	return message(IDS_NOSYSDIR);
   lstrcpy(buffer, sysdir);
   lstrcat(buffer, "\\");
#ifdef _WIN64
   lstrcat(buffer, MONITORDLL64);
#else
   if (bIsWow64)
       lstrcat(buffer, MONITORDLL64);
   else
#if _MSC_VER >= 1500
   lstrcat(buffer, MONITORDLL32);
#else
   lstrcat(buffer, is_winnt ? MONITORDLLNT : MONITORDLL95);
#endif
#endif
   if (!DeleteFile(buffer)) {
	/* Try again after a delay or two */
	Sleep(100);
        if (!DeleteFile(buffer)) {
	    Sleep(1000);
            if (!DeleteFile(buffer)) {
	        return message(IDS_ERROR_DELETE_DLL);
	    }
	}
   }

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

   message(IDS_UNINSTALLED);

   return 0;
}
