#
# Makefile for assignment 1 of CS452
#
builddir         = build
srcdir           = src
testdir          = tests
test             = $(testdir)/a.out
XCC              = gcc
AS               = as
LD               = ld
CFLAGS           = -nodefaultlibs -c -fPIC -Wall -I. -I./include -I/u/wbcowan/cs452/io/include -mcpu=arm920t -msoft-float
# -g: include hooks for gdb
# -c: only compile
# -mcpu=arm920t: generate code for the 920t architecture
# -fpic: emit position-independent code
# -Wall: report all warnings
# -nodefaultlibs: prevent linking standard libraries to remove memcpy errors
ASFLAGS          = -mcpu=arm920t -mapcs-32
# -mapcs: always generate a complete stack frame
LDFLAGS          = -init main -Map $(builddir)/kern.map -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2 -L/u/wbcowan/cs452/io/lib
SOURCE           = $(wildcard $(srcdir)/*.c)
SOURCEFILES      = $(SOURCE:.c=)
TARGET           = $(builddir)/kern.elf

.PHONY: all

.SUFFIXES: .c .o .h .s

.SECONDARY:

all: init clean target

debug: CFLAGS += -D DEBUG
debug: upload

init:
	mkdir -p build
	@echo "Source files:"
	@echo $(SOURCEFILES)

clean:
	-rm -f $(builddir)/*

test:
	@for case in $(wildcard $(testdir)/*.c) ; do \
		$(XCC) -g $$case -o $(test) \
		$(test) \
	done
	@-rm -rf $(test)
	@echo "All tests passed successfully."

upload: all
	-diff -s $(builddir)/kern.elf /u/cs452/tftp/ARM/$(USER)/kern.elf
	bin/cs452-upload.sh $(builddir)/kern.elf

$(builddir)/%.o: $(builddir)/%.s
	$(AS) $(ASFLAGS) $< -o $@

$(builddir)/%.s: $(srcdir)/%.c
	$(XCC) -S $(CFLAGS) $< -o $@

target: $(subst $(srcdir)/,$(builddir)/,$(addsuffix .o, $(SOURCEFILES)))
	$(LD) $(LDFLAGS) -o $(TARGET) $^ -lbwio -lgcc
