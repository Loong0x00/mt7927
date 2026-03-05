obj-m := mlo_tx_dist.o

KVER := $(shell uname -r)
KDIR := /usr/lib/modules/$(KVER)/build
MT76_SRC := /usr/src/mediatek-mt7927-2.1/mt76

# Include mt76 headers so we can access mt792x structures
ccflags-y := -I$(MT76_SRC) -I$(MT76_SRC)/mt7925

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load:
	sudo insmod mlo_tx_dist.ko

unload:
	-sudo rmmod mlo_tx_dist

reload: unload load

stats:
	@echo "Primary TX: $$(cat /sys/module/mlo_tx_dist/parameters/total_primary)"
	@echo "Secondary TX: $$(cat /sys/module/mlo_tx_dist/parameters/total_secondary)"
	@echo "Ratio: $$(cat /sys/module/mlo_tx_dist/parameters/distribute_ratio)"

.PHONY: all clean load unload reload stats
