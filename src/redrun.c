/* Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.
  
  This file is part of RedMon.
  
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  This software is distributed under licence and may not be copied, modified
  or distributed except as expressly authorised under the terms of the
  LICENCE.

*/

/* RedRun program */

/* Copy standard input to a temporary file, then run a program.
 * Designed to link RedMon with a program that wants its
 * input in a file, not stdin.
 * 
 * An example is running GSview.
 * In RedMon, the program to run would be
 *   c:\gstools\redrun.exe
 * and the arguments would be
 *   c:\gstools\gsview\gsview32.exe %1
 *
 * When started, RedRun will write standard input to a 
 * temporary file, until it reaches end of file.  It will then 
 * start the specified program.  RedRun will wait until the 
 * specified program terminates, then will delete the temporary
 * and terminate itself.
 */

#define STRICT
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <ctype.h>
#ifdef __BORLANDC__
#include <dir.h>
#else
#include <direct.h>	/* MSVC++ */
#endif

#define BUF_SIZE 4096

#define MAXSTR 256
TCHAR tempname[MAXSTR];
BYTE buf[1024];

TCHAR usage_message[] ="\
RedRun by Ghostgum Software Pty Ltd. 1998-12-05\n\
Copy stdin to a temporary file then run a program.\n\
RedRun will delete the temporary file after the program finishes.\n\
RedRun can be used to capture printer output redirected by RedMon.\n\
\n\
Usage: redrun program [arguments]\n\
   'program' will be started with given arguments.\n\
   An argument of %1 will be replaced by the temporary file name.\n\
   If no arguments are given, %1 is assumed.\n\
\n\
Examples:\n\
   redrun gsview32\n\
   redrun gsview32 %1\n\
   redrun epstool -b -c -otemp.eps %1\n\
   redrun command.com /c copy /b %1 outfile.txt\n\
\n\
";


void
usage(void)
{
    fputs(usage_message, stdout);
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
	fputs((char *)lpMessageBuffer, stdout);
	LocalFree(LocalHandle(lpMessageBuffer));
	fputs("\r\n", stdout);
    }
}


/* Create and open a scratch file with a given name prefix. */
/* Write the actual file name at fname. */
FILE *
gp_open_scratch_file(const char *prefix, char *fname, const char *mode)
{   char *temp;
    if ((temp = getenv("TEMP")) == NULL)
	getcwd(fname, MAXSTR);
    else
	lstrcpy(fname, temp);

    /* Prevent X's in path from being converted by mktemp. */
    for (temp = fname; *temp; temp++ ) {
	*temp = (char)tolower(*temp);
	if (*temp == '/')
	    *temp = '\\';
    }
    if (lstrlen(fname) && (fname[lstrlen(fname)-1] != '\\'))
	lstrcat(fname, "\\");

    lstrcat(fname, prefix);
    lstrcat(fname, "XXXXXX");
    mktemp(fname);
    return fopen(fname, mode);
}

/* Start child program with redirected standard input and output */
BOOL runprog(char *command, LPPROCESS_INFORMATION piProcInfo)
{
    STARTUPINFO siStartInfo;

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
    siStartInfo.dwFlags = 0;
    siStartInfo.wShowWindow = 0;	
    // siStartInfo.dwFlags = STARTF_USESHOWWINDOW;
    // siStartInfo.wShowWindow = SW_SHOWMINNOACTIVE;	
    // siStartInfo.wShowWindow = SW_HIDE;	
    siStartInfo.cbReserved2 = 0;
    siStartInfo.lpReserved2 = NULL;
    siStartInfo.hStdInput = 0;
    siStartInfo.hStdOutput = 0;
    siStartInfo.hStdError = 0;

    /* Create the child process. */

    return CreateProcess(NULL,
        command,       /* command line                       */
        NULL,          /* process security attributes        */
        NULL,          /* primary thread security attributes */
        FALSE,         /* handles are NOT inherited          */
        0,             /* creation flags                     */
        NULL,          /* use parent's environment           */
        NULL,          /* use parent's current directory     */
        &siStartInfo,  /* STARTUPINFO pointer                */
        piProcInfo);   /* receives PROCESS_INFORMATION       */
}

