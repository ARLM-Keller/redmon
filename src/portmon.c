/* Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.
  
  This file is part of RedMon.
  
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  This software is distributed under licence and may not be copied, modified
  or distributed except as expressly authorised under the terms of the
  LICENCE.

  Modified in 2010 by Jonas Oberschweiber.
*/

/* portmon.c */

/*
 * This is a Port Monitor for 
 *   Windows 95, 98
 *   Windows NT 3.5
 *   Windows NT 4.0
 *   Windows NT 5.0 (Windows 2000 Professional) - needs more testing (Jonas Oberschweiber)
 *
 * This file contains the general port monitor code
 * but not the code that actually does the useful work.
 *
 * For efficiency reasons, don't use the C run time library. 
 *
 * The Windows NT version must use Unicode, so Windows NT
 * specific code is conditionally compiled with a combination
 * of UNICODE, NT35, NT40 and NT50.
 */

/* Publicly accessibly functions in this Port Monitor are prefixed with
 * the following:
 *   rXXX   Windows 95 / NT3.5 / NT4
 *   rsXXX  Windows NT5 port monitor server
 *   rcXXX  Windows NT5 port monitor client (UI)
 */

#define STRICT
#include <windows.h>
#include <htmlhelp.h>
#include "portmon.h"
#include "portrc.h"

/* These are our only global variables */
TCHAR rekey[MAXSTR];   /* Registry key name for our use */
HINSTANCE hdll;
#ifdef UNICODE
int ntver;	/* 351, 400, 500 */
    /* 601 = Windows 7 or Windows Server 2008 R2
     * 600 = Windows Vista or Windows Server 2008
     * 502 = Windows XP x64 or Windows Server 2003
     * 501 = Windows XP
     * 500 = Windows 2000 
     * 400 = Windows NT 4.0
     * 351 = Windows NT 3.51
     */
#endif

/* REDATA must be defined */


/* we don't rely on the import library having XcvData,
 * since we may be compiling with VC++ 5.0 */
BOOL WINAPI XcvData(HANDLE hXcv, LPCWSTR pszDataName, 
    PBYTE pInputData, DWORD cbInputData, 
    PBYTE pOutputData, DWORD cbOutputData, PDWORD pcbOutputNeeded,
    PDWORD pdwStatus);


#define PORTSNAME TEXT("Ports")
#define BACKSLASH TEXT("\\")


/*
 * Required functions for a 95/NT3.51/NT4 Port Monitor are:
 *   AddPort
 *   AddPortEx              (NT only)
 *   ClosePort
 *   ConfigurePort
 *   DeletePort
 *   EndDocPort
 *   EnumPorts
 *   GetPrinterDataFromPort
 *   InitializeMonitor        (NT 3.51 only)
 *   InitializeMonitorEx      (95 only)
 *   InitializePrintMonitor   (NT 4.0 only)
 *   InitializePrintMonitor2  (NT 5.0 only)
 *   InitializePrintMonitorUI (NT 5.0 only)
 *   OpenPort
 *   ReadPort
 *   SetPortOpenTimeouts
 *   StartDocPort
 *   WritePort
 */


#ifdef DEBUG_REDMON
void
syslog(LPCTSTR buf)
{
#ifdef UNICODE
    int count;
    CHAR cbuf[1024];
    int UsedDefaultChar;
#endif
    DWORD cbWritten;
    HANDLE hfile;
    if (buf == NULL)
	buf = TEXT("(null)");

    if ((hfile = CreateFileA("c:\\temp\\redmon.log", GENERIC_WRITE, 
	0 /* no file sharing */, NULL, OPEN_ALWAYS, 0, NULL)) 
		!= INVALID_HANDLE_VALUE) {
	SetFilePointer(hfile, 0, NULL, FILE_END);
#ifdef UNICODE
	while (lstrlen(buf)) {
	    count = min(lstrlen(buf), sizeof(cbuf));
	    WideCharToMultiByte(CP_ACP, 0, buf, count,
		    cbuf, sizeof(cbuf), NULL, &UsedDefaultChar);
	    buf += count;
	    WriteFile(hfile, cbuf, count, &cbWritten, NULL);
	}
#else
	WriteFile(hfile, buf, lstrlen(buf), &cbWritten, NULL);
#endif
	CloseHandle(hfile);
    }
}

void
syserror(DWORD err)
{
LPVOID lpMessageBuffer;
TCHAR buf[MAXSTR];
    wsprintf(buf, TEXT(" error=%d\r\n"), err);
    syslog(buf);
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_FROM_SYSTEM,
	NULL, err,
	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* user default language */
	(LPTSTR) &lpMessageBuffer, 0, NULL);
    if (lpMessageBuffer) {
	syslog((LPTSTR)lpMessageBuffer);
	syslog(TEXT("\r\n"));
	LocalFree(LocalHandle(lpMessageBuffer));
    }
}

void syshex(DWORD num)
{
TCHAR buf[MAXSTR];
    wsprintf(buf, TEXT("0x%x"), num);
    syslog(buf);
}

void sysnum(DWORD num)
{
TCHAR buf[MAXSTR];
    wsprintf(buf, TEXT("%d"), num);
    syslog(buf);
}
#endif


void
show_help(HWND hwnd, int id)
{
    TCHAR topic[2*MAXSTR+7];
    TCHAR buf[MAXSTR];
    TCHAR helpfile[MAXSTR];
    TCHAR *p;

    /* get help file name */
    GetModuleFileName(hdll, helpfile, sizeof(helpfile)/sizeof(TCHAR));
    p = helpfile + lstrlen(helpfile) - 1;
    while (p >= helpfile) {
	if (*p == '\\') {
	    p++;
	    break;
        }
	p--;
    }
    LoadString(hdll, IDS_HELPFILE, p, 
	sizeof(helpfile)/sizeof(TCHAR) - (int)(p-helpfile));

    /* get topic name */
    LoadString(hdll, id, buf, sizeof(buf)/sizeof(TCHAR) - 1);
    wsprintf(topic, TEXT("%s::%s.htm"), helpfile, buf);
    for (p=topic; *p; p++)
	if (*p == ' ')
	    *p = '_';
#ifdef DEBUG_REDMON
syslog(TEXT("show_help \042"));
syslog(topic);
syslog(TEXT("\042\r\n"));
#endif

    /* show help */
    HtmlHelp(hwnd, topic, HH_DISPLAY_TOPIC, 0);
}

LONG
RedMonOpenKey(HANDLE hMonitor, LPCTSTR pszSubKey, REGSAM samDesired,
	PHANDLE phkResult)
{
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    syslog(TEXT("RedMonOpenKey "));
    syslog(pszSubKey);
    syslog(TEXT("\r\n"));
#endif
#ifdef NT50
    if (hMonitor) {
	/* NT50 */
	MONITORINIT *pMonitorInit = (MONITORINIT *)hMonitor;
	rc = pMonitorInit->pMonitorReg->fpOpenKey(
		pMonitorInit->hckRegistryRoot,
		pszSubKey,
		samDesired,
		phkResult,
		pMonitorInit->hSpooler);
    }
    else 
#endif
    {
	TCHAR buf[MAXSTR];
	lstrcpy(buf, rekey);
	if (pszSubKey) {
	    lstrcat(buf, BACKSLASH);
	    lstrcat(buf, pszSubKey);
	}
        rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, buf, 0, samDesired, 
		(PHKEY)phkResult);
    }
#ifdef DEBUG_REDMON
    if (rc)
	syserror(rc);
#endif
    return rc;
}

LONG
RedMonCloseKey(HANDLE hMonitor, HANDLE hcKey)
{
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    syslog(TEXT("RedMonCloseKey\r\n"));
#endif
#ifdef NT50
    if (hMonitor) {
	/* NT50 */
	MONITORINIT *pMonitorInit = (MONITORINIT *)hMonitor;
	rc = pMonitorInit->pMonitorReg->fpCloseKey(
		hcKey, pMonitorInit->hSpooler);
    }
    else 
#endif
    {
	rc = RegCloseKey(hcKey);
    }
#ifdef DEBUG_REDMON
    if (rc)
	syserror(rc);
#endif
    return rc;
}

LONG
RedMonEnumKey(HANDLE hMonitor, HANDLE hcKey, DWORD dwIndex, LPTSTR pszName,
	PDWORD pcchName)
{
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    syslog(TEXT("RedMonEnumKey "));
    sysnum(dwIndex);
    syslog(TEXT("\r\n"));
#endif
#ifdef NT50
    if (hMonitor) {
	/* NT50 */
	MONITORINIT *pMonitorInit = (MONITORINIT *)hMonitor;
	FILETIME ft;
	rc = pMonitorInit->pMonitorReg->fpEnumKey(
		hcKey, dwIndex, pszName, pcchName,
		&ft, pMonitorInit->hSpooler);
    }
    else 
#endif
    {
	rc = RegEnumKey(hcKey, dwIndex, pszName, *pcchName);
    }
#ifdef DEBUG_REDMON
    if (rc)
	syserror(rc);
    else {
	syslog(TEXT(" key="));
	syslog(pszName);
	syslog(TEXT("\r\n"));
    }
#endif
    return rc;
}

LONG
RedMonCreateKey(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszSubKey, 
    DWORD dwOptions, REGSAM samDesired, 
    PSECURITY_ATTRIBUTES pSecurityAttributes, 
    PHANDLE phckResult, PDWORD pdwDisposition)
{
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    syslog(TEXT("RedMonCreateKey "));
    syslog(pszSubKey);
    syslog(TEXT("\r\n"));
#endif
#ifdef NT50
    if (hMonitor) {
	/* NT50 */
	MONITORINIT *pMonitorInit = (MONITORINIT *)hMonitor;
	rc = pMonitorInit->pMonitorReg->fpCreateKey(
		hcKey, pszSubKey, dwOptions,
		samDesired, pSecurityAttributes, phckResult,
		pdwDisposition, pMonitorInit->hSpooler);
    }
    else 
#endif
    {
	rc = RegCreateKeyEx(hcKey, pszSubKey, 0, 0, dwOptions,
		samDesired, pSecurityAttributes, (PHKEY)phckResult,
		pdwDisposition);
    }
#ifdef DEBUG_REDMON
    if (rc)
	syserror(rc);
#endif
    return rc;
}

