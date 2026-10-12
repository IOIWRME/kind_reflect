# =============================================================================
# Makefile for a header-only library
#
#   make install               copy headers to $(INCLUDEDIR)/$(INCSUBDIR)
#   make install PREFIX=/usr/local
#   make install DESTDIR=/tmp/pkgroot PREFIX=/usr     (for packagers)
#   make install INCSUBDIR=     put headers directly in include/ (no subfolder)
#   make check                 compile every header on its own (sanity check)
#   make test                  build every test_src/**/*.cpp into test_bin/
#   make clean | uninstall
#
# Any variable marked "?=" can be overridden on the command line or via the
# environment, so most people never need to edit this file.
# =============================================================================

# ---- Project settings (edit these) ------------------------------------------
# Library name. Defaults to the project folder name.
NAME       ?= $(notdir $(CURDIR))
incdir     ?= inc
# Tests: each .cpp under testsrcdir becomes its own binary under testdir,
# e.g. test_src/foo.cpp -> test_bin/foo
testsrcdir ?= test_src
testdir    ?= test_bin
# Language flags the headers require (used by `make check`).
CXXSTD     ?= -std=c++26 -freflection

# ---- Where things get installed ---------------------------------------------
PREFIX     ?= $(HOME)/.local
INCLUDEDIR ?= $(PREFIX)/include
# Headers go in $(INCLUDEDIR)/$(INCSUBDIR)/. Set empty for a flat include dir.
INCSUBDIR  ?= $(NAME)
# Staging root for packagers; leave empty for a normal install.
DESTDIR    ?=

# ---- Where local (non-system) dependencies live -----------------------------
DEPS_PREFIX ?= $(HOME)/.local

CPPFLAGS ?=
CXXFLAGS ?= -g -Wall
LDFLAGS  ?=
LDLIBS   ?=

# ---- Internals (rarely need editing) ----------------------------------------
hdr := $(patsubst $(incdir)/%,%,$(shell find $(incdir) -type f \( -name '*.h' -o -name '*.hpp' \)))

test_src := $(shell find $(testsrcdir) -name '*.cpp' 2>/dev/null)
test_bin := $(patsubst $(testsrcdir)/%.cpp,$(testdir)/%,$(test_src))
test_dep := $(addsuffix .d,$(test_bin))

.PHONY: all install uninstall check test clean
all:
	@echo "Header-only library: nothing to build. Try 'make test', 'make check' or 'make install'."

test: $(test_bin)

$(test_bin): $(testdir)/%: $(testsrcdir)/%.cpp
	@mkdir -p $(@D)
	$(CXX) -I$(incdir) -I$(DEPS_PREFIX)/include $(CPPFLAGS) $(CXXSTD) $(CXXFLAGS) \
		-MMD -MP -MT $@ -MF $@.d $< -L$(DEPS_PREFIX)/lib $(LDFLAGS) -o $@ $(LDLIBS)

clean:
	rm -rf $(testdir)

install:
	install -d $(DESTDIR)$(INCLUDEDIR)/$(INCSUBDIR)
	tar -cf - -C $(incdir) $(hdr) | tar -xf - -C $(DESTDIR)$(INCLUDEDIR)/$(INCSUBDIR)

uninstall:
ifneq ($(strip $(INCSUBDIR)),)
	rm -rf $(DESTDIR)$(INCLUDEDIR)/$(INCSUBDIR)
else
	cd $(DESTDIR)$(INCLUDEDIR) && rm -f $(hdr)
endif

# Each header must compile when included by itself.
check:
	@for h in $(hdr); do \
		echo "checking $$h"; \
		echo "#include \"$$h\"" | $(CXX) -x c++ $(CXXSTD) $(CXXFLAGS) \
			-I$(incdir) -I$(DEPS_PREFIX)/include $(CPPFLAGS) -fsyntax-only - || exit 1; \
	done

-include $(test_dep)
