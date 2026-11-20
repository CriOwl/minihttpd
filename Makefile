SOURCES := $(wildcard src/*.c)
HEADERS := $(wildcard include/*.h)

.PHONY: all run clean

all: main.out

main.out: $(SOURCES) $(HEADERS) Makefile
	$(CC) $(CPPFLAGS) -I. $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES) $(LDLIBS)

run: main.out
	./$<

clean:
	$(RM) main.out
