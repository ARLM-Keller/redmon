/* Copyright (C) 1998, Ghostgum Software Pty Ltd.  All rights reserved.
  
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

/* enum.c */
/* List printers, ports, monitors etc. */
/* Russell Lang, 1998-01-05 */
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


char buffer[16384];
DWORD needed, returned;
int rc;
unsigned int i;
DRIVER_INFO_2 *dri2;
PRINTER_INFO_2 *pri2;
PORT_INFO_2 *pi2;
MONITOR_INFO_1 *mi;
DATATYPES_INFO_1 *dti1;
PRINTPROCESSOR_INFO_1 *ppi1;

int main(int argc, char *argv[])
{
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
   rc = EnumMonitors(NULL, 1, (LPBYTE)buffer, sizeof(buffer), 
      &needed, &returned);
   printf("%ld bytes   %ld\n", needed, returned);
   mi = (MONITOR_INFO_1 *)buffer;
   if (rc) {
      for (i=0; i<returned; i++) {
         printf("%d: \042%s\042\n", i, mi[i].pName);
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