LONG
RedMonDeleteKey(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszSubKey)
{
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    syslog(TEXT("RedMonDeleteKey "));
    syslog(pszSubKey);
    syslog(TEXT("\r\n"));
#endif
#ifdef NT50
    if (hMonitor) {
	/* NT50 */
	MONITORINIT *pMonitorInit = (MONITORINIT *)hMonitor;
	rc = pMonitorInit->pMonitorReg->fpDeleteKey(
		hcKey, pszSubKey, pMonitorInit->hSpooler);
    }
    else 
#endif
    {
	rc = RegDeleteKey(hcKey, pszSubKey); 
    }
#ifdef DEBUG_REDMON
    if (rc)
	syserror(rc);
#endif
    return rc;
}


LONG
RedMonSetValue(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszValue, DWORD dwType, 
	const BYTE* pData, DWORD cbData)
{
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    TCHAR buf[MAXSTR];
    syslog(TEXT("RedMonSetValue "));
    syslog(pszValue);
    if (dwType==REG_DWORD) {
        wsprintf(buf, TEXT(" REG_DWORD %ld"), (*(LPDWORD)pData));
	syslog(buf);
    }
    else if (dwType=REG_SZ) {
        wsprintf(buf, TEXT(" REG_SZ \042%s\042"), pData);
	syslog(buf);
    }
    syslog(TEXT("\r\n"));
#endif
#ifdef NT50
    if (hMonitor) {
	/* NT50 */
	MONITORINIT *pMonitorInit = (MONITORINIT *)hMonitor;
	rc = pMonitorInit->pMonitorReg->fpSetValue(hcKey, pszValue,
		dwType, pData, cbData, pMonitorInit->hSpooler);
    }
    else 
#endif
    {
	rc = RegSetValueEx(hcKey, pszValue, 0, dwType,
		pData, cbData);
    }
#ifdef DEBUG_REDMON
    if (rc)
	syserror(rc);
#endif
    return rc;
}


LONG
RedMonQueryValue(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszValue, 
	PDWORD pType, PBYTE pData, PDWORD pcbData)
{
    LONG rc = ERROR_SUCCESS;
#ifdef DEBUG_REDMON
    syslog(TEXT("RedMonQueryValue "));
    syslog(pszValue);
    syslog(TEXT("\r\n"));
#endif
#ifdef NT50
    if (hMonitor) {
	/* NT50 */
	MONITORINIT *pMonitorInit = (MONITORINIT *)hMonitor;
	rc = pMonitorInit->pMonitorReg->fpQueryValue(
		hcKey, pszValue, pType, pData, pcbData,
		pMonitorInit->hSpooler);
    }
    else 
#endif
    {
	rc = RegQueryValueEx(hcKey, pszValue, 0, pType, 
	    pData, pcbData);
    }
#ifdef DEBUG_REDMON
    if (rc)
	syserror(rc);
#endif
    return rc;
}


/* NT50 */
BOOL WINAPI rsEnumPorts(HANDLE hMonitor, LPTSTR pName, DWORD Level, 
	LPBYTE pPorts, DWORD cbBuf, LPDWORD pcbNeeded, LPDWORD pcReturned)
{
    HKEY hkey, hsubkey;
    TCHAR portname[MAXSHORTSTR], portdesc[MAXSHORTSTR];
    TCHAR monitorname[MAXSHORTSTR];
    TCHAR buf[MAXSTR];
    int needed;
    DWORD cbData, keytype;
    PORT_INFO_1 *pi1;
    PORT_INFO_2 *pi2;
    LPTSTR pstr;
    LONG rc;
    int i;

#ifdef DEBUG_REDMON
    syslog(TEXT("rsEnumPorts\r\n"));
    {TCHAR buf[MAXSTR];
    wsprintf(buf, TEXT("  hMonitor=0x%0x\r\n"), hMonitor);
    syslog(buf);
    }
    syslog(TEXT("  pName="));
    sysnum((DWORD)pName);
    syslog(TEXT("\r\n"));
#endif

    *pcbNeeded = 0;
    *pcReturned = 0;

    if (hMonitor == NULL) {
	if (pName != NULL)
	    return FALSE;
    }

    if ((Level < 1) || (Level > 2)) {
	SetLastError(ERROR_INVALID_LEVEL);
	return FALSE;
    }

    rc = RedMonOpenKey(hMonitor, PORTSNAME, KEY_READ, &hkey);
    if (rc != ERROR_SUCCESS)
	return TRUE;	/* There are no ports */

    LoadString(hdll, IDS_MONITORNAME, monitorname, 
	sizeof(monitorname)/sizeof(TCHAR)-1);

    /* First pass is to calculate the number of bytes needed */
    needed = 0;
    i = 0;
    cbData = sizeof(portname);
    rc = RedMonEnumKey(hMonitor, hkey, 0, portname, &cbData);
    while (rc == ERROR_SUCCESS) {
	needed += (lstrlen(portname) + 1) * sizeof(TCHAR);
	if (Level == 1) {
	    needed += sizeof(PORT_INFO_1);
	}
	else if (Level == 2) {
	    needed += sizeof(PORT_INFO_2);
	    needed += (lstrlen(monitorname) + 1) * sizeof(TCHAR);
	    lstrcpy(buf, PORTSNAME);
	    lstrcat(buf, BACKSLASH);
	    lstrcat(buf, portname);
	    rc = RedMonOpenKey(hMonitor, buf, KEY_READ, &hsubkey);
	    if (rc == ERROR_SUCCESS) {
		cbData = sizeof(portdesc);
		keytype = REG_SZ;
	        RedMonQueryValue(hMonitor, hsubkey, DESCKEY, &keytype, 
			(LPBYTE)portdesc, &cbData);
		if (rc == ERROR_SUCCESS)
		    needed += (lstrlen(portdesc) + 1) * sizeof(TCHAR);
		else
		    needed += 1 * sizeof(TCHAR);	/* empty string */
		RedMonCloseKey(hMonitor, hsubkey);
	    }
	    else {
		needed += 1 * sizeof(TCHAR);	/* empty string */
	    }
	}
	i++;
	cbData = sizeof(portname);
        rc = RedMonEnumKey(hMonitor, hkey, i, portname, &cbData);
    }
    *pcbNeeded = needed;
   
    if ((pPorts == NULL) || ((unsigned int)needed > cbBuf)) {
	RedMonCloseKey(hMonitor, hkey);
	SetLastError(ERROR_INSUFFICIENT_BUFFER);
	return FALSE;
    }

    /* Second pass to copy the data to the buffer */

    /* PORT_INFO_x structures must be placed at the beginning
     * of the buffer, and strings at the end of the buffer.
     * This is important!  It appears that one buffer is 
     * allocated, then each port monitor is called in turn to
     * add its entries to the buffer, between previous entries.
     */

    i = 0;
    pi1 = (PORT_INFO_1 *)pPorts;
    pi2 = (PORT_INFO_2 *)pPorts;
    pstr = (LPTSTR)(pPorts + cbBuf);
    cbData = sizeof(portname);
    rc = RedMonEnumKey(hMonitor, hkey, 0, portname, &cbData);
    while (rc == ERROR_SUCCESS) {
	if (Level == 1) {
	    pstr -= lstrlen(portname) + 1;
	    lstrcpy(pstr, portname);
	    pi1[i].pName = pstr;
	}
	else if (Level == 2){
	    pstr -= lstrlen(portname) + 1;
	    lstrcpy(pstr, portname);
	    pi2[i].pPortName = pstr;

	    pstr -= lstrlen(monitorname) + 1;
	    lstrcpy(pstr, monitorname);
	    pi2[i].pMonitorName = pstr;

	    lstrcpy(buf, PORTSNAME);
	    lstrcat(buf, BACKSLASH);
	    lstrcat(buf, portname);
	    rc = RedMonOpenKey(hMonitor, buf, KEY_READ, &hsubkey);
	    if (rc == ERROR_SUCCESS) {
		cbData = sizeof(portdesc);
		keytype = REG_SZ;
	        RedMonQueryValue(hMonitor, hsubkey, DESCKEY, &keytype, 
			(LPBYTE)portdesc, &cbData);
		if (rc != ERROR_SUCCESS)
		    portdesc[0] = '\0';
		RedMonCloseKey(hMonitor, hsubkey);
	    }
	    else {
		portdesc[0] = '\0';
	    }

	    pstr -= lstrlen(portdesc) + 1;
	    lstrcpy(pstr, portdesc);
	    pi2[i].pDescription = pstr;

	    /* Say that writing to this port is supported, */
	    /* but reading is not. */
	    /* Using fPortType = 3 is wrong. */
	    /* Options are PORT_TYPE_WRITE=1, PORT_TYPE_READ=2, */
	    /*   PORT_TYPE_REDIRECTED=4, PORT_TYPE_NET_ATTACHED=8 */
	    pi2[i].fPortType = PORT_TYPE_WRITE;
	    pi2[i].Reserved = 0;
	}
	i++;
	cbData = sizeof(portname);
        rc = RedMonEnumKey(hMonitor, hkey, i, portname, &cbData);
    }
    *pcReturned = i;
    RedMonCloseKey(hMonitor, hkey);

    return TRUE;
}

/* 95 / NT40 */
BOOL WINAPI rEnumPorts(LPTSTR pName, DWORD Level, LPBYTE  pPorts, 
         DWORD cbBuf, LPDWORD pcbNeeded, LPDWORD pcReturned)
{
    return rsEnumPorts(NULL, 
	pName, Level, pPorts, cbBuf, pcbNeeded, pcReturned);
}

typedef BOOL (WINAPI *pfnRealEnumPorts) 
	(LPTSTR pName, DWORD Level, LPBYTE pPorts, 
	DWORD cbBuf, LPDWORD pcbNeeded, LPDWORD pcReturned); 

