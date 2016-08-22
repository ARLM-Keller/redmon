/* Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.
  
  This file is part of RedMon.
  
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  This software is distributed under licence and may not be copied, modified
  or distributed except as expressly authorised under the terms of the
  LICENCE.

*/

/* redmonrc.h */

#include "portrc.h"

#define COPYRIGHT TEXT("Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All Rights Reserved.\n")
#define VERSION TEXT("2012-06-21  Version 1.9\n")
#define VERSION_MAJOR_NUMBER 1
#define VERSION_MINOR_NUMBER 90
#define VERSION_NUMBER VERSION_MAJOR_NUMBER * 100 + VERSION_MINOR_NUMBER
#define PUBLISHER TEXT("Ghostgum Software Pty Ltd")

/*
#define BETA
#define BETA_YEAR 2012
#define BETA_MONTH 12
#define BETA_DAY   31
*/

#ifndef DS_3DLOOK
#define DS_3DLOOK 0x0004L	/* for Windows 95 look */
#endif


#define IDC_COMMAND	201	
#define IDC_ARGS	202	
#define IDC_BROWSE	203
#define IDC_OUTPUT	204
#define IDC_PRINTER	205
#define IDC_PRINTERTEXT	206
#define IDC_SHOW	207
#define IDC_LOGFILE	208
#define IDC_RUNUSER	209
#define IDC_DELAY	210
#define IDC_PRINTERROR	211


#define IDC_PORTNAME	250
#define IDC_HELPBUTTON	251

#define IDD_CONFIGLOG	300
#define IDC_LOGUSE	301
#define IDC_LOGNAMEPROMPT   302
#define IDC_LOGNAME	    303
#define IDC_LOGDEBUG	    307

#define IDS_TITLE	401
#ifdef BETA
#define IDS_BETAEXPIRED	402
#endif

#define IDS_CONFIGPROP	  405
#define IDS_CONFIGLOGFILE 406
#define IDS_CONFIGUNKNOWN 407

/* help topics */
#define IDS_HELPADD	450
#define IDS_HELPCONFIG	451
#define IDS_HELPLOG	452

/* file filters */
#define IDS_FILTER_EXE 460
#define IDS_FILTER_TXT 461
#define IDS_FILTER_PROMPT 462

#define IDS_SHOWBASE	505
#define SHOW_NORMAL	0
#define SHOW_MIN	1
#define SHOW_HIDE	2
#define IDS_SHOWNORMAL	IDS_SHOWBASE+SHOW_NORMAL
#define IDS_SHOWMIN	IDS_SHOWBASE+SHOW_MIN
#define IDS_SHOWHIDE	IDS_SHOWBASE+SHOW_HIDE

#define IDS_OUTPUTBASE	510
#define OUTPUT_SELF	0
#define OUTPUT_PROMPT	1
#define OUTPUT_STDOUT	2
#define OUTPUT_FILE	3
#define OUTPUT_HANDLE	4
#define OUTPUT_LAST	4
#define IDS_OUTPUTSELF		IDS_OUTPUTBASE+OUTPUT_SELF
#define IDS_OUTPUTPROMPT	IDS_OUTPUTBASE+OUTPUT_PROMPT
#define IDS_OUTPUTSTDOUT	IDS_OUTPUTBASE+OUTPUT_STDOUT
#define IDS_OUTPUTFILE		IDS_OUTPUTBASE+OUTPUT_FILE
#define IDS_OUTPUTHANDLE	IDS_OUTPUTBASE+OUTPUT_HANDLE

/* end of redmonrc.h */
