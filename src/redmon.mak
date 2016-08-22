# Copyright (C) 1997-2001, Ghostgum Software Pty Ltd.  All rights reserved.
#  
#  This file is part of RedMon.
#  
#  This program is distributed with NO WARRANTY OF ANY KIND.  No author
#  or distributor accepts any responsibility for the consequences of using it,
#  or for whether it serves any particular purpose or works at all, unless he
#  or she says so in writing.  Refer to the RedMon Free Public Licence 
#  (the "Licence") for full details.
#  
#  Every copy of RedMon must include a copy of the Licence, normally in a 
#  plain ASCII text file named LICENCE.  The Licence grants you the right 
#  to copy, modify and redistribute RedMon, but only under certain conditions 
#  described in the Licence.  Among other things, the Licence requires that 
#  the copyright notice and this notice be preserved on all copies.

# redmon.mak
# Makefile for redmon.dll, a print monitor
# BC++ 4.5, MSVC++ 5.0, 6.0

# LANGUAGE=en (english), de (german), fr (french), se (swedish)
LANGUAGE=en

# VISUALC=0 for Borland C++ 4.5
# VISUALC=5 for MS Visual C++ 5.0
# VISUALC=6 for MS Visual C++ 5.0
VISUALC=5

# Edit DEVBASE as required
!if $(VISUALC)
#DEVBASE=f:\Program Files\Microsoft Visual Studio
DEVBASE=c:\devstudio
!else
DEVBASE=c:\bc45
!endif

# DEBUG=1 for debugging
DEBUG=1

# Shouldn't need editing below here

!if $(VISUALC)
# Microsoft Visual C++ 5.0
!if $(VISUALC) <= 5
COMPBASE = $(DEVBASE)\vc
RCOMP="$(DEVBASE)\sharedide\bin\rc"
!else
COMPBASE = $(DEVBASE)\vc98
RCOMP="$(DEVBASE)\common\msdev98\bin\rc"
!endif
COMPDIR=$(COMPBASE)\bin
INCDIR=$(COMPBASE)\include
LIBDIR=$(COMPBASE)\lib
LIBPRE=
LIBPOST=@lib2.rsp
LIBDEP=lib.rsp lib2.rsp
CCC=cl
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
HC=hcw /C /E
!else
COMPBASE=$(DEVBASE)
COMPDIR=$(COMPBASE)\bin
INCDIR=$(COMPBASE)\include
LIBDIR=$(COMPBASE)\lib
LIBPRE=-L$(COMPBASE)\lib
LIBPOST=
LIBDEP=
CCC=bcc32
EXEFLAG=-WE
DLLFLAG=-WD
OBJNAME=-o
#DEBUG=-v -DDEBUG
DEBUG=-DDEBUG
RCOMP=$(COMPDIR)\brcc32
RLINK=$(COMPDIR)\brc32
HC=$(COMPDIR)\hc31
!endif

CC="$(COMPDIR)\$(CCC)" $(CDEBUG) -I"$(INCDIR)"

all:	redmon95.dll redmonnt.dll redmon35.dll \
    redmon.hlp setup.exe unredmon.exe\
    redpr.exe redrun.exe redfile.exe enum.exe

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
	echo "$(LIBDIR)\comdlg32.lib" > lib.rsp
	echo "$(LIBDIR)\gdi32.lib" >> lib.rsp
	echo "$(LIBDIR)\user32.lib" >> lib.rsp
	echo "$(LIBDIR)\winspool.lib" >> lib.rsp
	echo "$(LIBDIR)\advapi32.lib" >> lib.rsp

lib2.rsp: redmon.mak
	echo /link > lib2.rsp
	echo "$(LIBDIR)\comdlg32.lib" >> lib2.rsp
	echo "$(LIBDIR)\gdi32.lib" >> lib2.rsp
	echo "$(LIBDIR)\user32.lib" >> lib2.rsp
	echo "$(LIBDIR)\winspool.lib" >> lib2.rsp
	echo "$(LIBDIR)\advapi32.lib" >> lib2.rsp

setup.exe: setup.c setup.h redmon.h redmonrc.h setup.res $(LIBDEP)
!if $(VISUALC)
	$(CC) -c $(EXEFLAG) setup.c
	"$(COMPDIR)\link" $(DEBUGLNK) /DEF:setup.def /OUT:setup.exe setup.obj @lib.rsp setup.res
!else
	$(CC) $(EXEFLAG) $(LIBPRE) setup.c
	$(RLINK) setup.res setup.exe
!endif

unredmon.exe: unredmon.c unredmon.res unredmon.h redmon.h redmonrc.h $(LIBDEP)
!if $(VISUALC)
	$(CC) -c $(EXEFLAG) unredmon.c
	"$(COMPDIR)\link" $(DEBUGLNK) /DEF:unredmon.def /OUT:unredmon.exe unredmon.obj @lib.rsp unredmon.res
!else
	$(CC) $(EXEFLAG) -L$(LIBDIR) unredmon.c
	$(RLINK) unredmon.res unredmon.exe
!endif

doc2rtf.exe: doc2rtf.c $(LIBDEP)
	$(CC) $(LIBPRE) doc2rtf.c $(LIBPOST) 


redmon.hlp: $(LANGUAGE)\redmon.txt $(LANGUAGE)\redmon.hpj doc2rtf.exe
	doc2rtf $(LANGUAGE)\redmon.txt $(LANGUAGE)\redmon.rtf redmon.cnt
	cd $(LANGUAGE)
	$(HC) redmon.hpj
	cd ..
	copy $(LANGUAGE)\redmon.hlp redmon.hlp
	
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

