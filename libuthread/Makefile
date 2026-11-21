# Target library
lib := libuthread.a

objs:= queue.o uthread.o context.o preempt.o sem.o
CFLAGS := -Wall -Wextra -Werror -MMD
CC:= gcc
ARFLAGS := rcs
AR := ar

all: $(lib)

## TODO: Phase 1
# dependency stuff from slides
deps:=$(patsubst %.o,%.d,$(objs))
-include $(deps)


$(lib): $(objs)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(lib) $(objs) $(deps) 