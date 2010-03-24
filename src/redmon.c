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

/* redmon.c */

/*
 * This is the redirection part of the RedMon port monitor.
 * 
 * This is a Port Monitor for 
 *   Windows 95, 98
 *   Windows NT 3.5
 *   Windows NT 4.0
 *   Windows NT 5.0 (Windows 2000 Professional) - needs more testing
 *
 * The monitor name is "Redirected Port" .
 * A write to any port provided by this monitor will be
 * redirected to a program using a pipe to stdin.
 *
 * An example is redirecting the output from the 
 * PostScript printer driver to Ghostscript, which then
 * writes to a non-PostScript printer.
 *
 * For efficiency reasons, don't use the C run time library. 
 *
 * The Windows NT version must use Unicode, so Windows NT
 * specific code is conditionally compiled with a combination
 * of UNICODE, NT35, NT40 and NT50.
 */

/* Publicly accessibly functions in this Port Monitor are prefixed with
 * the following:
 *   rXXX   Windows 95 / NT3.51 / NT4
 *   rsXXX  Windows NT5 port monitor server
 *   rcXXX  Windows NT5 port monitor client (UI)
 */

#define STRICT
#include <windows.h>
#include "portmon.h"
#include "redmon.h"
#ifdef BETA
#include <time.h>
#endif

/* to write a log file for debugging RedMon, uncomment the following line */
/*
#define DEBUG_REDMON
*/

/* Constant globals */
const TCHAR copyright[] = COPYRIGHT;
const TCHAR version[] = VERSION;

/* port configuration */
#define COMMANDKEY TEXT("Command")
#define ARGKEY TEXT("Arguments")
#define PRINTERKEY TEXT("Printer")
#define OUTPUTKEY TEXT("Output")
#define SHOWKEY TEXT("ShowWindow")
#define RUNUSERKEY TEXT("RunUser")
#define DELAYKEY TEXT("Delay")
#define LOGUSEKEY TEXT("LogFileUse")
#define LOGNAMEKEY TEXT("LogFileName")
#define LOGDEBUGKEY TEXT("LogFileDebug")
#define PRINTERRORKEY TEXT("PrintError")
#define LASTUSERKEY TEXT("LastUser")
#define LASTFILEKEY TEXT("LastFile")
#define REDMONUSERKEY TEXT("Software\\Ghostgum\\RedMon")
typedef struct reconfig_s {
    DWORD dwSize;	/* sizeof this structure */
    DWORD dwVersion;	/* version number of RedMon */
    TCHAR szPortName[MAXSTR];
    TCHAR szDescription[MAXSTR];
    TCHAR szCommand[MAXSTR];
    TCHAR szArguments[MAXSTR];
    TCHAR szPrinter[MAXSTR];
    DWORD dwOutput;
    DWORD dwShow;
    DWORD dwRunUser;
    DWORD dwDelay;
    DWORD dwLogFileUse;
    TCHAR szLogFileName[MAXSTR];
    DWORD dwLogFileDebug;
    DWORD dwPrintError;
};

struct redata_s {
    /* Members required by all RedMon implementations */
    HANDLE hPort;		/* handle to this structure */
    HANDLE hMonitor;		/* provided by NT5.0 OpenPort */
    RECONFIG config;		/* configuration stored in registry */
    TCHAR portname[MAXSHORTSTR];    /* Name obtained during OpenPort  */

    /* Details obtained during StartDocPort  */
    TCHAR command[1024];
    TCHAR pPrinterName[MAXSTR];	/* Printer name for RedMon port */
    TCHAR pDocName[MAXSTR];	/* Document Name (from StartDocPort) */
    DWORD JobId;
    TCHAR pUserName[MAXSTR];	/* User Name (from StartDocPort job info) */
    TCHAR pMachineName[MAXSTR];	/* Machine Name (from StartDocPort job info) */

    /* For running process */
    BOOL started;		/* true if process started */
    BOOL error;			/* true if process terminates early */
    HGLOBAL environment;	/* environment strings for process */
    HANDLE hChildStdinRd;
    HANDLE hChildStdinWr;	/* We write to this one */
    HANDLE hChildStdoutRd; 	/* We read from this one */
    HANDLE hChildStdoutWr;
    HANDLE hChildStderrRd; 	/* We read from this one */
    HANDLE hChildStderrWr;
#ifdef SAVESTD
    HANDLE hSaveStdin;
    HANDLE hSaveStdout;
    HANDLE hSaveStderr;
#endif
    HANDLE hPipeRd; 	/* We read printer output from this one */
    HANDLE hPipeWr;
    PROCESS_INFORMATION piProcInfo;
    /*  */
#ifdef OLD
    TCHAR logname[MAXSTR];
#endif
    HANDLE hLogFile;
    HANDLE hmutex;	/* To control access to pipe and file handles */
    HANDLE primary_token;  	/* primary token for caller */
    TCHAR pSessionId[MAXSTR];	/* session-id for WTS support */

    /* for write thread */
    HANDLE write_event;	/* To unblock write thread */
    BOOL write;		/* TRUE if write thread should keep running */
    HANDLE write_hthread;
    DWORD write_threadid;
    LPBYTE write_buffer;	/* data to write */
    DWORD write_buffer_length;	/* number of bytes of data to write */
    BOOL write_flag;		/* TRUE if WriteFile was successful */
    DWORD write_written;	/* number of bytes written */

    /* for output to second printer queue */
    TCHAR tempname[MAXSTR];	/* temporary file name  */
    HANDLE printer;		/* handle to a printer */ 
    DWORD printer_bytes;
    BYTE pipe_buf[PIPE_BUF_SIZE]; /* buffer for use in flush_stdout */
};

void write_error(REDATA *prd, DWORD err);
void write_string_to_log(REDATA *prd, LPCTSTR buf);
void redmon_cancel_job(REDATA *prd);
BOOL start_redirect(REDATA * prd);
void reset_redata(REDATA *prd);
BOOL check_process(REDATA *prd);
LRESULT APIENTRY GetSaveHookProc(HWND hDlg, UINT message, 
	WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK LogfileDlgProc(HWND hDlg, UINT message, 
	WPARAM wParam, LPARAM lParam);
int create_tempfile(LPTSTR filename, DWORD len);
int redmon_printfile(REDATA * prd, TCHAR *filename);
BOOL redmon_open_printer(REDATA *prd);
BOOL redmon_abort_printer(REDATA *prd);
BOOL redmon_close_printer(REDATA *prd);
BOOL redmon_write_printer(REDATA *prd, BYTE *ptr, DWORD len);
BOOL get_job_info(REDATA *prd);
BOOL make_env(REDATA * prd);
BOOL query_session_id(REDATA * prd);
BOOL get_filename_as_user(REDATA * prd);
BOOL redmon_print_error(REDATA *prd);

/* we don't rely on the import library having XcvData,
 * since we may be compiling with VC++ 5.0 */
BOOL WINAPI XcvData(HANDLE hXcv, LPCWSTR pszDataName, 
    PBYTE pInputData, DWORD cbInputData, 
    PBYTE pOutputData, DWORD cbOutputData, PDWORD pcbOutputNeeded,
    PDWORD pdwStatus);


#define PORTSNAME TEXT("Ports")
#define BACKSLASH TEXT("\\")
#define DEFAULT_DELAY 300   /* seconds */
#define MINIMUM_DELAY 15
#define PRINT_BUF_SIZE 16384

/* environment variables set for the program */
#define REDMON_PORT     TEXT("REDMON_PORT=")
#define REDMON_JOB      TEXT("REDMON_JOB=")
#define REDMON_PRINTER  TEXT("REDMON_PRINTER=")
#define REDMON_MACHINE  TEXT("REDMON_MACHINE=")
#define REDMON_USER     TEXT("REDMON_USER=")
#define REDMON_DOCNAME  TEXT("REDMON_DOCNAME=")
#define REDMON_FILENAME  TEXT("REDMON_FILENAME=")
#define REDMON_SESSIONID  TEXT("REDMON_SESSIONID=")
#define REDMON_TEMP     TEXT("TEMP=")
#define REDMON_TMP      TEXT("TMP=")



/* mutex used for controlling access to log file and pipe handles */
void
request_mutex(REDATA *prd)
{
    if ((prd->hmutex != NULL) && (prd->hmutex != INVALID_HANDLE_VALUE))
        WaitForSingleObject(prd->hmutex, 30000);
}

void
release_mutex(REDATA *prd)
{
    if ((prd->hmutex != NULL) && (prd->hmutex != INVALID_HANDLE_VALUE))
	ReleaseMutex(prd->hmutex);
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
    TCHAR title[MAXSTR];
    LoadString(hdll, IDS_BETAEXPIRED, buf, sizeof(buf)/sizeof(TCHAR)-1);
    LoadString(hdll, IDS_TITLE, title, sizeof(title)/sizeof(TCHAR)-1);
    MessageBox(HWND_DESKTOP, buf, title, MB_OK | MB_ICONHAND);
    return 1;
  }
  return 0;
}
#endif /* BETA */

/* The log file is single byte characters only */
/* Write a single character or wide character string to the log file,
 * converting it to single byte characters */
void write_string_to_log(REDATA *prd, LPCTSTR buf)
{
DWORD cbWritten;
#ifdef UNICODE
int count;
CHAR cbuf[256];
BOOL UsedDefaultChar;
#endif
    if (prd->hLogFile == INVALID_HANDLE_VALUE)
	return;

    request_mutex(prd);
#ifdef UNICODE
    while (lstrlen(buf)) {
	count = min(lstrlen(buf), sizeof(cbuf));
	WideCharToMultiByte(CP_ACP, 0, buf, count,
		cbuf, sizeof(cbuf), NULL, &UsedDefaultChar);
	buf += count;
	WriteFile(prd->hLogFile, cbuf, count, &cbWritten, NULL);
    }
#else
    WriteFile(prd->hLogFile, buf, lstrlen(buf), &cbWritten, NULL);
#endif
    FlushFileBuffers(prd->hLogFile);
    release_mutex(prd);
}

void
write_error(REDATA *prd, DWORD err)
{
LPVOID lpMessageBuffer;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_FROM_SYSTEM,
	NULL, err,
	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* user default language */
	(LPTSTR) &lpMessageBuffer, 0, NULL);
    if (lpMessageBuffer) {
	write_string_to_log(prd, (LPTSTR)lpMessageBuffer);
	LocalFree(LocalHandle(lpMessageBuffer));
	write_string_to_log(prd, TEXT("\r\n"));
    }
}

DWORD
redmon_sizeof_config(void)
{
    return sizeof(RECONFIG);
}

BOOL redmon_validate_config(RECONFIG *config)
{
    if (config == NULL)
	return FALSE;
    if (config->dwSize != sizeof(RECONFIG))
	return FALSE;
    if (config->dwVersion != VERSION_NUMBER)
	return FALSE;
    return TRUE;
}

LPTSTR redmon_init_config(RECONFIG *config)
{
    memset(config, 0, sizeof(RECONFIG));
    config->dwSize = sizeof(RECONFIG);
    config->dwVersion = VERSION_NUMBER;
    config->dwOutput = OUTPUT_SELF;
    config->dwShow = FALSE;
    config->dwRunUser = FALSE;
    config->dwDelay = DEFAULT_DELAY;
#ifdef BETA
    config->dwLogFileDebug = TRUE;  /* beta versions default to debug enabled */
#else
    config->dwLogFileDebug = FALSE;
#endif
    LoadString(hdll, IDS_MONITORNAME, config->szDescription, 
	sizeof(config->szDescription)/sizeof(TCHAR)-1);
    return config->szPortName;
}

/* read the configuration from the registry */
BOOL redmon_get_config(HANDLE hMonitor, LPCTSTR portname, RECONFIG *config)
{
    LONG rc = ERROR_SUCCESS;
    HANDLE hkey;
    TCHAR buf[MAXSTR];
    DWORD cbData;
    DWORD dwType;
#ifdef DEBUG_REDMON
    syslog(TEXT("redmon_get_config "));
    syslog(portname);
    syslog(TEXT("\r\n"));
    syslog(TEXT("  hMonitor="));
    syshex((DWORD)hMonitor);
    syslog(TEXT("\r\n"));
#endif
    redmon_init_config(config);
    lstrcpyn(config->szPortName, portname, sizeof(config->szPortName)-1);

    lstrcpy(buf, PORTSNAME);
    lstrcat(buf, BACKSLASH);
    lstrcat(buf, portname);
    rc = RedMonOpenKey(hMonitor, buf, KEY_READ, &hkey);
    if (rc != ERROR_SUCCESS) {
#ifdef DEBUG_REDMON
	syslog(TEXT("Failed to open registry key "));
	syslog(buf);
	syslog(TEXT("\r\n"));
#endif
	return FALSE;
    }

    cbData = sizeof(config->szDescription)-sizeof(TCHAR);
    rc = RedMonQueryValue(hMonitor, hkey, DESCKEY, &dwType, 
	(PBYTE)(config->szDescription), &cbData);
    cbData = sizeof(config->szCommand)-sizeof(TCHAR);
    rc = RedMonQueryValue(hMonitor, hkey, COMMANDKEY, &dwType, 
	(PBYTE)(config->szCommand), &cbData);
    cbData = sizeof(config->szArguments)-sizeof(TCHAR);
    rc = RedMonQueryValue(hMonitor, hkey, ARGKEY, &dwType, 
	(PBYTE)(config->szArguments), &cbData);
    cbData = sizeof(config->szPrinter)-sizeof(TCHAR);
    rc = RedMonQueryValue(hMonitor, hkey, PRINTERKEY, &dwType, 
	(PBYTE)(config->szPrinter), &cbData);
    cbData = sizeof(config->dwOutput);
    rc = RedMonQueryValue(hMonitor, hkey, OUTPUTKEY, &dwType, 
	(PBYTE)(&config->dwOutput), &cbData);
    if (config->dwOutput > OUTPUT_LAST)
	config->dwOutput = OUTPUT_SELF;
    cbData = sizeof(config->dwShow);
    rc = RedMonQueryValue(hMonitor, hkey, SHOWKEY, &dwType, 
	(PBYTE)(&config->dwShow), &cbData);
    cbData = sizeof(config->dwRunUser);
    rc = RedMonQueryValue(hMonitor, hkey, RUNUSERKEY, &dwType, 
	(PBYTE)(&config->dwRunUser), &cbData);
    cbData = sizeof(config->dwDelay);
    rc = RedMonQueryValue(hMonitor, hkey, DELAYKEY, &dwType, 
	(PBYTE)(&config->dwDelay), &cbData);
    cbData = sizeof(config->dwLogFileUse);
    rc = RedMonQueryValue(hMonitor, hkey, LOGUSEKEY, &dwType, 
	(PBYTE)(&config->dwLogFileUse), &cbData);
    cbData = sizeof(config->szLogFileName)-sizeof(TCHAR);
    rc = RedMonQueryValue(hMonitor, hkey, LOGNAMEKEY, &dwType, 
	(PBYTE)(config->szLogFileName), &cbData);
    cbData = sizeof(config->dwLogFileDebug);
    rc = RedMonQueryValue(hMonitor, hkey, LOGDEBUGKEY, &dwType, 
	(PBYTE)(&config->dwLogFileDebug), &cbData);
    cbData = sizeof(config->dwPrintError);
    rc = RedMonQueryValue(hMonitor, hkey, PRINTERRORKEY, &dwType, 
	(PBYTE)(&config->dwPrintError), &cbData);
    RedMonCloseKey(hMonitor, hkey);
    return TRUE;
}


