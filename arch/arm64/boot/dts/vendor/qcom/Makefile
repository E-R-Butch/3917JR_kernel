dtb-y += base.dtb
dtb-y += dtbo-00.dtb dtbo-01.dtbo dtbo-02.dtb dtbo-03.dtb          dtbo-04.dtb dtbo-05.dtb dtbo-06.dtb dtbo-07.dtb          dtbo-08.dtb dtbo-09.dtb dtbo-10.dtb dtbo-11.dtb          dtbo-12.dtb dtbo-13.dtb dtbo-14.dtb dtbo-15.dtb

dtb-y := $(patsubst %.dtbo,%.dtb,$(dtb-y))
always := $(dtb-y)
DTC_FLAGS += -@
