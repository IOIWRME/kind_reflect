# =============================================================================
# Makefile for a header-only library
#
#   make install               copy headers to $(INCLUDEDIR)/$(INCSUBDIR)
#   make install PREFIX=/usr/local
#   make install DESTDIR=/tmp/pkgroot PREFIX=/usr     (for packagers)
#   make install INCSUBDIR=     put headers directly in include/ (no subfolder)
#   make check                 compile every header on its own (sanity check)
#   make uninstall
#
# Any variable marked "?=" can be overridden on the command line or via the
# environment, so most people never need to edit this file.
# =============================================================================

# ---- Project settings (edit these) ------------------------------------------
# Library name. Defaults to the project folder name.
NAME       ?= $(notdir $(CURDIR))
incdir     ?= inc
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
CXXFLAGS ?= -Wall

# ---- Internals (rarely need editing) ----------------------------------------
hdr := $(patsubst $(incdir)/%,%,$(shell find $(incdir) -type f \( -name '*.h' -o -name '*.hpp' \)))

.PHONY: all install uninstall check
all:
	@echo "Header-only library: nothing to build. Try 'make install' or 'make check'."

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
