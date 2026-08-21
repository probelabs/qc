COSMOCC ?= /Users/buger/.local/cosmocc/bin/cosmocc
CFLAGS  ?= -Iinclude

SRCS = \
	src/util.c \
	src/globmatch.c \
	src/digest.c \
	src/parser.c \
	src/store.c \
	src/states.c \
	src/templates.c \
	src/commands.c \
	src/cli.c

.PHONY: all test clean cover-host

all: qc

qc: $(SRCS) include/qc.h
	$(COSMOCC) $(CFLAGS) -o qc $(SRCS)

tests/test_kernel: tests/test_kernel.c src/util.c src/globmatch.c src/digest.c src/parser.c src/store.c src/states.c include/qc.h
	$(COSMOCC) $(CFLAGS) -o tests/test_kernel tests/test_kernel.c \
		src/util.c src/globmatch.c src/digest.c src/parser.c src/store.c src/states.c

tests/test_acceptance: tests/test_acceptance.c
	$(COSMOCC) $(CFLAGS) -o tests/test_acceptance tests/test_acceptance.c

test: qc tests/test_kernel tests/test_acceptance
	./tests/test_kernel
	sh tests/run.sh
	./tests/test_acceptance

cover-host:
	sh scripts/host-c-mcdc.sh

clean:
	rm -f qc qc.aarch64.elf qc.com.dbg tests/test_kernel tests/test_kernel.aarch64.elf tests/test_kernel.com.dbg tests/test_acceptance tests/test_acceptance.aarch64.elf tests/test_acceptance.com.dbg
