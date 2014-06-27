# CS452 Microkernel Makefile
builddir         = build
srcdir           = src
testdir          = tests
test             = $(testdir)/a.out
XCC              = gcc
AS               = as
LD               = ld
CFLAGS           = -nodefaultlibs -c -fPIC -Wall -I. -I./include -mcpu=arm920t -msoft-float -O3 -DBUFFEREDIO
# -g: include hooks for gdb
# -c: only compile
# -mcpu=arm920t: generate code for the 920t architecture
# -fpic: emit position-independent code
# -Wall: report all warnings
# -nodefaultlibs: prevent linking standard libraries to remove memcpy errors
ASFLAGS          = -mcpu=arm920t -mapcs-32
# -mapcs: always generate a complete stack frame
LDFLAGS          = -init main -Map $(builddir)/kern.map -N -T src/orex.ld -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2 -L./lib
SOURCE           = $(wildcard $(srcdir)/*.[cs]) $(wildcard $(srcdir)/**/*.[cs])
SOURCEFILES      = $(basename $(SOURCE))
TARGET           = kernel.elf
DEBUG            ?= DEBUG

.PHONY: all

.SUFFIXES: .c .o .h .s

.SECONDARY:

.NOTPARALLEL: all upload test debug profile clean init

all: init target

debug: CFLAGS += -DDEBUG -D$(DEBUG)
debug: upload

profile: CFLAGS += -DPROFILE $(PROFILING)
profile: upload

test: CFLAGS += -DTEST
test: init
	@$(eval DEPENDENCIES := $(subst $(srcdir)/,$(builddir)/,$(addsuffix .o, $(SOURCEFILES))))
	@$(MAKE) > /dev/null
	rm $(builddir)/main.s $(builddir)/main.o
	@$(XCC) -S $(CFLAGS) $(testdir)/$(TEST) -o $(builddir)/main.s
	@$(AS) $(ASFLAGS) $(builddir)/main.s -o $(builddir)/main.o
	@echo "Compiled test $(TEST) -> $(builddir)/main.o"
	$(LD) $(LDFLAGS) -o $(builddir)/$(TARGET) $(DEPENDENCIES) -lbwio -lgcc
	@USER=`whoami`
	@-diff -s $(builddir)/$(TARGET) /u/cs452/tftp/ARM/$(USER)/$(TARGET)
	@bin/cs452-upload.sh $(builddir)/$(TARGET) $(USER)

init:
	@echo "CFLAGS: $(CFLAGS)"
	@-rm -rf $(builddir)/*
	@-cp -r $(srcdir)/*.s $(builddir)/
	@echo "Source files:"
	@echo $(SOURCEFILES)

clean:
	-rm -rf $(builddir)/*

upload: all
	@USER=`whoami`
	@-diff -s $(builddir)/$(TARGET) /u/cs452/tftp/ARM/$(USER)/$(TARGET)
	@bin/cs452-upload.sh $(builddir)/$(TARGET) $(USER)

$(builddir)/%.o: $(builddir)/%.s
	@$(AS) $(ASFLAGS) $< -o $@
	@echo "Compiled $@"

$(builddir)/%.s: $(srcdir)/%.c
	@$(eval DIRNAME=`dirname $@`)
	@mkdir -p $(DIRNAME)
	@$(XCC) -S $(CFLAGS) $< -o $@

target: $(subst $(srcdir)/,$(builddir)/,$(addsuffix .o, $(SOURCEFILES)))
	$(LD) $(LDFLAGS) -o $(builddir)/$(TARGET) $^ -lbwio -lgcc
