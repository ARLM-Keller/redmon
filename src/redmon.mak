# Copyright (C) 1997-2012, Ghostgum Software Pty Ltd.  All rights reserved.
#  
#  This file is part of RedMon.
#  
#  This software is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
#
#  This software is distributed under licence and may not be copied, modified
#  or distributed except as expressly authorised under the terms of the
#  LICENCE.


# redmon.mak
# Makefile for redmon.dll, a print monitor
# MSVC++ 9 (2008) and 10 (2010)
# Older versions may compile 32-bit, but not 64-bit version.
# Apart from the compiler, you will also need to install the 
# Windows 7.1 Driver Kit (or perhaps the older Windows DDK)
# and HTML Help compiler.

# LANGUAGE=en (english), de (german), fr (french), se (swedish)
LANGUAGE=en

# DEBUG=1 for debugging
DEBUG=0

# VISUALC=0 for Borland C++ 4.5			[untested]
# VISUALC=5 for MS Visual C++ 5.0		[untested]
# VISUALC=6 for MS Visual C++ 6.0		[untested]
# VISUALC=7 for MS Visual C++ 7.0 (.NET)	[untested]
# VISUALC=71 for MS Visual C++ 7.1 (.NET 2003)	[untested]
# VISUALC=8 for MS Visual C++ 8 (.NET 2005)	[x64 doesn't compile, untested]
# VISUALC=9 for MS Visual C++ 9 (Studio 2008)	[untested, compiles]
# VISUALC=10 for MS Visual C++ 10 (Studio 2010)

!if "$(_NMAKE_VER)" == "8.00.50727.42"
VISUALC=8
!endif
!if "$(_NMAKE_VER)" == "8.00.50727.762"
VISUALC=8
!endif
!if "$(_NMAKE_VER)" == "9.00.30729.01"
VISUALC=9
!endif
!if "$(_NMAKE_VER)" == "9.00.21022.08"
VISUALC=9
!endif
!if "$(_NMAKE_VER)" == "10.00.30319.01"
VISUALC=10
!endif
!ifndef VISUALC
VISUALC=10
!endif


# Edit DEVBASE as required
!if $(VISUALC) == 5
DEVBASE=c:\devstudio
!endif
!if $(VISUALC) == 6
DEVBASE=c:\Program Files\Microsoft Visual Studio
!endif
!if $(VISUALC) == 7
DEVBASE=c:\Program Files\Microsoft Visual Studio .NET
!endif
!if $(VISUALC) == 71
DEVBASE=c:\Program Files\Microsoft Visual Studio .NET 2003
!endif
!if $(VISUALC) == 8
DEVBASE=c:\Program Files\Microsoft Visual Studio 8
!endif
!if $(VISUALC) == 9
DEVBASE=c:\Program Files\Microsoft Visual Studio 9.0
!endif
!if $(VISUALC) == 10
DEVBASE=c:\Program Files\Microsoft Visual Studio 10.0
!endif


!if $(VISUALC) >= 9
# Windows Driver Kit 7.1.0
DDKINC=C:\WinDDK\7600.16385.1\inc\win7
DDKLIB=C:\WinDDK\7600.16385.1\lib\win7\i386
!else
# Windows 2003 SP1 DDK
DDKINC=C:\WinDDK\3790.1830\inc\wxp
DDKLIB=C:\WinDDK\3790.1830\lib\wxp\i386
!endif



# Shouldn't need editing below here

!if $(VISUALC) <= 5
COMPBASE = $(DEVBASE)\vc
RCOMP="$(DEVBASE)\sharedide\bin\rc"
!endif

!if $(VISUALC) == 6
COMPBASE = $(DEVBASE)\vc98
RCOMP="$(DEVBASE)\common\msdev98\bin\rc"
!endif

!if (($(VISUALC) == 7) || ($(VISUALC) == 71))
COMPBASE = $(DEVBASE)\Vc7
RCOMP="$(DEVBASE)\Vc7\bin\rc"
!endif

