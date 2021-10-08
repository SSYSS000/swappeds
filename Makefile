.PHONY: all

PREFIX = /usr/local

SOURCES = swappeds.c

all: $(SOURCES)
	$(CC) -O3 -o swappeds $(SOURCES)

clean:
	rm swappeds

install: all
	mkdir -p $(PREFIX)/bin
	cp -f swappeds $(PREFIX)/bin

uninstall:
	rm $(PREFIX)/swappeds
