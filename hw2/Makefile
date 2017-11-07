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

SRC = \
	$(SRCDIR)/hw2_window.cpp \
	$(SRCDIR)/hw2_app.cpp \
	$(SRCDIR)/hw2_error.cpp \
	$(SRCDIR)/main.cpp \
	$(SRCDIR)/scene_object.cpp \
	$(SRCDIR)/object.cpp \
	$(SRCDIR)/program.cpp
GEN = $(SRCDIR)/hw2_resources.cpp
BIN = hw2

SRCS = $(GEN) $(SRC)
OBJS = $(SRCS:$(SRCDIR)/%.cpp=$(BINDIR)/%.o)

.PHONY: all
all: $(BIN)

CXX = g++ -std=c++14 -Wall -Wextra -Werror -Wno-deprecated -g
PKGCONFIG = pkg-config
LIBS = gtkmm-3.0 gio-2.0 epoxy
CFLAGS = $(shell $(PKGCONFIG) --cflags $(LIBS) | sed 's/-I/-isystem/g') -iquote include/
LFLAGS = $(shell $(PKGCONFIG) --libs   $(LIBS))
GLIB_COMPILE_RESOURCES = $(shell $(PKGCONFIG) --variable=glib_compile_resources gio-2.0)
GLIB_COMPILE_SCHEMAS   = $(shell $(PKGCONFIG) --variable=glib_compile_schemas gio-2.0)

$(GEN): $(RESDIR)/hw2.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(RESDIR) --generate-dependencies $(RESDIR)/hw2.gresource.xml)
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

.PHONY: clean mrpropper
clean:
	$(VERBinfo) '\tCLEAN\t' $(GEN) $(OBJS) $(BIN) Makefile.deps
	$(VERBpref)rm -rf $(GEN) $(OBJS) $(BIN) Makefile.deps $(BINDIR)

mrpropper: clean
	$(VERBinfo) '\tCLEAN\t' Makefile.dummy
	$(VERBpref)rm -f Makefile.dummy

# == Internals ==

.SUFFIXES:

# Dependencies generations
.PHONY: Makefile.deps
Makefile.deps:
	@$(VERBinfo) '\tDEPS\t'
	@rm -f Makefile.deps
	$(VERBpref)$(foreach src,$(SRC),$(CXX) $(CFLAGS) -MM $(src) -MT $(src:$(SRCDIR)/%.cpp=$(BINDIR)/%.o) >> Makefile.deps;)

NODEPS=clean mrpropper
ifeq ($(words $(findstring $(MAKECMDGOALS), $(NODEPS))), 0)
  -include Makefile.deps
endif

# clean on changed Makefile trick
Makefile.dummy: Makefile
	@$(VERBinfo) '\tFORCING CLEAN\t(Makefile updated)'
	@touch $@
	@$(MAKE) --no-print-directory clean
-include Makefile.dummy