/* write the configuration to the registry */
BOOL redmon_set_config(HANDLE hMonitor, RECONFIG *config)
{
    LONG rc = ERROR_SUCCESS;
    HANDLE hkey;
    TCHAR buf[MAXSTR];

    if (!redmon_validate_config(config))
	return FALSE;

    lstrcpy(buf, PORTSNAME);
    lstrcat(buf, BACKSLASH);
    lstrcat(buf, config->szPortName);
    rc = RedMonOpenKey(hMonitor, buf, KEY_WRITE, &hkey);
    if (rc != ERROR_SUCCESS)
	return FALSE;

    if (rc == ERROR_SUCCESS)
	rc = RedMonSetValue(hMonitor, hkey, DESCKEY, REG_SZ, 
	    (PBYTE)(config->szDescription), 
	    sizeof(TCHAR)*(lstrlen(config->szDescription)+1));
    if (rc == ERROR_SUCCESS)
	rc = RedMonSetValue(hMonitor, hkey, COMMANDKEY, REG_SZ, 
	    (PBYTE)(config->szCommand), 
	    sizeof(TCHAR)*(lstrlen(config->szCommand)+1));
    if (rc == ERROR_SUCCESS)
	rc = RedMonSetValue(hMonitor, hkey, ARGKEY, REG_SZ, 
	    (PBYTE)(config->szArguments), 
	    sizeof(TCHAR)*(lstrlen(config->szArguments)+1));
    if (rc == ERROR_SUCCESS)
	rc = RedMonSetValue(hMonitor, hkey, PRINTERKEY, REG_SZ, 
	    (PBYTE)(config->szPrinter), 
	    sizeof(TCHAR)*(lstrlen(config->szPrinter)+1));
    if (rc == ERROR_SUCCESS)
	rc = RedMonSetValue(hMonitor, hkey, OUTPUTKEY, REG_DWORD, 
	    (PBYTE)(&config->dwOutput), sizeof(config->dwOutput));
    if (rc == ERROR_SUCCESS)
	rc = RedMonSetValue(hMonitor, hkey, SHOWKEY, REG_DWORD, 
	    (PBYTE)(&config->dwShow), sizeof(config->dwShow));
    if (rc == ERROR_SUCCESS)
	rc = RedMonSetValue(hMonitor, hkey, RUNUSERKEY, REG_DWORD, 
	    (PBYTE)(&config->dwRunUser), sizeof(config->dwRunUser));
    if (rc == ERROR_SUCCESS)
	rc = RedMonSetValue(hMonitor, hkey, DELAYKEY, REG_DWORD, 
	    (PBYTE)(&config->dwDelay), sizeof(config->dwDelay));
    if (rc == ERROR_SUCCESS)
	rc = RedMonSetValue(hMonitor, hkey, LOGUSEKEY, REG_DWORD, 
	    (PBYTE)(&config->dwLogFileUse), sizeof(config->dwLogFileUse));
    if (rc == ERROR_SUCCESS)
	rc = RedMonSetValue(hMonitor, hkey, LOGNAMEKEY, REG_SZ, 
	    (PBYTE)(config->szLogFileName), 
	    sizeof(TCHAR)*(lstrlen(config->szLogFileName)+1));
    if (rc == ERROR_SUCCESS)
	rc = RedMonSetValue(hMonitor, hkey, LOGDEBUGKEY, REG_DWORD, 
	    (PBYTE)(&config->dwLogFileDebug), sizeof(config->dwLogFileDebug));
    if (rc == ERROR_SUCCESS)
	rc = RedMonSetValue(hMonitor, hkey, PRINTERRORKEY, REG_DWORD, 
	    (PBYTE)(&config->dwPrintError), sizeof(config->dwPrintError));
    RedMonCloseKey(hMonitor, hkey);
    return (rc == ERROR_SUCCESS);
}


/* Suggest a port name and store it in portname.
 * len is the size in characters of portname.
 * nAttempt is the number of times we have attempted
 * to generate a unique port name and can be used
 * to add a numeric suffix.  On first call this will be 1.
 *
 * If we can't generate a unique name then return FALSE.
 * If we only support one possible name then return FALSE when nAttempt > 1.
 */
BOOL redmon_suggest_portname(TCHAR *portname, int len, int nAttempt)
{
    TCHAR rname[] = TEXT("RPT%d:");
#ifdef DEBUG_REDMON
syslog(TEXT("redmon_suggest_portname\r\n"));
#endif
    if (len < sizeof(rname)/sizeof(TCHAR) + 12)
	return FALSE;
    wsprintf(portname, rname, nAttempt);
#ifdef DEBUG_REDMON
syslog(TEXT("attempt="));
sysnum(nAttempt);
syslog(TEXT("\r\nPort="));
syslog(portname);
syslog(TEXT("\r\n"));
#endif
    return TRUE;
}

void
reset_redata(REDATA *prd)
{
    /* do not touch prd->portname, prd->hPort or prd->hMonitor */

    prd->started = FALSE;
    prd->error = FALSE;
    prd->command[0] = '\0';
    memset(&(prd->config), 0, sizeof(prd->config));
    prd->config.dwDelay = DEFAULT_DELAY;
    prd->pPrinterName[0] = '\0';
    prd->pDocName[0] = '\0';
    prd->JobId = 0;
    prd->hChildStdinRd = INVALID_HANDLE_VALUE;
    prd->hChildStdinWr = INVALID_HANDLE_VALUE;
    prd->hChildStdoutRd = INVALID_HANDLE_VALUE;
    prd->hChildStdoutWr = INVALID_HANDLE_VALUE;
    prd->hChildStderrRd = INVALID_HANDLE_VALUE;
    prd->hChildStderrWr = INVALID_HANDLE_VALUE;
#ifdef SAVESTD
    prd->hSaveStdin = INVALID_HANDLE_VALUE;
    prd->hSaveStdout = INVALID_HANDLE_VALUE;
    prd->hSaveStderr = INVALID_HANDLE_VALUE;
#endif
    prd->hPipeRd = INVALID_HANDLE_VALUE;
    prd->hPipeWr = INVALID_HANDLE_VALUE;
    prd->hLogFile = INVALID_HANDLE_VALUE;
    prd->piProcInfo.hProcess = INVALID_HANDLE_VALUE;
    prd->piProcInfo.hThread = INVALID_HANDLE_VALUE;
    prd->hmutex = INVALID_HANDLE_VALUE;
    prd->write = FALSE;
    prd->write_event = INVALID_HANDLE_VALUE;
    prd->write_hthread = INVALID_HANDLE_VALUE;
    prd->write_threadid = 0;
    prd->write_buffer = NULL;
    prd->write_buffer_length = 0;
    prd->tempname[0] = '\0';
    prd->printer = INVALID_HANDLE_VALUE;
    prd->printer_bytes = 0;
}

/* copy stdout and stderr to log file, if open */
BOOL
flush_stdout(REDATA *prd)
{
    DWORD bytes_available, dwRead, dwWritten;
    BOOL result;
    BOOL got_something = FALSE;

    request_mutex(prd);

    /* copy anything on stdout to printer or log file */
    bytes_available = 0;
    result = PeekNamedPipe(prd->hChildStdoutRd, NULL, 0, NULL, 
	    &bytes_available, NULL);
    while (result && bytes_available) {
	if (!ReadFile(prd->hChildStdoutRd, prd->pipe_buf, sizeof(prd->pipe_buf), 
	    &dwRead, NULL) || dwRead == 0) 
	    break;
	got_something = TRUE;
	if (prd->config.dwOutput == OUTPUT_STDOUT) {
	    if (prd->printer != INVALID_HANDLE_VALUE) {
		if (!redmon_write_printer(prd, prd->pipe_buf, dwRead)) {
		    redmon_abort_printer(prd);
		}
	    }
	}
	else if (prd->hLogFile != INVALID_HANDLE_VALUE) {
	    WriteFile(prd->hLogFile, prd->pipe_buf, dwRead, &dwWritten, NULL);
	    FlushFileBuffers(prd->hLogFile);
	}
	result = PeekNamedPipe(prd->hChildStdoutRd, NULL, 0, NULL, 
	    &bytes_available, NULL);
    }

    /* copy anything on stderr to log file */
    bytes_available = 0;
    result = PeekNamedPipe(prd->hChildStderrRd, NULL, 0, NULL, 
	    &bytes_available, NULL);
    while (result && bytes_available) {
	if (!ReadFile(prd->hChildStderrRd, prd->pipe_buf, sizeof(prd->pipe_buf), &dwRead, NULL) ||
	    dwRead == 0) break;
	got_something = TRUE;
	if (prd->hLogFile != INVALID_HANDLE_VALUE) {
	    WriteFile(prd->hLogFile, prd->pipe_buf, dwRead, &dwWritten, NULL);
	    FlushFileBuffers(prd->hLogFile);
	}
	result = PeekNamedPipe(prd->hChildStderrRd, NULL, 0, NULL, 
	    &bytes_available, NULL);
    }

    /* copy anything on printer pipe to the printer */
    if (prd->config.dwOutput == OUTPUT_HANDLE) {
	bytes_available = 0;
	result = PeekNamedPipe(prd->hPipeRd, NULL, 0, NULL, 
		&bytes_available, NULL);
	while (result && bytes_available) {
	    if (!ReadFile(prd->hPipeRd, prd->pipe_buf, sizeof(prd->pipe_buf), 
		&dwRead, NULL) || dwRead == 0) 
		break;
	    got_something = TRUE;
	    if (prd->printer != INVALID_HANDLE_VALUE) {
		if (!redmon_write_printer(prd, prd->pipe_buf, dwRead)) {
		    redmon_abort_printer(prd);
		}
	    }
	    result = PeekNamedPipe(prd->hPipeRd, NULL, 0, NULL, 
		&bytes_available, NULL);
	}
    }

    release_mutex(prd);
    return got_something;
}


/* Check if process is running.  */
/* Return TRUE if process is running, FALSE otherwise */
/* Shut down stdin pipe if we find process has terminated */
BOOL check_process(REDATA *prd)
{
    DWORD exit_status;
    if (prd->error)
	return FALSE;	/* process is not running */

    if (prd->piProcInfo.hProcess == INVALID_HANDLE_VALUE)
	prd->error = TRUE;
    if (!prd->error 
	&& GetExitCodeProcess(prd->piProcInfo.hProcess, &exit_status)
	&& (exit_status != STILL_ACTIVE))
	prd->error = TRUE;

    if (prd->error) {
	DWORD bytes_available, dwRead;
	BOOL result;
	BYTE buf[256];
	if (prd->config.dwLogFileDebug) {
	  write_string_to_log(prd, 
	    TEXT("REDMON check_process: process isn't running.\r\n"));
	  write_string_to_log(prd, 
	    TEXT("REDMON check_process: flushing child stdin to unblock WriteThread.\r\n"));
	}
	/* flush stdin pipe to unblock WriteThread */
	bytes_available = 0;
	result = PeekNamedPipe(prd->hChildStdinRd, NULL, 0, NULL, 
		    &bytes_available, NULL);
	while (result && bytes_available) {
	    ReadFile(prd->hChildStdinRd, buf, sizeof(buf), &dwRead, NULL);
	    result = PeekNamedPipe(prd->hChildStdinRd, NULL, 0, NULL, 
		&bytes_available, NULL);
	}

    }
    return !prd->error;
}

/* Thread to write to stdout pipe */
DWORD WINAPI WriteThread(LPVOID lpThreadParameter)
{
    HANDLE hPort = (HANDLE)lpThreadParameter;
    REDATA *prd = GlobalLock((HGLOBAL)hPort);
    

    if (prd == (REDATA *)NULL)
	return 1;

    if (prd->config.dwLogFileDebug)
	write_string_to_log(prd, TEXT("\r\nREDMON WriteThread: started\r\n"));

    while (prd->write && !prd->error) {
	WaitForSingleObject(prd->write_event, INFINITE);
	ResetEvent(prd->write_event);
	if (prd->write_buffer_length && prd->write_buffer) {
	    if (! (prd->write_flag = WriteFile(prd->hChildStdinWr, 
		prd->write_buffer, prd->write_buffer_length, 
		&prd->write_written, NULL)) )
		prd->write = FALSE;	/* get out of here */
	}
	prd->write_buffer = NULL;
	prd->write_buffer_length = 0;
    }

    CloseHandle(prd->write_event);
    prd->write_event = INVALID_HANDLE_VALUE;

    if (prd->config.dwLogFileDebug)
	write_string_to_log(prd, TEXT("\r\nREDMON WriteThread: ending\r\n"));

    GlobalUnlock(hPort);
    return 0;
}

#ifdef UNICODE
/* Windows NT */
/* Convert a SID into a text format */
BOOL ConvertSid(PSID pSid, LPTSTR pszSidText, LPDWORD dwBufferLen)
{
    PSID_IDENTIFIER_AUTHORITY psia;
    DWORD dwSubAuthorities;
    DWORD dwSidRev=SID_REVISION;
    DWORD dwCounter;
    DWORD dwSidSize;

    //
    // test if Sid passed in is valid
    //
    if(!IsValidSid(pSid)) return FALSE;

    // obtain SidIdentifierAuthority
    psia=GetSidIdentifierAuthority(pSid);

    // obtain sidsubauthority count
    dwSubAuthorities=*GetSidSubAuthorityCount(pSid);

    //
    // compute buffer length
    // S-SID_REVISION- + identifierauthority- + subauthorities- + NULL
    //
    dwSidSize=(15 + 12 + (12 * dwSubAuthorities) + 1) * sizeof(TCHAR);

    //
    // check provided buffer length.
    // If not large enough, indicate proper size and setlasterror
    //
    if (*dwBufferLen < dwSidSize){
	*dwBufferLen = dwSidSize;
	SetLastError(ERROR_INSUFFICIENT_BUFFER);
	return FALSE;
    }

    //
    // prepare S-SID_REVISION-
    //
    dwSidSize=wsprintf(pszSidText, TEXT("S-%lu-"), dwSidRev );

    //
    // prepare SidIdentifierAuthority
    //
    if ( (psia->Value[0] != 0) || (psia->Value[1] != 0) ){
	dwSidSize+=wsprintf(pszSidText + lstrlen(pszSidText),
			   TEXT("0x%02hx%02hx%02hx%02hx%02hx%02hx"),
			   (USHORT)psia->Value[0],
			   (USHORT)psia->Value[1],
			   (USHORT)psia->Value[2],
			   (USHORT)psia->Value[3],
			   (USHORT)psia->Value[4],
			   (USHORT)psia->Value[5]);
    }
    else{
       dwSidSize+=wsprintf(pszSidText + lstrlen(pszSidText),
			   TEXT("%lu"),
			   (ULONG)(psia->Value[5]      )   +
			   (ULONG)(psia->Value[4] <<  8)   +
			   (ULONG)(psia->Value[3] << 16)   +
			   (ULONG)(psia->Value[2] << 24)   );
    }

    //
    // loop through SidSubAuthorities
    //
    for (dwCounter=0 ; dwCounter < dwSubAuthorities ; dwCounter++){
       dwSidSize+=wsprintf(pszSidText + dwSidSize, TEXT("-%lu"),
       *GetSidSubAuthority(pSid, dwCounter) );
    }

    return TRUE;
}


/* Get a text format SID, so we can access the local user
 * profile under HKEY_USERS.
 */
