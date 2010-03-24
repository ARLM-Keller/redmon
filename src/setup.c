/* Copyright (C) 1997-2001, Ghostgum Software Pty Ltd.  All rights reserved.
  
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

/* RedMon setup program */
#include <windows.h>
#include "portmon.h"
#include "redmon.h"
#include "setup.h"
#ifdef BETA
#include <time.h>
#endif

char sysdir[256];
char exepath[256];
char source[256];
char destination[256];

char buffer[4096];
DWORD needed, returned;
unsigned int i;
MONITOR_INFO_1 *mi;
MONITOR_INFO_2 mi2;
BOOL silent = FALSE;
HKEY hkey, hsubkey;
LONG rc;
const TCHAR copyright[] = COPYRIGHT;
TCHAR title[256];
TCHAR monitorname[64];
char *p;
BOOL is_winnt = FALSE;
BOOL is_win4 = FALSE;
HINSTANCE phInstance;

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


#ifdef BETA
int
beta_expired(void)
{
  time_t today = time(NULL);
  struct tm *t;
  t = localtime(&today);
  if (t->tm_year+1900 < BETA_YEAR)
    return 0;
  if (t->tm_year+1900 > BETA_YEAR)
    return 1;    /* beta copy has expired */
  if (t->tm_mon+1 < BETA_MONTH)
    return 0;
  if (t->tm_mon+1 > BETA_MONTH)
    return 1;    /* beta copy has expired */
  if (t->tm_mday < BETA_DAY)
    return 0;
  return 1;    /* beta copy has expired */
}

int beta(void)
{
  if (beta_expired()) {
    TCHAR buf[MAXSTR];
    LoadString(phInstance, IDS_BETAEXPIRED, buf, sizeof(buf)/sizeof(TCHAR)-1);
    MessageBox(HWND_DESKTOP, buf, title, MB_OK | MB_ICONHAND);
    return 1;
  }
  return 0;
}

int beta_warn(void)
{
  TCHAR buf[MAXSTR];
  TCHAR mess[MAXSTR];
  LoadString(phInstance, IDS_BETAWARN, mess, sizeof(mess)/sizeof(TCHAR)-1);
  wsprintf(buf, mess, BETA_YEAR, BETA_MONTH, BETA_DAY);
  if (MessageBox(HWND_DESKTOP, buf, title, MB_OKCANCEL | MB_ICONEXCLAMATION)
	!= IDOK)
      return 1;
  return beta();
}
#endif /* BETA */

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

#ifdef BETA
     if (beta_warn())
	return 0;
