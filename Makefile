PREFIX = /usr/local

V ?= 0
V_GEN = $(V__gen_V_$(V))
V__gen_V_0 = @echo " GEN    " $@;
V__gen_V_1 =

V_CC = $(V__cc_V_$(V))
V__cc_V_0 = @echo " CC     " $@;
V__cc_V_1 =

V_LINK = $(V__link_V_$(V))
V__link_V_0 = @echo " LINK   " $@;
V__link_V_1 =

CC = gcc -std=c11 -Wall -Wextra -Werror
PKGCONFIG = $(shell which pkg-config)
CFLAGS = $(shell $(PKGCONFIG) --cflags gio-2.0 gtk+-3.0 epoxy)
LIBS = $(shell $(PKGCONFIG) --libs gio-2.0 gtk+-3.0 epoxy) -lm 
GLIB_COMPILE_RESOURCES = $(shell $(PKGCONFIG) --variable=glib_compile_resources gio-2.0)
GLIB_COMPILE_SCHEMAS = $(shell $(PKGCONFIG) --variable=glib_compile_schemas gio-2.0)

SRC = hw1-app.c hw1-app-window.c hw1-error.c main.c
GEN = hw1-resources.c
BIN = hw1

ALL = $(GEN) $(SRC)
OBJS = $(ALL:.c=.o)

all: $(BIN)

hw1-resources.c: hw1.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=. --generate-dependencies hw1.gresource.xml)
	$(V_GEN)$(GLIB_COMPILE_RESOURCES) hw1.gresource.xml --target=$@ --sourcedir=. --generate-source

%.o: %.c
	$(V_CC)$(CC) $(CFLAGS) -c -o $(@F) $<

$(BIN): $(OBJS)
	$(V_LINK)$(CC) -o $(@F) $(OBJS) $(LIBS)

install: $(BIN) io.bassi.hw1.desktop io.bassi.hw1.appdata.xml
	install -d -m 0755 $(PREFIX)/bin
	install -m0755 $(BIN) $(PREFIX)/bin/$(BIN)
	install -d -m 0755 $(PREFIX)/share/applications
	install -D -m0644 io.bassi.hw1.desktop $(PREFIX)/share/applications/io.bassi.hw1.desktop
	update-desktop-database -q $(PREFIX)/share/applications
	install -d -m 0755 $(PREFIX)/share/icons/hicolor/512x512/apps
	install -D -m0644 io.bassi.hw1.png $(PREFIX)/share/icons/hicolor/512x512/apps/io.bassi.hw1.png
	gtk-update-icon-cache -q -t -f $(PREFIX)/share/icons/hicolor
	install -d -m 0755 $(PREFIX)/share/appdata
	install -D -m0644 io.bassi.hw1.appdata.xml $(PREFIX)/share/appdata/io.bassi.hw1.appdata.xml

clean:
	@rm -f $(GEN) $(OBJS) $(BIN)
