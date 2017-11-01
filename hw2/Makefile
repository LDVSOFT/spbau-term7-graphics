# Verbose?

V ?= 0
ifeq ($(V), 0)
  VERBinfo = @echo -e
  VERBpref = @
else
  VERBinfo = @true
  VERBpref =
endif

# Build:

SRC = hw2_window.cpp hw2_app.cpp hw2_error.cpp main.cpp scene_object.cpp object.cpp
GEN = hw2_resources.cpp
BIN = hw2

SRCS = $(GEN) $(SRC)
OBJS = $(SRCS:.cpp=.o)

.PHONY: all
all: $(BIN)

CXX = g++ -std=c++14 -Wall -Wextra -Werror -Wno-deprecated -g
PKGCONFIG = pkg-config
LIBS = gtkmm-3.0 gio-2.0 epoxy
CFLAGS = $(shell $(PKGCONFIG) --cflags $(LIBS))
CFLAGSsys = $(shell echo $(CFLAGS) | sed 's/-I/-isystem/g')
LFLAGS = $(shell $(PKGCONFIG) --libs   $(LIBS))
GLIB_COMPILE_RESOURCES = $(shell $(PKGCONFIG) --variable=glib_compile_resources gio-2.0)
GLIB_COMPILE_SCHEMAS   = $(shell $(PKGCONFIG) --variable=glib_compile_schemas gio-2.0)

$(GEN): hw2.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=. --generate-dependencies hw2.gresource.xml)
	$(VERBinfo) '\tGLIB RESOURCES\t'$@
	$(VERBpref)$(GLIB_COMPILE_RESOURCES) --sourcedir=. --target=$@ --generate-source $<

libs.hpp.gch: libs.hpp
	$(VERBinfo) '\tCXX\t'$@
	$(VERBpref)$(CXX) $(CFLAGS) -c $< -o $@

%.o: %.cpp libs.hpp.gch
	$(VERBinfo) '\tCXX\t'$@
	$(VERBpref)$(CXX) $(CFLAGSsys) -c $< -o $@

$(BIN): $(OBJS)
	$(VERBinfo) '\tLINK\t'$@
	$(VERBpref)$(CXX) $(OBJS) $(LFLAGS) -o $@

.PHONY: clean mrpropper
clean:
	$(VERBinfo) '\tCLEAN\t' $(GEN) $(OBJS) $(BIN) Makefile.deps libs.hpp.gch
	$(VERBpref)rm -f $(GEN) $(OBJS) $(BIN) Makefile.deps libs.hpp.gch

mrpropper: clean
	$(VERBinfo) '\tCLEAN\t' Makefile.dummy
	$(VERBpref)rm -f Makefile.dummy

# == Internals ==

.SUFFIXES:

# Dependencies generations
.PHONY: Makefile.deps
Makefile.deps:
	@$(VERBinfo) '\tDEPS\t'
	@$(VERBpref)$(CXX) $(CFLAGSsys) -MM $(SRC) > Makefile.deps
	@$(VERBpref)$(CXX) $(CFLAGS) -MM libs.hpp >> Makefile.deps

NODEPS=clean mrpropper
ifeq ($(words $(findstring $(MAKECMDGOALS), $(NODEPS))), 0)
  -include Makefile.deps
endif

# clean on changed Makefile trick
Makefile.dummy: Makefile
	@$(VERBinfo) '\tFORCE CLEAN\t(Makefile updated)'
	@touch $@
	@$(MAKE) --print-directory clean
-include Makefile.dummy
