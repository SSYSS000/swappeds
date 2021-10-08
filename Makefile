.PHONY: all

all: main.c
	$(CC) -O3 -o swappeds main.c

clean:
	rm swappeds

install: all
	mkdir -p $PREFIX/bin
	cp -f swappeds $PREFIX/bin

uninstall:
	rm $PREFIX/swappeds