BOOL GetSid(LPTSTR pszSidText, LPDWORD dwSidTextLen)
{
    BOOL flag = TRUE;
    HANDLE htoken = INVALID_HANDLE_VALUE;
    TOKEN_INFORMATION_CLASS tic = TokenUser;
    TOKEN_USER *ptu = NULL;
    DWORD dwReturnLength = 0;
    DWORD dwTokenUserLength = 0;

#ifdef DEBUG_REDMON
    syslog(TEXT("GetSid\r\n"));
#endif

    /* get impersonation token of current thread */
    if ( !(flag = OpenThreadToken(GetCurrentThread() , 
        TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
	TRUE,
	&htoken)) ) {
	DWORD err = GetLastError();
#ifdef DEBUG_REDMON
	syslog(TEXT("OpenThreadToken failed\r\n"));
	syserror(err);
#endif
    }

    /* duplicate the token so we can query it */
    if (flag) {
	HANDLE hduptoken = INVALID_HANDLE_VALUE;
	if ( !(flag = DuplicateTokenEx(htoken, TOKEN_QUERY, NULL, 
	       SecurityImpersonation, TokenPrimary, &hduptoken)) ) {
#ifdef DEBUG_REDMON
	    DWORD err = GetLastError();
	    syslog(TEXT("DuplicateTokenEx\r\n"));
	    syserror(err);
#endif
	}
        CloseHandle(htoken);
	htoken = hduptoken;
    }

    if (flag)
	GetTokenInformation(htoken, tic, (LPVOID)ptu, 
	    dwTokenUserLength, &dwReturnLength);

    if (flag  && (dwReturnLength != 0) && 
	(GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
	HGLOBAL hglobal = GlobalAlloc(GPTR, (DWORD)dwReturnLength);
	ptu = GlobalLock(hglobal);
	if (ptu == NULL) {
	    flag = FALSE;
#ifdef DEBUG_REDMON
	    syslog(TEXT("Failed to allocate memory for SID\r\n"));
#endif
	}
	dwTokenUserLength = dwReturnLength;
	dwReturnLength = 0;
	if (flag) {
	    flag = GetTokenInformation(htoken, tic, (LPVOID)ptu,
		dwTokenUserLength, &dwReturnLength);
#ifdef DEBUG_REDMON
	    if (!flag) {	
		DWORD err = GetLastError();
		syslog(TEXT("GetTokenInformation\r\n"));
		syserror(err);
	    }
#endif
	}
	if (flag)
	    flag = ConvertSid((ptu->User.Sid), pszSidText, dwSidTextLen);
	GlobalUnlock(hglobal);
	GlobalFree(hglobal);
    }
    else
	flag = FALSE;


    if (htoken != INVALID_HANDLE_VALUE)
	CloseHandle(htoken);

#ifdef DEBUG_REDMON
    if (flag) {
	syslog(TEXT(" "));
	syslog(pszSidText);
	syslog(TEXT("\r\n"));
    }
    else
	syslog(TEXT(" failed\r\n"));
#endif
    if (flag)
	*dwSidTextLen = lstrlen(pszSidText);
    else
	*dwSidTextLen = 0;
    return flag;
}


/* When using "Prompt for filename", suggest previous name used by user.
 * The previous filename is stored in the user registry.  
 * Since we are running as the Local System account and the 
 * user registry isn't HKEY_CURRENT_USER, obtaint the SID for
 * the submitter and access the user registry under HKEY_USERS.
 * This only works if the user has logged into the computer interactively
 * once.  This isn't a problem because we don't allow "Prompt for filename"
 * unless the job is submitted locally.
 */
void get_user_filename(LPTSTR pszSid, LPTSTR pszFileName, DWORD dwFileNameLen)
{
    LONG rc;
    HKEY hkey;
    DWORD cbData;
    DWORD dwType;
    TCHAR pszKey[256];
    DWORD dwKeyLen = sizeof(pszKey)/sizeof(TCHAR);

#ifdef DEBUG_REDMON
    syslog(TEXT("get_user_filename  "));
    syslog(pszSid);
    syslog(TEXT("\r\n"));
#endif

    if (lstrlen(pszSid) + lstrlen(REDMONUSERKEY) + 2 >= 
	sizeof(pszKey)/sizeof(TCHAR)) {
#ifdef DEBUG_REDMON
	syslog(TEXT("pszKey buffer too small\r\n"));
#endif
	return;	/* buffer too small */
    }
    lstrcpy(pszKey, pszSid);
    lstrcat(pszKey, TEXT("\\"));
    lstrcat(pszKey, REDMONUSERKEY);
    
    /* Open registry key for this SID */
    /* We assume that the user has logged into this computer,
     * so their registry hive is loaded under HKEY_USERS.
     * If they are not logged in, this will fail. */
    rc = RegOpenKeyEx(HKEY_USERS, pszKey, 0, KEY_READ, (PHKEY)&hkey);

    if (rc == ERROR_SUCCESS) {
	cbData = dwFileNameLen;
	rc = RegQueryValueEx(hkey, LASTFILEKEY, 0, &dwType, 
		(PBYTE)(pszFileName), &cbData);
#ifdef DEBUG_REDMON
	if (rc == ERROR_SUCCESS) {
	    syslog(TEXT("LastFile="));
	    syslog(pszFileName);
	    syslog(TEXT("\r\n"));
	}
	else {
	    syslog(TEXT("No LastFile\r\n"));
	}
#endif
	RegCloseKey(hkey);
    }
#ifdef DEBUG_REDMON
    else {
	syslog(TEXT("RegOpenKeyEx "));
	syslog(pszKey);
	syslog(TEXT("\r\n"));
	syserror(rc);
    }
#endif
}

/* Save name obtained from "Prompt for filename" */
void save_user_filename(LPTSTR pszSid, LPTSTR pszFileName)
{
    LONG rc;
    HKEY hkey;
    TCHAR pszKey[256];

    /* Check if we can access the user registry */
    rc = RegOpenKeyEx(HKEY_USERS, pszSid, 0, KEY_WRITE, (PHKEY)&hkey);
    if (rc != ERROR_SUCCESS) {
#ifdef DEBUG_REDMON
	if (rc != ERROR_SUCCESS) {
	    syslog(TEXT("RegOpenKeyEx "));
	    syslog(pszSid);
	    syslog(TEXT("\r\n"));
	    syserror(rc);
	}
#endif
	/* Can't access user registry */
	/* They probably haven't logged on. */
	return;
    }
    RegCloseKey(hkey);

    if (lstrlen(pszSid) + lstrlen(REDMONUSERKEY) + 2 >= 
	sizeof(pszKey)/sizeof(TCHAR)) {
#ifdef DEBUG_REDMON
	syslog(TEXT("pszKey buffer too small\r\n"));
#endif
	return;	/* buffer too small */
    }
    lstrcpy(pszKey, pszSid);
    lstrcat(pszKey, TEXT("\\"));
    lstrcat(pszKey, REDMONUSERKEY);
    
    /* Open/Create the RedMon application key for this SID */
    rc = RegOpenKeyEx(HKEY_USERS, pszKey, 0, KEY_WRITE, (PHKEY)&hkey);
    if (rc != ERROR_SUCCESS) {
#ifdef DEBUG_REDMON
	syslog(TEXT("RegOpenKeyEx "));
	syslog(pszKey);
	syslog(TEXT("\r\n"));
	syserror(rc);
#endif
	rc = RegCreateKeyEx(HKEY_USERS, pszKey, 0, 0, 0, KEY_WRITE, 
		NULL, (PHKEY)&hkey, NULL);
#ifdef DEBUG_REDMON
	if (rc != ERROR_SUCCESS) {
	    syslog(TEXT("RegCreateKeyEx "));
	    syslog(pszKey);
	    syslog(TEXT("\r\n"));
	    syserror(rc);
	}
#endif
    }

    if (rc == ERROR_SUCCESS) {
	rc = RegSetValueEx(hkey, LASTFILEKEY, 0, REG_SZ,
	        (PBYTE)(pszFileName), sizeof(TCHAR)*(lstrlen(pszFileName)+1));
#ifdef DEBUG_REDMON
	if (rc != ERROR_SUCCESS) {
	    syslog(TEXT("RegSetValueEx\r\n "));
	    syslog(LASTFILEKEY);
	    syslog(TEXT("="));
	    syslog(pszFileName);
	    syslog(TEXT("\r\n"));
	    syserror(rc);
	}
#endif
	RegCloseKey(hkey);
    }
}

#else /* !UNICODE */
/* Windows 95 */
/* When using "Prompt for filename", suggest previous if same user.
 * The previous username and filename are stored in HKEY_USERS
 * under Software\Ghostgum\RedMon
 */
void get_last_filename(REDATA *prd)
{
    TCHAR last_user[MAXSTR];
    HKEY hkey;
    LONG rc;
    DWORD cbData;
    DWORD dwType;

    rc = RegOpenKeyEx(HKEY_CURRENT_USER, REDMONUSERKEY, 0,
	    KEY_READ, (PHKEY)&hkey);
    if (rc == ERROR_SUCCESS) {
	cbData = sizeof(last_user)-sizeof(TCHAR);
	rc = RegQueryValueEx(hkey, LASTUSERKEY, 0, &dwType, 
	    (PBYTE)(last_user), &cbData);
    }
    if ((rc == ERROR_SUCCESS) && (lstrcmp(prd->pUserName, last_user)==0)){
	cbData = sizeof(prd->tempname)-sizeof(TCHAR);
	rc = RegQueryValueEx(hkey, LASTFILEKEY, 0, &dwType, 
		(PBYTE)(prd->tempname), &cbData);
    }

}

void save_last_filename(REDATA *prd)
{
    HKEY hkey;
    LONG rc;

    rc = RegOpenKeyEx(HKEY_CURRENT_USER, REDMONUSERKEY, 0,
	    KEY_WRITE, (PHKEY)&hkey);
    if (rc != ERROR_SUCCESS) {
	rc = RegCreateKeyEx(HKEY_CURRENT_USER, REDMONUSERKEY, 
		0, 0, 0, KEY_WRITE, 
		NULL, (PHKEY)&hkey, NULL);
    }
    if (rc == ERROR_SUCCESS) {
	rc = RegSetValueEx(hkey, LASTUSERKEY, 0, REG_SZ,
	        (PBYTE)(prd->pUserName), 
		sizeof(TCHAR)*(lstrlen(prd->pUserName)+1));
    }
    if (rc == ERROR_SUCCESS) {
	rc = RegSetValueEx(hkey, LASTFILEKEY, 0, REG_SZ,
	        (PBYTE)(prd->tempname), 
		sizeof(TCHAR)*(lstrlen(prd->tempname)+1));
    }
}
#endif /* !UNICODE */

BOOL 
redmon_open_port(HANDLE hMonitor, LPTSTR pName, PHANDLE pHandle)
{
    HGLOBAL hglobal = GlobalAlloc(GPTR, (DWORD)sizeof(REDATA));
    REDATA *prd = (REDATA *)GlobalLock(hglobal);
#ifdef DEBUG_REDMON
    syslog(TEXT("redmon_open_port "));
    syslog(pName);
    syslog(TEXT("\r\n"));
#endif
    if (prd == (REDATA *)NULL) {
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return FALSE;
    }
    FillMemory((PVOID)prd, sizeof(REDATA), 0);
    reset_redata(prd);
    lstrcpy(prd->portname, pName);
    prd->hPort = hglobal;

    prd->hMonitor = hMonitor;

    /* Do the rest of the opening in rStartDocPort() */

    GlobalUnlock(hglobal);
    *pHandle = (HANDLE)hglobal;
    return TRUE;
} 

BOOL
redmon_close_port(HANDLE hPort)
{
    /* assume files were all closed in rEndDocPort() */
#ifdef DEBUG_REDMON
    syslog(TEXT("redmon_close_port: calling ClosePort\r\n"));
#endif

    if (hPort)
	GlobalFree((HGLOBAL)hPort);

    return TRUE;
}

BOOL
redmon_start_doc_port(REDATA *prd, LPTSTR pPrinterName, 
        DWORD JobId, DWORD Level, LPBYTE pDocInfo) 
{
    TCHAR buf[MAXSTR];
    int i;
    LPTSTR s;
    BOOL flag;
#ifndef UNICODE
    HANDLE hPrinter;
#endif

    if (prd == (REDATA *)NULL) {
	SetLastError(ERROR_INVALID_HANDLE);
	return FALSE;
    }
#ifdef DEBUG_REDMON
    syslog(TEXT("redmon_start_doc_port:\r\n"));
    {TCHAR buf[1024];
	if ((Level == 1) && pDocInfo) {
	    DOC_INFO_1 *dci1 = (DOC_INFO_1 *)pDocInfo;
	    wsprintf(buf, TEXT("\
  Level=1\r\n\
    DocumentName=\042%s\042\r\n\
    OutputFile=\042%s\042\r\n\
    Datatype=\042%s\042\r\n"),
		dci1->pDocName ? dci1->pDocName : TEXT("(null)"), 
		dci1->pOutputFile ? dci1->pOutputFile : TEXT("(null)"),
		dci1->pDatatype ? dci1->pDatatype : TEXT("(null)"));
	    syslog(buf);
	}
	else if ((Level == 2) && pDocInfo) {
	    DOC_INFO_2 *dci2 = (DOC_INFO_2 *)pDocInfo;
	    wsprintf(buf, TEXT("\
  Level=2\r\n\
    DocumentName=\042%s\042\r\n\
    OutputFile=\042%s\042\r\n\
    Datatype=\042%s\042\r\n\
    Mode=%d\r\n\
    JobId=%d\r\n"),
		dci2->pDocName ? dci2->pDocName : TEXT("(null)"), 
		dci2->pOutputFile ? dci2->pOutputFile : TEXT("(null)"),
		dci2->pDatatype ? dci2->pDatatype : TEXT("(null)"),
		dci2->dwMode, dci2->JobId);
	    syslog(buf);
	}
	else if ((Level == 3) && pDocInfo) {
	    DOC_INFO_3 *dci3 = (DOC_INFO_3 *)pDocInfo;
	    wsprintf(buf, TEXT("\
  Level=3\r\n\
    DocumentName=\042%s\042\r\n\
    OutputFile=\042%s\042\r\n\
    Datatype=\042%s\042\r\n\
    Flags=%d\r\n"),
		dci3->pDocName ? dci3->pDocName : TEXT("(null)"), 
		dci3->pOutputFile ? dci3->pOutputFile : TEXT("(null)"),
		dci3->pDatatype ? dci3->pDatatype : TEXT("(null)"),
		dci3->dwFlags);
	    syslog(buf);
	}
	else {
	    wsprintf(buf, TEXT("  Level=%d pDocInfo=%d\r\n"),
		Level, pDocInfo);
	    syslog(buf);
	}
    }
#endif

    reset_redata(prd);
    lstrcpy(prd->pPrinterName, pPrinterName);
    prd->JobId = JobId;
    /* remember document name, to be used for output job */
    if ((Level == 1) && pDocInfo) {
	DOC_INFO_1 *dci1 = (DOC_INFO_1 *)pDocInfo;
	lstrcpyn(prd->pDocName, dci1->pDocName, 
	    sizeof(prd->pDocName)/sizeof(TCHAR)-1);
    }
    else if ((Level == 2) && pDocInfo) {
	DOC_INFO_2 *dci2 = (DOC_INFO_2 *)pDocInfo;
	lstrcpyn(prd->pDocName, dci2->pDocName, 
	    sizeof(prd->pDocName)/sizeof(TCHAR)-1);
    }
    else if ((Level == 3) && pDocInfo) {
	DOC_INFO_3 *dci3 = (DOC_INFO_3 *)pDocInfo;
	lstrcpyn(prd->pDocName, dci3->pDocName, 
	    sizeof(prd->pDocName)/sizeof(TCHAR)-1);
    }
    else
	lstrcpy(prd->pDocName, TEXT("RedMon"));

    /* Get user name and machine name from job info */
    get_job_info(prd);

    /* get configuration for this port */
    prd->config.dwSize = sizeof(prd->config);
    prd->config.dwVersion = VERSION_NUMBER;
    if (!redmon_get_config(prd->hMonitor, prd->portname, &prd->config)) {
	SetLastError(REGDB_E_KEYMISSING);
	return FALSE;	/* There are no ports */
    }

    if (prd->config.dwLogFileUse) {
	/* Open optional log file */
	prd->hLogFile = INVALID_HANDLE_VALUE;
	if (lstrlen(prd->config.szLogFileName)) {
	    prd->hLogFile = CreateFile(prd->config.szLogFileName, 
		GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 
		FILE_ATTRIBUTE_NORMAL, NULL);
	}

	if (prd->config.dwLogFileDebug) {
	    LoadString(hdll, IDS_TITLE, buf, sizeof(buf)/sizeof(TCHAR)-1);
	    write_string_to_log(prd, buf);
	    write_string_to_log(prd, TEXT("\r\n"));
	    write_string_to_log(prd, copyright);
	    write_string_to_log(prd, version);
	}
    }

#ifndef UNICODE
    /* Make sure printer isn't in bidirectional mode since */
    /* failures in bidirectional access crash the spooler. */
    if (OpenPrinter(prd->pPrinterName, &hPrinter, NULL)) {
	HGLOBAL hglobal;
	PRINTER_INFO_2 *pi2;
	DWORD dwNeeded;
	int bidirectional = 0;
        hglobal = GlobalAlloc(GPTR, (DWORD)4096);
	pi2 = (PRINTER_INFO_2 *)GlobalLock(hglobal);
	if (pi2 == NULL) {
	    ClosePrinter(hPrinter);
	    return FALSE;
	}
	if (GetPrinter(hPrinter, 2, (BYTE *)pi2, 4096, &dwNeeded))
	    bidirectional = pi2->Attributes & (PRINTER_ATTRIBUTE_ENABLE_BIDI);
	GlobalUnlock(hglobal);
	GlobalFree(hglobal);
	ClosePrinter(hPrinter);
	if (bidirectional) {
	    write_string_to_log(prd, 
		TEXT("REDMON StartDocPort: returning FALSE.\r\n\
  You must disable bi-directional printer support for this printer\r\n"));
	    if (prd->hLogFile != INVALID_HANDLE_VALUE);
		CloseHandle(prd->hLogFile);
	    prd->hLogFile = INVALID_HANDLE_VALUE;
	    SetLastError(ERROR_INVALID_PRINTER_COMMAND);
	    return FALSE;
	}
    }
#endif


    flag = TRUE;	/* all is well */

    /* Find out which output method is used */
    if (prd->config.dwOutput == OUTPUT_FILE) {
	if (!create_tempfile(prd->tempname, 
		sizeof(prd->tempname)/sizeof(TCHAR))) {
	    write_string_to_log(prd, 
		TEXT("\r\nREDMON StartDocPort: temp file creation failed\r\n"));
	    flag = FALSE;
	}
    }

    /* Create anonymous inheritable pipe for printer output */
    if (prd->config.dwOutput == OUTPUT_HANDLE) {
	HANDLE hPipeTemp;
	SECURITY_ATTRIBUTES saAttr;
	/* Set the bInheritHandle flag so pipe handles are inherited. */
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;
        if (!CreatePipe(&hPipeTemp, &prd->hPipeWr, &saAttr, 0)) {
	    write_string_to_log(prd, 
		TEXT("\r\nREDMON StartDocPort: open printer pipe failed\r\n"));
	    flag = FALSE;
	}
	/* make the read handle non-inherited */
	if (!DuplicateHandle(GetCurrentProcess(), hPipeTemp,
		GetCurrentProcess(), &prd->hPipeRd, 0,
		FALSE,       /* not inherited */
		DUPLICATE_SAME_ACCESS)) {
	    write_string_to_log(prd, 
		TEXT("\r\nREDMON StartDocPort: duplicate printer pipe handle failed\r\n"));
	    flag = FALSE;
	}
	CloseHandle(hPipeTemp);
    }

    query_session_id(prd);
    make_env(prd);

    /* Prompt for output filename, to be passed as %1 */
    if (prd->config.dwOutput == OUTPUT_PROMPT) {
	OPENFILENAME ofn;
	TCHAR cReplace;
	TCHAR szFilter[MAXSTR];
	TCHAR szDir[MAXSTR];
#ifdef UNICODE
	TCHAR szSid[256];
	DWORD dwSidLen = sizeof(szSid)/sizeof(TCHAR)-1;
#endif
	TCHAR szComputerName[256];
	DWORD dwNameLen = sizeof(szComputerName)/sizeof(TCHAR)-1;
	TCHAR *p;

	/* Restrict to local user.  If we allowed remote access, the 
	 * Save As dialog would appear on the server, not the client.
	 */
	p = prd->pMachineName;
	while (*p && (*p=='\\'))	/* skip leading backslashes */
	    p++;
	szComputerName[0] = '\0';
	GetComputerName(szComputerName, &dwNameLen);
	if (lstrcmpi(p, szComputerName) != 0) {
	    /* The job was submitted from another computer. */
	    /* Do not allow this. */
	    write_string_to_log(prd, 
	        TEXT("\r\nREDMON StartDocPort: remote access to \042prompt for filename\042 is not permitted\r\n"));
	    SetLastError(ERROR_ACCESS_DENIED);
	    flag = FALSE;
	}

	if (flag) {
	    FillMemory((PVOID)&ofn, sizeof(ofn), 0);
	    ofn.lStructSize = sizeof(OPENFILENAME);
	    ofn.hwndOwner = HWND_DESKTOP;
	    ofn.lpstrFile = prd->tempname;
	    ofn.nMaxFile = sizeof(prd->tempname);
	    ofn.lpfnHook = GetSaveHookProc;	/* to bring to foreground */
	    ofn.Flags = OFN_ENABLEHOOK | OFN_EXPLORER;
	    if (LoadString(hdll, IDS_FILTER_PROMPT, szFilter, 
		sizeof(szFilter)/sizeof(TCHAR) -1 )) {
		cReplace = szFilter[lstrlen(szFilter)-1];
		for (i=0; szFilter[i] != '\0'; i++)
		    if (szFilter[i] == cReplace)
			szFilter[i] = '\0';
		ofn.lpstrFilter = szFilter;
		ofn.nFilterIndex = 0;
	    }
#ifdef UNICODE
	    /* Get SID for impersonation thread token
	     * If this user is local (as they should be), the text 
	     * formatted SID is a key under HKEY_USERS.
	     */
	    *szSid = '\0';
	    if (GetSid(szSid, &dwSidLen)) {
		get_user_filename(szSid, prd->tempname, 
		    sizeof(prd->tempname)/sizeof(TCHAR) - 1);
	    }
#else
	    get_last_filename(prd);
#endif
	    if (lstrlen(ofn.lpstrFile)) {
		lstrcpy(szDir, ofn.lpstrFile);
		for (i=lstrlen(szDir)-1; i; i--) {
		    if (szDir[i] == '\\') {
			lstrcpy(ofn.lpstrFile, szDir+i+1);
			szDir[i+1] = '\0';
			ofn.lpstrInitialDir = szDir;
			break;
		    }
		}
	    }
#ifdef NOTUSED /* This code doesn't work yet */
#if defined(UNICODE) && (defined(NT40) || defined(NT50))
	    if (prd->pSessionId && 
		!((prd->pSessionId[0] = '0') && (prd->pSessionId[1] == '\0')))
		flag = get_filename_as_user(prd);/* WTS uses separate process*/
	    else
#endif
#endif
	    flag = GetSaveFileName(&ofn);

	    if (flag) {
#if REDMON_DEBUG
		syslog(TEXT("GetSaveFileName returns \042"));
		syslog(prd->tempname);
		syslog(TEXT("\042\r\n"));
#endif
#ifdef UNICODE
		if (lstrlen(szSid))
		    save_user_filename(szSid, prd->tempname);
#else
		save_last_filename(prd);
#endif
	    }
	    else  {
		redmon_cancel_job(prd);
		write_string_to_log(prd, 
	      TEXT("\r\nREDMON StartDocPort: prompt for filename failed\r\n"));
	        SetLastError(ERROR_CANCELLED);
	    }
	}
    }
  
    if (!flag) {
	if (prd->hLogFile != INVALID_HANDLE_VALUE);
	    CloseHandle(prd->hLogFile);
	prd->hLogFile = INVALID_HANDLE_VALUE;
	if (prd->hPipeRd != INVALID_HANDLE_VALUE);
	    CloseHandle(prd->hPipeRd);
	prd->hPipeRd = INVALID_HANDLE_VALUE;
	if (prd->hPipeWr != INVALID_HANDLE_VALUE);
	    CloseHandle(prd->hPipeWr);
	prd->hPipeWr = INVALID_HANDLE_VALUE;
	if (prd->environment)
	    GlobalFree(prd->environment);
	prd->environment = NULL;
	return FALSE;
    }

    /* Launch application */

    /* Build command line */
    lstrcpy(prd->command, TEXT("\042"));
    lstrcat(prd->command, prd->config.szCommand);
    lstrcat(prd->command, TEXT("\042 "));

    /* copy arguments, substituting %1 for temp or prompted filename, */
    /* %h for printer pipe handle,  %d for document name, %u for the user */
    i = lstrlen(prd->command);
    for (s = prd->config.szArguments; 
	*s && (i < sizeof(prd->command)/sizeof(TCHAR)-1); s++) {
	if ( (*s == '%') && (*(s+1)=='1') &&
	  (i+lstrlen(prd->tempname) < sizeof(prd->command)/sizeof(TCHAR)-1) )
	{
	    /* copy temp or prompted filename */
	    prd->command[i] = '\0';
	    lstrcat(prd->command, prd->tempname);
	    i = lstrlen(prd->command);
	    s++;
        }
	else if ( (*s == '%') && (*(s+1)=='h') &&
	  (i+16 < sizeof(prd->command)/sizeof(TCHAR)-1) )
	{
	    /* copy printer pipe handle as hexadecimal */
	    prd->command[i] = '\0';
#ifdef _WIN64
	    wsprintf(&(prd->command[i]), TEXT("%08x%08x"), (DWORD)(((DWORD_PTR)prd->hPipeWr)>>32), (DWORD)((DWORD_PTR)prd->hPipeWr));
#else
	    wsprintf(&(prd->command[i]), TEXT("%08x"), (DWORD)(prd->hPipeWr));
#endif
	    i = lstrlen(prd->command);
	    s++;
        }
	else if ( (*s == '%') && (*(s+1)=='d') &&
	  (i+lstrlen(prd->pDocName) < sizeof(prd->command)/sizeof(TCHAR)-1) )
	{
	    /* copy document name, skipping invalid characters */
	    LPTSTR t;
	    for (t = prd->pDocName; *t; t++) {
		if ((*t != '<') && (*t != '>') && (*t != '\"') &&
		    (*t != '|') && (*t != '/') && (*t != '\\') &&
		    (*t != ':'))
		    prd->command[i++] = *t;
	    }
	    prd->command[i] = '\0';
	    i = lstrlen(prd->command);
	    s++;
        }
	else if ( (*s == '%') && (*(s+1)=='u') &&
	  (i+lstrlen(prd->pUserName) < sizeof(prd->command)/sizeof(TCHAR)-1) )
	{
	    /* copy user name, skipping invalid characters */
	    LPTSTR t;
	    for (t = prd->pUserName; *t; t++) {
		if ((*t != '<') && (*t != '>') && (*t != '\"') &&
		    (*t != '|') && (*t != '/') && (*t != '\\') &&
		    (*t != ':'))
		    prd->command[i++] = *t;
	    }
	    prd->command[i] = '\0';
	    i = lstrlen(prd->command);
	    s++;
	}
	else if ( (*s == '%') && (*(s+1)=='%') ) {
	    s++;
	    prd->command[i++] = *s;
	}
	else
	    prd->command[i++] = *s;
    }
    prd->command[i] = '\0';

    /* fix shutdown delay */
    if (prd->config.dwDelay < MINIMUM_DELAY)
	prd->config.dwDelay = MINIMUM_DELAY;

    if ( (prd->config.dwOutput == OUTPUT_STDOUT) ||
	 (prd->config.dwOutput == OUTPUT_HANDLE) ) {
	/* open a printer */
	if (!redmon_open_printer(prd)) {
	    write_string_to_log(prd, 
		TEXT("\r\nREDMON StartDocPort: open printer failed\r\n"));
	    if (prd->hLogFile != INVALID_HANDLE_VALUE)
	        CloseHandle(prd->hLogFile);
	    prd->hLogFile = INVALID_HANDLE_VALUE;
	    return FALSE;
	}
    }

    prd->hmutex = CreateMutex(NULL, FALSE, NULL);
    flag = start_redirect(prd);
    if (flag) {
        WaitForInputIdle(prd->piProcInfo.hProcess, 5000);

	/* Create thread to write to stdin pipe
	 * We need this to avoid a deadlock when stdin and stdout
	 * pipes are both blocked.
	 */
	prd->write_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (prd->write_event == NULL)
	    write_string_to_log(prd, 
		TEXT("couldn't create synchronization event\r\n"));
	prd->write = TRUE;
	prd->write_hthread = CreateThread(NULL, 0, &WriteThread, 
		prd->hPort, 0, &prd->write_threadid);
    }
    else {
	DWORD err = GetLastError();
	/* ENGLISH */
	if (prd->environment) {
	    GlobalUnlock(prd->environment);
	    GlobalFree(prd->environment);
	    prd->environment = NULL;
	}
	wsprintf(buf, 
	   TEXT("StartDocPort: failed to start process\r\n  Port = %s\r\n  Command = %s\r\n  Error = %ld\r\n"),
	   prd->portname, prd->command, err);
	switch(err) {
	    case ERROR_FILE_NOT_FOUND:
		lstrcat(buf, TEXT("  File not found\r\n"));
		break;
	    case ERROR_PATH_NOT_FOUND:
		lstrcat(buf, TEXT("  Path not found\r\n"));
		break;
	    case ERROR_BAD_PATHNAME:
		lstrcat(buf, TEXT("  Bad path name\r\n"));
		break;
	}
	write_string_to_log(prd, buf);
	write_error(prd, err);
    }

    if (prd->config.dwLogFileDebug) {
	wsprintf(buf, 
	  TEXT("REDMON StartDocPort: returning %d\r\n\
  %s\r\n\
  Printer=%s\r\n\
  JobId=%d\r\n"), 
	  flag, prd->command, prd->pPrinterName, prd->JobId);
	write_string_to_log(prd, buf);
	if ((Level == 1) && pDocInfo) {
	    DOC_INFO_1 *dci1 = (DOC_INFO_1 *)pDocInfo;
	    wsprintf(buf, TEXT("\
  Level=1\r\n\
    DocumentName=\042%s\042\r\n\
    OutputFile=\042%s\042\r\n\
    Datatype=\042%s\042\r\n"),
		dci1->pDocName ? dci1->pDocName : TEXT("(null)"), 
		dci1->pOutputFile ? dci1->pOutputFile : TEXT("(null)"),
		dci1->pDatatype ? dci1->pDatatype : TEXT("(null)"));
	    write_string_to_log(prd, buf);
	}
	else if ((Level == 2) && pDocInfo) {
	    DOC_INFO_2 *dci2 = (DOC_INFO_2 *)pDocInfo;
	    wsprintf(buf, TEXT("\
  Level=2\r\n\
    DocumentName=\042%s\042\r\n\
    OutputFile=\042%s\042\r\n\
    Datatype=\042%s\042\r\n\
    Mode=%d\r\n\
    JobId=%d\r\n"),
		dci2->pDocName ? dci2->pDocName : TEXT("(null)"), 
		dci2->pOutputFile ? dci2->pOutputFile : TEXT("(null)"),
		dci2->pDatatype ? dci2->pDatatype : TEXT("(null)"),
		dci2->dwMode, dci2->JobId);
	    write_string_to_log(prd, buf);
	}
	else if ((Level == 3) && pDocInfo) {
	    DOC_INFO_3 *dci3 = (DOC_INFO_3 *)pDocInfo;
	    wsprintf(buf, TEXT("\
  Level=3\r\n\
    DocumentName=\042%s\042\r\n\
    OutputFile=\042%s\042\r\n\
    Datatype=\042%s\042\r\n\
    Flags=%d\r\n"),
		dci3->pDocName ? dci3->pDocName : TEXT("(null)"), 
		dci3->pOutputFile ? dci3->pOutputFile : TEXT("(null)"),
		dci3->pDatatype ? dci3->pDatatype : TEXT("(null)"),
		dci3->dwFlags);
	    write_string_to_log(prd, buf);
	}
	else {
	    wsprintf(buf, TEXT("  Level=%d pDocInfo=%d\r\n"),
		Level, pDocInfo);
	    write_string_to_log(prd, buf);
	}
	wsprintf(buf, TEXT("  output=%d show=%d delay=%d runuser=%d\r\n"),
	    prd->config.dwOutput, prd->config.dwShow, 
	    prd->config.dwDelay, prd->config.dwRunUser);
	write_string_to_log(prd, buf);
    }

    if (!flag) {
	/* close all file and object handles */
	if (prd->hLogFile != INVALID_HANDLE_VALUE);
	    CloseHandle(prd->hLogFile);
	prd->hLogFile = INVALID_HANDLE_VALUE;

	if (prd->hChildStderrRd)
	    CloseHandle(prd->hChildStderrRd);
	if (prd->hChildStderrWr)
	    CloseHandle(prd->hChildStderrWr);
	if (prd->hChildStdoutRd)
	    CloseHandle(prd->hChildStdoutRd);
	if (prd->hChildStdoutWr)
	    CloseHandle(prd->hChildStdoutWr);
	if (prd->hChildStdinRd)
	    CloseHandle(prd->hChildStdinRd);
	if (prd->hChildStdinWr)
	    CloseHandle(prd->hChildStdinWr);
	if (prd->hPipeRd)
	    CloseHandle(prd->hPipeRd);
	if (prd->hPipeWr)
	    CloseHandle(prd->hPipeWr);
	prd->hChildStderrRd   = INVALID_HANDLE_VALUE;
	prd->hChildStderrWr   = INVALID_HANDLE_VALUE;
	prd->hChildStdoutRd   = INVALID_HANDLE_VALUE;
	prd->hChildStdoutWr   = INVALID_HANDLE_VALUE;
	prd->hChildStdinRd    = INVALID_HANDLE_VALUE;
	prd->hChildStdinWr    = INVALID_HANDLE_VALUE;
	prd->hPipeRd   = INVALID_HANDLE_VALUE;
	prd->hPipeWr   = INVALID_HANDLE_VALUE;
#ifdef SAVESTD
	SetStdHandle(STD_INPUT_HANDLE, prd->hSaveStdin);
	SetStdHandle(STD_OUTPUT_HANDLE, prd->hSaveStdout);
	SetStdHandle(STD_ERROR_HANDLE, prd->hSaveStdout);
#endif

	if (prd->hmutex != INVALID_HANDLE_VALUE)
	    CloseHandle(prd->hmutex);
	prd->hmutex    = INVALID_HANDLE_VALUE;
    }

    prd->started = flag;

    return flag;
}

void
redmon_cancel_job(REDATA *prd)
{
    TCHAR buf[MAXSTR];
    HANDLE hPrinter;
    if (OpenPrinter(prd->pPrinterName, &hPrinter, NULL)) {
	DWORD dwNeeded = 0;
	HGLOBAL hglobal = GlobalAlloc(GPTR, (DWORD)4096);
	JOB_INFO_1 *pjob = (JOB_INFO_1 *)GlobalLock(hglobal);
	if ((pjob != (JOB_INFO_1 *)NULL) && 
	     GetJob(hPrinter, prd->JobId, 1, (LPBYTE)pjob, 
		4096, &dwNeeded)) {
	    pjob->Status = JOB_STATUS_ERROR;
	    SetJob(hPrinter, prd->JobId, 1, (LPBYTE)pjob, 
		JOB_CONTROL_CANCEL);
	    if (prd->config.dwLogFileDebug && (prd->hLogFile != INVALID_HANDLE_VALUE)) {
		wsprintf(buf, 
	  TEXT("\r\nREDMON Cancelling print job\r\n"));
		write_string_to_log(prd, buf);
	    }
	}
	else {
	    SetJob(hPrinter, prd->JobId, 0, NULL, JOB_CONTROL_CANCEL);
	    if (prd->config.dwLogFileDebug && (prd->hLogFile != INVALID_HANDLE_VALUE)) {
		wsprintf(buf, 
	  TEXT("\r\nREDMON Cancelling print job (second method)\r\n"));
		write_string_to_log(prd, buf);
	    }
	}
	if (pjob != (JOB_INFO_1 *)NULL) {
	    GlobalUnlock(hglobal);
	    GlobalFree(hglobal);
	}
	ClosePrinter(hPrinter);
    }
}

/* WritePort is normally called between StartDocPort and EndDocPort,
 * but can be called outside this pair for bidirectional printers.
 */
BOOL redmon_write_port(REDATA *prd, LPBYTE  pBuffer, 
        DWORD   cbBuf, LPDWORD pcbWritten)
{
    TCHAR buf[MAXSTR];
    unsigned int sleep_count;

    if (prd == (REDATA *)NULL) {
	SetLastError(ERROR_INVALID_HANDLE);
	return FALSE;
    }

    *pcbWritten = 0;

    if (!prd->started) {
	if (prd->config.dwLogFileDebug) {
	    wsprintf(buf, 
	      TEXT("REDMON WritePort: called outside Start/EndDocPort.  Returning FALSE\r\n"));
	    MessageBox(NULL, buf, MONITORNAME, MB_OK);
	}
	return FALSE;
    }

    /* copy from output pipes to printer or log file */
    flush_stdout(prd);

    /* Make sure process is still running */
    check_process(prd);

    if (prd->error) {
	/* The process is no longer running, probably due to an error. */
	/* If we return an error from WritePort, the spooler crashes.
	 * To avoid this mess, don't return an error from WritePort.
	 * Instead carry on as if the error didn't occur.
	 * The only evidence of an error will be the log file.
	 */
	*pcbWritten = cbBuf;	/* say we wrote it all */
	if (prd->config.dwLogFileDebug 
	    && (prd->hLogFile != INVALID_HANDLE_VALUE)) {
	    wsprintf(buf, 
	      TEXT("\r\nREDMON WritePort: Process not running. \
Returning TRUE.\r\n    Ignoring %d bytes\r\n"), cbBuf);
	    write_string_to_log(prd, buf);
	}

	/* Cancel the print job */
	redmon_cancel_job(prd);

	return TRUE;	/* say we wrote it all */
    }

    if (prd->config.dwLogFileDebug && (prd->hLogFile != INVALID_HANDLE_VALUE)) {
	wsprintf(buf, 
	 TEXT("\r\nREDMON WritePort: about to write %d bytes to port.\r\n"),
	 cbBuf);
	write_string_to_log(prd, buf);
    }


    /* write to stdin pipe */
    prd->write_buffer_length = cbBuf;
    prd->write_buffer = pBuffer;
    prd->write_flag = TRUE;
    prd->write_written = 0;
    SetEvent(prd->write_event);
    if (prd->write && prd->write_buffer_length) {
	flush_stdout(prd);
	check_process(prd);
	Sleep(0);
    }
    sleep_count = 0;
    while (!prd->error && prd->write && prd->write_buffer_length) {
	/* wait for it to be written, while flushing stdout */
	if (flush_stdout(prd)) {
	    /* We succeeded in reading something from one of the
	     * pipes and the pipes are now empty.  Give up the
	     * remainder of our time slice to allow the
	     * other process to write something to the pipes
	     * or to read from stdin.
	     */
	    sleep_count = 0;
	    Sleep(0);
	}
	else if (prd->write_buffer_length) {
	    /* The pipes were empty, and the other process
	     * hasn't finished reading stdin.
	     * Pause a little until something is available or
	     * until stdin has been read.
	     * If the process is very slow and doesn't read stdin 
	     * within a reasonable time, sleep for 100ms to avoid 
	     * wasting CPU.
	     */
	    if (sleep_count < 10)
		Sleep(sleep_count * 5);
	    else
	        Sleep(100);
	    sleep_count++;
	}
	/* Make sure process is still running */
	check_process(prd);
    }
    *pcbWritten = prd->write_written;

    if (prd->error)
        *pcbWritten = cbBuf;

    if (prd->config.dwLogFileDebug && (prd->hLogFile != INVALID_HANDLE_VALUE)) {
	DWORD cbWritten;
	request_mutex(prd);
        WriteFile(prd->hLogFile, pBuffer, cbBuf, &cbWritten, NULL);
	FlushFileBuffers(prd->hLogFile);
	release_mutex(prd);
	wsprintf(buf, 
	  TEXT("\r\nREDMON WritePort: %s  count=%d written=%d\r\n"), 
	      (prd->write_flag ? TEXT("OK") : TEXT("Failed")),
	      cbBuf, *pcbWritten);
	write_string_to_log(prd, buf);
    }

    flush_stdout(prd);

    if (prd->error) {
	if (prd->config.dwLogFileDebug 
	    && (prd->hLogFile != INVALID_HANDLE_VALUE)) {
	    wsprintf(buf, 
	      TEXT("\r\nREDMON WritePort: Process not running. \
Returning TRUE.\r\n    Ignoring %d bytes\r\n"), cbBuf);
	    write_string_to_log(prd, buf);
	}
	/* Cancel the print job */
	redmon_cancel_job(prd);
    }

    return TRUE;	/* returning FALSE crashes Win95 spooler */
}

/* ReadPort can be called within a Start/EndDocPort pair,
 * and also outside this pair.
 */
BOOL redmon_read_port(REDATA *prd, LPBYTE pBuffer, 
        DWORD  cbBuffer, LPDWORD pcbRead)
{
    /* we don't support reading */
    *pcbRead = 0;

    if (prd == (REDATA *)NULL) {
	SetLastError(ERROR_INVALID_HANDLE);
	return FALSE;
    }

    Sleep(1000);	/* Pause a little */

    if (prd->config.dwLogFileDebug) {
	TCHAR buf[MAXSTR];
	wsprintf(buf, TEXT("REDMON ReadPort: returning FALSE. Process %s\r\n"),
	   prd->error ? TEXT("has an ERROR.") : TEXT("is OK."));
	write_string_to_log(prd, buf);
	wsprintf(buf, TEXT("REDMON ReadPort: You must disable bi-directional printer support for this printer\r\n"));
	write_string_to_log(prd, buf);
    }

    /* We don't support read port */
    return FALSE;
}

BOOL redmon_end_doc_port(REDATA *prd)
{
    TCHAR buf[MAXSTR];
    DWORD exit_status;
    HANDLE hPrinter;
    unsigned int i;

    if (prd == (REDATA *)NULL) {
	SetLastError(ERROR_INVALID_HANDLE);
	return FALSE;
    }
#ifdef DEBUG_REDMON
    syslog(TEXT("redmon_end_doc_port\r\n"));
#endif

    if (prd->config.dwLogFileDebug)
	write_string_to_log(prd, 
		TEXT("REDMON EndDocPort: starting\r\n"));

    /* tell write thread to shut down */
    prd->write_buffer_length = 0;
    prd->write_buffer = NULL;
    prd->write = FALSE;
    SetEvent(prd->write_event);
    Sleep(0);	/* let write thread terminate */

    for (i=0; i<20; i++) {
	if (GetExitCodeThread(prd->write_hthread, &exit_status)) {
	    if (exit_status != STILL_ACTIVE)
		break;
	}
	else
	    break;
	Sleep(50);
    }
    CloseHandle(prd->write_hthread);

    /* Close stdin to signal EOF */
    if (prd->hChildStdinWr != INVALID_HANDLE_VALUE)
	CloseHandle(prd->hChildStdinWr);

    flush_stdout(prd);

    /* wait here for up to 'delay' seconds until process ends */
    /* so that process has time to write stdout/err */
    exit_status = 0;
    for (i=0; i<prd->config.dwDelay; i++) {
	if (prd->piProcInfo.hProcess == INVALID_HANDLE_VALUE)
	    break;
	if (GetExitCodeProcess(prd->piProcInfo.hProcess, &exit_status)) {
	    if (exit_status != STILL_ACTIVE)
	        break;
	}
	else {
	    /* process doesn't exist */
	    break;
	}

        flush_stdout(prd);

        Sleep(1000);
    }

    flush_stdout(prd);

    if (prd->config.dwLogFileDebug) {
	wsprintf(buf, 
	    TEXT("REDMON EndDocPort: process %s after %d second%s\r\n"), 
	    (exit_status == STILL_ACTIVE) ? 
		TEXT("still running") : TEXT("finished"), 
	    i, (i != 1) ? TEXT("s") : TEXT(""));
	write_string_to_log(prd, buf);
    }

    /* if process still running, we might want to kill it
     * with TerminateProcess(), but since this can have bad
     * side effects, don't do it yet.
     */

    /* When the process terminates, it will close its copy of
     * of the stdout & stderr pipe write handles. 
     * The pipes remain open because we still have a handle to
     * to the write end. 
     * Close our copy of the write end of the stdout & stderr 
     * pipes to flush them.
     */
    CloseHandle(prd->hChildStdoutWr);
    CloseHandle(prd->hChildStderrWr);
    if (prd->hPipeWr != INVALID_HANDLE_VALUE);
        CloseHandle(prd->hPipeWr);

    /* copy anything on stdout/err to log file */
    flush_stdout(prd);

    /* Close the read end of the stdio pipes */
    CloseHandle(prd->hChildStderrRd);
    CloseHandle(prd->hChildStdoutRd);
    CloseHandle(prd->hChildStdinRd);
    if (prd->hPipeWr != INVALID_HANDLE_VALUE);
        CloseHandle(prd->hPipeRd);

    /* NT documentation says *we* should cancel the print job. */
    /* 95 documentation says nothing about this. */
    if (OpenPrinter(prd->pPrinterName, &hPrinter, NULL)) {
	SetJob(hPrinter, prd->JobId, 0, NULL, JOB_CONTROL_CANCEL);
	ClosePrinter(hPrinter);
    }

#ifdef UNUSED
    /* elsewhere, the NT documentation says we should use */
    /* JOB_CONTROL_SENT_TO_PRINTER */
    if (OpenPrinter(prd->pPrinterName, &hPrinter, NULL)) {
	SetJob(hPrinter, prd->JobId, 0, NULL, JOB_CONTROL_SENT_TO_PRINTER);
	ClosePrinter(hPrinter);
    }
#endif

    /* If we output to a temporary file, print it now */
    if (prd->config.dwOutput == OUTPUT_FILE) {
	if (prd->config.dwLogFileDebug) {
	    wsprintf(buf, 
		TEXT("\r\nREDMON EndDocPort: Copying %s to %s"), 
		   prd->tempname, prd->config.szPrinter);
	    write_string_to_log(prd, buf);
	}
	redmon_printfile(prd, prd->tempname); 
	if (prd->config.dwLogFileDebug)
	    write_string_to_log(prd, 
		TEXT("\r\nREDMON EndDocPort: Deleting temporary file"));
	DeleteFile(prd->tempname);
    }
    else if ( (prd->config.dwOutput == OUTPUT_STDOUT) || 
	(prd->config.dwOutput == OUTPUT_HANDLE)  ){
	redmon_close_printer(prd);
	if (prd->config.dwLogFileDebug)
	  write_string_to_log(prd, 
	    TEXT("\r\nREDMON EndDocPort: Closing printer for stdout or pipe"));
    }

    if (prd->config.dwLogFileDebug) {
	TCHAR buf[256];
	wsprintf(buf, 
	    TEXT("\r\nREDMON EndDocPort: %d bytes written to printer\r\n"), 
	    prd->printer_bytes);
	write_string_to_log(prd, buf);
    }

    if (prd->environment) {
	/* Are we responsible for this, or does the process delete
	 * its own environment? */
	GlobalUnlock(prd->environment);
	GlobalFree(prd->environment);
	prd->environment = NULL;
    }

    if (prd->config.dwRunUser)
	CloseHandle(prd->primary_token);

    if (prd->config.dwLogFileDebug)
	write_string_to_log(prd, 
		TEXT("REDMON EndDocPort: ending\r\n"));

    if (prd->hLogFile != INVALID_HANDLE_VALUE)
	CloseHandle(prd->hLogFile);

    if (prd->error && prd->config.dwPrintError && 
	((prd->config.dwOutput == OUTPUT_STDOUT) || (prd->config.dwOutput == OUTPUT_FILE) 
	 || (prd->config.dwOutput == OUTPUT_HANDLE)) )
	redmon_print_error(prd);

    if ((prd->hmutex != NULL) && (prd->hmutex != INVALID_HANDLE_VALUE))
	CloseHandle(prd->hmutex);

    if (prd->piProcInfo.hProcess != INVALID_HANDLE_VALUE)
	CloseHandle(prd->piProcInfo.hProcess);

    if (prd->piProcInfo.hThread != INVALID_HANDLE_VALUE)
	CloseHandle(prd->piProcInfo.hThread);

    reset_redata(prd);

    return TRUE;
}

BOOL
redmon_set_port_timeouts(REDATA *prd, LPCOMMTIMEOUTS lpCTO, DWORD reserved)
{
    /* Do nothing */

    if (prd == (REDATA *)NULL) {
	SetLastError(ERROR_INVALID_HANDLE);
	return FALSE;
    }

#ifdef DEBUG
    {
        TCHAR buf[MAXSTR];
	if (prd->config.dwLogFileDebug) {
	    wsprintf(buf, TEXT("REDMON SetPortTimeOuts: returning TRUE\r\n\
	    values = %d %d %d %d %d\r\n"),
		lpCTO->ReadIntervalTimeout,
		lpCTO->ReadTotalTimeoutMultiplier,
		lpCTO->ReadTotalTimeoutConstant,
		lpCTO->WriteTotalTimeoutMultiplier,
		lpCTO->WriteTotalTimeoutConstant);
	    write_string_to_log(prd, buf);
	}
    }
#endif

    return TRUE;
}
 


LRESULT APIENTRY 
GetSaveHookProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_INITDIALOG) {
	SetForegroundWindow(hDlg);
	BringWindowToTop(hDlg);
    }
    return 0;	// default handler
}