BOOL sPortExists(HANDLE hMonitor, LPTSTR pszPortName)
{
    BOOL port_exists = FALSE;
    HGLOBAL hglobal;
    LPBYTE ports;
    DWORD cbBuf, needed, returned;
    PORT_INFO_2 *pi2;
    int j;
    pfnRealEnumPorts RealEnumPorts;
#ifdef NT35
    HINSTANCE hWinspool;
#endif
    
#ifdef DEBUG_REDMON
    syslog(TEXT("sPortExists "));
    syslog(pszPortName);
    syslog(TEXT("\r\n"));
    {TCHAR buf[MAXSTR];
    wsprintf(buf, TEXT("  hMonitor=0x%0x\r\n"), hMonitor);
    syslog(buf);
    }
#endif
    /* Get the list of existing ports. */
    /* Ask for all ports, not just ours.
     * If we ask for only RedMon ports by calling rsEnumPorts, a user
     * can create a second LPT1: This isn't useful because it doesn't 
     * get serviced before the Local Port, and we can't delete it
     * with the Delete Port button.  Instead we have to use the
     * Registry Editor to clean up the mess.
     * To avoid this, stop users from entering any existing port name.
     */
#ifdef NT35
    /*
     * To enumerate all ports, we need to call EnumPorts in the 
     * winspool DLL, not our own implementation which has the
     * same name in the NT35 version.
     */
    hWinspool = LoadLibrary(TEXT("winspool.drv"));
    if (hWinspool < (HINSTANCE)HINSTANCE_ERROR)
	return FALSE;
    RealEnumPorts = (pfnRealEnumPorts)GetProcAddress(hWinspool, "EnumPortsW"); 
    if (RealEnumPorts == (pfnRealEnumPorts)NULL)
	return FALSE;
#else
    RealEnumPorts = EnumPorts;
#endif

    needed = 0;
    /* 1998-01-08
     * The TELES.FaxMon monitor is buggy - it returns an incorrect
     * value for "needed" if we use the following call.
     *    EnumPorts(NULL, 2, NULL, 0, &needed, &returned);
     * To dodge this bug, pass it buffer of non-zero size.
     */
   
    cbBuf = 4096;
    hglobal = GlobalAlloc(GPTR, (DWORD)cbBuf);  
    ports = GlobalLock(hglobal);
    if (ports == NULL) {
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
#ifdef NT35
	FreeLibrary(hWinspool);
#endif
	return FALSE;
    }

    if (!RealEnumPorts(NULL, 2, ports, cbBuf, &needed, &returned)) {
#ifdef DEBUG_REDMON
	DWORD err = GetLastError();
	syslog(TEXT("EnumPorts failed\r\n"));
	syserror(err);
#endif
	GlobalUnlock(hglobal);
	GlobalFree(hglobal);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
#ifdef NT35
	    FreeLibrary(hWinspool);
#endif
	    return FALSE;	/* give up */
	}
	else {
	    /* try again, with requested size */
            cbBuf = needed;
            /* If there are no ports installed, avoid trying to 
	     * allocate 0 bytes
	     */
	    hglobal = GlobalAlloc(GPTR, (DWORD)cbBuf+4);  
	    ports = GlobalLock(hglobal);
	    if (ports == NULL) {
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
#ifdef NT35
		FreeLibrary(hWinspool);
#endif
		return FALSE;
	    }
	    if (!RealEnumPorts(NULL, 2, ports, cbBuf, &needed, &returned)) {
#ifdef DEBUG_REDMON
		DWORD err = GetLastError();
		syslog(TEXT("EnumPorts failed\r\n"));
		syserror(err);
#endif
		GlobalUnlock(hglobal);
		GlobalFree(hglobal);
#ifdef NT35
		FreeLibrary(hWinspool);
#endif
		return FALSE;
		return FALSE;
	    }
	}
    }
#ifdef NT35
    FreeLibrary(hWinspool);
#endif
#ifdef DEBUG_REDMON
syslog(TEXT("EnumPorts returns "));
sysnum(returned);
syslog(TEXT(" port \r\n"));
#endif

    pi2 = (PORT_INFO_2 *)ports;

    for (j=0; j<(int)returned; j++) {
#ifdef DEBUG_REDMON
syslog(TEXT("  port "));
sysnum(j);
syslog(TEXT("='"));
syslog(pi2[j].pPortName);
syslog(TEXT("'\r\n"));
#endif
	if (lstrcmp(pszPortName, pi2[j].pPortName) == 0)
	    port_exists = TRUE;
    }

    GlobalUnlock(hglobal);
    GlobalFree(hglobal);

#ifdef DEBUG_REDMON
    syslog(TEXT("sPortExists returns "));
    syslog(port_exists ? TEXT("true\r\n") : TEXT("false\r\n"));
#endif

    return port_exists;
}


TCHAR XcvMonitor[] = TEXT("XcvMonitor");
TCHAR XcvPort[] = TEXT("XcvPort");

/* Copy to pServerName the printer name for accessing 
 * the port monitor server */
BOOL MakeXcvName(LPTSTR pServerName, LPCTSTR pServer, LPCTSTR pType,
	LPCTSTR pName)
{
    DWORD len = 2 + lstrlen(pServer) + 2 + lstrlen(pType) + 1 + 
		lstrlen(pName) + 1;
    if (len > MAXSTR)
	return FALSE;

    *pServerName = 0;
    if (pServer) {
	lstrcat(pServerName, pServer);
	lstrcat(pServerName, TEXT("\\"));
    }
    lstrcat(pServerName, TEXT(","));
    lstrcat(pServerName, pType);
    lstrcat(pServerName, TEXT(" "));
    if (pName)
	lstrcat(pServerName, pName);
    return TRUE;
}

/* 95 / NT 40 */
BOOL WINAPI rConfigurePort(LPTSTR pName, HWND hWnd, LPTSTR pPortName)
{
    DWORD config_len = redmon_sizeof_config();
    HGLOBAL hglobal = GlobalAlloc(GPTR, config_len);
    RECONFIG *pconfig = GlobalLock(hglobal);
    BOOL flag = TRUE;

    /* NT4 spooler does not support remote ConfigurePort calls */
    /* pName can be ignored */
    if (pName != NULL) {
	SetLastError(ERROR_ACCESS_DENIED);
	flag = FALSE;
    }

    if (flag && !redmon_get_config(NULL, pPortName, pconfig))
	flag = FALSE;

    if (flag && DialogBoxParam(hdll, MAKEINTRESOURCE(IDD_CONFIGPORT), 
		hWnd, ConfigDlgProc, (LPARAM)pconfig)) {
	flag = redmon_set_config(NULL, pconfig);
    }
    GlobalUnlock(hglobal);
    GlobalFree(hglobal);
    return flag;
}

#ifdef NT50
DWORD
cConfigurePortUI(HANDLE hXcv, HWND hWnd, PCWSTR pszPortName)
{
    DWORD config_len = redmon_sizeof_config();
    HGLOBAL hglobal = GlobalAlloc(GPTR, config_len);
    RECONFIG *pconfig = GlobalLock(hglobal);
    DWORD dwOutput = 0;
    DWORD dwNeeded = 0;
    DWORD dwError = ERROR_SUCCESS;

#ifdef DEBUG_REDMON
    syslog(TEXT("cConfigurePortUI\r\n"));
#endif

    /* get current configuration */
    if (!XcvData(hXcv, TEXT("GetConfig"), (PBYTE)pszPortName, 
	sizeof(TCHAR)*(lstrlen(pszPortName)+1),
	(PBYTE)pconfig, config_len, &dwNeeded, &dwError))
	dwError = GetLastError();

    /* update it */
    if (dwError == ERROR_SUCCESS) {
	if (DialogBoxParam(hdll, MAKEINTRESOURCE(IDD_CONFIGPORT), 
		    hWnd, ConfigDlgProc, (LPARAM)pconfig)) {
	    if (!XcvData(hXcv, TEXT("SetConfig"), 
		(PBYTE)pconfig, config_len, 
		(PBYTE)(&dwOutput), 0, &dwNeeded, &dwError))
		dwError = GetLastError();
	}
    }
    
    GlobalUnlock(hglobal);
    GlobalFree(hglobal);
    return dwError;
}

BOOL WINAPI rcConfigurePortUI(PCWSTR pszServer, HWND hWnd, PCWSTR pszPortName)
{
    TCHAR pszServerName[MAXSTR];
    HANDLE hXcv = NULL;
    DWORD dwError;
    PRINTER_DEFAULTS pd;

    if (!MakeXcvName(pszServerName, pszServer, XcvPort, pszPortName))
	return FALSE;

    pd.pDatatype = NULL;
    pd.pDevMode = NULL;
    pd.DesiredAccess = SERVER_ACCESS_ADMINISTER;

#ifdef DEBUG_REDMON
    syslog(TEXT("OpenPrinter "));
    syslog(pszServerName);
    syslog(TEXT("\r\n"));
#endif
    if (!OpenPrinter(pszServerName, &hXcv, &pd))
	return FALSE;

    dwError = cConfigurePortUI(hXcv, hWnd, pszPortName);

    ClosePrinter(hXcv);

    return (dwError == ERROR_SUCCESS);
}
#endif /* NT50 */

BOOL sAddPort(HANDLE hMonitor, LPTSTR pszPortName)
{
    LONG rc;
    HKEY hkey, hsubkey;
    TCHAR szPortDesc[MAXSHORTSTR];
#ifdef DEBUG_REDMON
    syslog(TEXT("sAddPort\r\n"));
#endif

    if (sPortExists(hMonitor, pszPortName))
	return FALSE;

    /* store new port name in registry */
    LoadString(hdll, IDS_MONITORNAME, szPortDesc, 
	    sizeof(szPortDesc)/sizeof(TCHAR)-1);
    rc = RedMonOpenKey(hMonitor, PORTSNAME, KEY_ALL_ACCESS, &hkey);
    if (rc != ERROR_SUCCESS) {
	/* try to create the Ports key */
        rc = RedMonOpenKey(hMonitor, NULL, KEY_ALL_ACCESS, &hkey);
	if (rc == ERROR_SUCCESS) {
	    rc = RedMonCreateKey(hMonitor, hkey, PORTSNAME, 0, 
		KEY_ALL_ACCESS, NULL, &hsubkey, NULL);
	    if (rc == ERROR_SUCCESS)
		RedMonCloseKey(hMonitor, hsubkey);
	    RedMonCloseKey(hMonitor, hkey);
	}
        rc = RedMonOpenKey(hMonitor, PORTSNAME, KEY_ALL_ACCESS, &hkey);
    }
    if (rc == ERROR_SUCCESS) {
	rc = RedMonCreateKey(hMonitor, hkey, pszPortName, 0, KEY_ALL_ACCESS, 
		NULL, &hsubkey, NULL);
	if (rc == ERROR_SUCCESS) {
	    rc = RedMonSetValue(hMonitor, hsubkey, DESCKEY, REG_SZ,
	      (CONST BYTE *)szPortDesc, (lstrlen(szPortDesc)+1)*sizeof(TCHAR));
	    RedMonCloseKey(hMonitor, hsubkey);
	}
	RedMonCloseKey(hMonitor, hkey);
    }
    return (rc == ERROR_SUCCESS);
}


