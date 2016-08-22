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

/* portmon.h */

#define MAXSTR 256

#ifndef __BORLANDC__
#define _export
#endif

typedef struct redata_s REDATA;
typedef struct reconfig_s RECONFIG;
extern HINSTANCE hdll;
extern const TCHAR copyright[];
extern const TCHAR version[];
extern int ntver;	/* 351, 400, 500 */

#define MAXSHORTSTR 64

/* registry named values */
#define DESCKEY TEXT("Description")

/* to write a log file for debugging RedMon, uncomment the following line */
/*
#define DEBUG_REDMON
*/


/***********************************************************************/
/* Functions defined in portmon.c */

LONG RedMonOpenKey(HANDLE hMonitor, LPCTSTR pszSubKey, REGSAM samDesired,
    PHANDLE phkResult);
LONG RedMonCloseKey(HANDLE hMonitor, HANDLE hcKey);
LONG RedMonEnumKey(HANDLE hMonitor, HANDLE hcKey, DWORD dwIndex, 
    LPTSTR pszName, PDWORD pcchName);
LONG RedMonCreateKey(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszSubKey, 
    DWORD dwOptions, REGSAM samDesired, 
    PSECURITY_ATTRIBUTES pSecurityAttributes, 
    PHANDLE phckResult, PDWORD pdwDisposition);
LONG RedMonDeleteKey(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszSubKey);
LONG RedMonSetValue(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszValue, 
    DWORD dwType, const BYTE* pData, DWORD cbData);
LONG RedMonQueryValue(HANDLE hMonitor, HANDLE hcKey, LPCTSTR pszValue, 
	PDWORD pType, PBYTE pData, PDWORD pcbData);

void show_help(HWND hwnd, int id);

/* for debugging */
void syslog(LPCTSTR buf);
void syserror(DWORD err);
void syshex(DWORD num);
void sysnum(DWORD num);

/***********************************************************************/
/* External functions needed by portmon.c */

/* read the configuration from the registry */
BOOL redmon_get_config(HANDLE hMonitor, LPCTSTR portname, RECONFIG *config);
/* write the configuration to the registry */
BOOL redmon_set_config(HANDLE hMonitor, RECONFIG *config);
DWORD redmon_sizeof_config(void);
BOOL redmon_validate_config(RECONFIG *config);
/* initialise the config structure and return a pointer to the port name */
LPTSTR redmon_init_config(RECONFIG *config);