BOOL
browse(HWND hwnd, UINT control, UINT filter, BOOL save)
{
OPENFILENAME ofn;
TCHAR szFilename[MAXSTR];	/* filename for OFN */
TCHAR szFilter[MAXSTR];
TCHAR szDir[MAXSTR];
TCHAR cReplace;
int i;
HANDLE hToken = NULL;

BOOL flag;
	/* setup OPENFILENAME struct */
	FillMemory((PVOID)&ofn, sizeof(ofn), 0);
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = szFilename;
	ofn.nMaxFile = sizeof(szFilename);
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	GetDlgItemText(hwnd, control, szFilename, 
		sizeof(szFilename)/sizeof(TCHAR) - 1);

	if ((lstrlen(szFilename) == 2) && (szFilename[1] == ':'))
	    lstrcat(szFilename, BACKSLASH);

	if (lstrlen(szFilename)) {
	    lstrcpy(szDir, szFilename);
	    for (i=lstrlen(szDir)-1; i; i--) {
		if (szDir[i] == '\\') {
		    lstrcpy(szFilename, szDir+i+1);
		    szDir[i+1] = '\0';
		    ofn.lpstrInitialDir = szDir;
		    break;
		}
	    }
	}

	if (LoadString(hdll, filter, szFilter, 
	    sizeof(szFilter)/sizeof(TCHAR) -1 )) {
	    cReplace = szFilter[lstrlen(szFilter)-1];
	    for (i=0; szFilter[i] != '\0'; i++)
	        if (szFilter[i] == cReplace)
		    szFilter[i] = '\0';
	    ofn.lpstrFilter = szFilter;
	    ofn.nFilterIndex = 0;
	}

	/* call the common dialog box */
/* The following call may be needed to make the dialog
appear on the correct session.
Need to call this in spoolss.dll, so we need to load this
DLL dynamically.

	hToken = RevertToPrinterSelf();
*/
	if (save)
	    flag = GetSaveFileName(&ofn);
	else
	    flag = GetOpenFileName(&ofn);
/*
	if (hToken)
            ImpersonatePrinterClient(hToken);
*/

	if (flag)
	    SetDlgItemText(hwnd, control, szFilename);
	return flag;
}

