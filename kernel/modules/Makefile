obj-m := snf_lkm.o

all:
	make -C linux M=$(shell pwd) modules
	rm -r -f *.mod.c .*.cmd *.symvers *.o

clean:
	make -C linux M=$(shell pwd) clean
