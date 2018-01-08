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

SRCDIR=src
RESDIR=res
BINDIR=bin
INCDIR=include

SRC = \
	$(SRCDIR)/hw4_window.cpp \
	$(SRCDIR)/hw4_app.cpp \
	$(SRCDIR)/hw4_error.cpp \
	$(SRCDIR)/main.cpp \
	$(SRCDIR)/scene_object.cpp \
	$(SRCDIR)/object.cpp \
	$(SRCDIR)/program.cpp
GEN_RES = $(SRCDIR)/hw4_resources.cpp
GEN_DAT = $(RESDIR)/marching_geometry.cl-gen
GEN_OCL = $(RESDIR)/marching_geometry.cl
GEN_HDR = $(INCDIR)/marching_geometry.hpp
BIN = hw4

GENERATED = $(OBJS) \
	Makefile.deps \
	$(BIN) $(GEN_RES) $(GEN_DAT) $(GEN_OCL) $(GEN_HDR) $(BINDIR) \

SRCS = $(GEN_RES) $(SRC)
OBJS = $(SRCS:$(SRCDIR)/%.cpp=$(BINDIR)/%.o)

.PHONY: all
all: $(BIN)

CXX = g++ -std=c++14 -Wall -Wextra -Werror -Wno-deprecated -Wno-error=ignored-attributes \
	-g -gcolumn-info -Og
#	-flto -O3 -march=native
PKGCONFIG = pkg-config
LIBS = gtkmm-3.0 gio-2.0 epoxy OpenCL
CFLAGS = $(shell $(PKGCONFIG) --cflags $(LIBS) | sed 's/-I/-isystem/g') -iquote$(INCDIR)
LFLAGS = $(shell $(PKGCONFIG) --libs   $(LIBS))
GLIB_COMPILE_RESOURCES = $(shell $(PKGCONFIG) --variable=glib_compile_resources gio-2.0)
GLIB_COMPILE_SCHEMAS   = $(shell $(PKGCONFIG) --variable=glib_compile_schemas gio-2.0)

$(GEN_HDR): $(GEN_DAT)
	$(VERBinfo) '\tDATA\t'$@

$(GEN_DAT): $(RESDIR)/marching_geometry_data.py
	$(VERBinfo) '\tDATA\t'$@
	$(VERBpref)$< > $@ 2> $(GEN_HDR)

$(GEN_OCL): $(GEN_DAT) $(RESDIR)/marching_geometry.cl-tail
	$(VERBinfo) '\tCONCAT\t'$@
	$(VERBpref)cat $^ > $@

$(GEN_RES): $(RESDIR)/hw4.gresource.xml $(patsubst %,$(RESDIR)/%,$(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies $(RESDIR)/hw4.gresource.xml))
	$(VERBinfo) '\tGLIB RESOURCES\t'$@
	$(VERBpref)$(GLIB_COMPILE_RESOURCES) --sourcedir=$(RESDIR) --target=$@ --generate-source $<

$(BINDIR)/%.o: $(SRCDIR)/%.cpp | bin
	$(VERBinfo) '\tCXX\t'$@
	$(VERBpref)$(CXX) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJS)
	$(VERBinfo) '\tLINK\t'$@
	$(VERBpref)$(CXX) $(OBJS) $(LFLAGS) -o $@

$(BINDIR):
	$(VERBinfo) '\tMKDIR\t'$@
	$(VERBpref)mkdir $@

.PHONY: clean mrpropper info
info:
	@echo 'CFLAGS='$(CFLAGS)
	@echo 'LFLAGS='$(LFLAGS)

clean:
	$(VERBinfo) '\tCLEAN\t' $(GENERATED)
	$(VERBpref)rm -rf $(GENERATED)

mrpropper: clean
	$(VERBinfo) '\tCLEAN\t' Makefile.dummy
	$(VERBpref)rm -f Makefile.dummy

# == Internals ==

.SUFFIXES:

# Dependencies generations
.PHONY: Makefile.deps
Makefile.deps: $(GEN_HDR)
	@$(VERBinfo) '\tDEPS\t'
	@rm -f Makefile.deps
	$(VERBpref)$(foreach src,$(SRC),$(CXX) $(CFLAGS) -MM $(src) -MT $(src:$(SRCDIR)/%.cpp=$(BINDIR)/%.o) >> Makefile.deps;)

NODEPS=clean mrpropper info
ifeq ($(words $(findstring $(MAKECMDGOALS), $(NODEPS))), 0)
  -include Makefile.deps
endif

# clean on changed Makefile trick
Makefile.dummy: Makefile
	@$(VERBinfo) '\tFORCING CLEAN\t(Makefile updated)'
	@touch $@
	@$(MAKE) --no-print-directory clean
-include Makefile.dummy
