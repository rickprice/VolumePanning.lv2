PLUGIN_NAME = volumepanning
INSTALL_DIR = $(HOME)/.lv2/$(PLUGIN_NAME).lv2

CC     = gcc
CFLAGS = $(shell pkg-config --cflags lv2) -std=c99 -fPIC -fvisibility=hidden \
         -O2 -Wall -Wextra -Wpedantic
LDFLAGS = -shared -Wl,-soname,$(PLUGIN_NAME).so -lm

all: $(PLUGIN_NAME).so

$(PLUGIN_NAME).so: $(PLUGIN_NAME).c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

install: all
	mkdir -p $(INSTALL_DIR)
	cp $(PLUGIN_NAME).so manifest.ttl volumepanning.ttl $(INSTALL_DIR)/

uninstall:
	rm -rf $(INSTALL_DIR)

clean:
	rm -f $(PLUGIN_NAME).so

.PHONY: all install uninstall clean