/* Add Port and Configure Port dialogs */
/* WM_INITDIALOG lParam is a pointer to RECONFIG */
BOOL CALLBACK AddDlgProc(HWND hDlg, UINT message, 
        WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ConfigDlgProc(HWND hDlg, UINT message, 
	WPARAM wParam, LPARAM lParam);

BOOL redmon_suggest_portname(TCHAR *portname, int len, int nAttempt);
void reset_redata(REDATA *prd);

/* Implementation of port monitor functions */
BOOL redmon_open_port(HANDLE hMonitor, LPTSTR pName, PHANDLE pHandle);
BOOL redmon_close_port(HANDLE hPort);
BOOL redmon_start_doc_port(REDATA *prd, LPTSTR pPrinterName, 
        DWORD JobId, DWORD Level, LPBYTE pDocInfo);
BOOL redmon_write_port(REDATA *prd, LPBYTE  pBuffer, 
        DWORD   cbBuf, LPDWORD pcbWritten);
BOOL redmon_read_port(REDATA *prd, LPBYTE pBuffer, 
        DWORD  cbBuffer, LPDWORD pcbRead);
BOOL redmon_end_doc_port(REDATA *prd);
BOOL redmon_set_port_timeouts(REDATA *prd, LPCOMMTIMEOUTS lpCTO, 
	DWORD reserved);

/***********************************************************************/
/* Stuff to allow compilation without the latest compiler or DDK */

#ifndef PORT_TYPE_WRITE
/* PORT_INFO_2 isn't defined in BC++ 4.5 include files */

typedef struct _PORT_INFO_2A { 
    LPSTR     pPortName; 
    LPSTR     pMonitorName; 
    LPSTR     pDescription; 
    DWORD     fPortType; 
    DWORD     Reserved; 
} PORT_INFO_2A, *PPORT_INFO_2A, *LPPORT_INFO_2A; 
typedef struct _PORT_INFO_2W { 
    LPWSTR    pPortName; 
    LPWSTR    pMonitorName; 
    LPWSTR    pDescription; 
    DWORD     fPortType; 
    DWORD     Reserved; 
} PORT_INFO_2W, *PPORT_INFO_2W, *LPPORT_INFO_2W; 

#ifdef UNICODE 
typedef PORT_INFO_2W PORT_INFO_2; 
typedef PPORT_INFO_2W PPORT_INFO_2; 
typedef LPPORT_INFO_2W LPPORT_INFO_2; 
#else 
typedef PORT_INFO_2A PORT_INFO_2; 
typedef PPORT_INFO_2A PPORT_INFO_2; 
typedef LPPORT_INFO_2A LPPORT_INFO_2; 
#endif // UNICODE 
 
#define PORT_TYPE_WRITE         0x0001 
#define PORT_TYPE_READ          0x0002 
#define PORT_TYPE_REDIRECTED    0x0004 
#define PORT_TYPE_NET_ATTACHED  0x0008 
 
#endif

#ifndef DI_CHANNEL
/* DI_CHANNEL is used by DOC_INFO_2 */
typedef struct _DOC_INFO_2A {
    LPSTR     pDocName;
    LPSTR     pOutputFile;
    LPSTR     pDatatype;
    DWORD   dwMode;
    DWORD   JobId;
} DOC_INFO_2A, *PDOC_INFO_2A, *LPDOC_INFO_2A;
typedef struct _DOC_INFO_2W {
    LPWSTR    pDocName;
    LPWSTR    pOutputFile;
    LPWSTR    pDatatype;
    DWORD   dwMode;
    DWORD   JobId;
} DOC_INFO_2W, *PDOC_INFO_2W, *LPDOC_INFO_2W;
#ifdef UNICODE
typedef DOC_INFO_2W DOC_INFO_2;
typedef PDOC_INFO_2W PDOC_INFO_2;
typedef LPDOC_INFO_2W LPDOC_INFO_2;
#else
typedef DOC_INFO_2A DOC_INFO_2;
typedef PDOC_INFO_2A PDOC_INFO_2;
typedef LPDOC_INFO_2A LPDOC_INFO_2;
#endif // UNICODE

#define DI_CHANNEL              1    // start direct read/write channel,
#endif // !defined(DI_CHANNEL)

#ifndef DI_MEMORYMAP_WRITE
/* DI_MEMORYMAP_WRITE is used by DOC_INFO_3 */
typedef struct _DOC_INFO_3A {
    LPSTR     pDocName;
    LPSTR     pOutputFile;
    LPSTR     pDatatype;
    DWORD     dwFlags;
} DOC_INFO_3A, *PDOC_INFO_3A, *LPDOC_INFO_3A;
typedef struct _DOC_INFO_3W {
    LPWSTR    pDocName;
    LPWSTR    pOutputFile;
    LPWSTR    pDatatype;
    DWORD     dwFlags;
} DOC_INFO_3W, *PDOC_INFO_3W, *LPDOC_INFO_3W;
#ifdef UNICODE
typedef DOC_INFO_3W DOC_INFO_3;
typedef PDOC_INFO_3W PDOC_INFO_3;
typedef LPDOC_INFO_3W LPDOC_INFO_3;
#else
typedef DOC_INFO_3A DOC_INFO_3;
typedef PDOC_INFO_3A PDOC_INFO_3;
typedef LPDOC_INFO_3A LPDOC_INFO_3;
#endif // UNICODE

// Tell StartDocPrinter to not use AddJob and ScheduleJob for local printing
#define DI_MEMORYMAP_WRITE          0  // Unknown value - don't have header 
#endif // !defined(DI_CHANNEL)

/* From winsplp.h */
HANDLE RevertToPrinterSelf(VOID);
BOOL ImpersonatePrinterClient(HANDLE  hToken);

#ifdef UNICODE
/* The biggest difference between NT and 95 is that NT uses wide characters
 * (Unicode) for strings, while 95 uses ANSI characters (single byte).
 */
#ifdef NT35
/* Windows NT 3.51 exports separate procedures */
#define rAddPortEx	_export AddPortExW
#define rAddPort	_export AddPortW
#define rClosePort	_export ClosePort
#define rConfigurePort	_export ConfigurePortW
#define rDeletePort	_export DeletePortW
#define rEndDocPort	_export EndDocPort
#define rEnumPorts	_export EnumPortsW
#define rOpenPort	_export OpenPort
#define rReadPort	_export ReadPort
#define rStartDocPort	_export StartDocPort
#define rWritePort	_export WritePort
BOOL WINAPI _export InitializeMonitor(LPWSTR pRegistryRoot);
#endif /* NT40 */


#ifdef NT40
/* Windows NT4 structure */
/* The MONITOR structure has an extra member cf. Windows 95 */
typedef struct _MONITOR {
        BOOL (WINAPI *pfnEnumPorts)(LPWSTR pName, DWORD Level, LPBYTE pPorts,
                DWORD cbBuf, LPDWORD pcbNeeded, LPDWORD pcReturned);
        BOOL (WINAPI *pfnOpenPort)(LPWSTR pName, PHANDLE pHandle);
        BOOL (WINAPI *pfnOpenPortEx)(LPWSTR pPortName,LPWSTR pPrinterName,
                PHANDLE pHandle, struct _MONITOR FAR *pMonitor);
        BOOL (WINAPI *pfnStartDocPort)(HANDLE hPort,LPWSTR pPrinterName,
		DWORD JobId, DWORD Level,LPBYTE pDocInfo);
        BOOL (WINAPI *pfnWritePort)(HANDLE hPort,LPBYTE pBuffer,DWORD cbBuf,
                LPDWORD pcbWritten);
        BOOL (WINAPI *pfnReadPort)(HANDLE hPort,LPBYTE pBuffer,DWORD cbBuffer,
                LPDWORD pcbRead);
        BOOL (WINAPI *pfnEndDocPort)(HANDLE hPort);
        BOOL (WINAPI *pfnClosePort)(HANDLE hPort);
        BOOL (WINAPI *pfnAddPort)(LPWSTR pName, HWND hWnd, LPWSTR pMonitorName);
        BOOL (WINAPI *pfnAddPortEx)(LPWSTR pName, DWORD Level, LPBYTE lpBuffer,
                LPWSTR lpMonitorName);
        BOOL (WINAPI *pfnConfigurePort)(LPWSTR pName, HWND hWnd, 
		LPWSTR pPortName);
        BOOL (WINAPI *pfnDeletePort)(LPWSTR pName, HWND hWnd, LPWSTR pPortName);
        BOOL (WINAPI *pfnGetPrinterDataFromPort)(HANDLE hPort, DWORD ControlID,
                LPWSTR pValueName, LPWSTR lpInBuffer, DWORD cbInBuffer,
                LPWSTR lpOutBuffer, DWORD cbOutBuffer, LPDWORD lpcbReturned);
        BOOL (WINAPI *pfnSetPortTimeOuts)(HANDLE hPort,LPCOMMTIMEOUTS lpCTO,
                DWORD reserved /* must be set to 0 */);
} MONITOR, FAR *LPMONITOR;

typedef struct _MONITOREX {
        DWORD  dwMonitorSize;
        MONITOR  Monitor;
} MONITOREX, FAR *LPMONITOREX;

LPMONITOREX WINAPI _export InitializePrintMonitor(LPWSTR pRegistryRoot);
#endif /* NT40 */

#ifdef NT50
/* Windows 2000 (NT5) structures and functions */
typedef struct _BINARY_CONTAINER {
  DWORD  cbBuf;
  LPBYTE  pData;
} BINARY_CONTAINER, *PBINARY_CONTAINER;

typedef struct _BIDI_DATA {
  DWORD  dwBidiType;
  union {
     BOOL  bData;
     INT  iData;
     LPWSTR  sData;
     FLOAT  fData;
     BINARY_CONTAINER  biData;
  } u;
} BIDI_DATA, *PBIDI_DATA, *LPBIDI_DATA;

typedef struct _BIDI_REQUEST_DATA {
  DWORD  dwReqNumber;
  LPWSTR  pSchema;
  BIDI_DATA  data;
} BIDI_REQUEST_DATA, *PBIDI_REQUEST_DATA, *LPBIDI_REQUEST_DATA;
typedef struct _BIDI_REQUEST_CONTAINER {
    DWORD Version;
    DWORD Flags;
    DWORD Count;
    BIDI_REQUEST_DATA aDATa[1];
} BIDI_REQUEST_CONTAINER, *PBIDI_REQUEST_CONTAINER, *LPBIDI_REQUEST_CONTAINER;

typedef struct _BIDI_RESPONSE_DATA {
  DWORD  dwResult;
  DWORD  dwReqNumber;
  LPWSTR  pSchema;
  BIDI_DATA  data;
} BIDI_RESPONSE_DATA, *PBIDI_RESPONSE_DATA, *LPBIDI_RESPONSE_DATA;

typedef struct _BIDI_RESPONSE_CONTAINER {
  DWORD  Version;
  DWORD  Flags;
  DWORD  Count;
  BIDI_RESPONSE_DATA aData[1];
} BIDI_RESPONSE_CONTAINER, *PBIDI_RESPONSE_CONTAINER, *LPBIDI_RESPONSE_CONTAINER; 

/* MONITOR2 structure is returned by InitializePrintMonitor2 */
typedef struct _MONITOR2 
{ 
    DWORD cbSize; 
    BOOL (WINAPI *pfnEnumPorts) 
	(HANDLE hMonitor, LPWSTR pName, DWORD Level, LPBYTE pPorts, 
	DWORD cbBuf, LPDWORD pcbNeeded, LPDWORD pcReturned); 
    BOOL (WINAPI *pfnOpenPort) 
	(HANDLE hMonitor, LPWSTR pName, PHANDLE pHandle); 
    BOOL (WINAPI *pfnOpenPortEx) 
	(HANDLE hMonitor, HANDLE hMonitorPort, LPWSTR pPortName, 
	LPWSTR pPrinterName, PHANDLE pHandle, 
	struct _MONITOR2 FAR *pMonitor2); 
    BOOL (WINAPI *pfnStartDocPort) 
	(HANDLE hPort, LPWSTR pPrinterName, DWORD JobId, 
	DWORD Level, LPBYTE pDocInfo); 
    BOOL (WINAPI *pfnWritePort) 
	(HANDLE hPort, LPBYTE pBuffer, DWORD cbBuf, 
	LPDWORD pcbWritten); 
    BOOL (WINAPI *pfnReadPort) 
	(HANDLE hPort, LPBYTE pBuffer, DWORD cbBuffer, 
	LPDWORD pcbRead); 
    BOOL (WINAPI *pfnEndDocPort) 
	(HANDLE hPort); 
    BOOL (WINAPI *pfnClosePort) 
	(HANDLE hPort); 
    BOOL (WINAPI *pfnAddPort) 		/* Obsolete. Should be NULL */
	(HANDLE hMonitor, LPWSTR pName, HWND hWnd, 
	LPWSTR pMonitorName); 
    BOOL (WINAPI *pfnAddPortEx) 	/* Obsolete. Should be NULL */
	(HANDLE hMonitor, LPWSTR pName, DWORD Level, 
	LPBYTE lpBuffer, LPWSTR lpMonitorName); 
    BOOL (WINAPI *pfnConfigurePort) 	/* Obsolete. Should be NULL */
	(HANDLE hMonitor, LPWSTR pName, HWND hWnd, 
	LPWSTR pPortName); 
    BOOL (WINAPI *pfnDeletePort) 	/* Obsolete. Should be NULL */
	(HANDLE hMonitor, LPWSTR pName, HWND hWnd, 
	LPWSTR pPortName); 
    BOOL (WINAPI *pfnGetPrinterDataFromPort) 
	(HANDLE hPort, DWORD ControlID, LPWSTR pValueName, 
	LPWSTR lpInBuffer, DWORD cbInBuffer, LPWSTR lpOutBuffer, 
	DWORD cbOutBuffer, LPDWORD lpcbReturned); 
    BOOL (WINAPI *pfnSetPortTimeOuts) 
	(HANDLE hPort, LPCOMMTIMEOUTS lpCTO, 
	DWORD reserved /* must be set to 0 */ ); 
    BOOL (WINAPI *pfnXcvOpenPort) 
	(HANDLE hMonitor, LPCWSTR pszObject, 
	ACCESS_MASK GrantedAccess, PHANDLE phXcv); 
    DWORD (WINAPI *pfnXcvDataPort) 
	(HANDLE hXcv, LPCWSTR pszDataName, PBYTE pInputData, 
	DWORD cbInputData, PBYTE pOutputData, DWORD cbOutputData, 
	PDWORD pcbOutputNeeded); 
    BOOL (WINAPI *pfnXcvClosePort) 
	(HANDLE hXcv); 
    VOID (WINAPI *pfnShutdown) 
	(HANDLE hMonitor); 
#ifdef NOTUSED
    DWORD (WINAPI *pfnSendRecvBidiDataFromPort)
	(HANDLE hPort, DWORD dwAccessBit,
	LPCWSTR pAction, PBIDI_REQUEST_CONTAINER pReqData,
	PBIDI_RESPONSE_CONTAINER *ppResData);
#endif
} MONITOR2, *PMONITOR2, FAR *LPMONITOR2; 

/* MONITORREG is in MONITORINIT */
/* Use these function pointer instead of Win32 reg functions
 * because we can't tell if we should write to the local registry 
 * or the cluster registry */
/* Windows 2000 DDK documentation says these should not have WINAPI, 
 * but they crash unless you use WINAPI.
 */
typedef struct _MONITORREG { 
    DWORD cbSize; 
    LONG 
    (WINAPI *fpCreateKey)( 
	HANDLE hcKey, 
	LPCTSTR pszSubKey, 
	DWORD dwOptions, 
	REGSAM samDesired, 
	PSECURITY_ATTRIBUTES pSecurityAttributes, 
	PHANDLE phckResult, 
	PDWORD pdwDisposition, 
	HANDLE hSpooler 
	); 
    LONG 
    (WINAPI *fpOpenKey)( 
	HANDLE hcKey, 
	LPCTSTR pszSubKey, 
	REGSAM samDesired, 
	PHANDLE phkResult, 
	HANDLE hSpooler 
	); 
    LONG 
    (WINAPI *fpCloseKey)( 
	HANDLE hcKey, 
	HANDLE hSpooler 
	); 
    LONG 
    (WINAPI *fpDeleteKey)( 
	HANDLE hcKey, 
	LPCTSTR pszSubKey, 
	HANDLE hSpooler 
	); 
    LONG 
    (WINAPI *fpEnumKey)( 
	HANDLE hcKey, 
	DWORD dwIndex, 
	LPTSTR pszName, 
	PDWORD pcchName, 
	PFILETIME pftLastWriteTime, 
	HANDLE hSpooler 
	); 
    LONG 
    (WINAPI *fpQueryInfoKey)( 
	HANDLE hcKey, 
	PDWORD pcSubKeys, 
	PDWORD pcbKey, 
	PDWORD pcValues, 
	PDWORD pcbValue, 
	PDWORD pcbData, 
	PDWORD pcbSecurityDescriptor, 
	PFILETIME pftLastWriteTime, 
	HANDLE hSpooler 
	); 
    LONG 
    (WINAPI *fpSetValue)( 
	HANDLE hcKey, 
	LPCTSTR pszValue, 
	DWORD dwType, 
	const BYTE* pData, 
	DWORD cbData, 
	HANDLE hSpooler 
	); 
    LONG 
    (WINAPI *fpDeleteValue)( 
	HANDLE hcKey, 
	LPCTSTR pszValue, 
	HANDLE hSpooler 
	); 
    LONG 
    (WINAPI *fpEnumValue)( 
	HANDLE hcKey, 
	DWORD dwIndex, 
	LPTSTR pszValue, 
	PDWORD pcbValue, 
	PDWORD pType, 
	PBYTE pData, 
	PDWORD pcbData, 
	HANDLE hSpooler 
	); 
    LONG 
    (WINAPI *fpQueryValue)( 
	HANDLE hcKey, 
	LPCTSTR pszValue, 
	PDWORD pType, 
	PBYTE pData, 
	PDWORD pcbData, 
	HANDLE hSpooler 
	); 
} MONITORREG, *PMONITORREG; 

/* MONITORINIT is an input parameter to InitializePrintMonitor2 */
typedef struct _MONITORINIT { 
    DWORD cbSize; 
    HANDLE hSpooler; 
    HANDLE hckRegistryRoot; 
    PMONITORREG pMonitorReg; 
    BOOL bLocal; 
} MONITORINIT, *PMONITORINIT; 


/* returned by InitializePrintMonitorUI */
typedef struct _MONITORUI 
{ 
    DWORD dwMonitorUISize; 
    BOOL (WINAPI *pfnAddPortUI) 
	(PCWSTR pszServer, 
	HWND hWnd, 
	PCWSTR pszPortNameIn, 
	PWSTR *ppszPortNameOut); 
    BOOL (WINAPI *pfnConfigurePortUI) 
	(PCWSTR pName, 
	HWND hWnd, 
	PCWSTR pPortName); 
    BOOL (WINAPI *pfnDeletePortUI) 
	(PCWSTR pszServer, 
	HWND hWnd, 
	PCWSTR pszPortName); 
} MONITORUI, FAR *PMONITORUI; 

LPMONITOR2 WINAPI 
    InitializePrintMonitor2( 
	PMONITORINIT pMonitorInit, 
	PHANDLE phMonitor 
	); 

PMONITORUI WINAPI 
    InitializePrintMonitorUI(VOID); 
#endif /* NT50 */

#else   /* !UNICODE */
/* Windows 95 structure */
/* The MONITOR structure is returned by InitializeMonitorEx() */
typedef struct _MONITOR {
     BOOL (WINAPI *pfnEnumPorts)(LPTSTR pName,DWORD Level,LPBYTE  pPorts, 
         DWORD cbBuf, LPDWORD pcbNeeded, LPDWORD pcReturned);
    BOOL (WINAPI *pfnOpenPort)(LPTSTR  pName,PHANDLE pHandle);
    BOOL (WINAPI *pfnOpenPortEx)(LPTSTR  pPortName, 
        LPTSTR  pPrinterName, PHANDLE pHandle, struct _MONITOR 
        FAR *pMonitor);
    BOOL (WINAPI *pfnStartDocPort)(HANDLE  hPort, LPTSTR  pPrinterName, 
        DWORD   JobId, DWORD   Level, LPBYTE  pDocInfo);
    BOOL (WINAPI *pfnWritePort)(HANDLE  hPort, LPBYTE  pBuffer, 
        DWORD   cbBuf, LPDWORD pcbWritten);
    BOOL (WINAPI *pfnReadPort)(HANDLE hPort, LPBYTE pBuffer, 
        DWORD  cbBuffer, LPDWORD pcbRead);
    BOOL (WINAPI *pfnEndDocPort)(HANDLE   hPort);
    BOOL (WINAPI *pfnClosePort)(HANDLE  hPort);
    BOOL (WINAPI *pfnAddPort)(LPTSTR   pName, HWND    hWnd, 
        LPTSTR   pMonitorName);
    BOOL (WINAPI *pfnConfigurePort)(LPTSTR   pName, HWND  hWnd, 
        LPTSTR pPortName);
    BOOL (WINAPI *pfnDeletePort)(LPTSTR   pName, HWND    hWnd, 
        LPTSTR   pPortName);
    BOOL (WINAPI *pfnGetPrinterDataFromPort)(HANDLE hPort, 
        DWORD ControlID, LPTSTR  pValueName, LPTSTR  lpInBuffer, 
        DWORD   cbInBuffer, LPTSTR  lpOutBuffer, DWORD   cbOutBuffer, 
        LPDWORD lpcbReturned);
    BOOL (WINAPI *pfnSetPortTimeOuts)(HANDLE  hPort, 
        LPCOMMTIMEOUTS lpCTO, DWORD reserved); 
} MONITOR, FAR *LPMONITOR;

/* InitializeMonitorEx is exported by the Port or Language
 * Monitor DLL.
 */
BOOL WINAPI _export InitializeMonitorEx(
    LPTSTR     pRegisterRoot,
    LPMONITOR  pMonitor
);

#endif /* !UNICODE */

/* end of portmon.h */
