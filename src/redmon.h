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

/* redmon.h */

#include "redmonrc.h"

#define PIPE_BUF_SIZE 4096


#define MONITORNAME TEXT("Redirected Port")
#define MONITORDLL95 TEXT("redmon95.dll")
#define MONITORDLL35 TEXT("redmon35.dll")
#define MONITORDLLNT TEXT("redmonnt.dll")
#define MONITORENV95 TEXT("Windows 4.0")
#ifdef _WIN64
  #ifdef _M_AMD64
  //#define MONITORENVNT TEXT("Windows NT x64")
  #define MONITORENVNT TEXT("Windows x64")
  #else
  #ifdef _M_IA64
  #define MONITORENVNT TEXT("Windows IA64")
  #else
  #error (unknown target)
  #endif
  #endif
#else
#define MONITORENVNT TEXT("Windows NT x86")
#endif
#define MONITORHLP TEXT("redmon.hlp")
#define MONITORKEY TEXT("Redirection Port Monitor")

#define UNINSTALLPROG TEXT("unredmon.exe")
#define UNINSTALLKEY TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall")
#define UNINSTALLSTRINGKEY TEXT("UninstallString")
#define DISPLAYNAMEKEY TEXT("DisplayName")

#define REDCONF TEXT("redconf.exe")

/* end of redmon.h */
