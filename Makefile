
SOURCES += external/00_simplebinder/v2link.cpp Main.cpp

INCFLAGS += -Iexternal/00_simplebinder

PROJECT_BASENAME = resourceRW

RC_LEGALCOPYRIGHT ?= Copyright (C) 2014-2015 miahmie; Copyright (C) 2019-2020 Julian Uy; See details of license at license.txt, or the source code location.

include external/tp_stubz/Rules.lib.make
