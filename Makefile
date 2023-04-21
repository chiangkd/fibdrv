CONFIG_MODULE_SIG = n
TARGET_MODULE := fibdrv

obj-m := $(TARGET_MODULE).o
$(TARGET_MODULE)-objs := src/fibdrv.o src/bignum.o

ccflags-y := -std=gnu99 -Wno-declaration-after-statement 

ifneq ($(FIBMODE),)
ccflags-y += -D FIBMODE=$(strip $(FIBMODE))
endif

ifneq ($(NOHASH),0)
ccflags-y += -D NOHASH=$(strip $(NOHASH))
endif

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied

all: $(GIT_HOOKS) client measure
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) client out measure mult time_output/*.txt
load:
	sudo insmod $(TARGET_MODULE).ko
unload:
	sudo rmmod $(TARGET_MODULE) || true >/dev/null

client: src/client.c
	$(CC) -o $@ $^

measure: src/measure.c
	$(CC) -o $@ $^

PRINTF = env printf
PASS_COLOR = \e[32;01m
NO_COLOR = \e[0m
pass = $(PRINTF) "$(PASS_COLOR)$1 Passed [-]$(NO_COLOR)\n"

mult: src/multithread.c
	$(CC) -pthread -o $@ $^

check: all
	$(MAKE) unload
	$(MAKE) load
	sudo ./client > out
	$(MAKE) unload
	@diff -u out scripts/expected.txt && $(call pass)
	@scripts/verify.py

output:
	$(MAKE) unload
	$(MAKE) load
	sudo taskset -c 0 ./measure > time_measure/$(FILENAME).txt
	$(MAKE) unload

output_mul:
	$(MAKE) unload
	$(MAKE) load
	sudo taskset -c 0 ./mult > time_measure/$(FILENAME).txt
	$(MAKE) unload