!if $(VISUALC) == 8
COMPBASE = $(DEVBASE)\Vc
RCOMP="$(DEVBASE)\Vc\bin\rc"
VFLAGS=/wd4996
#PLATLIBDIR64=$(COMPBASE)\PlatformSDK\lib\AMD64
PLATLIBDIR64=C:\WinDDK\3790.1830\lib\wnet\AMD64
!endif

!if $(VISUALC) == 9
COMPBASE = $(DEVBASE)\Vc
RCOMP="c:\Program Files\Microsoft SDKs\Windows\v6.0A\bin\rc"
VFLAGS=/wd4996
PLATLIBDIR64=c:\Program Files\Microsoft SDKs\Windows\v6.0A\lib\x64
!endif

!if $(VISUALC) == 10
COMPBASE = $(DEVBASE)\Vc
RCOMP="c:\Program Files\Microsoft SDKs\Windows\v7.0A\bin\rc"
VFLAGS=/wd4996
PLATLIBDIR64=c:\Program Files\Microsoft SDKs\Windows\v7.0A\lib\x64
!endif

COMPDIR=$(COMPBASE)\bin
INCDIR=$(COMPBASE)\include
LIBDIR=$(COMPBASE)\lib
LIBPRE=
LIBPOST=@lib2.rsp
LIBDEP=lib.rsp lib2.rsp lib64.rsp
CCC=cl
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LIBDIR64=$(COMPBASE)\lib\amd64
EXEFLAG=
DLLFLAG=/LD
OBJNAME=/Fo
!if $(DEBUG)
CDEBUG=/Zi -DDEBUG
DEBUGLNK=/DEBUG
!else
CDEBUG=
DEBUGLNK=
!endif
RLINK=$(RCOMP)


!if ($(VISUALC) == 10)
PLATLIBDIR=c:\Prograil Files\Microsoft SDKs\Windows\v&.0A\Lib
!else if ($(VISUALC) == 8) || ($(VISUALC) == 9)
PLATLIBDIR=$(COMPBASE)\PlatformSDK\lib
!else if (($(VISUALC) == 7) || ($(VISUALC) == 71))
#PLATLIBDIR=$(COMPBASE)\PlatformSDK\lib
PLATLIBDIR=C:\WinDDK\7600.16385.1\lib\win7\i386
!else
PLATLIBDIR=$(LIBDIR)
!endif

CC="$(COMPDIR)\$(CCC)" $(CDEBUG) /nologo -I"$(INCDIR)" -I"$(DDKINC)" $(VFLAGS)
CC64="$(COMPDIR64)\$(CCC)" $(CDEBUG) -I"$(INCDIR)" -I"$(DDKINC)" $(VFLAGS)

all: redmon32.dll \
    redmon.chm setup.exe unredmon.exe\
    redpr.exe redrun.exe redfile.exe enum.exe\
    redmon64.dll setup64.exe unredmon64.exe
# redmonnt.dll redmon95.dll redmon35.dll  # these might still work but haven't been tested

.c.obj:
	$(CC) -c $*.c

# common to 95 and NT
redmon.res: $(LANGUAGE)\redmon.rc redmon.h redmonrc.h
	copy $(LANGUAGE)\redmon.rc redmon.rc
	$(RCOMP) -i"$(INCDIR)" -r redmon.rc

setup.res: $(LANGUAGE)\setup.rc setup.h redmonrc.h
	copy $(LANGUAGE)\setup.rc setup.rc
	$(RCOMP) -i"$(INCDIR)" -r setup.rc

unredmon.res: $(LANGUAGE)\unredmon.rc unredmon.h redmonrc.h
	copy $(LANGUAGE)\unredmon.rc unredmon.rc
	$(RCOMP) -i"$(INCDIR)" -r unredmon.rc

lib.rsp: redmon.mak
	echo "$(DDKLIB)\comdlg32.lib" > lib.rsp
	echo "$(DDKLIB)\gdi32.lib" >> lib.rsp
	echo "$(DDKLIB)\user32.lib" >> lib.rsp
	echo "$(DDKLIB)\winspool.lib" >> lib.rsp
	echo "$(DDKLIB)\advapi32.lib" >> lib.rsp
	echo "$(DDKLIB)\spoolss.lib" >> lib.rsp
	echo "$(DDKLIB)\psapi.lib" >> lib.rsp
	echo "$(DDKLIB)\userenv.lib" >> lib.rsp
	echo "$(DDKLIB)\htmlhelp.lib" >> lib.rsp

