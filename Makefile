DIR_LIB:=lib
include $(DIR_LIB)/make-pal/pal.mak
DIR_SRC:=src
DIR_INCLUDE:=include
DIR_BUILD:=build
DIR_BUILD_LIB:=build-lib
CC:=gcc

MAIN_NAME:=softsim
MAIN_SRC:=$(wildcard $(DIR_SRC)/*.c)
MAIN_OBJ:=$(MAIN_SRC:$(DIR_SRC)/%.c=$(DIR_BUILD)/%.o)
MAIN_DEP:=$(MAIN_OBJ:%.o=%.d)
MAIN_CC_FLAGS:=-W -Wall -Wextra -Wconversion -Wshadow -O2 \
	-I$(DIR_INCLUDE) -I$(DIR_BUILD_LIB)/uicc/$(DIR_INCLUDE) -I$(DIR_BUILD_LIB)/scraw/$(DIR_INCLUDE) \
	-L$(DIR_BUILD_LIB)/uicc -L$(DIR_BUILD_LIB)/scraw \
	$(shell pkg-config --cflags libpcsclite) \
	-luicc -lscraw -lpcsclite
MAIN_LD_FLAGS:=

all: all-lib main
all-fast: main
all-dbg: MAIN_CC_FLAGS+=-g -DDEBUG
all-dbg: main
all-lib: uicc scraw

# Build softsim.
main: $(DIR_BUILD) $(DIR_BUILD)/$(MAIN_NAME).$(EXT_BIN)
$(DIR_BUILD)/$(MAIN_NAME).$(EXT_BIN): $(MAIN_OBJ)
	$(CC) $(^) -o $(@) $(MAIN_CC_FLAGS) $(MAIN_LD_FLAGS)

# Build uicc.
uicc: $(DIR_BUILD_LIB)
	cd $(DIR_LIB)/uicc && $(MAKE)
	$(call pal_mkdir,$(DIR_BUILD_LIB)/uicc)
	$(call pal_mkdir,$(DIR_BUILD_LIB)/uicc/$(DIR_INCLUDE))
	$(call pal_cp,$(DIR_LIB)/uicc/build/$(LIB_PREFIX)uicc.$(EXT_LIB_STATIC),$(DIR_BUILD_LIB)/uicc/$(LIB_PREFIX)uicc.$(EXT_LIB_STATIC))
	$(call pal_cpdir,$(DIR_LIB)/uicc/include,$(DIR_BUILD_LIB)/uicc/$(DIR_INCLUDE))

# Build scraw.
scraw: $(DIR_BUILD_LIB)
	cd $(DIR_LIB)/scraw && $(MAKE)
	$(call pal_mkdir,$(DIR_BUILD_LIB)/scraw)
	$(call pal_mkdir,$(DIR_BUILD_LIB)/scraw/$(DIR_INCLUDE))
	$(call pal_cp,$(DIR_LIB)/scraw/build/$(LIB_PREFIX)scraw.$(EXT_LIB_STATIC),$(DIR_BUILD_LIB)/scraw/$(LIB_PREFIX)scraw.$(EXT_LIB_STATIC))
	$(call pal_cpdir,$(DIR_LIB)/scraw/include,$(DIR_BUILD_LIB)/scraw/$(DIR_INCLUDE))

# Compile source files to object files.
$(DIR_BUILD)/%.o: $(DIR_SRC)/%.c
	$(CC) $(<) -o $(@) $(MAIN_CC_FLAGS) -c -MMD

# Recompile source files after a header they include changes.
-include $(MAIN_DEP)

$(DIR_BUILD) $(DIR_BUILD_LIB):
	$(call pal_mkdir,$(@))
clean:
	$(call pal_rmdir,$(DIR_BUILD))
	$(call pal_rmdir,$(DIR_BUILD_LIB))
	cd $(DIR_LIB)/uicc && $(MAKE) clean
	cd $(DIR_LIB)/scraw && $(MAKE) clean

.PHONY: all all-fast all-lib main uicc scraw clean
