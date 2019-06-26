BUILDDIR = build/
LV2NAME = humanizer
LIB_EXT = .so
DESTDIR = /usr/lib64/lv2/
BUNDLE = $(LV2NAME).lv2


TARGETS = 

TARGETS += $(BUILDDIR)$(LV2NAME)$(LIB_EXT)

# check for build-dependencies
ifeq ($(shell pkg-config --exists lv2 || echo no), no)
  $(error "LV2 SDK was not found")
endif

all: $(BUILDDIR)manifest.ttl $(BUILDDIR)$(LV2NAME).ttl $(TARGETS)

$(BUILDDIR)manifest.ttl: manifest.ttl.in Makefile
	mkdir -p $(BUILDDIR)
	sed "s/@LIB_EXT@/$(LIB_EXT)/" manifest.ttl.in > $(BUILDDIR)/manifest.ttl

$(BUILDDIR)$(LV2NAME).ttl: Makefile $(LV2NAME).ttl
	mkdir -p $(BUILDDIR)
	cp $(LV2NAME).ttl  $(BUILDDIR)$(LV2NAME).ttl

$(BUILDDIR)$(LV2NAME)$(LIB_EXT): humanizer.c Makefile
	mkdir -p $(BUILDDIR)
	gcc -Wall -Wextra -Wno-unused `pkg-config --cflags --libs lv2` humanizer.c -fPIC -DPIC -shared -o $(BUILDDIR)$(LV2NAME)$(LIB_EXT)

.PHONY: install clean uninstall

uninstall:
	rm -rf $(DESTDIR)$(LV2NAME).lv2

install: all
	install -d $(DESTDIR)$(BUNDLE)
	install -m644 $(BUILDDIR)manifest.ttl $(BUILDDIR)$(LV2NAME).ttl $(DESTDIR)/$(BUNDLE)
	install -m755 $(BUILDDIR)$(LV2NAME)$(LIB_EXT) $(DESTDIR)/$(BUNDLE)

clean:
	rm -rf $(BUILDDIR)