lib2.rsp: redmon.mak
	echo /link > lib2.rsp
	echo "$(DDKLIB)\comdlg32.lib" >> lib2.rsp
	echo "$(DDKLIB)\gdi32.lib" >> lib2.rsp
	echo "$(DDKLIB)\user32.lib" >> lib2.rsp
	echo "$(DDKLIB)\winspool.lib" >> lib2.rsp
	echo "$(DDKLIB)\advapi32.lib" >> lib2.rsp

lib64.rsp: redmon.mak
	echo "$(PLATLIBDIR64)\comdlg32.lib" > lib64.rsp
	echo "$(PLATLIBDIR64)\gdi32.lib" >> lib64.rsp
	echo "$(PLATLIBDIR64)\kernel32.lib" >> lib64.rsp
	echo "$(PLATLIBDIR64)\user32.lib" >> lib64.rsp
	echo "$(PLATLIBDIR64)\winspool.lib" >> lib64.rsp
	echo "$(PLATLIBDIR64)\advapi32.lib" >> lib64.rsp
	echo "$(PLATLIBDIR64)\psapi.lib" >> lib64.rsp
	echo "$(PLATLIBDIR64)\userenv.lib" >> lib64.rsp
	echo "$(PLATLIBDIR64)\htmlhelp.lib" >> lib64.rsp
	echo "/NODEFAULTLIB:libcmt" >> lib64.rsp
	echo "$(LIBDIR64)\libcmt.lib" >> lib64.rsp

setup.exe: setup.c setup.h redmon.h redmonrc.h setup.res setup.def setup_x86.manifest $(LIBDEP)
	$(CC) -c $(EXEFLAG) setup.c
	"$(COMPDIR)\link" $(DEBUGLNK) /DEF:setup.def /OUT:setup.exe setup.obj @lib.rsp setup.res
	mt -nologo -manifest setup_x86.manifest -outputresource:setup.exe;#1

unredmon.exe: unredmon.c unredmon.res unredmon.h redmon.h redmonrc.h unredmon.def unredmon_x86.manifest $(LIBDEP)
	$(CC) -c $(EXEFLAG) unredmon.c
	"$(COMPDIR)\link" $(DEBUGLNK) /DEF:unredmon.def /OUT:unredmon.exe unredmon.obj @lib.rsp unredmon.res
	mt -nologo -manifest unredmon_x86.manifest -outputresource:unredmon.exe;#1

!if $(VISUALC) >= 8
setup64.exe: setup.c setup.h redmon.h redmonrc.h setup.res setup_x64.manifest $(LIBDEP)
	$(CC64) -c $(EXEFLAG) $(OBJNAME)setup64.obj setup.c
	"$(COMPDIR64)\link" $(DEBUGLNK) /OUT:setup64.exe setup64.obj @lib64.rsp setup.res
	mt -nologo -manifest setup_x64.manifest -outputresource:setup64.exe;#1

unredmon64.exe: unredmon.c unredmon.res unredmon.h redmon.h redmonrc.h unredmon_x64.manifest $(LIBDEP)
	$(CC64) -c $(EXEFLAG) $(OBJNAME)unredmon64.obj unredmon.c
	"$(COMPDIR64)\link" $(DEBUGLNK) /OUT:unredmon64.exe unredmon64.obj @lib64.rsp unredmon.res
	mt -nologo -manifest unredmon_x64.manifest -outputresource:unredmon64.exe;#1
!else
# Don't build 64-bit with older compilers
setup64.exe: setup.c
unredmon64.exe: umredmon.c
!endif

doc2hhp.exe: doc2hhp.c $(LIBDEP)
	$(CC) $(LIBPRE) doc2hhp.c $(LIBPOST) 

redmon.chm: $(LANGUAGE)\redmon.txt doc2hhp.exe
	-mkdir htmlhelp
