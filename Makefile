# CS452 Microkernel Makefile
builddir         = build
srcdir           = src
testdir          = tests
test             = $(testdir)/a.out
XCC              = gcc
AS               = as
LD               = ld
CFLAGS           = -fno-builtin -nostdlib -nodefaultlibs -c -fPIC -Wall -I. -I./include -mcpu=arm920t -msoft-float -O3 -DBUFFEREDIO
TRACK            ?= a
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
TARGET           ?= kernel-2.elf
DEBUG            ?= DEBUG
SILENT           ?= false

.PHONY: all

.SUFFIXES: .c .o .h .s

.SECONDARY:

.NOTPARALLEL: all upload test debug profile clean init debugtest target

all: init target

debug: CFLAGS += -DDEBUG -D$(DEBUG)
debug: upload

profile: CFLAGS += -DPROFILE $(PROFILING)
profile: upload

debugtest: CFLAGS += -DDEBUG -DTEST
debugtest: all

test: CFLAGS += -DTEST -DDEBUG -D$(DEBUG)
test:
	@$(eval DEPENDENCIES := $(subst $(srcdir)/,$(builddir)/,$(addsuffix .o, $(SOURCEFILES))))
	@-if [ "$(SILENT)" != "false" ]; then \
		$(MAKE) debugtest SILENT=true TRACK=$(TRACK); \
	else \
		$(MAKE) debugtest SILENT=true TRACK=$(TRACK); \
	fi
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
	@if [ "$(TRACK)" = "b" ]; then \
		echo "Track Selected: Track B"; \
	elif [ "$(TRACK)" = "a" ]; then \
		echo "Track Selected: Track A"; \
	else \
		echo "Unknown Track Selected: $(TRACK)"; \
		exit 1; \
	fi
	@track/parse_track track/new/track$(TRACK)_new -C src/track_data.c -H include/track_data.h
	@echo "Source files:"
	@echo $(SOURCEFILES)

clean:
	-rm -rf $(builddir)/*

upload: all
	@USER=`whoami`
	@-if [ "$(SILENT)" != "true" ]; then \
		diff -s $(builddir)/$(TARGET) /u/cs452/tftp/ARM/$(USER)/$(TARGET) ; \
		bin/cs452-upload.sh $(builddir)/$(TARGET) $(USER) ; \
	fi

$(builddir)/%.o: $(builddir)/%.s
	@$(AS) $(ASFLAGS) $< -o $@
	@echo "Compiled $@"

$(builddir)/%.s: $(srcdir)/%.c
	@$(eval DIRNAME=`dirname $@`)
	@mkdir -p $(DIRNAME)
	@$(XCC) -S $(CFLAGS) $< -o $@

target: $(subst $(srcdir)/,$(builddir)/,$(addsuffix .o, $(SOURCEFILES)))
	@if [ "$(SILENT)" = "true" ]; then \
		$(LD) $(LDFLAGS) -o $(builddir)/$(TARGET) $^ -lbwio -lgcc > /dev/null ; \
	else \
		$(LD) $(LDFLAGS) -o $(builddir)/$(TARGET) $^ -lbwio -lgcc ; \
	fi