/* 95 / NT 40 */
BOOL WINAPI rAddPort(LPTSTR pName, HWND hWnd, LPTSTR pMonitorName)
{
    TCHAR portdesc[MAXSHORTSTR];
    TCHAR buf[MAXSTR], str[MAXSTR];
    BOOL unique;
    int i;
    DWORD config_len = redmon_sizeof_config();
    HGLOBAL hglobal;
    RECONFIG *pconfig;
    BOOL flag = TRUE;
    LPTSTR portname;

    /* NT4 spooler does not support remote AddPort calls */
    /* pName and pMonitorName can be ignored */
    if (pName != NULL) {
	SetLastError(ERROR_ACCESS_DENIED);
	return FALSE;
    }

    hglobal = GlobalAlloc(GPTR, config_len);
    pconfig = GlobalLock(hglobal);
    portname = redmon_init_config(pconfig);

    unique = TRUE;
    do {
	if (!unique) {
	    LoadString(hdll, IDS_NOTUNIQUE, str, sizeof(str)/sizeof(TCHAR)-1);
	    wsprintf(buf, str, portname);
	    LoadString(hdll, IDS_ADDPORT, str, sizeof(str)/sizeof(TCHAR)-1);
	    if (MessageBox(hWnd, buf, str, MB_OKCANCEL) == IDCANCEL) {
		flag = FALSE;
		break;
	    }
	}

	/* Suggest a unique port name */
	for (i=1; i<100; i++) {
	    if (!redmon_suggest_portname(portname, MAXSHORTSTR, i))
		break;
	    if (!sPortExists(NULL, portname))
		break;
	}
        LoadString(hdll, IDS_MONITORNAME, portdesc, 
		sizeof(portdesc)/sizeof(TCHAR)-1);

	if (!DialogBoxParam(hdll, MAKEINTRESOURCE(IDD_ADDPORT), 
		hWnd, AddDlgProc, (LPARAM)pconfig)) {
	    flag = FALSE;
	    break;
	}

	if (lstrlen(portname) && portname[lstrlen(portname)-1] != ':')
	    lstrcat(portname, TEXT(":"));	/* append ':' if not present */

        if (sPortExists(NULL, portname))
	    unique = FALSE;
    } while (!unique);
    
    if (flag)
	flag = sAddPort(NULL, portname);

    if (flag)
	flag = redmon_set_config(NULL, pconfig);

    GlobalUnlock(hglobal);
    GlobalFree(hglobal);

    return flag;
}

#ifdef NT50
LONG cAddPortUI(HANDLE hXcv, HWND hWnd, 
	PCWSTR pszPortNameIn, PWSTR pszPortNameOut)
{
    DWORD dwStatus;
    DWORD dwOutput;
    DWORD dwNeeded;
    TCHAR str[MAXSTR];
    TCHAR buf[MAXSTR];
    BOOL unique;
    int i;
    DWORD config_len = redmon_sizeof_config();
    HGLOBAL hglobal;
    RECONFIG *pconfig;
    LPTSTR portname;	/* pointer to port name in RECONFIG */

#ifdef DEBUG_REDMON
    syslog(TEXT("cAddPortUI "));
    syslog(pszPortNameIn);
    syslog(TEXT("\r\n"));
#endif
    *pszPortNameOut = '\0';

    hglobal = GlobalAlloc(GPTR, config_len);
    pconfig = GlobalLock(hglobal);
    if (pconfig == NULL)
	return ERROR_NOT_ENOUGH_MEMORY;
    portname = redmon_init_config(pconfig);

    dwStatus = ERROR_SUCCESS;
    unique = TRUE;
    do {
	if (!unique) {
	    LoadString(hdll, IDS_NOTUNIQUE, str, sizeof(str)/sizeof(TCHAR)-1);
	    wsprintf(buf, str, portname);
	    LoadString(hdll, IDS_ADDPORT, str, sizeof(str)/sizeof(TCHAR)-1);
	    if (MessageBox(hWnd, buf, str, MB_OKCANCEL) == IDCANCEL) {
		dwStatus = ERROR_CANCELLED;
		break;
	    }
	}
	if (dwStatus == ERROR_CANCELLED)
	    break;

	/* Suggest a unique port name */
	for (i=1; i<10; i++) {
	    if (!redmon_suggest_portname(portname, MAXSHORTSTR, i))
		break;
	    dwStatus = ERROR_SUCCESS;
	    if (!XcvData(hXcv, TEXT("PortExists"), (PBYTE)portname, 
		    sizeof(TCHAR)*(lstrlen(portname)+1),
		    (PBYTE)(&dwOutput), 0, &dwNeeded, &dwStatus)) {
		dwStatus = GetLastError();
	    }
	    if (dwStatus == ERROR_SUCCESS)
		break;	/* unique name */
	}

	if (!DialogBoxParam(hdll, MAKEINTRESOURCE(IDD_ADDPORT), 
		hWnd, AddDlgProc, (LPARAM)pconfig)) {
	    dwStatus = ERROR_CANCELLED;
	    break;
	}

	if (lstrlen(portname) && portname[lstrlen(portname)-1] != ':')
	    lstrcat(portname, TEXT(":"));	/* append ':' if not present */

	dwStatus = ERROR_SUCCESS;
	if (!XcvData(hXcv, TEXT("PortExists"), (PBYTE)portname, 
		sizeof(TCHAR)*(lstrlen(portname)+1),
		(PBYTE)(&dwOutput), 0, &dwNeeded, &dwStatus))
	    dwStatus = GetLastError();
        if (dwStatus != ERROR_SUCCESS)
	    unique = FALSE;
    } while (!unique);

    if (dwStatus == ERROR_SUCCESS) {
	if (!XcvData(hXcv, TEXT("AddPort"), (PBYTE)portname, 
		sizeof(TCHAR)*(lstrlen(portname)+1),
		(PBYTE)(&dwOutput), 0, &dwNeeded, &dwStatus))
	    dwStatus = GetLastError();
	else 
	    lstrcpyn(pszPortNameOut, portname, MAXSTR-1);
    }
    
    if (dwStatus == ERROR_SUCCESS) {
	/* Set configuration if supplied */
	dwOutput = 0;
	dwNeeded = 0;
	if (!XcvData(hXcv, TEXT("SetConfig"), 
	    (PBYTE)pconfig, config_len, 
	    (PBYTE)(&dwOutput), 0, &dwNeeded, &dwStatus))
	    dwStatus = GetLastError();
    }

    GlobalUnlock(hglobal);
    GlobalFree(hglobal);

    return dwStatus;
}

BOOL WINAPI rcAddPortUI(PCWSTR pszServer, HWND hWnd, 
	PCWSTR pszPortNameIn, PWSTR *ppszPortNameOut)
{
    TCHAR pszServerName[MAXSTR];
    TCHAR portname[MAXSTR];
    PRINTER_DEFAULTS pd;
    HANDLE hXcv = NULL;
    DWORD dwError;

#ifdef DEBUG_REDMON
    syslog(TEXT("rcAddPortUI\r\n"));
    syslog(TEXT("pszServer=\042"));
    syslog(pszServer);
    syslog(TEXT("\042\r\n"));
    syslog(TEXT("pszPortNameIn=\042"));
    syslog(pszPortNameIn);
    syslog(TEXT("\042\r\n"));
#endif
    if (!MakeXcvName(pszServerName, pszServer, XcvMonitor, pszPortNameIn)) {
#ifdef DEBUG_REDMON
	syslog(TEXT("MakeXcvName failed\r\n"));
#endif
	return FALSE;
    }

    pd.pDatatype = NULL;
    pd.pDevMode = NULL;
    pd.DesiredAccess = SERVER_ACCESS_ADMINISTER;

#ifdef DEBUG_REDMON
    syslog(TEXT("OpenPrinter "));
    syslog(pszServerName);
    syslog(TEXT("\r\n"));
#endif
    if (!OpenPrinter(pszServerName, &hXcv, &pd)) {
#ifdef DEBUG_REDMON
	TCHAR buf[MAXSTR];
	wsprintf(buf, TEXT("OpenPrinter failed error=%d\r\n"), 
	   GetLastError());
	syslog(buf);
	wsprintf(buf, TEXT(" OpenPrinter handle=0x%x\r\n"), hXcv);
	syslog(buf);
#endif
	return FALSE;
    }

    dwError = cAddPortUI(hXcv, hWnd, pszPortNameIn, portname);

    if (dwError == ERROR_SUCCESS) {
#ifdef DEBUG_REDMON
	syslog(TEXT("Added port "));
	syslog(portname);
	syslog(TEXT("\r\n"));
#endif
	if (ppszPortNameOut) {
	    DWORD len = sizeof(TCHAR)*(lstrlen(portname)+1);
	    *ppszPortNameOut = GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, len);
	    if (*ppszPortNameOut)
		CopyMemory(*ppszPortNameOut, portname, len);
	}
    }

    ClosePrinter(hXcv);

    return (dwError == ERROR_SUCCESS);
}
#endif /* NT50 */


#if defined(NT35) || defined(NT40)
/* Windows NT only */
/* untested */
BOOL WINAPI rAddPortEx(LPWSTR pName, DWORD Level, LPBYTE lpBuffer, 
     LPWSTR lpMonitorName)
{
    TCHAR *portname;

    if (Level == 1) {
	PORT_INFO_1 *addpi1 = (PORT_INFO_1 *)lpBuffer;
	portname = addpi1->pName;
    }
    if (Level == 2) {
	PORT_INFO_2 *addpi2 = (PORT_INFO_2 *)lpBuffer;
	portname = addpi2->pPortName;
    }

    return sAddPort(NULL, portname);
}
#endif


BOOL sDeletePort(HANDLE hMonitor, LPCTSTR pPortName)
{
    HKEY hkey;
    LONG rc;
#ifdef DEBUG_REDMON
    syslog(TEXT("sDeletePort "));
    syslog(pPortName);
    syslog(TEXT("\r\n"));
#endif
    rc = RedMonOpenKey(hMonitor, PORTSNAME, KEY_ALL_ACCESS, &hkey);
#ifdef DEBUG_REDMON
    syslog(TEXT(" hMonitor="));
    syshex((DWORD)hMonitor);
    syslog(TEXT("\r\n"));
    syslog(TEXT(" hKey="));
    syshex((DWORD)hkey);
    syslog(TEXT("\r\n"));
    syslog(TEXT(" pPortName="));
    syshex((DWORD)pPortName);
    syslog(TEXT(" \042"));
    syslog(pPortName);
    syslog(TEXT("\042\r\n"));
#endif
    if (rc == ERROR_SUCCESS) {
	rc = RedMonDeleteKey(hMonitor, hkey, pPortName);
	RedMonCloseKey(hMonitor, hkey);
    }
    return (rc == ERROR_SUCCESS);
}

