/* Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.
  
  This file is part of RedMon.
  
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  This software is distributed under licence and may not be copied, modified
  or distributed except as expressly authorised under the terms of the
  LICENCE.

*/

/* RedPr program */

/* Windows PRint program */
/* A Win32 command line program for sending files to a printer.
 * RedPr writes to the Windows printer queue and is better than
 * using "COPY /B filename LPT1:"
 *
 * Printer names may be either the Windows 95/NT printer name
 * such as "Apple LaserWriter II NT", or a port name such
 * as "LPT1:".   If a port name is used, RedPr will replace it 
 * with the name of the first printer to use that port.
 *
 * Examples:
 *   RedPr -P"Apple LaserWriter II NT" file.ps
 *   RedPr -pLPT1: file.ps

 * Actually, -P and -p are interchangeable.  RedPr first looks
 * for a printer with the given name. If that fails it tries to 
 * find a printer which uses the given port.
 */

/* This program was originally written to read from a PostScript 
 * printer connected to a Windows 95 printer port.
 * This may not work because the PostScript language monitor
 * may take charge of reading from the port, and not pass anything
 * back to ReadPrinter.
 * We are hoping that DOC_INFO_2 dwMode=DI_CHANNEL will override this.
 *
 * On 1997-12-05, MSDN Online Library said DOC_INFO_2 structure was 
 * not supported by Windows NT.  This structure is required in the
 * StartDocPrinter() call to get a clear bi-directional channel
 * to the printer. 
 * Bi-directional is not available under Windows NT.
 */

/* Added wildcard support for filenames, 1998-12-02 */


#define STRICT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define PRINT_BUF_SIZE 4096

BOOL bidirectional = FALSE;
BOOL verbose = FALSE;
BOOL is_winnt = FALSE;

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
#endif

BOOL
enum_printers(PRINTER_INFO_2 **ppri2, LPDWORD pcount, BOOL display);

void
usage(void)
{
PRINTER_INFO_2 *pri2;
DWORD count;
    fprintf(stdout, "RED PRint for Win32 by Ghostgum Software Pty Ltd. 1998-12-05\n");
    fprintf(stdout, "Usage: RedPr [-P\042printer\042] [-pport] [-b] [-h] [-v] filename ...\n");
    fprintf(stdout, "  -P\042printer\042  See below for printer names.\n");
    fprintf(stdout, "  -pport       See below for port names.\n");
    fprintf(stdout, "  -b           Bi-directional.  Attempt to read from printer.\n");

    fprintf(stdout, "  -h           Help\n");
    fprintf(stdout, "  -v           Verbose.  Display debugging messages.\n");
    fprintf(stdout, "\n");
    /* display list of printers */
    if (enum_printers(&pri2, &count, TRUE))
	free(pri2);
}

void
write_error(DWORD err)
{
LPVOID lpMessageBuffer;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	FORMAT_MESSAGE_FROM_SYSTEM,
	NULL, err,
	MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* user default language */
	(LPTSTR) &lpMessageBuffer, 0, NULL);
    if (lpMessageBuffer) {
	fputs((LPTSTR)lpMessageBuffer, stdout);
	LocalFree(LocalHandle(lpMessageBuffer));
	fputs("\r\n", stdout);
    }
}