int
lstrcmpn(LPTSTR s1, LPTSTR s2, int len2)
{
int flag = 0;
    for (; len2 && *s1 && *s2 ; len2--) {
	flag = *s1 - *s2;
	s1++;
	s2++;
    }
    return flag;
}

/* Get the directory for temporary files */
/* Store the result in buf */
/* Don't copy if length is greater than len characters */
BOOL 
get_temp(LPTSTR value, int len) 
{
TCHAR buf[256];
DWORD dwLength;
    /* If we run from the Windows NT SYSTEM account, many 
     * environment variables aren't set.  We need to look
     * in several places to find the name of a directory
     * for temporary files.
     */

    if (!len)
	return FALSE;
    value[0] = '\0';

    dwLength = GetEnvironmentVariable(TEXT("TEMP"), value, len);
    if (dwLength == 0)
        dwLength = GetEnvironmentVariable(TEXT("TMP"), value, len);

    if (dwLength == 0) {
	HKEY hkey;
	DWORD cbData, keytype;
	value[0] = '\0';
        if ( RegOpenKeyEx(HKEY_USERS, TEXT(".DEFAULT\\Environment"), 0, 
		KEY_ALL_ACCESS, &hkey) == ERROR_SUCCESS ) {
	    keytype = REG_SZ;
	    buf[0] = '\0';
	    cbData = sizeof(buf);
	    if (RegQueryValueEx(hkey, TEXT("TEMP"), 0, &keytype, 
		(LPBYTE)buf, &cbData) == ERROR_SUCCESS)
		dwLength = cbData;
	    if (dwLength == 0) {
		buf[0] = '\0';
		cbData = sizeof(buf);
		if (RegQueryValueEx(hkey, TEXT("TMP"), 0, &keytype, 
		    (LPBYTE)buf, &cbData) == ERROR_SUCCESS)
		    dwLength = cbData;
	    }
	    RegCloseKey(hkey);
	    if (dwLength) {
		/* Replace %USERPROFILE%, %SystemDrive% etc. */
		dwLength = ExpandEnvironmentStrings(buf, value, len);
	    }
	}
    }

    if (dwLength == 0)
	GetWindowsDirectory(value, len);
    if (dwLength)
	return TRUE;
    return FALSE;
}

/* create a temporary file.
 * If temporary filename is shorter than len, create the file
 * and store name in filename.
 * Return TRUE if success.
 */
int
create_tempfile(LPTSTR filename, DWORD len)
{
TCHAR temp[256];
TCHAR buf[256];
LPTSTR p;
int i;
HANDLE hf;

    get_temp(temp, sizeof(temp)/sizeof(TCHAR));

    if (lstrlen(temp) > 0) {
	p = temp + lstrlen(temp) - 1;
	if (*p != '\\')
	    lstrcat(temp, TEXT("\\"));
    }
    lstrcat(temp, TEXT("RedMon"));
    for (i=0; i<100000; i++) {
	wsprintf(buf, TEXT("%s%02d.%03d"), temp, i / 1000, i % 1000);
	if ((int)len < lstrlen(buf) + 1)
	    return FALSE;
        hf = CreateFile(buf, GENERIC_WRITE, 0 /* no sharing */, 
	    NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hf != INVALID_HANDLE_VALUE) {
	    CloseHandle(hf);
	    lstrcpy(filename, buf);
	    return TRUE;
	}
    }
    return FALSE;
}