redmon95.dll: redmon95.obj port95.obj redmon.res $(LIBDEP)
!if $(VISUALC)
	"$(COMPDIR)\link" $(DEBUGLNK) /DLL /DEF:redmon95.def /OUT:redmon95.dll redmon95.obj port95.obj @lib.rsp redmon.res
!else
	$(CC) $(DLLFLAG) -L$(LIBDIR) -eredmon95.dll redmon95.obj
	$(RLINK) redmon.res redmon95.dll
!endif


# NT 4/5 DLL
redmonnt.obj: redmon.c portmon.h redmon.h redmonrc.h
	$(CC) -c $(DLLFLAG) -DUNICODE -DNT50 -DNT40 $(OBJNAME)redmonnt.obj redmon.c

portnt.obj: portmon.c portmon.h redmon.h redmonrc.h
	$(CC) -c $(DLLFLAG) -DUNICODE -DNT50 -DNT40 $(OBJNAME)portnt.obj portmon.c

redmonnt.dll: redmonnt.obj portnt.obj redmon.res $(LIBDEP) redmonnt.def
!if $(VISUALC)
	"$(COMPDIR)\link" $(DEBUGLNK) /DLL /DEF:redmonnt.def /OUT:redmonnt.dll redmonnt.obj portnt.obj @lib.rsp redmon.res
!else
	$(CC) $(DLLFLAG) -L$(LIBDIR) -eredmonnt.dll redmonnt.obj
	$(RLINK) redmon.res redmonnt.dll
!endif


# NT 3.5 DLL
redmon35.obj: redmon.c portmon.h redmon.h redmonrc.h
	$(CC) -c $(DLLFLAG) -DUNICODE -DNT35 $(OBJNAME)redmon35.obj redmon.c

port35.obj: portmon.c portmon.h redmon.h redmonrc.h
	$(CC) -c $(DLLFLAG) -DUNICODE -DNT35 $(OBJNAME)port35.obj portmon.c

redmon35.dll: redmon35.obj port35.obj redmon.res $(LIBDEP) redmon35.def
!if $(VISUALC)
	"$(COMPDIR)\link" $(DEBUGLNK) /DLL /DEF:redmon35.def /OUT:redmon35.dll redmon35.obj port35.obj @lib.rsp redmon.res
!else
	$(CC) $(DLLFLAG) -L$(LIBDIR) -eredmon35.dll redmon35.obj
	$(RLINK) redmon.res redmon35.dll
!endif


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
!if $(VISUALC)
	"$(COMPDIR)\link" $(DEBUGLNK) /OUT:mon2.exe redmon2.obj @lib.rsp redmon.res
!else
	$(CC) $(EXEFLAG) $(LIBPRE) -emon2.exe redmon2.obj $(LIBPOST) 
	$(RLINK) redmon.res mon2.exe
!endif

redmon4.obj: redmon.c redmon.h redmonrc.h
	$(CC) -c $(EXEFLAG) -DMAKEEXE -DUNICODE -DNT40 $(OBJNAME)redmon4.obj redmon.c

mon4.exe: redmon4.obj redmon.res lib.rsp
!if $(VISUALC)
	"$(COMPDIR)\link" $(DEBUGLNK) /OUT:mon4.exe redmon4.obj @lib.rsp redmon.res
!else
	$(CC) $(EXEFLAG) $(LIBPRE) -emon4.exe redmon4.obj $(LIBPOST) 
	$(RLINK) redmon.res mon4.exe
!endif

redmon5.obj: redmon.c redmon.h redmonrc.h
	$(CC) -c $(EXEFLAG) -DMAKEEXE -DUNICODE -DNT50 $(OBJNAME)redmon5.obj redmon.c

mon5.exe: redmon5.obj redmon.res lib.rsp
!if $(VISUALC)
	"$(COMPDIR)\link" $(DEBUGLNK) /OUT:mon5.exe redmon5.obj @lib.rsp redmon.res
!else
	$(CC) $(EXEFLAG) $(LIBPRE) -emon5.exe redmon5.obj $(LIBPOST) 
	$(RLINK) redmon.res mon5.exe
!endif

cleanlanguage:
	-del redmon.rc
	-del redmon.res
	-del setup.rc
	-del setup.res
	-del unredmon.rc
	-del unredmon.res
	-del $(LANGUAGE)\redmon.rtf
	-del redmon.hlp

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
	-del unredmon.obj
	-del unredmon.lib
	-del unredmon.exp
	-del unredmon.map
	-del unredmon.tr2
	-del unredmon.ilk
	-del unredmon.pdb
	-del doc2rtf.obj
	-del doc2rtf.exe
	-del doc2rtf.ilk
	-del doc2rtf.pdb
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
	-del vc50.pdb
	-del vc60.pdb

veryclean: clean
	-del redmon35.dll
	-del redmon95.dll
	-del redmonnt.dll
	-del redmon.hlp
	-del redmon.cnt
	-del setup.exe
	-del unredmon.exe
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
	copy redmon.hlp ..
	copy redmon35.dll ..
	copy redmon95.dll ..
	copy redmonnt.dll ..
	copy setup.exe ..
	copy unredmon.exe ..
	copy redpr.exe ..
	copy redrun.exe ..
	copy redfile.exe ..
	copy enum.exe ..
	cd ..
	-del src.zip
	zip -9 -r src -@ < src.txt
	-del redmon.zip
	zip -9 -r redmon -@ < src\dist.txt
	zip -z redmon < FILE_ID.DIZ
	-del src.txt
	cd src

betazip: 
	-del redmonb.zip
	zip -9 redmonb README.TXT LICENCE redmon95.dll redmonnt.dll
	zip -9 redmonb redmon.hlp setup.exe unredmon.exe redpr.exe redrun.exe redfile.exe
	zip -z redmonb < FILE_ID.DIZ

