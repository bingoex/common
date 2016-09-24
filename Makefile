
COMM = ../common
BASE = ../common/base
C_COMM = ../common/comm/src

CUR_DIR=$(shell pwd)


ifeq ($(ARCH), 32)
	#CFLAGS += -march=pentium4 -m32 -pthread
	CFLAGS += -m32 -pthread
else
	CFLAGS += -m64 -pthread
endif

CFLAGS += -g3 -ggdb -Wall -Wno-write-strings # -o2 -fno-inline -Werror
CFLAGS += -pipe -fno-ident -FPIC -shared # -z defs

INC += -I./ -I$(COMM) -I$(BASE)

C_COMM_LIB = $(C_COMM)/cm_lib.a -I$(C_COMM) 

LIB += -lpthread -ldl

TARGET=libcommon.a
OBJ = $(COMM)/test.o \
	  $(BASE)/misc.o \
	  $(BASE)/notify.o \
	  $(BASE)/procmon.o \
	  $(BASE)/shmcommu.o \
	  

all : $(TARGET)

$(TARGET) : $(OBJ)
	ar crs $@ $^ 
	chmod +x $@

%.o : %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC) $(C_COMM_LIB)

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $< $(INC) $(C_COMM_LIB)

clean : 
	rm -f $(OBJ) $(TARGET)

echoall :
	@echo $(ARCH);
	@echo $(CFLAGS);
	@echo $(INC);
	@echo $(LIB);
	@echo $(TARGET);
	@echo $(CXX);
	@echo $(CC);