/* True Win32 method, using OpenPrinter, WritePrinter etc. */
int 
redmon_printfile(REDATA * prd, TCHAR *filename)
{
HGLOBAL hbuffer;
BYTE *buffer;
DWORD cbRead;
HANDLE hread;

    if (prd->config.szPrinter[0] == '\0')
	return FALSE;

    /* allocate buffer for reading data */
    if ((hbuffer = GlobalAlloc(GPTR, (DWORD)PRINT_BUF_SIZE)) == NULL)
	return FALSE;
    if ((buffer = (BYTE *)GlobalLock(hbuffer)) == (BYTE *)NULL) {
	GlobalFree(hbuffer);
        return FALSE;
    }
	
    /* open file to print */
    if ((hread = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, 
	NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE) {
	if (prd->config.dwLogFileDebug) {
	    DWORD err = GetLastError();
	    TCHAR buf[256];
	    wsprintf(buf, TEXT("CreateFile() failed, error code = %d\r\n"), 
		err);
	    write_string_to_log(prd, buf);
	    write_error(prd, err);
	}
	GlobalUnlock(hbuffer);
	GlobalFree(hbuffer);
	return FALSE;
    }

    /* open a printer */
    if (!redmon_open_printer(prd)) {
	CloseHandle(hread);
	GlobalUnlock(hbuffer);
	GlobalFree(hbuffer);
	return FALSE;
    }

    while (ReadFile(hread, buffer, PRINT_BUF_SIZE, &cbRead, NULL)
		&& cbRead) {
	if (!redmon_write_printer(prd, buffer, cbRead)) {
	    CloseHandle(hread);
	    redmon_abort_printer(prd);
	    return FALSE;
	}
    }
    CloseHandle(hread);
    GlobalUnlock(hbuffer);
    GlobalFree(hbuffer);

    redmon_close_printer(prd);

    return TRUE;
}



BOOL
redmon_open_printer(REDATA *prd)
{
    TCHAR buf[256];
    DOC_INFO_1 di;
    PRINTER_INFO_2 *prinfo;
    HGLOBAL hglobal;
    DWORD needed, count;
    BOOL fail = FALSE;

    prd->printer = INVALID_HANDLE_VALUE;
    if (lstrlen(prd->config.szPrinter) == 0)
	return FALSE;

    /* open a printer */
    if (!OpenPrinter(prd->config.szPrinter, &prd->printer, NULL)) {
	if (prd->config.dwLogFileDebug) {
	    DWORD err = GetLastError();
	    wsprintf(buf, 
	      TEXT("OpenPrinter() failed for \042%s\042, error code = %d\r\n"), 
		prd->config.szPrinter, err);
	    write_string_to_log(prd, buf);
	    write_error(prd, err);
	}
	return FALSE;
    }
    /* from here until ClosePrinter, should AbortPrinter on error */

    /* make sure we don't try to write to our own port! */
    needed = 0;
    GetPrinter(prd->printer, 2, NULL, 0, &needed);
    if (needed == 0)
	fail = TRUE;
    if (!fail) 
        if ((hglobal = GlobalAlloc(GPTR, (DWORD)needed)) == NULL)
	    fail = TRUE;
    if (!fail) 
	if ((prinfo = (PRINTER_INFO_2 *)GlobalLock(hglobal)) == 
		(PRINTER_INFO_2 *)NULL) {
	    GlobalFree(hglobal);
	    fail = TRUE;
	}
    if (!fail) {
	count = needed;
	if (!GetPrinter(prd->printer, 2, (LPBYTE)prinfo, count, &needed)) {
	    GlobalUnlock(hglobal);
	    GlobalFree(hglobal);
	    fail = TRUE;
	}
    }
    if (!fail) {
	if (lstrcmp(prinfo->pPortName, prd->portname) == 0) {
	  write_string_to_log(prd,
	    TEXT("Talking to yourself is a sign of madness.\r\n"));
	  write_string_to_log(prd,
	    TEXT("Writing to the port being redirected is not permitted.\r\n"));
	    fail = TRUE;
	}
	GlobalUnlock(hglobal);
	GlobalFree(hglobal);
    }
    if (fail) {
	redmon_abort_printer(prd);
	return FALSE;
    }


    di.pDocName = prd->pDocName;	/* keep original document name */
    di.pOutputFile = NULL;
    di.pDatatype = TEXT("RAW");	/* for available types see EnumPrintProcessorDatatypes */
    if (!StartDocPrinter(prd->printer, 1, (LPBYTE)&di)) {
	if (prd->config.dwLogFileDebug) {
	    DWORD err = GetLastError();
	    wsprintf(buf, 
		TEXT("StartDocPrinter() failed, error code = %d\r\n"), 
		err);
	    write_string_to_log(prd, buf);
	    write_error(prd, err);
	}
	redmon_abort_printer(prd);
	return FALSE;
    }
    return TRUE;
}

BOOL
redmon_abort_printer(REDATA *prd)
{
    if (prd->config.dwLogFileDebug)
	write_string_to_log(prd, TEXT("AbortPrinter\r\n"));
    if (prd->printer != INVALID_HANDLE_VALUE)
	AbortPrinter(prd->printer);
    prd->printer = INVALID_HANDLE_VALUE;
    return TRUE;
}

BOOL
redmon_close_printer(REDATA *prd)
{
    TCHAR buf[256];
    if (prd->printer == INVALID_HANDLE_VALUE)
	return TRUE;
    if (!EndDocPrinter(prd->printer)) {
	if (prd->config.dwLogFileDebug) {
	    DWORD err = GetLastError();
	    wsprintf(buf, 
		TEXT("EndDocPrinter() failed, error code = %d\r\n"), err);
	    write_string_to_log(prd, buf);
	    write_error(prd, err);
	}
	redmon_abort_printer(prd);
	return FALSE;
    }

    if (!ClosePrinter(prd->printer)) {
	if (prd->config.dwLogFileDebug) {
	    DWORD err = GetLastError();
	    wsprintf(buf, 
		TEXT("ClosePrinter() failed, error code = %d\r\n"), 
		err);
	    write_string_to_log(prd, buf);
	    write_error(prd, err);
	}
        prd->printer = INVALID_HANDLE_VALUE;
	return FALSE;
    }
    prd->printer = INVALID_HANDLE_VALUE;
    return TRUE;
}

BOOL
redmon_write_printer(REDATA *prd, BYTE *ptr, DWORD len)
{
DWORD cbWritten;
    if (prd->printer == INVALID_HANDLE_VALUE)
	return FALSE;
    if (!WritePrinter(prd->printer, (LPVOID)ptr, len, &cbWritten)) {
	TCHAR buf[256];
	DWORD err = GetLastError();
	wsprintf(buf, 
	    TEXT("WritePrinter() failed, error code = %d\r\n"), err);
	write_string_to_log(prd, buf);
	write_error(prd, err);
	return FALSE;
    }
    if (len != cbWritten) {
	TCHAR buf[256];
	wsprintf(buf, 
	    TEXT("WritePrinter() error: wrote %d bytes of %d\r\n"), 
	    cbWritten, len);
	write_string_to_log(prd, buf);
    }
    prd->printer_bytes += cbWritten;
    return TRUE;
}


BOOL CALLBACK
AbortProc(HDC hdcPrn, int code)
{
    if (code == SP_OUTOFDISK)
	return FALSE;	/* cancel job */
    return TRUE;
}

/* This may be called if an error has occurred,
 * to print out the log file as an error report.
 */
BOOL 
redmon_print_error(REDATA *prd)
{
    HDC hdc;
    TCHAR driverbuf[2048];
    TCHAR *device, *driver, *output, *p;
    DOCINFO di;
    HFONT hfont;
    HANDLE hfile;
    LOGFONT lf;
    CHAR line[128];
    int xdpi, ydpi;
    int x, y, ymin, ymax, yskip;
    BOOL error = FALSE;
    

    /* open printer */
    device = prd->config.szPrinter;
    GetProfileString(TEXT("Devices"), device, TEXT(""), driverbuf, 
	sizeof(driverbuf) / sizeof(TCHAR));
    p = driverbuf;
    driver = p;
    while (*p && *p != ',')
	p++;
    if (*p)
	*p++ = '\0';
    output = p;
    while (*p && *p != ',')
	p++;
    if (*p)
	*p++ = '\0';
 
    if ( (hdc = CreateDC(driver, device, NULL, NULL)) == NULL) {
	return FALSE;
    }

    SetAbortProc(hdc, AbortProc);
    di.cbSize = sizeof(di);
    di.lpszDocName = TEXT("RedMon error report");
    di.lpszOutput = output;
    if ((error = (StartDoc(hdc, &di) == SP_ERROR)) != 0) {
	TCHAR buf[1024];
	DWORD last_error = GetLastError();
	wsprintf(buf, 
	    TEXT("StartDoc \042%s\042,\042%s\042,\042%s\042 Error=%d"),
	    device, di.lpszDocName, di.lpszOutput, last_error);
	MessageBox(HWND_DESKTOP, buf, TEXT("RedMon"), MB_OK);
    }
	
    if (!error && (StartPage(hdc) != SP_ERROR)) {
	xdpi = GetDeviceCaps(hdc, LOGPIXELSX);
	ydpi = GetDeviceCaps(hdc, LOGPIXELSY);
	memset(&lf, 0, sizeof(LOGFONT));
	lf.lfHeight = 10 * ydpi / 72;	/* 10 pts high */
	lstrcpy(lf.lfFaceName, TEXT("Arial"));
	hfont = CreateFontIndirect(&lf);
	SelectObject(hdc, hfont);

	ymin = 36 * ydpi / 72;	/* top of page */
	ymax = 756 * ydpi / 72;	/* bottom of page */
	yskip = 10 * ydpi / 72;	/* line spacing */
	x = 36 * xdpi / 72;		/* left margin */
	y = ymin;

	/* write RedMon header */
	p = TEXT("RedMon error report");
	TextOut(hdc, x, y, p, lstrlen(p));
	y+=yskip;
	lstrcpy(driverbuf, copyright);
	p = driverbuf + lstrlen(driverbuf) - 1;
	if (*p == '\n')
	    *p = '\0';
	p = driverbuf;
	TextOut(hdc, x, y, p, lstrlen(p));
	y+=yskip;
	lstrcpy(driverbuf, version);
	p = driverbuf + lstrlen(driverbuf) - 1;
	if (*p == '\n')
	    *p = '\0';
	p = driverbuf;
	TextOut(hdc, x, y, p, lstrlen(p));
	y+=yskip;
	p = driverbuf;
	wsprintf(driverbuf, TEXT("  Port=\042%s\042  Printer=\042%s\042"), 
	    prd->portname, prd->pPrinterName);
	TextOut(hdc, x, y, p, lstrlen(p));
	y+=yskip;
	wsprintf(driverbuf, TEXT("  DocumentName=\042%s\042"), prd->pDocName);
	TextOut(hdc, x, y, p, lstrlen(p));
	y+=yskip;
/*
	wsprintf(driverbuf, TEXT("  Command=\042%s\042), prd->command);
	TextOut(hdc, x, y, p, lstrlen(p));
	y+=yskip;
*/
	y+=yskip;


	if (prd->config.szLogFileName[0] != '\0') {
	    /* log file was written (but isn't currently open) */
	    /* open log file */
	    if ((hfile = CreateFile(prd->config.szLogFileName, GENERIC_READ, 
		FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) 
		!= INVALID_HANDLE_VALUE) {
		/* copy log file to printer */
		int i = 0;
		DWORD dwRead = 0;
		BOOL eol = FALSE; 
		while (ReadFile(hfile, &line[i], 1, &dwRead, NULL) &&
		    (dwRead == 1)) {
		    eol = FALSE;
		    switch (line[i]) {
			case '\r':
			    /* do nothing */
			    break;
			case '\n':
			    /* end of line */
			    eol = TRUE;
			    line[i]='\0';
			    break;
			default:
			    i++;
			    if (i > 80) {
				eol = TRUE;
				line[i]='\0';
			    }
		    }

		    if (eol) {
			if (y >= ymax) {
			    EndPage(hdc);
			    StartPage(hdc);
			    y = ymin;
			}
			TextOutA(hdc, x, y, line, i);
			y+=yskip;
			i = 0;
		    }
		    dwRead = 0;
		}
		if (i)
		    TextOutA(hdc, x, y, line, i);
		CloseHandle(hfile);
	    }
	}
	/* close printer */
	EndPage(hdc);
	EndDoc(hdc);
    }

    DeleteDC(hdc);
    return TRUE;
}


/* Add Port dialog box */
LRESULT CALLBACK 
AddDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
#if defined(_WIN64) || defined(GWLP_USERDATA)
    RECONFIG *pconfig = (RECONFIG *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
#else
    RECONFIG *pconfig = (RECONFIG *)GetWindowLong(hDlg, GWL_USERDATA);
#endif
    switch(message) {
        case WM_INITDIALOG:
	    /* save pointer to port name */
	    pconfig = (RECONFIG *)lParam;
#if defined(_WIN64) || defined(GWLP_USERDATA)
	    SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)lParam);
#else
	    SetWindowLong(hDlg, GWL_USERDATA, (LONG)lParam);
#endif
	    SetDlgItemText(hDlg, IDC_PORTNAME, pconfig->szPortName);
	    SetForegroundWindow(hDlg);
            return( TRUE);
        case WM_COMMAND:
            switch(LOWORD(wParam)) {
	      case IDC_HELPBUTTON:
		  show_help(hDlg, IDS_HELPADD);
                  return(TRUE);
              case IDC_PORTNAME:
                  return(TRUE);
	      case IDOK:
	        GetDlgItemText(hDlg, IDC_PORTNAME, pconfig->szPortName,
			MAXSHORTSTR-1);
		EndDialog(hDlg, TRUE);
		return(TRUE);
	      case IDCANCEL:
                EndDialog(hDlg, FALSE);
                return(TRUE);
              default:
                return(FALSE);
            }
        default:
            return(FALSE);
    }
}

/* Log file dialog box */
LRESULT CALLBACK 
LogfileDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message) {
        case WM_INITDIALOG:
	    {
	    RECONFIG *config;
	    TCHAR buf[MAXSTR];
	    TCHAR str[MAXSTR];

	    /* save pointer to configuration data */
#if defined(_WIN64) || defined(GWLP_USERDATA)
	    SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)lParam);
#else
	    SetWindowLong(hDlg, GWL_USERDATA, (LONG)lParam);
#endif

	    config = (RECONFIG *)lParam;

	    SetDlgItemText(hDlg, IDC_LOGNAME, config->szLogFileName);
	    SendDlgItemMessage(hDlg, IDC_LOGDEBUG, BM_SETCHECK, 
		config->dwLogFileDebug, 0);
	    SendDlgItemMessage(hDlg, IDC_LOGUSE, BM_SETCHECK, 
		config->dwLogFileUse, 0);

	    EnableWindow(GetDlgItem(hDlg, IDC_LOGNAMEPROMPT), 
		config->dwLogFileUse);
	    EnableWindow(GetDlgItem(hDlg, IDC_LOGNAME), 
		config->dwLogFileUse);
	    EnableWindow(GetDlgItem(hDlg, IDC_LOGDEBUG), 
		config->dwLogFileUse);
	    EnableWindow(GetDlgItem(hDlg, IDC_BROWSE), 
		config->dwLogFileUse);

	    LoadString(hdll, IDS_CONFIGLOGFILE, str, 
		sizeof(str)/sizeof(TCHAR) - 1);
	    wsprintf(buf, str, config->szPortName);
	    SetWindowText(hDlg, buf);
	    }
            return( TRUE);
        case WM_COMMAND:
            switch(LOWORD(wParam)) {
	      case IDC_HELPBUTTON:
		  show_help(hDlg, IDS_HELPLOG);
                  return(TRUE);
	      case IDC_BROWSE:
		  browse(hDlg, IDC_LOGNAME, IDS_FILTER_TXT, TRUE);
                  return(TRUE);
	      case IDC_LOGUSE:
		  if (HIWORD(wParam) == BN_CLICKED) {
		    BOOL enabled = (BOOL)SendDlgItemMessage(hDlg, 
				IDC_LOGUSE, BM_GETCHECK, 0, 0);
		    enabled = !enabled;
		    SendDlgItemMessage(hDlg, IDC_LOGUSE, 
			BM_SETCHECK, enabled, 0);
		    EnableWindow(GetDlgItem(hDlg, IDC_LOGNAMEPROMPT), enabled);
		    EnableWindow(GetDlgItem(hDlg, IDC_LOGNAME), enabled);
		    EnableWindow(GetDlgItem(hDlg, IDC_LOGDEBUG), enabled);
		    EnableWindow(GetDlgItem(hDlg, IDC_BROWSE), enabled);
		   }
		   return TRUE;
	      case IDC_LOGFILE:
                    return(TRUE);
/* should we fall through to IDOK */
	      case IDOK:
		{
#if defined(_WIN64) || defined(GWLP_USERDATA)
		    RECONFIG *config = 
			    (RECONFIG *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
#else
		    RECONFIG *config = 
			    (RECONFIG *)GetWindowLong(hDlg, GWL_USERDATA);
#endif
		    config->dwLogFileUse = (BOOL)SendDlgItemMessage(hDlg, 
			IDC_LOGUSE, BM_GETCHECK, 0, 0);
		    if (config->dwLogFileUse) {
		      GetDlgItemText(hDlg, IDC_LOGNAME, config->szLogFileName, 
		    	sizeof(config->szLogFileName)/sizeof(TCHAR) - 1);
		      config->dwLogFileDebug = SendDlgItemMessage(hDlg, 
			IDC_LOGDEBUG, BM_GETCHECK, 0, 0);
		    }
		}
		EndDialog(hDlg, TRUE);
		return(TRUE);
	      case IDCANCEL:
                EndDialog(hDlg, FALSE);
                return(TRUE);
              default:
                return(FALSE);
            }
        default:
            return(FALSE);
    }
}

