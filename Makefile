obj-m += dt_i2c.o

all: module dt lcd_write
	echo Builded Device Tree Overlay and kernel module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
dt: testoverlay.dts
	dtc -@ -I dts -O dtb -o testoverlay.dtbo testoverlay.dts
lcd_write: lcd_write.c 
	gcc -Wall -O2 -o lcd_write lcd_write.c
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf testoverlay.dtbo lcd_write