DIR_LIB:=lib
include $(DIR_LIB)/make-pal/pal.mak
DIR_SRC:=src
DIR_INCLUDE:=include
DIR_BUILD:=build
CC:=gcc

MAIN_NAME:=swsim
MAIN_SRC:=$(wildcard $(DIR_SRC)/*.c)
MAIN_OBJ:=$(MAIN_SRC:$(DIR_SRC)/%.c=$(DIR_BUILD)/$(MAIN_NAME)/%.o)
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
	-I$(DIR_INCLUDE) \
	-I$(DIR_LIB)/uicc/include \
	-I$(DIR_LIB)/scraw/include \
	-L$(DIR_LIB)/uicc/build \
	-L$(DIR_LIB)/scraw/build\
	$(shell pkg-config --cflags libpcsclite) \
	-luicc \
	-lscraw \
	-lpcsclite

all: main
.PHONY: all

main: $(DIR_BUILD) $(DIR_BUILD)/$(MAIN_NAME) $(DIR_BUILD)/$(MAIN_NAME).$(EXT_BIN)
main-dbg: MAIN_CC_FLAGS+=-g -DDEBUG -fsanitize=address
main-dbg: main
.PHONY: main main-dbg

# Build swsim.
$(DIR_BUILD)/$(MAIN_NAME).$(EXT_BIN): $(DIR_LIB)/uicc/build/$(LIB_PREFIX)uicc.$(EXT_LIB_STATIC) $(DIR_LIB)/scraw/build/$(LIB_PREFIX)scraw.$(EXT_LIB_STATIC) $(MAIN_OBJ)
	$(CC) $(MAIN_OBJ) -o $(@) $(MAIN_CC_FLAGS)

# Build uicc.
$(DIR_LIB)/uicc/build/$(LIB_PREFIX)uicc.$(EXT_LIB_STATIC):
	cd $(DIR_LIB)/uicc && $(MAKE) main-dbg

# Build scraw.
$(DIR_LIB)/scraw/build/$(LIB_PREFIX)scraw.$(EXT_LIB_STATIC):
	cd $(DIR_LIB)/scraw && $(MAKE)

# Compile source files to object files.
$(DIR_BUILD)/$(MAIN_NAME)/%.o: $(DIR_SRC)/%.c
	$(CC) $(<) -o $(@) $(MAIN_CC_FLAGS) -c -MMD

# Recompile source files after a header they include changes.
-include $(MAIN_DEP)

$(DIR_BUILD) $(DIR_BUILD)/$(MAIN_NAME):
	$(call pal_mkdir,$(@))
clean:
	$(call pal_rmdir,$(DIR_BUILD))
	cd $(DIR_LIB)/uicc && $(MAKE) clean
	cd $(DIR_LIB)/scraw && $(MAKE) clean
.PHONY: clean