BOOL WINAPI rDeletePort(LPTSTR pName, HWND hWnd, LPTSTR pPortName)
{
    if (pName != NULL)
	return FALSE;

    return sDeletePort(NULL, pPortName);
}

#ifdef NT50
BOOL WINAPI rcDeletePortUI(PCWSTR pszServer, HWND hWnd, PCWSTR pszPortName)
{
    TCHAR pszServerName[MAXSTR];
    HANDLE hXcv = NULL;
    DWORD dwStatus = ERROR_SUCCESS;
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwOutput = 0;
    DWORD dwNeeded = 0;
    PRINTER_DEFAULTS pd;

#ifdef DEBUG_REDMON
    syslog(TEXT("rcDeletePortUI "));
    syslog(pszPortName);
    syslog(TEXT("\r\n"));
#endif

    if (!MakeXcvName(pszServerName, pszServer, XcvPort, pszPortName)) {
#ifdef DEBUG_REDMON
	syslog(TEXT("MakeXcvName failed\r\n"));
#endif
	return FALSE;
    }

    pd.pDatatype = NULL;
    pd.pDevMode = NULL;
    pd.DesiredAccess = SERVER_ACCESS_ADMINISTER;

#ifdef DEBUG_REDMON
    syslog(TEXT("OpenPrinter "));
    syslog(pszServerName);
    syslog(TEXT("\r\n"));
#endif

    if (!OpenPrinter(pszServerName, &hXcv, &pd)) {
#ifdef DEBUG_REDMON
	dwError = GetLastError();
	syslog(TEXT("OpenPrinter failed\r\n"));
	syserror(dwError);
#endif
	return FALSE;
    }

#ifdef DEBUG_REDMON
    {TCHAR buf[MAXSTR];
    wsprintf(buf, TEXT("rcDeletePortUI hXcv=0x%x\r\n"), hXcv); 
    syslog(buf);
    }
#endif

    /* Delete port */
    if (!XcvData(hXcv, TEXT("DeletePort"), (PBYTE)pszPortName, 
	sizeof(TCHAR)*(lstrlen(pszPortName)+1),
	(PBYTE)(&dwOutput), 0, &dwNeeded, &dwStatus)) {
#ifdef DEBUG_REDMON
	dwError = GetLastError();
	syslog(TEXT("XcvData failed\r\n"));
	syserror(dwError);
#endif
    }

#ifdef DEBUG_REDMON
    syslog(TEXT("XcvData DeletePort returns "));
    sysnum(dwStatus);
    syslog(TEXT("\r\n"));
    if (dwStatus)
        syserror(dwStatus);
#endif

    ClosePrinter(hXcv);

    return (dwStatus == ERROR_SUCCESS);
}
#endif


#ifdef NT50
typedef struct tagREXCVPORT {
    DWORD dwSize;
    HANDLE hMonitor;
    TCHAR szPortName[MAXSTR];
    ACCESS_MASK GrantedAccess;
} REXCVPORT;

BOOL WINAPI rsXcvOpenPort(HANDLE hMonitor, LPCWSTR pszObject, 
	ACCESS_MASK GrantedAccess, PHANDLE phXcv)
{
    REXCVPORT *xcv;
#ifdef DEBUG_REDMON
    syslog(TEXT("rsXcvOpenPort\r\n"));
#endif
    *phXcv = (PHANDLE)NULL;
    xcv = GlobalLock(GlobalAlloc(GPTR, (DWORD)sizeof(REXCVPORT)));
    if (xcv == NULL)
	return FALSE;

    memset(xcv, 0, sizeof(REXCVPORT));
    xcv->dwSize = sizeof(REXCVPORT);
    xcv->hMonitor = hMonitor;
    lstrcpyn(xcv->szPortName, pszObject, 
	sizeof(xcv->szPortName)/sizeof(TCHAR) - 1);
    xcv->GrantedAccess = GrantedAccess;
#ifdef DEBUG_REDMON
    {TCHAR buf[MAXSTR];
    wsprintf(buf, TEXT("rsXcvOpenPort hXcv=0x%x\r\n"), xcv); 
    wsprintf(buf, TEXT(" hMonitor = 0x%x\r\n"), xcv->hMonitor); 
    syslog(buf);
    }
#endif
    *phXcv = (PHANDLE)xcv;
    return TRUE;
}
 

DWORD WINAPI rsXcvDataPort(HANDLE hXcv, LPCWSTR pszDataName, PBYTE pInputData, 
	DWORD cbInputData, PBYTE pOutputData, DWORD cbOutputData, 
	PDWORD pcbOutputNeeded)
{
    REXCVPORT *xcv = (REXCVPORT *)hXcv;

#ifdef DEBUG_REDMON
    syslog(TEXT("rsXcvDataPort\r\n  "));
    syslog(pszDataName);
    syslog(TEXT("\r\n"));
    syshex((DWORD)hXcv);
    syslog(TEXT("\r\n"));
#endif

    if ((xcv == NULL) || (xcv->dwSize != sizeof(REXCVPORT))) {
#ifdef DEBUG_REDMON
        syslog(TEXT("Invalid parameter\r\n"));
#endif
	return ERROR_INVALID_PARAMETER;
    }

    if (lstrcmp(pszDataName, TEXT("AddPort"))==0) {
	if (!(xcv->GrantedAccess & SERVER_ACCESS_ADMINISTER))
	    return ERROR_ACCESS_DENIED;
	/* pInputData contains the port name */
	if (sAddPort(xcv->hMonitor, (LPTSTR)pInputData))
	    return ERROR_SUCCESS;
    }
    else if (lstrcmp(pszDataName, TEXT("DeletePort"))==0) {
	if (!(xcv->GrantedAccess & SERVER_ACCESS_ADMINISTER))
	    return ERROR_ACCESS_DENIED;
	if (sDeletePort(xcv->hMonitor, (LPTSTR)pInputData))
	    return ERROR_SUCCESS;
    }
    else if (lstrcmp(pszDataName, TEXT("MonitorUI"))==0) {
	/* Our server and UI DLLs are combined */
	TCHAR buf[MAXSTR];
        DWORD len;
	if (pOutputData == NULL)
	    return ERROR_INSUFFICIENT_BUFFER;
	len = GetModuleFileName(hdll, buf, sizeof(buf)/sizeof(TCHAR)-1);
#ifdef DEBUG_REDMON
	{
	TCHAR mess[MAXSTR];
	wsprintf(mess, TEXT("GetModuleFileName=\042%s\042 len=%d\r\n"),
	    buf, len);
	syslog(mess);
	}
#endif
	*pcbOutputNeeded = (len + 1) * sizeof(TCHAR);
	if (*pcbOutputNeeded > cbOutputData)
	   return ERROR_INSUFFICIENT_BUFFER;
	memcpy(pOutputData, buf, *pcbOutputNeeded);
#ifdef DEBUG_REDMON
        syslog(TEXT(" returned UI name="));
        syslog((LPTSTR)pOutputData);
        syslog(TEXT("\r\n"));
#endif
	return ERROR_SUCCESS;
    }
    else if (lstrcmp(pszDataName, TEXT("PortExists"))==0) {
	/* pInputData contains the port name */
	if (sPortExists(xcv->hMonitor, (LPTSTR)pInputData))
	    return ERROR_PRINTER_ALREADY_EXISTS;	/* TRUE */
	else
	    return ERROR_SUCCESS;			/* FALSE */
    }
    else if (lstrcmp(pszDataName, TEXT("GetConfig"))==0) {
	*pcbOutputNeeded = redmon_sizeof_config();
	if (*pcbOutputNeeded > cbOutputData)
	   return ERROR_INSUFFICIENT_BUFFER;
	if (redmon_get_config(xcv->hMonitor, (LPCTSTR)pInputData,
	    (RECONFIG *)pOutputData))
	    return ERROR_SUCCESS;
    }
    else if (lstrcmp(pszDataName, TEXT("SetConfig"))==0) {
	if (!(xcv->GrantedAccess & SERVER_ACCESS_ADMINISTER))
	    return ERROR_ACCESS_DENIED;
	if (!redmon_validate_config((RECONFIG *)pInputData))
	    return ERROR_INVALID_PARAMETER;
	if (redmon_set_config(xcv->hMonitor, (RECONFIG *)pInputData))
	    return ERROR_SUCCESS;
    }

    return ERROR_INVALID_PARAMETER;
}

BOOL WINAPI rsXcvClosePort(HANDLE hXcv)
{
    REXCVPORT *xcv = (REXCVPORT *)hXcv;
#ifdef DEBUG_REDMON
    syslog(TEXT("rsXcvClosePort\r\n"));
#endif
    if ((xcv == NULL) || (xcv->dwSize != sizeof(REXCVPORT)))
	return FALSE;
    GlobalUnlock(GlobalHandle(hXcv));
    GlobalFree(GlobalHandle(hXcv));
    return TRUE;
}

VOID WINAPI rsShutdown(HANDLE hMonitor) 
{
    /* undo InitializePrintMonitor2 */
    /* nothing to do */
#ifdef DEBUG_REDMON
    syslog(TEXT("rsShutDown\r\n"));
#endif
}


#ifdef MAKEEXE
/* test routine which calls local function */
BOOL WINAPI XcvData(HANDLE hXcv, LPCWSTR pszDataName, 
    PBYTE pInputData, DWORD cbInputData, 
    PBYTE pOutputData, DWORD cbOutputData, PDWORD pcbOutputNeeded,
    PDWORD pdwStatus) 
{
    *pdwStatus = rsXcvDataPort(hXcv, pszDataName, pInputData, cbInputData,
	pOutputData, cbOutputData, pcbOutputNeeded);
    return TRUE;
}
#else	/* !MAKEEXE */
/* #if _MSC_VER <= 1100   /* MSVC++ 5.0 or earlier */
#if _MSC_VER <= 1200   /* MSVC++ 6.0 or earlier */
/* We don't have XcvData in the import library, so we must use
 * run time linking.  This is inefficient, but fortunately we 
 * only need this for port configuration, not for normal use.
 * Using run time linking also allows to have one DLL which 
 * works for both Windows NT4 and 2000. */
