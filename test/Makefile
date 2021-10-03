obj-m+=timerdev.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	$(CC) -g timercontroller.c -o timerc
helper:
	$(CC) -g timercontroller.c -o timerc
driver:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm timerc
clean.helper:
	rm timerc
clean.driver:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