/* Obtain a list of available printers */
/* Return a malloc'd list pointed to by ppri2 */
/* If display==TRUE, display list on stdout */
BOOL
enum_printers(PRINTER_INFO_2 **ppri2, LPDWORD pcount, BOOL display)
{
unsigned int i;
DWORD needed;
PRINTER_INFO_2 *pri2;
char *enumbuffer;

    /* enumerate all available printers */
    EnumPrinters(PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL, NULL, 
	2, NULL, 0, &needed, pcount);
    if (needed == 0) {
	/* no printers */
	fprintf(stdout, "No printers installed"); 
	return FALSE;
    }
    enumbuffer = malloc(needed);
    if (enumbuffer == (char *)NULL) {
	fprintf(stdout, "Out of memory"); 
	return FALSE;
    }
    if (!EnumPrinters(PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL, NULL, 
	2, (LPBYTE)enumbuffer, needed, &needed, pcount)) {
	DWORD err = GetLastError();
	free(enumbuffer);
	fprintf(stdout, "EnumPrinters() failed, error = %d", 
		err);
	write_error(err);
	return FALSE;
    }
    pri2 = (PRINTER_INFO_2 *)enumbuffer;

    /* Windows NT doesn't identify the default printer in 
     * PRINTER_INFO_2 Attributes.  Instead we need to query
     * the registry to find the default printer for the
     * current user.
     */
    if (is_winnt) {
	HKEY hkey;
	TCHAR defprint[256];
	defprint[0] = '\0';
        if (RegOpenKeyEx(HKEY_CURRENT_USER, 
	  "Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
          0, KEY_READ, &hkey) == ERROR_SUCCESS) {
		DWORD cbData = sizeof(defprint);
		DWORD keytype = REG_SZ;
		RegQueryValueEx(hkey, "Device", 0, &keytype, 
			(LPBYTE)defprint, &cbData);
		strtok(defprint, ",");
	}
        for (i=0; i<*pcount; i++) {
	    if (lstrcmp(pri2[i].pPrinterName, defprint) == 0)
		pri2[i].Attributes |= PRINTER_ATTRIBUTE_DEFAULT;
	}
    }

    if (display || verbose) {
	fprintf(stdout, "Printers (default is *):\n");
	for (i=0; i<*pcount; i++) {
	    fprintf(stdout, "%c \042%s\042 on %s\n",
		pri2[i].Attributes & PRINTER_ATTRIBUTE_DEFAULT ? '*' : ' ',
		pri2[i].pPrinterName, pri2[i].pPortName);
        }
    }
    *ppri2 = pri2;
    return TRUE;
}

void
read_printer(HANDLE hprinter)
{
    /* Timeouts for printer ports have both a timeout constant 
     * and a per character timeout.  Use a small read buffer to 
     * try reduce the time taken to return from ReadPrinter().
     */
    char read_buffer[16];
    /* assume we will read no more than double what we send */
    int try_count = 2 * PRINT_BUF_SIZE / sizeof(read_buffer);
    DWORD read_count = 1;

    /* keep trying to read until an error
     * or ReadPrinter returns 0 bytes,
     * or we have called ReadPrinter enough times to 
     *  read PRINT_BUFFER_SIZE bytes.
     */
    while (bidirectional && try_count && read_count) {
	try_count--;
	if (ReadPrinter(hprinter, (LPVOID)read_buffer, sizeof(read_buffer),
		&read_count)) {
	    fwrite(read_buffer, 1, read_count, stdout);
	    if (verbose)
	       fprintf(stdout, "\nRead %d bytes from printer\n", read_count);
	}
	else {
	    DWORD error = GetLastError();
	    bidirectional = FALSE;
	    if (error == ERROR_INVALID_FLAGS) {
		fprintf(stdout, "Printer is not bi-directional\n");
		if (verbose)
		    fprintf(stdout, "Error = %d\n", error);
	    }
	    else if (error == ERROR_PRINT_CANCELLED) {
		/* this isn't an error */
	       if (verbose) {
		    fprintf(stdout, "ReadPrinter: Job has finished\n");
		    fprintf(stdout, "Error = %d\n", error);
	       }
	    }
	    else {
		fprintf(stdout, "ReadPrinter() failed, error = %d\n",
		    error);
		write_error(error);
	    }
	}
    }
}


