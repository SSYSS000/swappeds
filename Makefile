.PHONY: all

PREFIX  := /usr/local

TARGET  := swappeds
SOURCES := swappeds.c
CFLAGS  := -Wall -Wextra -O3

all: $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm $(TARGET)

install: all
	mkdir -p $(PREFIX)/bin
	cp -f $(TARGET) $(PREFIX)/bin

uninstall:
	rm $(PREFIX)/$(TARGET)