/* Parse command line, handling quotes and embedded spaces
 * Return pointer to command to run.
 * Return NULL pointer on error.
 */
char *parse_run(char *tempname)
{
    LPSTR p;
    char *args;
    char *d, *e;
    BOOL added_temp = FALSE;

    p = GetCommandLine();
   
    args = (char *)malloc(lstrlen(p)+lstrlen(tempname)+2);
    if (args == (char *)NULL)
	return NULL;

    /* skip over our program name, if present */
    while ((*p) && (*p != ' ')) {
	if (*p == '\042') {
	    /* skip over quoted items */
	    p++;
	    while ((*p) && (*p != '\042'))
		p++;
	}
	if (*p)
	    p++;
    }

    /* skip over leading spaces before first argument */
    while ((*p) && (*p == ' '))
	p++;

    /* copy remaining arguments, substituting %1 with tempname */
    d = args;
    while (*p) {
	// for each argument
        e = d;
        while ((*p) && (*p != ' ')) {
	    if (*p == '\042') {
		// copy quoted items */
		*d++ = *p++;
		while ((*p) && (*p != '\042'))
		    *d++ =*p++;
		*d++ = *p;
	    }
	    else 
		*d++ = *p;
	    if (*p)
		p++;
        }
	*d = '\0';

        /* insert temp filename if needed */
	if (lstrcmp(e, "%1") == 0) {
	    d = e;	/* replace %1 */
	    *d++ = '\042';
	    lstrcpy(d, tempname);
	    d+=lstrlen(d);
	    *d++ = '\042';
	    *d = '\0';
	    added_temp = TRUE;
	}

	while ((*p) && (*p == ' '))
	    *d++ = *p++;	// Copy trailing spaces
    }

    if (!added_temp) {
	*d++ = ' ';
	*d++ = '\042';
	lstrcpy(d, tempname);
	d+=lstrlen(d);
	*d++ = '\042';
	*d = '\0';
    }

    return args;
}



int main(int argc, char *argv[])
{
FILE *f;
PROCESS_INFORMATION piProcInfo;
DWORD exit_status;
int len;
char *command;


    if (argc < 2) {
	usage();
	return 1;
    }

    /* create temporary file */
    if ( (f = gp_open_scratch_file("red", tempname, "wb"))
	== (FILE *)NULL) {
	fprintf(stdout, "Can't open temporary file \042%s\042\n", tempname);
	return 2;
    }
    setmode(fileno(stdin), O_BINARY);
    while ( (len = fread(buf, 1, sizeof(buf), stdin)) != 0) {
	fwrite(buf, 1, len, f);
    }
    fclose(f);

    command = parse_run(tempname);
    if (command == (char *)NULL) {
	usage();
	fprintf(stdout, "\nError parsing arguments\n");
	return 1;
    }


    /* Create process */
    if (!runprog(command, &piProcInfo)) {
	DWORD error = GetLastError();
	fprintf(stdout, "Can't run program:\n  %s\n", command);
	switch (error) {
	    case ERROR_FILE_NOT_FOUND:
		fprintf(stdout, "File not found\n");
		break;
	    default:
		fprintf(stdout, "Error %d\n", error);
	}
	write_error(error);
	unlink(tempname);
	return 2;
    }
    
    /* wait until process starts */
    WaitForInputIdle(piProcInfo.hProcess, 5000);
    Sleep(1000);	/* Wait for console apps */

    /* Wait until process finishes */
    while (GetExitCodeProcess(piProcInfo.hProcess, &exit_status)
	&& (exit_status == STILL_ACTIVE))
	Sleep(1000);

    /* Delete temporary file */
    unlink(tempname);

    return 0;
}