int 
print_file(char *filename, char *printer_name)
{
DWORD count;
char *buffer;
FILE *f;
HANDLE hprinter;
DOC_INFO_1 dci1;
DOC_INFO_2 dci2;
DWORD written;
DWORD JobId;
DWORD error;

    if ((buffer = malloc(PRINT_BUF_SIZE)) == (char *)NULL)
        return FALSE;
	
    if ((f = fopen(filename, "rb")) == (FILE *)NULL) {
	fprintf(stdout, "Can't find file \042%s\042\n", filename);
	free(buffer);
	return FALSE;
    }


    /* open a printer */
    if (!OpenPrinter(printer_name, &hprinter, NULL)) {
	error = GetLastError();
	if (verbose)
	    fprintf(stdout, 
		"OpenPrinter() failed for \042%s\042, error = %d\n",
		printer_name, error);
	write_error(error);
	fprintf(stdout, "Unknown printer\n");
	free(buffer);
	return FALSE;
    }
    /* from here until ClosePrinter, should AbortPrinter on error */

    if (!is_winnt && bidirectional) {
	dci2.pDocName = filename;
	dci2.pOutputFile = NULL;
	dci2.pDatatype = "RAW";
	/* dwMode=0 is a normal print job */
	/* dwMode=DI_CHANNEL opens a communication channel to the printer */
	dci2.dwMode = DI_CHANNEL;
	dci2.JobId = 0;	/* reserved */
	JobId = StartDocPrinter(hprinter, 2, (LPBYTE)&dci2);
    }
    else {
	dci1.pDocName = filename;
	dci1.pOutputFile = NULL;
	/* for available types see EnumPrintProcessorDatatypes */
	dci1.pDatatype = "RAW";
	JobId = StartDocPrinter(hprinter, 1, (LPBYTE)&dci1);
    }
    if (verbose)
	printf("JobId=%d\n", JobId);
    if (JobId == 0) {
	error = GetLastError();
	if (error != ERROR_BUSY || verbose) {
	    fprintf(stdout, "StartDocPrinter() failed, error = %d\n", 
		GetLastError());
	    write_error(error);
	}
        if (error == ERROR_BUSY)
	    fprintf(stdout, "Printer \042%s\042 is busy\n", printer_name);
	AbortPrinter(hprinter);
	free(buffer);
	return FALSE;
    }
   

    while ((count = fread(buffer, 1, PRINT_BUF_SIZE, f)) != 0 ) {
	if (verbose)
	    fputc('.', stdout);
	if (!WritePrinter(hprinter, (LPVOID)buffer, count, &written)) {
	    free(buffer);
	    fclose(f);
	    AbortPrinter(hprinter);
	    return FALSE;
	}
	read_printer(hprinter);
    }
    fclose(f);
    free(buffer);
    if (verbose)
	fputc('\n', stdout);

    read_printer(hprinter);

    if (!EndDocPrinter(hprinter)) {
	DWORD err = GetLastError();
	fprintf(stdout, "EndDocPrinter() failed, error = %d\n", 
	    err);
	write_error(err);
	AbortPrinter(hprinter);
	return FALSE;
    }

    if (!ClosePrinter(hprinter)) {
	DWORD err = GetLastError();
	fprintf(stdout, "ClosePrinter() failed, error = %d\n", 
	    err);
	write_error(err);
	return FALSE;
    }
    return TRUE;
}

/* Borland C++ doesn't parse the command line correctly
 * when the program name contains spaces.
 * Implement our own command line parsing.
 * Return zero on success, non-zero on fail.
 */
int parse_args(int *pargc, char **pargv[])
{
#define MAXCMDTOKENS 50
    int argc;
    LPSTR argv[MAXCMDTOKENS];
    LPSTR p;
    char *args;
    char *d, *e;

    // If called with "gswin32c.exe arg1 arg2"
    // lpszCmdLine returns:
    //    "arg1 arg2" if called with CreateProcess(NULL, command, ...)
    //    "arg2"      if called with CreateProcess(command, args, ...)
    // GetCommandLine() returns
    //    ""gswin32c.exe" arg1 arg2" 
    //          if called with CreateProcess(NULL, command, ...)
    //    "  arg1 arg2"      
    //          if called with CreateProcess(command, args, ...)
    // Consequently we must use GetCommandLine() 
    p = GetCommandLine();
   
    argc = 0;
    args = (char *)malloc(lstrlen(p)+1);
    if (args == (char *)NULL)
	return 1;

    // Parse command line handling quotes.
    d = args;
    while (*p) {
	// for each argument

	if (argc >= MAXCMDTOKENS - 1)
	    break;

        e = d;
        while ((*p) && (*p != ' ')) {
	    if (*p == '\042') {
		// Remove quotes, skipping over embedded spaces.
		// Doesn't handle embedded quotes.
		p++;
		while ((*p) && (*p != '\042'))
		    *d++ =*p++;
	    }
	    else 
		*d++ = *p;
	    if (*p)
		p++;
        }
	*d++ = '\0';
	argv[argc++] = e;

	while ((*p) && (*p == ' '))
	    p++;	// Skip over trailing spaces
    }
    argv[argc] = NULL;

    if (lstrlen(argv[0]) == 0) {
        char command[256];
	GetModuleFileName(NULL, command, sizeof(command)-1);
	argv[0] = (char *)malloc(lstrlen(command)+1);
	lstrcpy(argv[0], command);
    }

    *pargv = (char **)malloc(sizeof(char *) * (argc+2));
    memcpy(*pargv, argv, sizeof(char *) * (argc+2));
    *pargc = argc;
    
    return 0;
}