# Currently no images in help file
#	-copy $(LANGUAGE)\binary\*.gif htmlhelp
	copy $(LANGUAGE)\redmon.txt htmlhelp
	cd htmlhelp
	..\doc2hhp redmon.txt redmon.hhp
	-"C:\Program Files\HTML Help Workshop\hhc.exe" redmon.hhp
	cd ..
	copy htmlhelp\redmon.chm .

# doc2html.c sources are in GSview
doc2html.exe: doc2html.c $(LIBDEP)
	$(CC) $(LIBPRE) doc2html.c $(LIBPOST) 

html: $(LANGUAGE)\redmon.txt doc2html.exe
	doc2html $(LANGUAGE)\redmon.txt redmon.html

# 95/98 DLL
redmon95.obj: redmon.c portmon.h redmon.h redmonrc.h
	$(CC) -c $(DLLFLAG) $(OBJNAME)redmon95.obj redmon.c

port95.obj: portmon.c portmon.h redmon.h redmonrc.h
	$(CC) -c $(DLLFLAG) $(OBJNAME)port95.obj portmon.c

redmon95.dll: redmon95.obj port95.obj redmon.res redmon95.def $(LIBDEP)
	"$(COMPDIR)\link" $(DEBUGLNK) /DLL /DEF:redmon95.def /OUT:redmon95.dll redmon95.obj port95.obj @lib.rsp redmon.res


# NT 64-bit DLL
!if $(VISUALC) >= 8
redmon64.obj: redmon.c portmon.h redmon.h redmonrc.h
	$(CC64) -c $(DLLFLAG) -DUNICODE -DNT50 $(OBJNAME)redmon64.obj redmon.c

port64.obj: portmon.c portmon.h redmon.h redmonrc.h
	$(CC64) -c $(DLLFLAG) -DUNICODE -DNT50 $(OBJNAME)port64.obj portmon.c

redmon64.dll: redmon64.obj port64.obj redmon.res $(LIBDEP)
	"$(COMPDIR64)\link" $(DEBUGLNK) /DLL /OUT:redmon64.dll redmon64.obj port64.obj @lib64.rsp redmon.res
!else
# Don't build 64-bit with older compilers
redmon64.dll: redmon.c
!endif

# NT 5 DLL (Windows 2000 and later)
redmon32.obj: redmon.c portmon.h redmon.h redmonrc.h
	$(CC) -c $(DLLFLAG) -DUNICODE -DNT50 $(OBJNAME)redmon32.obj redmon.c

port32.obj: portmon.c portmon.h redmon.h redmonrc.h
	$(CC) -c $(DLLFLAG) -DUNICODE -DNT50 $(OBJNAME)port32.obj portmon.c

redmon32.dll: redmon32.obj port32.obj redmon.res $(LIBDEP) redmon32.def
	"$(COMPDIR)\link" $(DEBUGLNK) /DLL /DEF:redmon32.def /OUT:redmon32.dll redmon32.obj port32.obj @lib.rsp redmon.res



# NT 4/5 DLL (Windows NT 4 / Windows 2000 and later)
redmonnt.obj: redmon.c portmon.h redmon.h redmonrc.h
	$(CC) -c $(DLLFLAG) -DUNICODE -DNT50 -DNT40 $(OBJNAME)redmonnt.obj redmon.c

portnt.obj: portmon.c portmon.h redmon.h redmonrc.h
	$(CC) -c $(DLLFLAG) -DUNICODE -DNT50 -DNT40 $(OBJNAME)portnt.obj portmon.c

redmonnt.dll: redmonnt.obj portnt.obj redmon.res $(LIBDEP) redmonnt.def
	"$(COMPDIR)\link" $(DEBUGLNK) /DLL /DEF:redmonnt.def /OUT:redmonnt.dll redmonnt.obj portnt.obj @lib.rsp redmon.res


# NT 3.5 DLL
redmon35.obj: redmon.c portmon.h redmon.h redmonrc.h
	$(CC) -c $(DLLFLAG) -DUNICODE -DNT35 $(OBJNAME)redmon35.obj redmon.c

