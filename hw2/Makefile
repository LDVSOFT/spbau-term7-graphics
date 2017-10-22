.SUFFIXES:
	
CC = g++ -std=c++14 -Wall -Wextra -Werror -Wno-deprecated -g
PKGCONFIG = pkg-config
LIBS = gtkmm-3.0 gio-2.0 epoxy
CFLAGS = $(shell $(PKGCONFIG) --cflags $(LIBS))
LFLAGS = $(shell $(PKGCONFIG) --libs   $(LIBS))
GLIB_COMPILE_RESOURCES = $(shell $(PKGCONFIG) --variable=glib_compile_resources gio-2.0)
GLIB_COMPILE_SCHEMAS   = $(shell $(PKGCONFIG) --variable=glib_compile_schemas gio-2.0)

SRC = hw2_window.cpp hw2_app.cpp main.cpp
GEN = hw2_resources.cpp
BIN = hw2

SRCS = $(GEN) $(SRC)
OBJS = $(SRCS:.cpp=.o)

all: $(BIN)

.PHONY: DEBUG
DEBUG:
	@echo "!! CFLAGS=$(CFLAGS)"
	@echo "!! LFLAGS=$(LFLAGS)"

deps = $($(CC) -MM $(1) | sed 's/^[^:]*:[ ]*//')

$(GEN): hw2.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=. --generate-dependencies hw2.gresource.xml) Makefile
	$(GLIB_COMPILE_RESOURCES) --sourcedir=. --target=$@ --generate-source $<

%.o: %.cpp $(call deps,%.cpp) Makefile
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJS) Makefile
	$(CC) $(OBJS) $(LFLAGS) -o $@

.PHONY: clean
clean:
	@rm -f $(GEN) $(OBJS) $(BIN)