typedef BOOL (WINAPI *pfnXcvData)(HANDLE hXcv, LPCWSTR pszDataName, 
    PBYTE pInputData, DWORD cbInputData, 
    PBYTE pOutputData, DWORD cbOutputData, PDWORD pcbOutputNeeded,
    PDWORD pdwStatus);

BOOL WINAPI XcvData(HANDLE hXcv, LPCWSTR pszDataName, 
    PBYTE pInputData, DWORD cbInputData, 
    PBYTE pOutputData, DWORD cbOutputData, PDWORD pcbOutputNeeded,
    PDWORD pdwStatus) 
{
    pfnXcvData fpXcvData = NULL;
    BOOL flag = FALSE;
    /* Load the spooler DLL.  This shouldn't be too slow, since
     * the DLL should already be loaded */
    HINSTANCE hInstance = LoadLibrary(TEXT("winspool.drv"));
#ifdef DEBUG_REDMON
    syslog(TEXT("loader for XcvData "));
    syslog(pszDataName);
    syslog(TEXT("\r\n"));
#endif
    if (hInstance != NULL) {
	fpXcvData = (pfnXcvData)GetProcAddress(hInstance, "XcvDataW"); 
	if (fpXcvData != NULL) {
	    flag = fpXcvData(hXcv, pszDataName, pInputData, cbInputData,
		pOutputData, cbOutputData, pcbOutputNeeded, pdwStatus);
#ifdef DEBUG_REDMON
	    {TCHAR buf[MAXSTR];
	    DWORD dwError = GetLastError();
	    wsprintf(buf, 
		TEXT("XcvData returns %d, dwStatus=%d, GetLastError=%d\r\n"),
		flag, *pdwStatus, dwError);
	    syslog(buf);
	    if (flag == 0)
		syserror(dwError);
	    wsprintf(buf, TEXT("hXcv=0x%x\r\n"), hXcv);
	    syslog(buf);
	    }
#endif
	}
#ifdef DEBUG_REDMON
	else {
	    syslog(TEXT("failed to load XcvData\r\n"));
	}
#endif
	FreeLibrary(hInstance);
    }
    return flag;
} 
#endif	/* MSVC++ 5.0 or earlier */
#endif	/* !MAKEEXE */

#endif /* NT50 */


BOOL WINAPI rsOpenPort(HANDLE hMonitor, LPTSTR pName, PHANDLE pHandle)
{
    BOOL flag;
#ifdef BETA
    if (beta())
	return FALSE;
#endif

#ifdef DEBUG_REDMON
    syslog(TEXT("rsOpenPort"));
    syslog(TEXT("  hMonitor="));
    syshex((DWORD)hMonitor);
    syslog(TEXT("\r\n"));
    syslog(TEXT("  pName="));
    syslog(pName);
    syslog(TEXT("\r\n"));
    syslog(TEXT("\r\n"));
#endif

    flag = redmon_open_port(hMonitor, pName, pHandle); 

#ifdef DEBUG_REDMON
    syslog(TEXT("rsOpenPort returns "));
    sysnum(flag);
    syslog(TEXT("\r\n"));
#endif
    return flag;
} 


/* 95 / NT4 */
BOOL WINAPI rOpenPort(LPTSTR pName, PHANDLE pHandle)
{
    return rsOpenPort(NULL, pName, pHandle);
}

#ifdef UNUSED
/* Don't use OpenPortEx in a Port Monitor */
BOOL WINAPI rOpenPortEx(LPTSTR  pPortName, 
        LPTSTR  pPrinterName, PHANDLE pHandle, struct _MONITOR 
        FAR *pMonitor);
#endif



/* 95 / NT 40 / NT 50 */
BOOL WINAPI rStartDocPort(HANDLE hPort, LPTSTR pPrinterName, 
        DWORD JobId, DWORD Level, LPBYTE pDocInfo)
{
    REDATA *prd = GlobalLock((HGLOBAL)hPort);
    BOOL flag;

#ifdef DEBUG_REDMON
    syslog(TEXT("rStartDocPort\r\n"));
    syslog(TEXT("  hPort="));
    syshex((DWORD)hPort);
    syslog(TEXT("\r\n"));
#endif

    if (prd == (REDATA *)NULL) {
	SetLastError(ERROR_INVALID_HANDLE);
	return FALSE;
    }

    reset_redata(prd);

    flag = redmon_start_doc_port(prd, pPrinterName, JobId, Level, pDocInfo);

    GlobalUnlock((HGLOBAL)hPort);

#ifdef DEBUG_REDMON
    syslog(TEXT("rStartDocPort returns "));
    sysnum(flag);
    syslog(TEXT("\r\n"));
#endif
    return flag;
}


/* WritePort is normally called between StartDocPort and EndDocPort,
 * but can be called outside this pair for bidirectional printers.
 */
BOOL WINAPI rWritePort(HANDLE  hPort, LPBYTE  pBuffer, 
        DWORD   cbBuf, LPDWORD pcbWritten)
{
    REDATA *prd = GlobalLock((HGLOBAL)hPort);
    BOOL flag;
#ifdef DEBUG_REDMON
    syslog(TEXT("rWritePort "));
    sysnum(cbBuf);
    syslog(TEXT("\r\n"));
#endif

    if (prd == (REDATA *)NULL) {
	SetLastError(ERROR_INVALID_HANDLE);
	return FALSE;
    }

    *pcbWritten = 0;
    flag = redmon_write_port(prd, pBuffer, cbBuf, pcbWritten);

    GlobalUnlock((HGLOBAL)hPort);

#ifdef DEBUG_REDMON
    syslog(TEXT("rWritePort returns TRUE\r\n"));
#endif
    return TRUE;	/* returning FALSE crashes Win95 spooler */
}

/* ReadPort can be called within a Start/EndDocPort pair,
 * and also outside this pair.
 */
BOOL WINAPI rReadPort(HANDLE hPort, LPBYTE pBuffer, 
        DWORD  cbBuffer, LPDWORD pcbRead)
{
    BOOL flag;
    REDATA *prd = GlobalLock((HGLOBAL)hPort);
#ifdef DEBUG_REDMON
    syslog(TEXT("rReadPort\r\n"));
#endif

    /* we don't support reading */
    *pcbRead = 0;

    if (prd == (REDATA *)NULL) {
	SetLastError(ERROR_INVALID_HANDLE);
#ifdef DEBUG_REDMON
        syslog(TEXT("rReadPort returns FALSE.  Not our port handle.\r\n"));
#endif
	return FALSE;
    }

    flag = redmon_read_port(prd, pBuffer, cbBuffer, pcbRead);

    GlobalUnlock((HGLOBAL)hPort);

#ifdef DEBUG_REDMON
    syslog(TEXT("rReadPort returns "));
    sysnum(flag);
    syslog(TEXT("\r\n"));
#endif
    return flag;
}

BOOL WINAPI rEndDocPort(HANDLE hPort)
{
    REDATA *prd = GlobalLock((HGLOBAL)hPort);
    BOOL flag;
#ifdef DEBUG_REDMON
    syslog(TEXT("rEndDocPort\r\n"));
#endif

    if (prd == (REDATA *)NULL) {
	SetLastError(ERROR_INVALID_HANDLE);
	return FALSE;
    }

    flag = redmon_end_doc_port(prd);

    reset_redata(prd);

    GlobalUnlock((HGLOBAL)hPort);

#ifdef DEBUG_REDMON
    syslog(TEXT("rEndDocPort returns TRUE\r\n"));
#endif
    return TRUE;
}


BOOL WINAPI 
rClosePort(HANDLE hPort)
{
    BOOL flag;
#ifdef DEBUG_REDMON
    syslog(TEXT("rClosePort\r\n"));
#endif
    flag = redmon_close_port(hPort);
#ifdef DEBUG_REDMON
    syslog(TEXT("rClosePort returns "));
    sysnum(flag);
    syslog(TEXT("\r\n"));
#endif
    return flag;
}


BOOL WINAPI 
rSetPortTimeOuts(HANDLE  hPort, LPCOMMTIMEOUTS lpCTO, DWORD reserved)
{
    REDATA *prd = GlobalLock((HGLOBAL)hPort);
    BOOL flag;
#ifdef DEBUG_REDMON
    syslog(TEXT("rSetPortTimeouts\r\n"));
#endif
    if (prd == (REDATA *)NULL) {
	SetLastError(ERROR_INVALID_HANDLE);
	return FALSE;
    }

    flag = redmon_set_port_timeouts(prd, lpCTO, reserved);

    GlobalUnlock((HGLOBAL)hPort);

#ifdef DEBUG_REDMON
    syslog(TEXT("rSetPortTimeouts returns "));
    sysnum(flag);
    syslog(TEXT("\r\n"));
#endif
    return flag;
}
 
#ifdef UNICODE
void
GetNTversion(void)
{
   OSVERSIONINFO osvi;
   osvi.dwOSVersionInfoSize = sizeof(osvi);
   if (GetVersionEx(&osvi)) {
	ntver = osvi.dwMajorVersion * 100 + osvi.dwMinorVersion;
   }
   else {
	ntver = 0;
   }
}
#endif /* UNICODE */

/* Exported functions */


#ifdef UNICODE
#ifdef NT35
/* Windows NT 3.51 */
PORTMONEXPORT BOOL WINAPI _export
InitializeMonitor(LPWSTR pRegisterRoot)
{
#ifdef UNUSED
    TCHAR buf[MAXSTR];
    wsprintf(buf, TEXT("InitializeMonitor: registrykey=%s"), 
		pRegisterRoot);
    MessageBox(NULL, buf, MONITORNAME, MB_OK);
#endif
#ifdef DEBUG_REDMON
    syslog(TEXT("InitializeMonitor "));
    syslog(pRegisterRoot);
    syslog(TEXT("\r\n"));
#endif

    if (lstrlen(pRegisterRoot) + 1 > sizeof(rekey) / sizeof(TCHAR)) 
	return FALSE;

    lstrcpy(rekey, pRegisterRoot);
    GetNTversion();

    return TRUE;
}
#endif

#ifdef NT40
/* Windows NT4 version */
MONITOREX mex = {
    {sizeof(MONITOR)},
    {
    rEnumPorts,
    rOpenPort,
    NULL, /* OpenPortEx */
    rStartDocPort,
    rWritePort,
    rReadPort,
    rEndDocPort,
    rClosePort,
    rAddPort,
    rAddPortEx,
    rConfigurePort,
    rDeletePort,
    NULL, /* GetPrinterDataFromPort */
    rSetPortTimeOuts,
    }
};