/* setup up combo box with list of printers */
void
init_printer_list(HWND hDlg, LPTSTR portname)
{
unsigned int i;
DWORD count, needed;
PRINTER_INFO_2 *prinfo;
LPTSTR enumbuffer;
HGLOBAL hglobal;
BOOL fail = FALSE;

    /* enumerate all available printers */
    needed = 0;
    EnumPrinters(PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL, 
	NULL, 2, NULL, 0, &needed, &count);
    if (needed == 0)
	fail = TRUE;
    if (!fail) 
        if ((hglobal = GlobalAlloc(GPTR, (DWORD)needed)) == NULL)
	    fail = TRUE;
    if (!fail) 
	if ((enumbuffer = (LPTSTR)GlobalLock(hglobal)) == (LPTSTR)NULL) {
	    GlobalFree(hglobal);
	    fail = TRUE;
	}

    if (!fail)
	if (!EnumPrinters(PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL, 
	    NULL, 2, (LPBYTE)enumbuffer, needed, &needed, &count)) {
	    GlobalUnlock(hglobal);
	    GlobalFree(hglobal);
	    fail = TRUE;
	}

    if (fail) {
	EnableWindow(GetDlgItem(hDlg, IDC_PRINTER), FALSE);
    }
    else {
	prinfo = (PRINTER_INFO_2 *)enumbuffer;
	SendDlgItemMessage(hDlg, IDC_PRINTER, CB_RESETCONTENT, 
	    (WPARAM)0, (LPARAM)0);
	for (i=0; i<count; i++) {
	    if (lstrcmp(prinfo[i].pPortName, portname) != 0)
	      SendDlgItemMessage(hDlg, IDC_PRINTER, CB_ADDSTRING, 0, 
		(LPARAM)(prinfo[i].pPrinterName));
	}
	GlobalUnlock(hglobal);
	GlobalFree(hglobal);
    }
}


/* Configure Port dialog box */
LRESULT CALLBACK 
ConfigDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message) {
        case WM_INITDIALOG:
	    {
	    RECONFIG *config;
	    TCHAR buf[MAXSTR];
	    TCHAR str[MAXSTR];
#ifdef OLD
	    HKEY hkey;
	    DWORD cbData, keytype;
	    DWORD show;
	    DWORD output;
	    DWORD runuser;
	    DWORD delay;
	    DWORD printerror;
#endif
	    int i;

	    /* save pointer to configuration data */
#if defined(_WIN64) || defined(GWLP_USERDATA)
	    SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)lParam);
#else
	    SetWindowLong(hDlg, GWL_USERDATA, (LONG)lParam);
#endif
	    config = (RECONFIG *)lParam;

	    SetDlgItemText(hDlg, IDC_COMMAND, config->szCommand);
	    SetDlgItemText(hDlg, IDC_ARGS, config->szArguments);

	    SendDlgItemMessage(hDlg, IDC_OUTPUT, CB_RESETCONTENT,
		(WPARAM)0, (LPARAM)0);
	    for (i=0; i<=OUTPUT_LAST; i++) {
		LoadString(hdll, IDS_OUTPUTBASE + i, buf, 
		    sizeof(buf)/sizeof(TCHAR) - 1);
		SendDlgItemMessage(hDlg, IDC_OUTPUT, CB_ADDSTRING,
		    (WPARAM)0, (LPARAM)buf);
	    }
	    SendDlgItemMessage(hDlg, IDC_OUTPUT, CB_SETCURSEL,
		(WPARAM)config->dwOutput, (LPARAM)0);

	    init_printer_list(hDlg, config->szPortName);
	    if (SendDlgItemMessage(hDlg, IDC_PRINTER, CB_SELECTSTRING, -1, 
	        (LPARAM)(config->szPrinter)) == CB_ERR)
	        SendDlgItemMessage(hDlg, IDC_PRINTER, CB_SETCURSEL, 0, 0);

	    EnableWindow(GetDlgItem(hDlg, IDC_PRINTER), 
	      ((config->dwOutput == OUTPUT_STDOUT) 
		|| (config->dwOutput == OUTPUT_FILE) 
		|| (config->dwOutput == OUTPUT_HANDLE)) );
	    EnableWindow(GetDlgItem(hDlg, IDC_PRINTERTEXT), 
	      ((config->dwOutput == OUTPUT_STDOUT) 
		|| (config->dwOutput == OUTPUT_FILE) 
		|| (config->dwOutput == OUTPUT_HANDLE)) );

	    SendDlgItemMessage(hDlg, IDC_SHOW, CB_RESETCONTENT,
		(WPARAM)0, (LPARAM)0);
	    for (i=0; i<=SHOW_HIDE; i++) {
		LoadString(hdll, IDS_SHOWBASE + i, buf, 
		    sizeof(buf)/sizeof(TCHAR) - 1);
		SendDlgItemMessage(hDlg, IDC_SHOW, CB_ADDSTRING,
		    (WPARAM)0, (LPARAM)buf);
	    }
	    SendDlgItemMessage(hDlg, IDC_SHOW, CB_SETCURSEL,
		(WPARAM)config->dwShow, (LPARAM)0);

	    SendDlgItemMessage(hDlg, IDC_RUNUSER, BM_SETCHECK,
		(WPARAM)config->dwRunUser, (LPARAM)0);
#ifndef UNICODE
	    EnableWindow(GetDlgItem(hDlg, IDC_RUNUSER), FALSE);
#endif
#if (defined(NT40) || defined(NT50)) && !defined(__BORLANDC__)
	    if (ntver >= 400)
	        EnableWindow(GetDlgItem(hDlg, IDC_RUNUSER), TRUE);
#endif

	    SetDlgItemInt(hDlg, IDC_DELAY, config->dwDelay, FALSE);


	    SendDlgItemMessage(hDlg, IDC_PRINTERROR, BM_SETCHECK, 
		config->dwPrintError, 0);

	    LoadString(hdll, IDS_CONFIGPROP, str, 
		sizeof(str)/sizeof(TCHAR) - 1);
	    wsprintf(buf, str, config->szPortName);
	    SetWindowText(hDlg, buf);
	    SetForegroundWindow(hDlg);
	    }
            return( TRUE);
        case WM_COMMAND:
            switch(LOWORD(wParam)) {
	      case IDC_HELPBUTTON:
		  show_help(hDlg, IDS_HELPCONFIG);
                  return(TRUE);
	      case IDC_BROWSE:
		  browse(hDlg, IDC_COMMAND, IDS_FILTER_EXE, FALSE);
                  return(TRUE);
	      case IDC_LOGFILE:
		  DialogBoxParam(hdll, MAKEINTRESOURCE(IDD_CONFIGLOG), hDlg, 
			LogfileDlgProc, 
#if defined(_WIN64) || defined(GWLP_USERDATA)
			(LPARAM)GetWindowLongPtr(hDlg, GWLP_USERDATA));
#else
			(LPARAM)GetWindowLong(hDlg, GWL_USERDATA));
