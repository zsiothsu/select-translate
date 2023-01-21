#!/bin/make

export CC		:= gcc

export ECHO		:= echo
export RM		:= rm
export GREP		:= grep
export XARGS	:= xargs
export CD		:= cd
export LS		:= ls
export CP		:= cp
export INSTALL	:= install

OUTPUT_DIR		:= build
SYSEXEC_DIR 	:= /usr/bin


SOURCE_FILE		:= main.cpp cJSON/cJSON.c
TARGET			:= select-translate

CXXFLAGS		:= -Wall -I cJSON
LDFLAGS			:= -lstdc++ -lcurl

all: CXXFLAGS += -O2
all: $(OUTPUT_DIR)/$(TARGET)

release: CXXFLAGS += -O3
release: $(OUTPUT_DIR)/$(TARGET)

debug: CXXFLAGS += -Os -g -DDEBUG
debug: $(OUTPUT_DIR)/$(TARGET)

$(OUTPUT_DIR)/$(TARGET) : $(SOURCE_FILE)
	@$(CC) $(SOURCE_FILE) $(CXXFLAGS) $(LDFLAGS) -o $@

.PHONY: clean
clean:
	$(CD) $(OUTPUT_DIR) && $(LS) | $(GREP) -v ".keep" | $(XARGS) $(RM)

.PHONY: install
install:
	@$(INSTALL) $(OUTPUT_DIR)/$(TARGET) $(SYSEXEC_DIR)/

.PHONY: uninstall
uninstall:
	@$(RM) $(SYSEXEC_DIR)/$(TARGET)