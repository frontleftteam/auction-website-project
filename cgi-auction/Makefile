# ---- config ----
BIN_DIR      ?= ./bin
CGI_DIR      ?= /usr/lib/cgi-bin

CC           ?= gcc
CXX          ?= g++
CFLAGS       ?= -O2 -Wall -Wextra -pedantic
CXXFLAGS     ?= -O2 -std=c++17 -Wall -Wextra -pedantic
LDFLAGS      ?=

# Prefer pkg-config for sqlite3, fall back to -lsqlite3
SQLITE3_CFLAGS := $(shell pkg-config --cflags sqlite3 2>/dev/null)
SQLITE3_LIBS   := $(shell pkg-config --libs   sqlite3 2>/dev/null)
ifeq ($(strip $(SQLITE3_LIBS)),)
  SQLITE3_LIBS := -lsqlite3
endif

# ---- targets ----
TARGETS := \
  $(BIN_DIR)/auth.cgi \
  $(BIN_DIR)/bid_sell.cgi \
  $(BIN_DIR)/list_open.cgi \
  $(BIN_DIR)/transactions.cgi

.PHONY: all clean install init-db devserver

all: $(TARGETS)

# C++ CGI pattern
$(BIN_DIR)/%.cgi: %.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(SQLITE3_CFLAGS) $< -o $@ $(LDFLAGS) $(SQLITE3_LIBS)
	chmod 755 $@

# C CGI pattern
$(BIN_DIR)/%.cgi: %.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $(SQLITE3_CFLAGS) $< -o $@ $(LDFLAGS) $(SQLITE3_LIBS)
	chmod 755 $@

$(BIN_DIR):
	mkdir -p $@

# Explicit file mappings (in case your make omits pattern matches)
$(BIN_DIR)/auth.cgi:        auth.cpp
$(BIN_DIR)/bid_sell.cgi:    bid_sell.cpp
$(BIN_DIR)/list_open.cgi:   list_open.cpp
$(BIN_DIR)/transactions.cgi: transactions.c

# Install to your server's CGI directory (override CGI_DIR=... if needed)
install: all
	install -d "$(CGI_DIR)"
	install -m 755 $(TARGETS) "$(CGI_DIR)"
	@echo "Installed CGI binaries to $(CGI_DIR)"

# Optional: initialize DB from schema.sql (the apps also auto-create tables)
init-db: schema.sql
	sqlite3 auction.db < schema.sql
	@echo "Initialized auction.db from schema.sql"

# Quick local dev server (Python 3) — serves ./cgi-bin symlink to $(BIN_DIR)
# Visit http://localhost:8000/cgi-bin/list_open.cgi
devserver: all
	ln -snf $(BIN_DIR) cgi-bin
	python3 -m http.server --cgi 8000

clean:
	rm -rf $(BIN_DIR) cgi-bin
