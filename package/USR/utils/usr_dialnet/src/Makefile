# all:usr_dialnet
# usr_dialnet:usr_dialnet.o
# 	$(CC) $(LDFLAGS) usr_dialnet.o -o usr_dialnet
# usr_dialnet.o:usr_dialnet.c
# 	$(CC) $(CFLAGS) -c usr_dialnet.c
# clean:
# 	rm *.o
# build usr_dialnet executable when user executes "make"
CFLAGS += -I./

MODULES=usr_dialnet gpio_opt
STARGET=usr_dialnet

#将SUBDIR中所有的.c文件作为目标.o
SUBDIR := $(MODULES)
SRC :=$(foreach dir, $(SUBDIR), $(shell ls $(dir)/*.c))
SRC += $(shell ls *.c)
OBJ := $(patsubst %.c, %.o, $(SRC) )

.PHONY: all clean

all: $(STARGET)

$(STARGET):$(OBJ)
	$(CC) -o $@ $(LDFLAG)  $(OBJ)

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@ 

clean:
	rm *.o