PORTMONEXPORT LPMONITOREX WINAPI _export
InitializePrintMonitor(LPWSTR pRegisterRoot)
{
#ifdef DEBUG_REDMON
    syslog(TEXT("InitializePrintMonitor "));
    syslog(pRegisterRoot);
    syslog(TEXT("\r\n"));
#endif
#ifdef UNUSED
    wsprintf(buf, TEXT("InitializePrintMonitor: registrykey=%s"), 
		pRegisterRoot);
    MessageBox(NULL, buf, MONITORNAME, MB_OK);
#endif

    if (lstrlen(pRegisterRoot) + 1 > sizeof(rekey) / sizeof(TCHAR)) 
	return FALSE;

    lstrcpy(rekey, pRegisterRoot);
    GetNTversion();

    return &mex;
}
#endif	/* NT40 */

#ifdef NT50
/* Windows 2000 version */

MONITOR2 mon2 = {
    sizeof(MONITOR2),
    rsEnumPorts,
    rsOpenPort,
    NULL, /* OpenPortEx */
    rStartDocPort,
    rWritePort,
    rReadPort,
    rEndDocPort,
    rClosePort,
    NULL, /* AddPort */
    NULL, /* AddPortEx */
    NULL, /* ConfigurePort */
    NULL, /* DeletePort */
    NULL, /* GetPrinterDataFromPort */
    rSetPortTimeOuts,
    rsXcvOpenPort,
    rsXcvDataPort,
    rsXcvClosePort,
    rsShutdown
#ifdef NOTUSED
    ,NULL /* SendRecvBidiDataFromPort */
#endif
};

PORTMONEXPORT LPMONITOR2 WINAPI _export
InitializePrintMonitor2(PMONITORINIT pMonitorInit, PHANDLE phMonitor)
{
#ifdef UNUSED
    MessageBox(NULL, TEXT("InitializePrintMonitor2"), MONITORNAME, MB_OK);
#endif
    GetNTversion();
#ifdef DEBUG_REDMON
    syslog(TEXT("InitializePrintMonitor2"));
    syslog(TEXT("\r\n  cbSize="));
    sysnum(pMonitorInit->cbSize);
    syslog(TEXT("\r\n  hSpooler="));
    syshex((DWORD)(pMonitorInit->hSpooler));
    syslog(TEXT("\r\n  hckRegistryRoot="));
    syshex((DWORD)(pMonitorInit->hckRegistryRoot));
    syslog(TEXT("\r\n  pMonitorReg="));
    syshex((DWORD)(pMonitorInit->pMonitorReg));
    syslog(TEXT("\r\n  bLocal="));
    sysnum(pMonitorInit->bLocal);
    syslog(TEXT("\r\n  ntver="));
    sysnum(ntver);
    syslog(TEXT("\r\n"));
#endif
    GetNTversion();
    /* return pMonitorInit as hMonitor */
    *phMonitor = (PHANDLE)pMonitorInit;
    return &mon2;
} 

// The following are wrappers that start an elevated redconf.exe.

BOOL WINAPI rcAddPortUIOuter(PCWSTR pszServer, HWND hWnd, 
	PCWSTR pszPortNameIn, PWSTR *ppszPortNameOut)
{
    SHELLEXECUTEINFOA tempInfo;
    tempInfo.cbSize = sizeof(SHELLEXECUTEINFOA);
    tempInfo.fMask = 0;
    tempInfo.hwnd = NULL;
    tempInfo.lpVerb = "runas";
    tempInfo.lpFile = "redconf.exe";
    tempInfo.lpParameters = "1Redirected Port";
    tempInfo.lpDirectory = "C:\\";
    tempInfo.nShow = SW_NORMAL;
    ShellExecuteExA(&tempInfo);
}

BOOL WINAPI ShowExternalUIWithPortName(PCWSTR pszServer, HWND hWnd, 
                                        PCWSTR pszPortNameIn, char cmd)
{
    SHELLEXECUTEINFOA tempInfo;
    int len = wcslen(pszPortNameIn), i;
    char *args = (char *)malloc(sizeof(char) * (wcslen(pszPortNameIn) + 3));
    args[0] = cmd;
    for (i = 0; i < len; i++) {
        args[i + 1] = (char)pszPortNameIn[i];
    }
    args[len] = ':';
    args[len + 1] = '\0';
    tempInfo.cbSize = sizeof(SHELLEXECUTEINFOA);
    tempInfo.fMask = 0;
    tempInfo.hwnd = NULL;
    tempInfo.lpVerb = "runas";
    tempInfo.lpFile = "redconf.exe";
    tempInfo.lpParameters = args;
    tempInfo.lpDirectory = "C:\\";
    tempInfo.nShow = SW_NORMAL;
    ShellExecuteExA(&tempInfo);
}

BOOL WINAPI rcConfigurePortUIOuter(PCWSTR pszServer, HWND hWnd,
                                   PCWSTR pszPortNameIn)
{
    ShowExternalUIWithPortName(pszServer, hWnd, pszPortNameIn, '2');
}

BOOL WINAPI rcDeletePortUIOuter(PCWSTR pszServer, HWND hWnd,
                                PCWSTR pszPortNameIn)
{
    ShowExternalUIWithPortName(pszServer, hWnd, pszPortNameIn, '3');
}

MONITORUI mui = {
    sizeof(MONITORUI),
    rcAddPortUIOuter,
    rcConfigurePortUIOuter,
    rcDeletePortUIOuter
};

PORTMONEXPORT PMONITORUI WINAPI _export
InitializePrintMonitorUI(VOID)
{
#ifdef DEBUG_REDMON
    syslog(TEXT("InitializePrintMonitorUI\r\n"));
#endif
    GetNTversion();
    return &mui;
} 
#endif	/* NT50 */

#else	/* !UNICODE */
/* Windows 95 version */
PORTMONEXPORT BOOL WINAPI _export
InitializeMonitorEx(LPTSTR pRegisterRoot, LPMONITOR pMonitor)
{
#ifdef DEBUG_REDMON
    syslog(TEXT("InitializeMonitorEx "));
    syslog(pRegisterRoot);
    syslog(TEXT("\r\n"));
#endif
#ifdef UNUSED
    wsprintf(buf, TEXT("InitializeMonitorEx: registrykey=%s pmonitor=0x%x"), 
		pRegisterRoot, pMonitor);
    MessageBox(NULL, buf, MONITORNAME, MB_OK);
#endif

    if (lstrlen(pRegisterRoot) + 1 > sizeof(rekey) / sizeof(TCHAR)) 
	return FALSE;

    lstrcpy(rekey, pRegisterRoot);

    pMonitor->pfnEnumPorts = rEnumPorts;
    pMonitor->pfnOpenPort = rOpenPort;
    pMonitor->pfnOpenPortEx = NULL;
    pMonitor->pfnStartDocPort = rStartDocPort;
    pMonitor->pfnWritePort = rWritePort;
    pMonitor->pfnReadPort = rReadPort;
    pMonitor->pfnEndDocPort = rEndDocPort;
    pMonitor->pfnClosePort = rClosePort;
    pMonitor->pfnAddPort = rAddPort;
    pMonitor->pfnConfigurePort = rConfigurePort;
    pMonitor->pfnDeletePort = rDeletePort;
    pMonitor->pfnGetPrinterDataFromPort = NULL;
    pMonitor->pfnSetPortTimeOuts = rSetPortTimeOuts;

    return TRUE;

}
#endif	/* !UNICODE */


/* DLL entry point for Borland C++ */
#ifdef __BORLANDC__
#pragma argsused
#endif
BOOL WINAPI _export
DllEntryPoint(HINSTANCE hInst, DWORD fdwReason, LPVOID lpReserved)
{
    hdll = hInst;
    DisableThreadLibraryCalls(hInst);
    return TRUE;
}

#ifdef __BORLANDC__
#pragma argsused
#endif
/* DLL entry point for Microsoft Visual C++ */
PORTMONEXPORT BOOL WINAPI
DllMain(HINSTANCE hInst, DWORD fdwReason, LPVOID lpReserved)
{
    return DllEntryPoint(hInst, fdwReason, lpReserved);
}



#ifdef MAKEEXE

#ifdef NT50
/* for testing Windows 2000 code on Windows NT 4, we create a 
 * MONITORREG structure */
LONG WINAPI CreateKey(HANDLE hcKey, LPCTSTR pszSubKey, DWORD dwOptions, 
	REGSAM samDesired, PSECURITY_ATTRIBUTES pSecurityAttributes, 
	PHANDLE phckResult, PDWORD pdwDisposition, HANDLE hSpooler)
{
    return RegCreateKeyEx(hcKey, pszSubKey, 0, 0, dwOptions,
		samDesired, pSecurityAttributes, (PHKEY)phckResult,
		pdwDisposition);
} 

LONG WINAPI OpenKey(HANDLE hcKey, LPCTSTR pszSubKey, REGSAM samDesired, 
	PHANDLE phkResult, HANDLE hSpooler)
{
    return RegOpenKeyEx(hcKey, pszSubKey, 0, samDesired, 
		(PHKEY)phkResult);
} 

LONG WINAPI CloseKey(HANDLE hcKey, HANDLE hSpooler)
{
    return RegCloseKey(hcKey);
} 

LONG WINAPI DeleteKey(HANDLE hcKey, LPCTSTR pszSubKey, HANDLE hSpooler) 
{
    return RegDeleteKey(hcKey, pszSubKey); 
}

LONG WINAPI EnumKey(HANDLE hcKey, DWORD dwIndex, LPTSTR pszName, PDWORD pcchName, 
    PFILETIME pftLastWriteTime, HANDLE hSpooler)
{
    return RegEnumKeyEx(hcKey, dwIndex, pszName, pcchName, 0, NULL, NULL,
		pftLastWriteTime);
}
 
LONG WINAPI QueryInfoKey(HANDLE hcKey, PDWORD pcSubKeys, PDWORD pcbKey, 
    PDWORD pcValues, PDWORD pcbValue, PDWORD pcbData, 
    PDWORD pcbSecurityDescriptor, PFILETIME pftLastWriteTime, HANDLE hSpooler)
{
    return RegQueryInfoKey(hcKey, NULL, NULL, 0,
	pcSubKeys, pcbKey, NULL,
	pcValues, pcbValue, pcbData, 
	pcbSecurityDescriptor, pftLastWriteTime);
}

