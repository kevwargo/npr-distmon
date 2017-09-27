CC=gcc
CFLAGS=-c -g -O2 -Wall -std=gnu99 -D_POSIX_C_SOURCE=200809L -Iinclude -pthread
LD=gcc
LDFLAGS=-pthread

SOURCES=src/distmon.c
SOURCES+=src/node.c
SOURCES+=src/socket.c
SOURCES+=src/events.c
SOURCES+=src/message.c
SOURCES+=src/dllist.c
SOURCES+=src/distenv.c
SOURCES+=src/debug.c

HEADERS=include/node.h
HEADERS+=include/socket.h
HEADERS+=include/events.h
HEADERS+=include/message.h
HEADERS+=include/dllist.h
HEADERS+=include/debug.h


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

distmon: $(call SourcesToObjects, $(SOURCES))
	$(LD) $(LDFLAGS) -o $@ $^
clean:
	rm -rf obj distmon

$(foreach src, $(SOURCES), $(eval $(call DefineRule, $(src))))

