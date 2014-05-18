# CS452 Microkernel Makefile
builddir         = build
srcdir           = src
testdir          = tests
test             = $(testdir)/a.out
XCC              = gcc
AS               = as
LD               = ld
CFLAGS           = -nodefaultlibs -c -fPIC -Wall -I. -I./include -mcpu=arm920t -msoft-float
# -g: include hooks for gdb
# -c: only compile
# -mcpu=arm920t: generate code for the 920t architecture
# -fpic: emit position-independent code
# -Wall: report all warnings
# -nodefaultlibs: prevent linking standard libraries to remove memcpy errors
ASFLAGS          = -mcpu=arm920t -mapcs-32
# -mapcs: always generate a complete stack frame
LDFLAGS          = -init main -Map $(builddir)/kern.map -N -T src/orex.ld -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2 -L./lib
SOURCE           = $(wildcard $(srcdir)/*.[cs])
SOURCEFILES      = $(basename $(SOURCE))
TARGET           = assn1.elf

.PHONY: all

.SUFFIXES: .c .o .h .s

.SECONDARY:

.NOTPARALLEL: all upload

all: clean init target

debug: CFLAGS += -DDEBUG
debug: upload

init:
	mkdir -p build
	@-cp -r $(srcdir)/*.s $(builddir)/
	@echo "Source files:"
	@echo $(SOURCEFILES)

clean:
	-rm -f $(builddir)/*

test:
	@for case in $(wildcard $(testdir)/*.c) ; do \
		echo $$case ; \
		$(XCC) -I./include -g $$case -o $(test) ; \
		$(test) ; \
	done
	@-rm -rf $(test)
	@echo "All tests passed successfully."

upload: all
	USER=`whoami`
	-diff -s $(builddir)/$(TARGET) /u/cs452/tftp/ARM/$(USER)/$(TARGET)
	bin/cs452-upload.sh $(builddir)/$(TARGET) $(USER)

$(builddir)/%.o: $(builddir)/%.s
	$(AS) $(ASFLAGS) $< -o $@

$(builddir)/%.s: $(srcdir)/%.c
	$(XCC) -S $(CFLAGS) $< -o $@

target: $(subst $(srcdir)/,$(builddir)/,$(addsuffix .o, $(SOURCEFILES)))
	$(LD) $(LDFLAGS) -o $(builddir)/$(TARGET) $^ -lbwio -lgcc
