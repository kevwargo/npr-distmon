CC=gcc
CFLAGS=-c -g -O2 -Wall -std=gnu99 -D_POSIX_C_SOURCE=200809L -Iinclude
LD=gcc
LDFLAGS=

SOURCES=src/distmon.c
SOURCES+=src/node.c
SOURCES+=src/socket.c
SOURCES+=src/events.c

HEADERS=include/node.h
HEADERS+=include/socket.h


define SourcesToObjects =
$(addprefix obj/, $(addsuffix .o, $(notdir $(basename $(1)))))
endef

define DefineRule =
$(call SourcesToObjects, $(1)): $(1) Makefile $(HEADERS)
	$$(CC) $$(CFLAGS) $$< -o $$@
endef


.PHONY: clean
.PHONY: all

all: obj distmon

obj:
	mkdir -p obj

$(foreach src, $(SOURCES), $(eval $(call DefineRule, $(src))))

distmon: $(call SourcesToObjects, $(SOURCES))
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -rf obj distmon