port35.obj: portmon.c portmon.h redmon.h redmonrc.h
	$(CC) -c $(DLLFLAG) -DUNICODE -DNT35 $(OBJNAME)port35.obj portmon.c

redmon35.dll: redmon35.obj port35.obj redmon.res $(LIBDEP) redmon35.def
	"$(COMPDIR)\link" $(DEBUGLNK) /DLL /DEF:redmon35.def /OUT:redmon35.dll redmon35.obj port35.obj @lib.rsp redmon.res


# RED PRint
redpr.exe: redpr.c $(LIBDEP)
	$(CC) $(LIBPRE) redpr.c $(LIBPOST) 

# REDirect and RUN
redrun.exe: redrun.c $(LIBDEP)
	$(CC) $(LIBPRE) redrun.c $(LIBPOST) 

# REDirect to FILE lib2.rsp
redfile.exe: redfile.c $(LIBDEP)
	$(CC) $(LIBPRE) redfile.c $(LIBPOST) 

# testing only
enum.exe: enum.c $(LIBDEP)
	$(CC) $(LIBPRE) enum.c $(LIBPOST) 

redmon2.obj: redmon.c redmon.h redmonrc.h
	$(CC) -c $(EXEFLAG) -DMAKEEXE $(OBJNAME)redmon2.obj redmon.c

mon2.exe: redmon2.obj redmon.res $(LIBDEP)
	"$(COMPDIR)\link" $(DEBUGLNK) /OUT:mon2.exe redmon2.obj @lib.rsp redmon.res

redmon4.obj: redmon.c redmon.h redmonrc.h
	$(CC) -c $(EXEFLAG) -DMAKEEXE -DUNICODE -DNT40 $(OBJNAME)redmon4.obj redmon.c

mon4.exe: redmon4.obj redmon.res lib.rsp
	"$(COMPDIR)\link" $(DEBUGLNK) /OUT:mon4.exe redmon4.obj @lib.rsp redmon.res

redmon5.obj: redmon.c redmon.h redmonrc.h
	$(CC) -c $(EXEFLAG) -DMAKEEXE -DUNICODE -DNT50 $(OBJNAME)redmon5.obj redmon.c

mon5.exe: redmon5.obj redmon.res lib.rsp
	"$(COMPDIR)\link" $(DEBUGLNK) /OUT:mon5.exe redmon5.obj @lib.rsp redmon.res

cleanlanguage:
	-del redmon.rc
	-del redmon.res
	-del setup.rc
	-del setup.res
	-del unredmon.rc
	-del unredmon.res
	-del $(LANGUAGE)\redmon.rtf
	-del redmon.chm

