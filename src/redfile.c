/* Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.
  
  This file is part of RedMon.
  
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  This software is distributed under licence and may not be copied, modified
  or distributed except as expressly authorised under the terms of the
  LICENCE.

*/

/* RedFile program */

/* Copy standard input to a named file.
 * Designed to save to a file any printer data redirected by RedMon.
 *
 * You could could connect a printer to FILE:, but you keep getting
 * a prompt for the filename.
 * Instead of using RedMon and RedFile, in Windows 95 you can edit
 * win.ini [Ports] to add a named file.  This might not work under
 * Windows NT.
 */

#define STRICT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>

#define MAXSTR 256
BYTE buf[1024];

TCHAR usage_message[] ="\
RedFile by Ghostgum Software Pty Ltd. 2000-04-29\n\
Redirect stdin to a file.\n\
RedFile can be used to capture printer output redirected by RedMon.\n\
\n\
Usage: redfile filename\n\
filename may contain an integer format specifier such as %03d\n\
to create a unique numbered file.\n\
\n\
Examples:\n\
   echo test | redfile testfile.txt\n\
   echo test | redfile test%04d.txt\n\
   echo test > testfile.txt\n\
\n\
";


void
usage(void)
{
    fputs(usage_message, stdout);
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

void
make_unique(char *fname, int len, const char *format)
{
FILE *f;
const char *p;
BOOL bUseNumbered;
int number_length;
int max_number;

    /* check if user wants numbered unique file */
    p = format; 
    bUseNumbered = FALSE;
    number_length = 0;
    while ( (*p) && !bUseNumbered) {
	if (*p == '%') {
	   p++;
	   /* make sure it is integer format */
	   while (isdigit(*p)) {
		number_length += (int)(*p) - '0';
		p++;
	   }
	   if (*p == 'd') {
		bUseNumbered = TRUE;
	   }
	}
	p++;
    }
    if (bUseNumbered && ((int)strlen(format) + number_length >= len))
	bUseNumbered = FALSE;	/* would overflow */
    max_number = 0;
    if (number_length != 0) {
	while (number_length) {
	    max_number = max_number * 10 + 9;
	    number_length--;
	}
    }
    else
	max_number = 999;

    memset(fname, 0, len);
    if (bUseNumbered) {
	int i = 0;
	while (bUseNumbered) {
	    if (i > max_number)
		break; 
	    sprintf(fname, format, i);
	    if ((f = fopen(fname, "rb")) == (FILE *)NULL)
		bUseNumbered = FALSE;	/* found a unique name */
	    else
		fclose(f);
	    i++;
	}
    }
    else
	strncpy(fname, format, len-1);
}

int main(int argc, char *argv[])
{
FILE *f;
int len;
int nargc;
char **nargv;
char fname[MAXSTR];

    if (parse_args(&nargc, &nargv)) {
	usage();
	return 1;
    }

    if (nargc != 2) {
	usage();
	return 1;
    }

    make_unique(fname, sizeof(fname), nargv[1]);

    /* create file */
    if ( (f = fopen(fname, "wb")) == (FILE *)NULL) {
	fprintf(stdout, "Can't open file \042%s\042\n", fname);
	return 2;
    }
    setmode(fileno(stdin), O_BINARY);
    while ( (len = fread(buf, 1, sizeof(buf), stdin)) != 0) {
	fwrite(buf, 1, len, f);
    }
    fclose(f);

    return 0;
}

