DIR_LIB:=lib
include $(DIR_LIB)/make-pal/pal.mak
DIR_SRC:=src
DIR_INCLUDE:=include
DIR_BUILD:=build
DIR_TEST:=test
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
	-I$(DIR_LIB)/swicc/include \
	-L$(DIR_LIB)/swicc/build \
	-lswicc \
	$(ARG)
MAIN_SWICC_TARGET:=main
MAIN_SWICC_ARG:=$(ARG_SWICC)

TEST_NAME:=test
TEST_SRC:=$(wildcard $(DIR_TEST)/$(DIR_SRC)/*.c)
TEST_OBJ:=$(TEST_SRC:$(DIR_TEST)/$(DIR_SRC)/%.c=$(DIR_BUILD)/$(TEST_NAME)/%.o)
TEST_DEP:=$(TEST_OBJ:%.o=%.d)
TEST_CC_FLAGS:=\
	-W \
	-Wall \
	-Wextra \
	-Werror \
	-Wno-unused-parameter \
	-Wconversion \
	-Wshadow \
	-O2 \
	-I$(DIR_INCLUDE) \
	-I. \
	-I$(DIR_LIB)/swicc/include \
	-I$(DIR_TEST)/$(DIR_INCLUDE) \
	-I$(DIR_LIB)/tau \
	-L$(DIR_LIB)/swicc/build \
	-L$(DIR_BUILD) \
	-lswicc \
	-fsanitize=address \
	$(ARG)

all: main test
.PHONY: all

main: $(DIR_BUILD) $(DIR_BUILD)/$(MAIN_NAME) $(DIR_BUILD)/$(MAIN_NAME).$(EXT_BIN)
main-dbg: MAIN_SWICC_TARGET:=main-dbg
main-dbg: MAIN_SWICC_ARG+=-fsanitize=address
main-dbg: MAIN_CC_FLAGS+=-g -fsanitize=address -DDEBUG
main-dbg: main
.PHONY: main main-dbg

test: $(DIR_BUILD) $(DIR_BUILD)/$(TEST_NAME) $(DIR_BUILD)/$(TEST_NAME).$(EXT_BIN)
test-dbg: MAIN_SWICC_TARGET:=main-dbg
test-dbg: MAIN_SWICC_ARG+=-fsanitize=address
test-dbg: TEST_CC_FLAGS+=-g -DDEBUG
test-dbg: test
.PHONY: test test-dbg

# Build swSIM.
$(DIR_BUILD)/$(MAIN_NAME).$(EXT_BIN): $(DIR_LIB)/swicc/build/$(LIB_PREFIX)swicc.$(EXT_LIB_STATIC) $(MAIN_OBJ)
	$(CC) $(MAIN_OBJ) -o $(@) $(MAIN_CC_FLAGS)

# Create the test binary.
$(DIR_BUILD)/$(TEST_NAME).$(EXT_BIN): $(DIR_LIB)/swicc/build/$(LIB_PREFIX)swicc.$(EXT_LIB_STATIC) $(TEST_OBJ)
	$(CC) $(TEST_OBJ) -o $(@) $(TEST_CC_FLAGS)

# Build swICC.
$(DIR_LIB)/swicc/build/$(LIB_PREFIX)swicc.$(EXT_LIB_STATIC):
	cd $(DIR_LIB)/swicc && $(MAKE) $(MAIN_SWICC_TARGET) ARG="$(MAIN_SWICC_ARG)"

# Compile source files to object files.
$(DIR_BUILD)/$(MAIN_NAME)/%.o: $(DIR_SRC)/%.c
	$(CC) $(<) -o $(@) $(MAIN_CC_FLAGS) -c -MMD
$(DIR_BUILD)/$(DIR_TEST)/%.o: $(DIR_TEST)/$(DIR_SRC)/%.c
	$(CC) $(<) -o $(@) $(TEST_CC_FLAGS) -c -MMD

# Recompile source files after a header they include changes.
-include $(MAIN_DEP)
-include $(TEST_DEP)

$(DIR_BUILD) $(DIR_BUILD)/$(MAIN_NAME) $(DIR_BUILD)/$(DIR_TEST):
	$(call pal_mkdir,$(@))
clean:
	$(call pal_rmdir,$(DIR_BUILD))
	cd $(DIR_LIB)/swicc && $(MAKE) clean
.PHONY: clean