clean:	cleanlanguage
	-del redmon.cnt
	-del redmon.rws
	-del redmon35.tr2
	-del redmon35.obj
	-del redmon35.lib
	-del redmon35.exp
	-del redmon35.map
	-del redmon35.ilk
	-del redmon35.pdb
	-del redmon95.tr2
	-del redmon95.obj
	-del redmon95.lib
	-del redmon95.exp
	-del redmon95.map
	-del redmon95.ilk
	-del redmon95.pdb
	-del redmonnt.tr2
	-del redmonnt.obj
	-del redmonnt.lib
	-del redmonnt.exp
	-del redmonnt.map
	-del redmonnt.ilk
	-del redmonnt.pdb
	-del redmon32.obj
	-del redmon32.lib
	-del redmon32.exp
	-del redmon32.map
	-del redmon32.ilk
	-del redmon32.pdb
	-del redmon64.obj
	-del redmon64.lib
	-del redmon64.exp
	-del redmon64.map
	-del redmon64.ilk
	-del redmon64.pdb
	-del port35.tr2
	-del port35.obj
	-del port35.lib
	-del port35.exp
	-del port35.map
	-del port35.ilk
	-del port35.pdb
	-del port95.tr2
	-del port95.obj
	-del port95.lib
	-del port95.exp
	-del port95.map
	-del port95.ilk
	-del port95.pdb
	-del portnt.tr2
	-del portnt.obj
	-del portnt.lib
	-del portnt.exp
	-del portnt.map
	-del portnt.ilk
	-del portnt.pdb
	-del port32.obj
	-del port32.lib
	-del port32.exp
	-del port32.map
	-del port32.ilk
	-del port32.pdb
	-del port64.obj
	-del port64.lib
	-del port64.exp
	-del port64.map
	-del port64.ilk
	-del port64.pdb
	-del redmon2.obj
	-del redmon4.obj
	-del redmon5.obj
	-del setup.obj
	-del setup.lib
	-del setup.exp
	-del setup.map
	-del setup.tr2
	-del setup.ilk
	-del setup.pdb
	-del setup64.obj
	-del setup64.lib
	-del setup64.exp
	-del setup64.map
	-del setup64.tr2
	-del setup64.ilk
	-del setup64.pdb
	-del unredmon.obj
	-del unredmon.lib
	-del unredmon.exp
	-del unredmon.map
	-del unredmon.tr2
	-del unredmon.ilk
	-del unredmon.pdb
	-del unredmon64.obj
	-del unredmon64.lib
	-del unredmon64.exp
	-del unredmon64.map
	-del unredmon64.tr2
	-del unredmon64.ilk
	-del unredmon64.pdb
	-del doc2hhp.obj
	-del doc2hhp.exe
	-del doc2hhp.ilk
	-del doc2hhp.pdb
	-del doc2html.obj
	-del doc2html.exe
	-del doc2html.ilk
	-del doc2html.pdb
	-del mon2.exe
	-del mon2.ilk
	-del mon2.pdb
	-del mon4.exe
	-del mon4.ilk
	-del mon4.pdb
	-del mon5.exe
	-del mon5.ilk
	-del mon5.pdb
	-del tdconfig.td2
	-del redpr.obj
	-del redpr.tr2
	-del redpr.ilk
	-del redpr.pdb
	-del redrun.obj
	-del redrun.tr2
	-del redrun.ilk
	-del redrun.pdb
	-del redfile.obj
	-del redfile.tr2
	-del redfile.ilk
	-del redfile.pdb
	-del enum.obj
	-del enum.ilk
	-del enum.pdb
	-del lib.rsp
	-del lib2.rsp
	-del lib64.rsp
	-del vc50.pdb
	-del vc60.pdb
	-del vc70.pdb
	-del vc80.pdb
	-del vc90.pdb
	-del vc100.pdb
	-rmdir /s /q htmlhelp
	-mkdir htmlhelp

veryclean: clean
	-del redmon35.dll
	-del redmon95.dll
	-del redmonnt.dll
	-del redmon32.dll
	-del redmon64.dll
	-del redmon.chm
	-del redmon.cnt
	-del setup.exe
	-del unredmon.exe
	-del setup64.exe
	-del unredmon64.exe
	-del redpr.exe
	-del redrun.exe
	-del redfile.exe
	-del enum.exe
	-del redmon.html

zip:
	copy src.txt ..
        copy README.TXT ..
	copy FILE_ID.DIZ ..
	copy LICENCE ..
	copy redmon.chm ..
#	copy redmon35.dll ..
#	copy redmon95.dll ..
#	copy redmonnt.dll ..
	copy redmon32.dll ..
	copy redmon64.dll ..
	copy setup.exe ..
	copy setup64.exe ..
	copy unredmon.exe ..
	copy unredmon64.exe ..
	copy redpr.exe ..
	copy redrun.exe ..
	copy redfile.exe ..
	copy enum.exe ..
	cd ..
	-del src.zip
	zip -9 -r -X src -@ < src.txt
	-del redmon.zip
	zip -9 -r -X redmon -@ < src\dist.txt
	zip -z redmon < FILE_ID.DIZ
	-del src.txt
	cd src

betazip: 
	-del redmonb.zip
	zip -9 redmonb README.TXT LICENCE redmon.chm 
	zip -9 redmonb redmon32.dll setup.exe unredmon.exe  
	zip -9 redmonb redmon64.dll setup32.exe unredmon32.exe
	zip -9 redmonb redpr.exe redrun.exe redfile.exe
	zip -z redmonb < FILE_ID.DIZ

