PROGS = libelisa.a

CC = gcc

CLEANFILES = $(PROGS) *.o *.d

NO_MAN =
CFLAGS = -O3 -pipe
CFLAGS += -g -rdynamic
CFLAGS += -Werror
CFLAGS += -Wall -Wunused-function
CFLAGS += -Wextra
CFLAGS += -static -fPIC

CFLAGS += -I$(dir $(abspath $(lastword $(MAKEFILE_LIST))))include

LDFLAGS +=

C_SRCS = main.c

C_OBJS = $(C_SRCS:.c=.o)

OBJS = $(C_OBJS)

CLEANFILES += $(C_OBJS)

.PHONY: all
all: $(PROGS)

$(PROGS): $(OBJS)
	ar qc -o $@ $^

clean:
	-@rm -rf $(CLEANFILES)
