CC=gcc
CFLAGS=-c -O2 -Wall -std=gnu99 -D_POSIX_C_SOURCE=200809L -Iinclude
LD=gcc
LDFLAGS=

SOURCES=src/distmon.c
SOURCES+=src/node.c
SOURCES+=src/socket.c


define SourcesToObjects =
$(addprefix obj/, $(addsuffix .o, $(notdir $(basename $(1)))))
endef

define DefineRule =
$(call SourcesToObjects, $(1)): $(1) Makefile
	$$(CC) $$(CFLAGS) $$< -o $$@
endef


.PHONY: clean
.PHONY: all

all: distmon

$(foreach src, $(SOURCES), $(eval $(call DefineRule, $(src))))

distmon: $(call SourcesToObjects, $(SOURCES))
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f obj/* distmon