int main(int argc, char *argv[])
{
PRINTER_INFO_2 *pri2;
DWORD count;
char printer_name[256];
int i;
unsigned int j;
int nargc;
char **nargv;
DWORD version = GetVersion();
BOOL success = TRUE;
    printer_name[0] = '\0';
    if ((HIWORD(version) & 0x8000)==0)
	is_winnt = TRUE;

    if (parse_args(&nargc, &nargv)) {
	usage();
	return 1;
    }

    if (nargc < 2) {
	usage();
	return 1;
    }
    /* Get default printer name */
    if (enum_printers(&pri2, &count, FALSE)) {
	for (j=0; j<count; j++) {
	    if (pri2[j].Attributes & PRINTER_ATTRIBUTE_DEFAULT)
	        strcpy(printer_name, pri2[j].pPrinterName);
        }
    }
    else 
	return 1;

    if (verbose && (lstrlen(printer_name) == 0)) {
	  fprintf(stdout, "Warning: no default printer\n");
    }

    for (i=1; i<nargc; i++) {
	if (nargv[i][0] == '-') {
	    switch(nargv[i][1]) {
		case 'h':
		    usage();
		    i = nargc;	/* don't process any more arguments */
		    break;
		case 'b':
		    if (is_winnt) {
			fprintf(stdout, 
			  "Bi-directional is not supported in Windows NT\n");
			i = nargc;	/* don't process any more arguments */
		    }
		    else
		        bidirectional = TRUE;
		    break;
		case 'v':
		    verbose = TRUE;
		    break;
		case 'P':
		case 'p':
		    if (nargv[i][2] == '"')
			strcpy(printer_name, nargv[i]+3);
		    else
			strcpy(printer_name, nargv[i]+2);
		    if (strlen(printer_name) 
			&& printer_name[strlen(printer_name)-1]=='"')
			printer_name[strlen(printer_name)-1] = '\0';

		    for (j=0; j<count; j++) {
			if (strcmp(pri2[j].pPrinterName, printer_name) == 0)
			    break;
		    }
		    if (j == count) {
			/* Printer name wasn't found */
			/* Try to match a port */
			for (j=0; j<count; j++) {
			    if (strcmp(pri2[j].pPortName, printer_name) == 0) {
				strcpy(printer_name, pri2[j].pPrinterName);
				break;
			    }
			}
			if (j == count) {
			  usage();
			  fprintf(stdout, "\nPrinter \042%s\042 is unknown\n", 
			    printer_name);
			  i = nargc;	/* don't process any more arguments */
			}
		    }
		    break;
		default:
		    usage();
		    fprintf(stdout, "\n\nUnknown option %s\n", nargv[i]);
		    free(pri2);
		    return 1;
	    }
	}
	else {
	    if (lstrlen(printer_name) == 0) {
		  fprintf(stdout, "\nNo printer selected\n");
	    }
	    else {
		HANDLE find_handle;
		WIN32_FIND_DATA find_data;
		BOOL found = TRUE;
		char basepath[512];
	        char path[1024];
		int k;
	        strcpy(basepath, nargv[i]);
		k = strlen(basepath) - 1;
		while (k>=0) {
		    if ( (basepath[k] == '\\') || 
			 (basepath[k] == '/') ||
			 (basepath[k] == ':') ) {
			basepath[k+1] = '\0';
			break;
		    }
		    k--;
		}
		if (k<=0)
		   basepath[0] = '\0';
		find_handle = FindFirstFile(nargv[i], &find_data);
		if (find_handle == INVALID_HANDLE_VALUE) {
		    fprintf(stdout, "Can't find file \042%s\042\n", nargv[i]);
		    success = FALSE;
		}
		else {
		  while (found) {
		    strcpy(path, basepath);
		    strcat(path, find_data.cFileName);
		    if (verbose)
			fprintf(stdout, "Printing \042%s\042 to \042%s\042\n",
			    path, printer_name);
		    success = print_file(path, printer_name);
		    if (!success)
			break;
		    found = FindNextFile(find_handle, &find_data);
		  }
		  FindClose(find_handle);
		}
	    }
	}
    }

    free(pri2);
    return (success ? 0 : 1);
}

