ccflags-y += -std=gnu99 -Wno-declaration-after-statement

CFILES = driver.c

obj-m := mymodule.o
mymodule-objs := $(CFILES:.c=.o)

build:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
install:
	echo 'KERNEL=="hoge[0-9]*", GROUP="root", MODE="0666"' >> /etc/udev/rules.d/mymodule.rules
	insmod mymodule.ko
uninstall:
	rmmod mymodule.ko
	rm -v /etc/udev/rules.d/mymodule.rules