#endif
                  return(TRUE);
	      case IDC_OUTPUT:
		  if (HIWORD(wParam) == CBN_SELCHANGE)  {
		      int i = SendDlgItemMessage(hDlg, IDC_OUTPUT, 
			CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
	      	      BOOL enabled = ((i == OUTPUT_STDOUT) || 
			    (i == OUTPUT_FILE) || (i == OUTPUT_HANDLE));
		      EnableWindow(GetDlgItem(hDlg, IDC_PRINTER), enabled); 
		      EnableWindow(GetDlgItem(hDlg, IDC_PRINTERTEXT), enabled); 
		      EnableWindow(GetDlgItem(hDlg, IDC_PRINTERROR), enabled); 
		  }
                  return(TRUE);
              case IDC_COMMAND:
              case IDC_ARGS:
                    return(TRUE);
/* should we fall through to IDOK */
	      case IDOK:
		{
		    BOOL success;
#if defined(_WIN64) || defined(GWLP_USERDATA)
		    RECONFIG *config = 
			    (RECONFIG *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
#else
		    RECONFIG *config = 
			    (RECONFIG *)GetWindowLong(hDlg, GWL_USERDATA);
#endif


		    GetDlgItemText(hDlg, IDC_COMMAND, config->szCommand, 
			sizeof(config->szCommand)/sizeof(TCHAR) - 1);
		    GetDlgItemText(hDlg, IDC_ARGS, config->szArguments, 
			sizeof(config->szArguments)/sizeof(TCHAR) - 1);
		    config->dwOutput = SendDlgItemMessage(hDlg, IDC_OUTPUT, 
			CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
		    GetDlgItemText(hDlg, IDC_PRINTER, config->szPrinter, 
			sizeof(config->szPrinter)/sizeof(TCHAR) - 1);
		    config->dwShow = SendDlgItemMessage(hDlg, IDC_SHOW, 
			CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
		    config->dwRunUser = (BOOL)SendDlgItemMessage(hDlg, 
			IDC_RUNUSER, BM_GETCHECK, 0, 0);
		    config->dwDelay = (DWORD)GetDlgItemInt(hDlg, IDC_DELAY, 
			&success, FALSE);
 		    if (!success)
			config->dwDelay = DEFAULT_DELAY;
		    if ((config->dwOutput == OUTPUT_STDOUT) 
			|| (config->dwOutput == OUTPUT_FILE) 
			|| (config->dwOutput == OUTPUT_HANDLE)) {
			config->dwPrintError = SendDlgItemMessage(hDlg, 
			    IDC_PRINTERROR, BM_GETCHECK, 0, 0);
		    }
		}
		EndDialog(hDlg, TRUE);
		return(TRUE);
	      case IDCANCEL:
                EndDialog(hDlg, FALSE);
                return(TRUE);
              default:
                return(FALSE);
            }
        default:
            return(FALSE);
    }
}

BOOL
get_job_info(REDATA *prd)
{
HGLOBAL hPrinter;
    if (OpenPrinter(prd->pPrinterName, &hPrinter, NULL)) {
	DWORD dwNeeded = 0;
	HGLOBAL hglobal = GlobalAlloc(GPTR, (DWORD)4096);
	JOB_INFO_1 *pjob = (JOB_INFO_1 *)GlobalLock(hglobal);
	if ((pjob != (JOB_INFO_1 *)NULL) && 
	     GetJob(hPrinter, prd->JobId, 1, (LPBYTE)pjob, 
		4096, &dwNeeded)) {
	    lstrcpyn(prd->pMachineName, pjob->pMachineName, 
		sizeof(prd->pMachineName)/sizeof(TCHAR)-1);
	    lstrcpyn(prd->pUserName, pjob->pUserName, 
		sizeof(prd->pUserName)/sizeof(TCHAR)-1);
	}
	if (pjob != (JOB_INFO_1 *)NULL) {
	    GlobalUnlock(hglobal);
	    GlobalFree(hglobal);
	}
	ClosePrinter(hPrinter);
	return TRUE;
    }
    return FALSE;
}

/* return length of env in characters (which may not be bytes) */
int
env_length(LPTSTR env)
{
LPTSTR p;
    p = env;
    while (*p) {
	while (*p)
	    p++;
	p++;
    }
    p++;
    return (p - env);
}

/* join two environment blocks together, returning a handle
 * to a new environment block.  The caller must free this with
 * GlobalFree() when it is finished.
 */
HGLOBAL
join_env(LPTSTR env1, LPTSTR env2)
{
int len1, len2;
HGLOBAL henv;
LPTSTR env;
    len1 = env_length(env1);	/* length in characters, not bytes */
    len2 = env_length(env2);
    henv = GlobalAlloc(GPTR, (DWORD)((len1 + len2)*sizeof(TCHAR)));  
    env = GlobalLock(henv);
    if (env == NULL)
	return NULL;
    MoveMemory(env, env1, len1*sizeof(TCHAR));
    env += len1 - 1;
    MoveMemory(env, env2, len2*sizeof(TCHAR)); 
    GlobalUnlock(henv);
    return henv;
}

/* Append a string to an environment block.
 * It is assumed that the environment block has sufficient
 * space.
 *  env is the environment block.
 *  name is the environment variable name, which includes a trailing '='.
 *  len is the length of name in bytes, including the trailing null.
 *  value is the environment variable value.
 */
void
append_env(LPTSTR env, LPTSTR name, int len, LPTSTR value)
{
int oldlen;
  oldlen = env_length(env);
  env = env + oldlen - 1;
  MoveMemory(env, name, len);
  env += len/sizeof(TCHAR) - 1;
  MoveMemory(env, value, lstrlen(value)*sizeof(TCHAR));
  env += lstrlen(value) + 1;
  *env = '\0';
}

/* create an environment variable block which contains
 * some RedMon extras about the print job.
 */
HGLOBAL make_job_env(REDATA *prd)
{
int len;
TCHAR buf[32];
TCHAR temp[256];
HGLOBAL henv;
LPTSTR env;
BOOL bTEMP = FALSE;
BOOL bTMP = FALSE;

    wsprintf(buf, TEXT("%d"), prd->JobId);
    len = sizeof(REDMON_PORT) + 
          sizeof(REDMON_JOB) + 
          sizeof(REDMON_PRINTER) + 
          sizeof(REDMON_MACHINE) + 
          sizeof(REDMON_USER) + 
          sizeof(REDMON_DOCNAME) + 
          sizeof(REDMON_FILENAME) + 
          sizeof(REDMON_SESSIONID) + 
          (lstrlen(prd->portname) + 
          lstrlen(buf) + 1 +
          lstrlen(prd->pPrinterName) + 1 +
          lstrlen(prd->pMachineName) + 1 +
          lstrlen(prd->pUserName) + 1 +
          lstrlen(prd->pDocName) + 1 +
          lstrlen(prd->tempname) + 1 +
          lstrlen(prd->pSessionId) + 1 +
	  1) * sizeof(TCHAR);

    if (GetEnvironmentVariable(TEXT("TEMP"), temp, 
	sizeof(temp)/sizeof(TCHAR)-1) == 0)
	bTEMP = TRUE;	/* Need to define TEMP */
    if (GetEnvironmentVariable(TEXT("TMP"), temp, 
	sizeof(temp)/sizeof(TCHAR)-1) == 0)
	bTMP = TRUE;	/* Need to define TMP */
    get_temp(temp, sizeof(temp)/sizeof(TCHAR));
    if (bTEMP)
	len += sizeof(REDMON_TEMP) + (lstrlen(temp) + 1) * sizeof(TCHAR);
    if (bTMP)
	len += sizeof(REDMON_TMP) + (lstrlen(temp) + 1) * sizeof(TCHAR);

    henv = GlobalAlloc(GPTR, len);
    env = GlobalLock(henv);
    if (env == NULL)
	return NULL;
    append_env(env, REDMON_PORT, sizeof(REDMON_PORT), prd->portname);
    append_env(env, REDMON_JOB, sizeof(REDMON_JOB), buf);
    append_env(env, REDMON_PRINTER, sizeof(REDMON_PRINTER), prd->pPrinterName);
    append_env(env, REDMON_MACHINE, sizeof(REDMON_MACHINE), prd->pMachineName);
    append_env(env, REDMON_USER, sizeof(REDMON_USER), prd->pUserName);
    append_env(env, REDMON_DOCNAME, sizeof(REDMON_DOCNAME), prd->pDocName);
    append_env(env, REDMON_FILENAME, sizeof(REDMON_FILENAME), prd->tempname);
    append_env(env, REDMON_SESSIONID, sizeof(REDMON_SESSIONID), prd->pSessionId);
    if (bTEMP)
	append_env(env, REDMON_TEMP, sizeof(REDMON_TEMP), temp);
    if (bTMP)
	append_env(env, REDMON_TMP, sizeof(REDMON_TMP), temp);
    GlobalUnlock(henv);
    return henv;
}



/* write contents of the environment block to the log file */
void
dump_env(REDATA *prd, LPTSTR env)
{
LPTSTR name, next;
    write_string_to_log(prd, TEXT("Environment:\n  "));
    next = env;
    while (*next) {
        name = next;
	while (*next)
	    next++;
	write_string_to_log(prd, name);
	write_string_to_log(prd, TEXT("\n  "));
	next++;
    }
    write_string_to_log(prd, TEXT("\n"));
}

BOOL make_env(REDATA * prd)
{
LPTSTR env, extra_env;
HGLOBAL h_extra_env;
    /* Add some environment variables */
    /* It would be simpler to use SetEnvironmentVariable()
     * and then GetEnvironmentStrings(), then to delete
     * the environment variables we added, but this could
     * cause problems if two RedMon ports simultaneously
     * did this.
     */
    env = GetEnvironmentStrings();
    h_extra_env= make_job_env(prd);
    extra_env = GlobalLock(h_extra_env);
    prd->environment = join_env(env, extra_env);
    GlobalUnlock(h_extra_env);
    GlobalFree(h_extra_env);
    FreeEnvironmentStrings(env);

    if (prd->hLogFile != INVALID_HANDLE_VALUE) {
        env = GlobalLock(prd->environment);
	dump_env(prd, env);
	GlobalUnlock(prd->environment);
    }
    return TRUE;
}

#ifdef UNICODE
/* Query session id from a primary token obtained from
 * the impersonation token of the thread.
 * Session id is needed to identify client a WTS session.
 */
BOOL query_session_id(REDATA * prd)
{
    BOOL fRet = TRUE;
    HANDLE htoken = NULL, hduptoken = NULL;
    TCHAR buf[MAXSTR];

    /* get impersonation token */
    if ( !(fRet = OpenThreadToken(GetCurrentThread() , 
	TOKEN_DUPLICATE | TOKEN_IMPERSONATE, 
	TRUE, &htoken)) ) {
	DWORD err = GetLastError();
	wsprintf(buf, TEXT("OpenThreadToken failed, error code=%d\r\n"),
	    err);
	write_string_to_log(prd, buf);
	write_error(prd, err);
	lstrcpy(prd->pSessionId, TEXT("0"));
    }

    if (fRet) {
	/* Duplicate it to create a primary token */
	if ( !(fRet = DuplicateTokenEx(htoken, TOKEN_ALL_ACCESS, NULL, 
	       SecurityImpersonation, TokenPrimary, &hduptoken)) ) {
	    DWORD err = GetLastError();
	    wsprintf(buf, TEXT("DuplicateTokenEx failed, error code=%d\r\n"),
	    err);
	    write_string_to_log(prd, buf);
	    write_error(prd, err);
	    lstrcpy(prd->pSessionId, TEXT("0"));
	}
        CloseHandle(htoken);
    }

    if ( fRet ) {
	DWORD dwRetLen = 0;
	DWORD dwSessionId = 0;
	/* query session-id from token */
	fRet = GetTokenInformation(hduptoken, TokenSessionId, 
			   &dwSessionId, sizeof (DWORD), &dwRetLen);
	if (fRet)
	    wsprintf(prd->pSessionId, TEXT("%ld"), dwSessionId);
        CloseHandle(hduptoken);
    }

    return(fRet);
}
#else
BOOL query_session_id(REDATA * prd)
{
    lstrcpy(prd->pSessionId, TEXT("0"));
    return TRUE;
}
#endif


/* start_redirect() was originally based on an example in the Win32 SDK
 * which used GetStdHandle() and SetStdHandle() to redirect stdio.
 * The example works under Windows 95, but not under NT.
 * For NT, we need to use 
 *  siStartInfo.dwFlags = STARTF_USESTDHANDLES;
 *  siStartInfo.hStdInput = prd->hChildStdinRd;
 *  siStartInfo.hStdOutput = prd->hChildStdoutWr;
 *  siStartInfo.hStdError = prd->hChildStderrWr;
 * The SDK example does NOT include these.  Most strange for an 
 * example that was written before Windows 95 existed!
 * STARTF_USESTDHANDLES also works for Windows 95, so the original 
 * code is commented out with #ifdef SAVESTD / #endif
 */

/* Start child program with redirected standard input and output */
BOOL start_redirect(REDATA * prd)
{
    SECURITY_ATTRIBUTES saAttr;
    STARTUPINFO siStartInfo;
    HANDLE hPipeTemp;
    LPVOID env;

    /* Set the bInheritHandle flag so pipe handles are inherited. */
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

#ifdef SAVESTD
    /* Save the current standard handles . */
    prd->hSaveStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    prd->hSaveStderr = GetStdHandle(STD_ERROR_HANDLE);
    prd->hSaveStdin = GetStdHandle(STD_INPUT_HANDLE);
#endif

    /* Create anonymous inheritable pipes for STDIN, STDOUT and STDERR
     * for child. For each pipe, create a noninheritable duplicate handle
     * of our end of the pipe, then close the inheritable handle.
     */
    if (!CreatePipe(&prd->hChildStdinRd, &hPipeTemp, &saAttr, 0))
	return FALSE;
    if (!DuplicateHandle(GetCurrentProcess(), hPipeTemp,
            GetCurrentProcess(), &prd->hChildStdinWr, 0,
            FALSE,       /* not inherited */
            DUPLICATE_SAME_ACCESS)) {
        CloseHandle(hPipeTemp);
	return FALSE;
    }
    CloseHandle(hPipeTemp);

    if (!CreatePipe(&hPipeTemp, &prd->hChildStdoutWr, &saAttr, 0))
	return FALSE;	/* cleanup of pipes will occur in caller */
    if (!DuplicateHandle(GetCurrentProcess(), hPipeTemp,
            GetCurrentProcess(), &prd->hChildStdoutRd, 0,
            FALSE,       /* not inherited */
            DUPLICATE_SAME_ACCESS)) {
        CloseHandle(hPipeTemp);
	return FALSE;
    }
    CloseHandle(hPipeTemp);

    if (!CreatePipe(&hPipeTemp, &prd->hChildStderrWr, &saAttr, 0))
	return FALSE;
    if (!DuplicateHandle(GetCurrentProcess(), hPipeTemp,
            GetCurrentProcess(), &prd->hChildStderrRd, 0,
            FALSE,       /* not inherited */
            DUPLICATE_SAME_ACCESS)) {
        CloseHandle(hPipeTemp);
	return FALSE;
    }
    CloseHandle(hPipeTemp);

#ifdef SAVESTD
    if (!SetStdHandle(STD_OUTPUT_HANDLE, prd->hChildStdoutWr))
	return FALSE;
    if (!SetStdHandle(STD_ERROR_HANDLE, prd->hChildStderrWr))
	return FALSE;
    if (!SetStdHandle(STD_INPUT_HANDLE, prd->hChildStdinRd))
	return FALSE;
#endif

    /* Now create the child process. */

    /* Set up members of STARTUPINFO structure. */

    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.lpReserved = NULL;
    siStartInfo.lpDesktop = NULL;
    siStartInfo.lpTitle = NULL;  /* use executable name as title */
    siStartInfo.dwX = siStartInfo.dwY = CW_USEDEFAULT;		/* ignored */
    siStartInfo.dwXSize = siStartInfo.dwYSize = CW_USEDEFAULT;	/* ignored */
    siStartInfo.dwXCountChars = 80;
    siStartInfo.dwYCountChars = 25;
    siStartInfo.dwFillAttribute = 0;			/* ignored */
    siStartInfo.dwFlags = STARTF_USESTDHANDLES;
    siStartInfo.wShowWindow = SW_SHOWNORMAL;		/* ignored */
    if (prd->config.dwShow != SHOW_NORMAL)
        siStartInfo.dwFlags |= STARTF_USESHOWWINDOW;
    if (prd->config.dwShow == SHOW_MIN)
        siStartInfo.wShowWindow = SW_SHOWMINNOACTIVE;	
    else if (prd->config.dwShow == SHOW_HIDE)
        siStartInfo.wShowWindow = SW_HIDE;	
    siStartInfo.cbReserved2 = 0;
    siStartInfo.lpReserved2 = NULL;
    siStartInfo.hStdInput = prd->hChildStdinRd;
    siStartInfo.hStdOutput = prd->hChildStdoutWr;
    siStartInfo.hStdError = prd->hChildStderrWr;

    if (prd->environment)
        env = GlobalLock(prd->environment);
    else
	env = NULL;

    /* Create the child process. */

#if defined(UNICODE) && (defined(NT40) || defined(NT50)) && !defined(__BORLANDC__)
    if (prd->config.dwRunUser) {
	BOOL flag;
	HANDLE htoken;
	TCHAR buf[MAXSTR];
        /* CreateProcessAsUser is only supported by NT3.51 and later. */
	/* get impersonation token of current thread */
	if ( !(flag = OpenThreadToken(GetCurrentThread() , 
	    TOKEN_DUPLICATE | TOKEN_IMPERSONATE, 
	    TRUE, &htoken)) ) {
	    DWORD err = GetLastError();
	    wsprintf(buf, TEXT("OpenThreadToken failed, error code=%d\r\n"),
		err);
	    write_string_to_log(prd, buf);
	    write_error(prd, err);
	}
	/* Duplicate it to create a primary token */
	if ( !(flag = DuplicateTokenEx(htoken, TOKEN_ALL_ACCESS, NULL, 
	       SecurityImpersonation, TokenPrimary, &prd->primary_token)) ) {
	    DWORD err = GetLastError();
	    wsprintf(buf, TEXT("DuplicateTokenEx failed, error code=%d\r\n"),
		err);
	    write_string_to_log(prd, buf);
	    write_error(prd, err);
	}
        CloseHandle(htoken);

	/* Be default, the process is created on an invisible desktop.
	 * that can't receive input.
	 * Instead we try to put the process on the main desktop.
	 * We need to change the DACL of the desktop, but I don't know
	 * how to do this yet, so the desktop code is disabled.
	 */
	/*
        siStartInfo.lpDesktop = "winsta0\\desktop";
	*/
	if ( !(flag = CreateProcessAsUser(prd->primary_token, NULL,
		prd->command,  /* command line                       */
		NULL,          /* process security attributes        */
		NULL,          /* primary thread security attributes */
		TRUE,          /* handles are inherited              */
		CREATE_UNICODE_ENVIRONMENT,  /* creation flags       */
		env,           /* environment                        */
		NULL,          /* use parent's current directory     */
		&siStartInfo,  /* STARTUPINFO pointer                */
		&prd->piProcInfo))  /* receives PROCESS_INFORMATION  */
	   ) {
	    DWORD err = GetLastError();
	    wsprintf(buf, TEXT("CreateProcessAsUser failed, error code=%d\r\n"),
		err);
	    write_string_to_log(prd, buf);
	    write_error(prd, err);
	}
	
	if (!flag)
	   prd->config.dwRunUser = FALSE;
    }
    if (!prd->config.dwRunUser)
#endif

    if (!CreateProcess(NULL,
        prd->command,  /* command line                       */
        NULL,          /* process security attributes        */
        NULL,          /* primary thread security attributes */
        TRUE,          /* handles are inherited              */
#ifdef UNICODE
	CREATE_UNICODE_ENVIRONMENT,  /* creation flags       */
#else
	0,             /* creation flags                     */
#endif
	env,           /* environment                        */
        NULL,          /* use parent's current directory     */
        &siStartInfo,  /* STARTUPINFO pointer                */
        &prd->piProcInfo))  /* receives PROCESS_INFORMATION  */
	  return FALSE;

    /* After process creation, restore the saved STDIN and STDOUT. */

#ifdef SAVESTD
    if (!SetStdHandle(STD_INPUT_HANDLE, prd->hSaveStdin))
	return FALSE;
    if (!SetStdHandle(STD_OUTPUT_HANDLE, prd->hSaveStdout))
	return FALSE;
    if (!SetStdHandle(STD_ERROR_HANDLE, prd->hSaveStderr))
	return FALSE;
#endif

    return TRUE;
}

#if defined(UNICODE) && (defined(NT40) || defined(NT50)) && !defined(__BORLANDC__)
/* Create another process to get the filename.
 * This is needed to make the SaveAs dialog appear on the WTS client
 * instead of the server.
 */
BOOL get_filename_as_user(REDATA * prd)
{
    SECURITY_ATTRIBUTES saAttr;
    STARTUPINFO siStartInfo;
    HANDLE hPipeTemp;
    HANDLE hPipeWrite;
    HANDLE hPipeRead;
    LPVOID env;
    TCHAR command[MAXSTR];
    BOOL flag = TRUE;
    HANDLE htoken;
    TCHAR buf[MAXSTR];
    DWORD dwBytesRead;

    /* Set the bInheritHandle flag so pipe handles are inherited. */
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    /* Create anonymous inheritable pipes for retrieving filename
     * from user.  Create a noninheritable duplicate handle
     * of our end of the pipe, then close the inheritable handle.
     */

    if (!CreatePipe(&hPipeTemp, &hPipeWrite, &saAttr, 0))
	return FALSE;	/* cleanup of pipes will occur in caller */
    if (!DuplicateHandle(GetCurrentProcess(), hPipeTemp,
            GetCurrentProcess(), &hPipeRead, 0,
            FALSE,       /* not inherited */
            DUPLICATE_SAME_ACCESS)) {
        CloseHandle(hPipeTemp);
	return FALSE;
    }
    CloseHandle(hPipeTemp);


    wsprintf(command, TEXT("redmonfn.exe %lu %s"), hPipeWrite, prd->tempname);
    write_string_to_log(prd, command);
    write_string_to_log(prd, TEXT("\r\n"));

    /* Now create the child process. */

    /* Set up members of STARTUPINFO structure. */

    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.lpReserved = NULL;
    siStartInfo.lpDesktop = NULL;
    siStartInfo.lpTitle = NULL;  /* use executable name as title */
    siStartInfo.dwX = siStartInfo.dwY = CW_USEDEFAULT;		/* ignored */
    siStartInfo.dwXSize = siStartInfo.dwYSize = CW_USEDEFAULT;	/* ignored */
    siStartInfo.dwXCountChars = 80;
    siStartInfo.dwYCountChars = 25;
    siStartInfo.dwFillAttribute = 0;			/* ignored */
    siStartInfo.dwFlags = STARTF_USESTDHANDLES;
    siStartInfo.wShowWindow = SW_SHOWNORMAL;		/* ignored */
    siStartInfo.dwFlags |= STARTF_USESHOWWINDOW;
    siStartInfo.cbReserved2 = 0;
    siStartInfo.lpReserved2 = NULL;
    siStartInfo.hStdInput = NULL;
    siStartInfo.hStdOutput = NULL;
    siStartInfo.hStdError = NULL;

    if (prd->environment)
        env = GlobalLock(prd->environment);
    else
	env = NULL;

    /* Create the child process. */

    /* CreateProcessAsUser is only supported by NT3.51 and later. */
    /* get impersonation token of current thread */
    if ( !(flag = OpenThreadToken(GetCurrentThread() , 
	TOKEN_DUPLICATE | TOKEN_IMPERSONATE, 
	TRUE, &htoken)) ) {
	DWORD err = GetLastError();
	wsprintf(buf, TEXT("OpenThreadToken failed, error code=%d\r\n"),
	    err);
	write_string_to_log(prd, buf);
	write_error(prd, err);
    }
    /* Duplicate it to create a primary token */
    if ( !(flag = DuplicateTokenEx(htoken, TOKEN_ALL_ACCESS, NULL, 
	   SecurityImpersonation, TokenPrimary, &prd->primary_token)) ) {
	DWORD err = GetLastError();
	wsprintf(buf, TEXT("DuplicateTokenEx failed, error code=%d\r\n"),
	    err);
	write_string_to_log(prd, buf);
	write_error(prd, err);
    }
    CloseHandle(htoken);

    /* Be default, the process is created on an invisible desktop.
     * that can't receive input.
     * By settings lpDesktop to an empty string, the system will
     * try to use the existing desktop of the impersonated user.
     */
    siStartInfo.lpDesktop = TEXT("");
    if ( !(flag = CreateProcessAsUser(prd->primary_token, NULL,
	    command,  /* command line                       */
	    NULL,          /* process security attributes        */
	    NULL,          /* primary thread security attributes */
	    TRUE,          /* handles are inherited              */
	    CREATE_UNICODE_ENVIRONMENT,  /* creation flags       */
	    env,           /* environment                        */
	    NULL,          /* use parent's current directory     */
	    &siStartInfo,  /* STARTUPINFO pointer                */
	    &prd->piProcInfo))  /* receives PROCESS_INFORMATION  */
       ) {
	DWORD err = GetLastError();
	wsprintf(buf, TEXT("CreateProcessAsUser failed, error code=%d\r\n"),
	    err);
	write_string_to_log(prd, buf);
	write_error(prd, err);
    }

    /* read pipe to get filename */
    if (flag)
	flag = ReadFile(hPipeRead, buf, sizeof(buf)-1, &dwBytesRead, NULL);
    if (flag) {

	if (dwBytesRead < sizeof(prd->tempname))
	    CopyMemory(prd->tempname, buf, dwBytesRead);
write_string_to_log(prd, TEXT("redmonfn.exe returns '"));
write_string_to_log(prd, buf);
write_string_to_log(prd, TEXT("'\r\n"));
wsprintf(buf, TEXT("  read %d bytes\r\n"), dwBytesRead);
write_string_to_log(prd, buf);
    }

    if (prd->piProcInfo.hProcess != INVALID_HANDLE_VALUE) {
	CloseHandle(prd->piProcInfo.hProcess);
	prd->piProcInfo.hProcess = INVALID_HANDLE_VALUE;
    }

    if (prd->piProcInfo.hThread != INVALID_HANDLE_VALUE) {
	CloseHandle(prd->piProcInfo.hThread);
	prd->piProcInfo.hThread = INVALID_HANDLE_VALUE;
    }

    return flag;
}
#endif

/* end of redmon.c */
