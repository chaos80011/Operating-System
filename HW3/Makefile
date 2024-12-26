CC = gcc
OBJ := kfetch.o
obj-m += kfetch_mod_312551006.o
EXE = kfetch

all: $(EXE)
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

$(EXE): $(OBJ)
	$(CC) $(OBJ) -o $(EXE)

load: kfetch_mod_312551006.ko
	sudo insmod kfetch_mod_312551006.ko

unload:
	sudo rmmod kfetch_mod_312551006

kfetch.o: kfetch.h kfetch.c
	$(CC) -c kfetch.c

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm kfetch