LONG WINAPI SetValue(HANDLE hcKey, LPCTSTR pszValue, DWORD dwType, const BYTE* pData, 
    DWORD cbData, HANDLE hSpooler)
{
    return RegSetValueEx(hcKey, pszValue, 0, dwType,
		pData, cbData);
}

LONG WINAPI DeleteValue(HANDLE hcKey, LPCTSTR pszValue, HANDLE hSpooler)
{
    return RegDeleteValue(hcKey, pszValue); 
}

LONG WINAPI EnumValue(HANDLE hcKey, DWORD dwIndex, LPTSTR pszValue, PDWORD pcbValue, 
    PDWORD pType, PBYTE pData, PDWORD pcbData, HANDLE hSpooler)
{
    return RegEnumValue(hcKey, dwIndex, pszValue, pcbValue,
		NULL, pType, pData, pcbData);
}

LONG WINAPI QueryValue(HANDLE hcKey, LPCTSTR pszValue, PDWORD pType, PBYTE pData, 
    PDWORD pcbData, HANDLE hSpooler)
{
    return RegQueryValueEx(hcKey, pszValue, 0, pType, 
	    pData, pcbData);
}

MONITORREG mr = {
    sizeof(MONITORREG),
    CreateKey,
    OpenKey,
    CloseKey,
    DeleteKey,
    EnumKey,
    QueryInfoKey,
    SetValue,
    DeleteValue,
    EnumValue,
    QueryValue
};

MONITORINIT mi = {
    sizeof(MONITORINIT),
    NULL, 	/* hSpooler */
    NULL, 	/* hckRegistryRoot - needs updating */
    &mr,	/* pMonitorReg */
    TRUE,	/* bLocal */
};
#endif	/* NT50 */



BYTE pPorts[4096];
DWORD cbBuf, cbNeeded, cReturned;
TCHAR mess[MAXSTR];
#define QUICKBROWN "The quick brown fox jumps over the lazy dog."
/*
#define PSTEST "(c:/rjl/monitor/test.txt) (w) file\n dup (Hello, world\\n) writestring closefile\n(Hello, world\\n) print flush\n"
#define PSTEST "(c:/gstools/gs5.50/colorcir.ps) run flush\n"
#define PSTEST "(c:/data/src/a20.ps) run flush\n"
*/
#define PSTEST "(c:/data/src/a20.ps) run flush\n"

#ifdef NT50
/* test Windows 2000 monitor on NT4.0 */
int PASCAL 
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int cmdShow)
{
    LONG rc;
    BOOL flag;
    DWORD dwError;
    DWORD written;
    HANDLE hport;
    PORT_INFO_2 *pi2;
    PORT_INFO_1 *pi1;
    DOC_INFO_1 dci1;
    int i;
    LPMONITOR2 pmon;
    HANDLE hMonitor;
    HANDLE hXcv;
    TCHAR monitorname[256];
    TCHAR mess[256];
    TCHAR portname[MAXSTR];
    hdll = hInstance;
    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
/*
	TEXT("System\\CurrentControlSet\\control\\Print\\Monitors\\Redirected Port"),
*/
	TEXT("System\\CurrentControlSet\\Control\\Print\\Monitors\\Bardimm Plugin test"),
	0, KEY_ALL_ACCESS, (PHKEY)&(mi.hckRegistryRoot));
    pmon = InitializePrintMonitor2(&mi, &hMonitor);

    LoadString(hdll, IDS_MONITORNAME, monitorname, 
	sizeof(monitorname)/sizeof(TCHAR)-1);

    /* need test local version of XcvData which calls rsXcvDataPort directly */
    flag = rsXcvOpenPort(hMonitor, TEXT("RPT1:"), SERVER_ACCESS_ADMINISTER, &hXcv);
    /* interactive add port */
    dwError = cAddPortUI(hXcv, HWND_DESKTOP, TEXT("RPT1:"), portname);
    /* interactive configure port */
    dwError = cConfigurePortUI(hXcv, HWND_DESKTOP, TEXT("RPT1:"));
    flag = rsXcvClosePort(hXcv);

    cbBuf = 0;
    flag = rsEnumPorts(hMonitor, NULL, 2, pPorts, cbBuf, &cbNeeded, &cReturned);
    cbBuf = cbNeeded;
    flag = rsEnumPorts(hMonitor, NULL, 2, pPorts, cbBuf, &cbNeeded, &cReturned);
    pi2 = (PORT_INFO_2 *)pPorts;

    for (i=0; i<cReturned; i++) {
	wsprintf(mess, TEXT("\042%s\042 \042%s\042 \042%s\042 %d\n"), 
	   pi2[i].pPortName,
	   pi2[i].pMonitorName,
	   pi2[i].pDescription,
	   pi2[i].fPortType
	   );
        MessageBox(NULL, mess, TEXT("EXE for Redirect Monitor"), MB_OK);
    }

    flag = rsOpenPort(hMonitor, TEXT("RPT1:"), &hport);
    dci1.pDocName = TEXT("Test document");
    dci1.pOutputFile = NULL;
    dci1.pDatatype = NULL;
    flag = rStartDocPort(hport, TEXT("Apple LaserWriter II NT v47.0"), 1, 1, (LPBYTE)&dci1);
//    rWritePort(hport, (LPBYTE)&QUICKBROWN, strlen(QUICKBROWN), &written);
    flag = rWritePort(hport, (LPBYTE)&PSTEST, strlen(PSTEST), &written);
    flag = rWritePort(hport, (LPBYTE)&PSTEST, strlen(PSTEST), &written);
    flag = rEndDocPort(hport);
    flag = rClosePort(hport);

    flag = sDeletePort(hMonitor, TEXT("RPT2:"));
    flag = sDeletePort(hMonitor, TEXT("RPT3:"));
    flag = sDeletePort(hMonitor, TEXT("RPT4:"));

    RegCloseKey((HKEY)(mi.hckRegistryRoot));

    return 0;
}

#else
int PASCAL 
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int cmdShow)
{
    LONG rc;
    DWORD written;
    HANDLE hport;
    PORT_INFO_2 *pi2;
    PORT_INFO_1 *pi1;
    DOC_INFO_1 dci1;
    int i;
#ifdef UNICODE
#ifdef NT50
    LPMONITOR2 pmon;
    HMONITOR hMonitor;
#else
    LPMONITOREX pmon;
#endif
#else
    MONITOR mon;
#endif
    TCHAR monitorname[256];
    TCHAR mess[256];
    hdll = hInstance;
#ifdef UNICODE
#ifdef NT50
    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
	TEXT("System\\CurrentControlSet\\control\\Print\\Monitors\\Redirected Port"),
	0, samDesired, (PHKEY)&(mi.hckRegistryRoot));
    pmon = InitializePrintMonitor2(&mi, &hMonitor);
#else
    pmon = InitializePrintMonitor(TEXT("System\\CurrentControlSet\\control\\Print\\Monitors\\Redirected Port"));
#endif
#else
    InitializeMonitorEx(TEXT("System\\CurrentControlSet\\control\\Print\\Monitors\\Redirected Port"), &mon);
#endif

    LoadString(hdll, IDS_MONITORNAME, monitorname, 
	sizeof(monitorname)/sizeof(TCHAR)-1);

#ifdef UNUSED
    rAddPort(NULL, HWND_DESKTOP, monitorname);
    rAddPort(NULL, HWND_DESKTOP, monitorname);
#endif
    rAddPort(NULL, HWND_DESKTOP, monitorname);

#ifdef UNICODE
    pi2 = (PORT_INFO_2 *)malloc(sizeof(PORT_INFO_2));
    pi2->pPortName = TEXT("RPT7:");
    pi2->pMonitorName = MONITORNAME;
    pi2->pDescription = TEXT("Redirected Port 7");
    pi2->fPortType = PORT_TYPE_WRITE;
    pi2->Reserved = 0;
    rAddPortEx(NULL, 2, (PBYTE)pi2, TEXT("Redirected Port"));
/*
    rDeletePort(NULL, HWND_DESKTOP, TEXT("RPT7:"));
*/

    pi1 = (PORT_INFO_1 *)malloc(sizeof(PORT_INFO_1));
    pi1->pName = TEXT("RPT8:");
    rAddPortEx(NULL, 1, (PBYTE)pi1, TEXT("Redirected Port"));
    rDeletePort(NULL, HWND_DESKTOP, TEXT("RPT8:"));
    free(pi2);
#endif
    rConfigurePort(NULL, HWND_DESKTOP, TEXT("RPT1:"));

    cbBuf = 0;
    rEnumPorts(NULL, 2, pPorts, cbBuf, &cbNeeded, &cReturned);
    cbBuf = cbNeeded;
    rEnumPorts(NULL, 2, pPorts, cbBuf, &cbNeeded, &cReturned);
    pi2 = (PORT_INFO_2 *)pPorts;

    for (i=0; i<cReturned; i++) {
	wsprintf(mess, TEXT("\042%s\042 \042%s\042 \042%s\042 %d\n"), 
	   pi2[i].pPortName,
	   pi2[i].pMonitorName,
	   pi2[i].pDescription,
	   pi2[i].fPortType
	   );
        MessageBox(NULL, mess, TEXT("EXE for Redirect Monitor"), MB_OK);
    }
#ifdef UNUSED
#endif

    rOpenPort(TEXT("RPT1:"), &hport);
    dci1.pDocName = TEXT("Test document");
    dci1.pOutputFile = NULL;
    dci1.pDatatype = NULL;
#ifdef UNICODE
    rStartDocPort(hport, TEXT("Apple LaserWriter II NT v47.0"), 1, 1, (LPBYTE)&dci1);
#else
    rStartDocPort(hport, TEXT("Apple LaserWriter II NT"), 1, 1, (LPBYTE)&dci1);
#endif
//    rWritePort(hport, (LPBYTE)&QUICKBROWN, strlen(QUICKBROWN), &written);
    rWritePort(hport, (LPBYTE)&PSTEST, strlen(PSTEST), &written);
    rWritePort(hport, (LPBYTE)&PSTEST, strlen(PSTEST), &written);
    rEndDocPort(hport);
    rClosePort(hport);

    rDeletePort(NULL, HWND_DESKTOP, TEXT("RPT2:"));
    rDeletePort(NULL, HWND_DESKTOP, TEXT("RPT3:"));
    rDeletePort(NULL, HWND_DESKTOP, TEXT("RPT4:"));

    return 0;
}
#endif	/* ! NT50 */

#endif	/* MAKEEXE */

/* end of portmon.c */
