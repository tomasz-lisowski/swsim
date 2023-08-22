DIR_LIB:=../../lib
include $(DIR_LIB)/make-pal/pal.mak
DIR_SRC:=src
DIR_TEST:=test
DIR_INCLUDE:=include
DIR_BUILD:=build
CC:=gcc

MAIN_NAME:=efimsi
MAIN_SRC:=$(wildcard $(DIR_SRC)/*.c)
MAIN_OBJ:=$(MAIN_SRC:$(DIR_SRC)/%.c=$(DIR_BUILD)/%.o)
MAIN_DEP:=$(MAIN_OBJ:%.o=%.d)
MAIN_CC_FLAGS:=\
	-W \
	-Wall \
	-Wextra \
	-Werror \
	-Wno-unused-parameter \
	-Wconversion \
	-Wshadow \
	-O2 \
	-fsanitize=address \
	-I$(DIR_INCLUDE)

all: main
.PHONY: all

main: $(DIR_BUILD) $(DIR_BUILD)/$(MAIN_NAME).$(EXT_BIN)
.PHONY: main

# Create the binary.
$(DIR_BUILD)/$(MAIN_NAME).$(EXT_BIN): $(MAIN_OBJ)
	$(CC) $(MAIN_OBJ) -o $(@) $(MAIN_CC_FLAGS)

# Compile source files to object files.
$(DIR_BUILD)/%.o: $(DIR_SRC)/%.c
	$(CC) $(<) -o $(@) $(MAIN_CC_FLAGS) -c -MMD

# Recompile source files after a header they include changes.
-include $(MAIN_DEP)

$(DIR_BUILD):
	$(call pal_mkdir,$(@))
clean:
	$(call pal_rmdir,$(DIR_BUILD))
.PHONY: clean