#endif
     mi2.pName = monitorname;
     mi2.pEnvironment = is_winnt ? MONITORENVNT : MONITORENV95;
     mi2.pDLLName =     is_winnt ? MONITORDLLNT : MONITORDLL95 ;

     /* Check if already installed */
     if (EnumMonitors(NULL, 1, (LPBYTE)buffer, sizeof(buffer), 
	&needed, &returned)) {
	  mi = (MONITOR_INFO_1 *)buffer;
	  for (i=0; i<returned; i++) {
	  if (lstrcmp(mi[i].pName, monitorname) == 0)
	       return message(IDS_ALREADY_INSTALLED);
	   }
     }
     else
	  return message(IDS_ENUMMONITORS_FAILED);

     /* Warn user about what we are about to do */
     if (!silent) {
         TCHAR buf[256];
	 LoadString(phInstance, IDS_INTRO, buf, sizeof(buf)/sizeof(TCHAR)-1);
         if (MessageBox(HWND_DESKTOP, buf, title, MB_YESNO) != IDYES)
	     return 0;
     }

     /* copy files to Windows system directory */
     if (!GetSystemDirectory(sysdir, sizeof(sysdir)))
	  return message(IDS_NOSYSDIR);
     lstrcat(sysdir, "\\");

     /* get path to EXE */
     GetModuleFileName(hInstance, exepath, sizeof(exepath));
     if ((p = strrchr(exepath,'\\')) != (char *)NULL)
	  p++;
     else
	  p = exepath;
     *p = '\0';

     lstrcpy(destination, sysdir);
     lstrcat(destination, mi2.pDLLName);
     lstrcpy(source, exepath);
     lstrcat(source, mi2.pDLLName);
     if (!CopyFile(source, destination, FALSE))
	  return message(IDS_ERROR_COPY_DLL);
     lstrcpy(destination, sysdir);
     lstrcat(destination, MONITORHLP);
     lstrcpy(source, exepath);
     lstrcat(source, MONITORHLP);
     if (!CopyFile(source, destination, FALSE))
	  return message(IDS_ERROR_COPY_HELP);
     lstrcpy(destination, sysdir);
     lstrcat(destination, UNINSTALLPROG);
     lstrcpy(source, exepath);
     lstrcat(source, UNINSTALLPROG);
     if (!CopyFile(source, destination, FALSE))
	  return message(IDS_ERROR_COPY_UNINSTALL);

     if (!AddMonitor(NULL, 2, (LPBYTE)&mi2)) {
	  return message(IDS_ADDMONITOR_FAILED);
     }

     /* write registry entries for uninstall */
     if ((rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, UNINSTALLKEY, 0, 
	  KEY_ALL_ACCESS, &hkey)) != ERROR_SUCCESS) {
	/* failed to open key, so try to create it */
        rc = RegCreateKey(HKEY_LOCAL_MACHINE, UNINSTALLKEY, &hkey);
     }
     if (rc == ERROR_SUCCESS) {
	  if (RegCreateKey(hkey, MONITORKEY, &hsubkey) == ERROR_SUCCESS) {
	       lstrcpy(buffer, title);
	       RegSetValueEx(hsubkey, DISPLAYNAMEKEY, 0, REG_SZ,
		    (CONST BYTE *)buffer, lstrlen(buffer)+1);
	       lstrcpy(buffer, sysdir);
	       lstrcat(buffer, UNINSTALLPROG);
	       RegSetValueEx(hsubkey, UNINSTALLSTRINGKEY, 0, REG_SZ,
		    (CONST BYTE *)buffer, lstrlen(buffer)+1);
	       RegCloseKey(hsubkey);
	  }
	  RegCloseKey(hkey);
     }

     /* Disable removing of uninstall program, in case someone 
      * uninstalls then installs again without rebooting Windows.
      */
     wsprintf(source, TEXT("%s%s"), sysdir, UNINSTALLPROG);
     if (is_winnt) {
#define RENAME_KEY "PendingFileRenameOperations"
	 if ((rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
	    "SYSTEM\\CurrentControlSet\\Control\\Session Manager",
	    0, KEY_ALL_ACCESS, &hkey)) == ERROR_SUCCESS) {
	      DWORD dwType = REG_MULTI_SZ;
	      DWORD dwLength;
	      if ( (rc = RegQueryValueEx(hkey, RENAME_KEY, NULL, &dwType, 
		    buffer, &dwLength)) == ERROR_SUCCESS ) {
		/* look to see if unredmon.exe is mentioned */
		LPSTR p = buffer;
		LPSTR q;
		while (*p) {
		    q = p;
		    q += strlen(q) + 1;	/* skip existing name */
		    q += strlen(q) + 1;	/* skip new name */
		    if (lstrcmp(p, source) == 0) {
			MoveMemory(p, q, dwLength - (q - buffer));
			dwLength -= (q-p);
		    }
		    else
			p = q;
		}
		RegSetValueEx(hkey, RENAME_KEY, 0, dwType, 
		    buffer, dwLength); 
	      }
	 }
#undef RENAME_KEY
     }
     else {
	char ini_name[256];
	GetWindowsDirectory(ini_name, sizeof(ini_name));
	lstrcat(ini_name, "\\wininit.ini");
	GetPrivateProfileString("Rename", "NUL", "", 
	    buffer, sizeof(buffer), ini_name);
	if (lstrcmp(source, buffer) == 0) {
	    WritePrivateProfileString("Rename", "NUL", NULL, ini_name);
	}
	SetLastError(0);
     }

#ifdef UNUSED
     if ((rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
	"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
	0, KEY_ALL_ACCESS, &hkey)) == ERROR_SUCCESS) {
	  RegDeleteValue(hkey, MONITORKEY);
     }
#endif

     message(IDS_INSTALL_OK);

     return 0;
}


