
COMM = ../common

CUR_DIR=$(shell pwd)


ifeq ($(ARCH), 32)
	#CFLAGS += -march=pentium4 -m32 -pthread
	CFLAGS += -m32 -pthread
else
	CFLAGS += -m64 -pthread
endif

CFLAGS += -g3 -ggdb -Wall -Wno-write-strings # -o2 -fno-inline -Werror
CFLAGS += -pipe -fno-ident -FPIC -shared # -z defs

INC += -I./ -I$(COMM) 

LIB += -lpthread -ldl

TARGET=libcommon.a
OBJ = $(COMM)/test.o \
	  $(COMM)/socket.o \
	  $(COMM)/misc.o \
	  $(COMM)/notify.o \
	  $(COMM)/crc32.o \
	  $(COMM)/keygen.o \

all : $(TARGET)

$(TARGET) : $(OBJ)
	ar crs $@ $^ 
	chmod +x $@

%.o : %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC) 

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $< $(INC)

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

