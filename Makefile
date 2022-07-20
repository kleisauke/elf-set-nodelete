CXXFLAGS += -std=c++11 -Wall -Wextra -pedantic
PREFIX ?= /usr/local

elf-set-nodelete: elf-set-nodelete.cpp

clean:
	rm -f elf-set-nodelete

install: elf-set-nodelete
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install elf-set-nodelete $(DESTDIR)$(PREFIX)/bin/elf-set-nodelete

uninstall:
	rm -f $(PREFIX)/bin/elf-set-nodelete

.PHONY: clean install uninstall
