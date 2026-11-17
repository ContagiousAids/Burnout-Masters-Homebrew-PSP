TARGET = BurnoutMastersPSP
OBJS = src/main.o
INCDIR =
CFLAGS = -O2 -G0 -Wall -Wextra -fno-strict-aliasing
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBS = -lpspgum -lpspgu -lpspdebug -lpspdisplay -lpspctrl -lpspiofilemgr -lpspkernel -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Burnout Masters PSP
PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
