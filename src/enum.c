/* Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.
  
  This file is part of RedMon.
  
  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

  This software is distributed under licence and may not be copied, modified
  or distributed except as expressly authorised under the terms of the
  LICENCE.

*/

/* enum.c */
/* List printers, ports, monitors etc. */
/* Russell Lang, 1998-01-05 */
/* Updated 2005-02-13 to give more details on NT */
#include <windows.h>
#include <stdio.h>


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


char buffer[65536];
DWORD needed, returned;
int rc;
unsigned int i;
DRIVER_INFO_2 *dri2;
PRINTER_INFO_2 *pri2;
PORT_INFO_2 *pi2;
MONITOR_INFO_1 *mi;
MONITOR_INFO_2 *mi2;
DATATYPES_INFO_1 *dti1;
PRINTPROCESSOR_INFO_1 *ppi1;
BOOL is_winnt = FALSE;

int main(int argc, char *argv[])
{
   DWORD version = GetVersion();
   if ((HIWORD(version) & 0x8000)==0)
      is_winnt = TRUE;

   rc = GetPrintProcessorDirectory(NULL, NULL, 1, buffer, 
      sizeof(buffer), &needed);
   if (rc)
      printf("PrintProcessorDirectory = %s\n", buffer);

   printf("\nPrinters: ");
   rc = EnumPrinters(PRINTER_ENUM_CONNECTIONS | PRINTER_ENUM_LOCAL, 
	  NULL, 2, (LPBYTE)buffer, sizeof(buffer), 
      &needed, &returned);
   printf("%ld bytes   %ld\n", needed, returned);
   pri2 = (PRINTER_INFO_2 *)buffer;
   if (rc) {
      for (i=0; i<returned; i++) {
         printf("\042%s\042 \042%s\042 \042%s\042 \042%s\042 \042%s\042\n", 
	   pri2[i].pPrinterName,
	   pri2[i].pPortName,
	   pri2[i].pDriverName,
	   pri2[i].pPrintProcessor,
	   pri2[i].pDatatype
	   );
      }
   }
   else
      printf("EnumPrinters() failed\n");

   printf("\nPrinter Drivers: ");
   rc = EnumPrinterDrivers(NULL, NULL, 2, (LPBYTE)buffer, sizeof(buffer), 
          &needed, &returned);
   printf("%ld bytes   %ld\n", needed, returned);
   dri2 = (DRIVER_INFO_2 *)buffer;
   if (rc) {
      for (i=0; i<returned; i++) {
         printf("\042%s\042 \042%s\042 \042%s\042 \042%s\042 \042%s\042\n", 
	   dri2[i].pName,
	   dri2[i].pEnvironment,
	   dri2[i].pDriverPath,
	   dri2[i].pDataFile,
	   dri2[i].pConfigFile
	   );
      }
   }
   else
      printf("EnumPrinterDrivers() failed\n");

   printf("\nPorts: (buffer = NULL, length = 0) ");
   rc = EnumPorts(NULL, 2, NULL, 0, &needed, &returned);
   printf("%ld bytes   %ld\n", needed, returned);

   printf("\nPorts: ");
   rc = EnumPorts(NULL, 2, (LPBYTE)buffer, sizeof(buffer), 
      &needed, &returned);
   printf("%ld bytes   %ld\n", needed, returned);
   pi2 = (PORT_INFO_2 *)buffer;
   if (rc) {
      for (i=0; i<returned; i++) {
         printf("\042%s\042 \042%s\042 \042%s\042 %d\n", 
	   pi2[i].pPortName,
	   pi2[i].pMonitorName,
	   pi2[i].pDescription,
	   pi2[i].fPortType
	   );
      }
   }
   else
      printf("EnumPorts() failed\n");

   printf("\nMonitors: ");
   rc = EnumMonitors(NULL, is_winnt ? 2 : 1, (LPBYTE)buffer, sizeof(buffer), 
      &needed, &returned);
   printf("%ld bytes   %ld\n", needed, returned);
   mi = (MONITOR_INFO_1 *)buffer;
   mi2 = (MONITOR_INFO_2 *)buffer;
   if (rc) {
      for (i=0; i<returned; i++) {
	 if (is_winnt)
             printf("%d: \042%s\042 \042%s\042 \042%s\042\n", 
		i, mi2[i].pName, mi2[i].pEnvironment, mi2[i].pDLLName);
	 else
             printf("%d: \042%s\042 \n", i, mi[i].pName);
      }
   }
   else
      printf("EnumMonitors() failed\n");

   printf("\nPrintProcessors: ");
   rc = EnumPrintProcessors(NULL, NULL, 1, 
      (LPBYTE)buffer, sizeof(buffer), 
      &needed, &returned);
   printf("%ld bytes   %ld\n", needed, returned);
   ppi1 = (PRINTPROCESSOR_INFO_1 *)buffer;
   if (rc) {
      for (i=0; i<returned; i++) {
         printf("\042%s\042\n", ppi1[i].pName);
      }
   }
   else
      printf("EnumPrintProcessor() failed\n");

   printf("\nPrintProcessorDatatypes (WinPrint): ");
   rc = EnumPrintProcessorDatatypes(NULL, "WinPrint", 1, 
      (LPBYTE)buffer, sizeof(buffer), 
      &needed, &returned);
   printf("%ld bytes   %ld\n", needed, returned);
   dti1 = (DATATYPES_INFO_1 *)buffer;
   if (rc) {
      for (i=0; i<returned; i++) {
         printf("\042%s\042\n", dti1[i].pName);
      }
   }
   else
      printf("EnumPrintProcessorDatatypes() failed\n");


   
   return 0;
}
