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

/* redmonrc.h */

#include "portrc.h"

#define COPYRIGHT TEXT("Copyright (C) 1997-2001, Ghostgum Software Pty Ltd.  All Rights Reserved.\n")
#define VERSION TEXT("2001-10-28  Version 1.7\n")
#define VERSION_NUMBER 17

/*
#define BETA
#define BETA_YEAR 2001
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
