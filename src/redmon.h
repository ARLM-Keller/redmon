/* Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.
  
  This file is part of RedMon.
  
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  This software is distributed under licence and may not be copied, modified
  or distributed except as expressly authorised under the terms of the
  LICENCE.

*/

/* redmon.h */

#include "redmonrc.h"

#define PIPE_BUF_SIZE 4096


#define MONITORNAME TEXT("Redirected Port")
#define MONITORDLL95 TEXT("redmon95.dll")
#define MONITORDLL35 TEXT("redmon35.dll")
#define MONITORDLLNT TEXT("redmonnt.dll")
#define MONITORDLL32 TEXT("redmon32.dll")
#define MONITORDLL64 TEXT("redmon64.dll")
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
#define MONITORENV32 TEXT("Windows NT x86")
#define MONITORENV64 TEXT("Windows x64")
#define MONITORHLP TEXT("redmon.chm")
#define MONITORKEY TEXT("Redirection Port Monitor")

#ifdef _WIN64
#define UNINSTALLPROG TEXT("unredmon64.exe")
#else
#define UNINSTALLPROG TEXT("unredmon.exe")
#endif
#define UNINSTALLKEY TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall")
#define UNINSTALLSTRINGKEY TEXT("UninstallString")
#define DISPLAYNAMEKEY TEXT("DisplayName")

#define REDCONF TEXT("redconf.exe")

/* end of redmon.h */
