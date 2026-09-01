CC := cc

BIN_DIR := bin
OBJ_DIR := obj
SRC_DIR := src
INC_DIR := include
TARGET  := $(BIN_DIR)/owsg

# --- source discovery ---
# All .c files under src/, split into "ours" and "thirdparty" so we can
# apply different warning flags to each. shell find is used (rather than
# make's wildcard) so this also picks up files in subdirectories we add
# later without editing the Makefile.
ALL_SRCS        := $(shell find $(SRC_DIR) -name '*.c')
THIRDPARTY_SRCS := $(shell find $(SRC_DIR)/thirdparty -name '*.c' 2>/dev/null)
OUR_SRCS        := $(filter-out $(THIRDPARTY_SRCS),$(ALL_SRCS))

# Mirror src/ under obj/, one .o (and one .d, via -MMD) per .c.
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(ALL_SRCS))
DEPS := $(OBJS:.o=.d)

# --- includes ---
# Our own public headers: normal -I, warnings here are ours to fix.
# Thirdparty headers: -isystem, so warnings *inside* these headers are
# never attributed to files that merely #include them, even when those
# files are compiled with our strict flags below.
#
# Each thirdparty library gets its own -isystem entry rooted one level
# above that library's public include folder(s), matching how its
# sources #include it (e.g. glad's gl.c does #include <glad/gl.h>, and
# glad/gl.h lives at include/thirdparty/glad/glad/gl.h - so the search
# root is include/thirdparty/glad, not include/thirdparty itself).
OWN_INCLUDES        := -I$(INC_DIR)
THIRDPARTY_INCLUDES := -isystem $(INC_DIR)/thirdparty/glad

# pkg-config resolves GLFW's cflags/libs for the machine we're building
# on, rather than us hardcoding -lglfw and hoping the search paths line
# up.
GLFW_CFLAGS := $(shell pkg-config --cflags glfw3)
GLFW_LIBS   := $(shell pkg-config --libs glfw3)

# glad's loader needs libdl on Linux to call dlopen/dlsym when resolving
# GL function pointers at runtime.
SYS_LIBS := -ldl -lm

# --- warning flags: applied ONLY to our own code ---
WARN_FLAGS := \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Wshadow \
	-Wconversion \
	-Wsign-conversion \
	-Wcast-qual \
	-Wwrite-strings \
	-Wformat=2 \
	-Wundef \
	-Wstrict-prototypes \
	-Wold-style-definition \
	-Wimplicit-fallthrough \
	-Wlogical-op \
	-Wcast-align \
	-Wvla \
	-Wnull-dereference \
	-Wdouble-promotion \
	-Wformat-overflow=2 \
	-Wformat-truncation=2 \
	-Walloc-zero \
	-Warray-bounds=2 \
	-Wstringop-overflow=4 \
	-Wstrict-overflow=5 \
	-Wswitch-enum \
	-Wpointer-arith \
	-Winit-self \
	-Werror

# Thirdparty code is vendored, not ours to fix - compile it quietly.
THIRDPARTY_WARN_FLAGS := -w

STD_FLAGS := -std=c11
# -MMD: emit a .d dependency file per .o, next to it, listing the
#       headers that .c file transitively includes (excluding system
#       headers), so editing a header correctly triggers a rebuild of
#       everything that includes it.
# -MP:  add a phony target for each header dependency, so deleting or
#       renaming a header doesn't break the build with a "no rule to
#       make target" error on a stale .d file.
DEP_FLAGS := -MMD -MP

# Debug build by default.
OPT_FLAGS := -O0 -g

CFLAGS            := $(STD_FLAGS) $(OPT_FLAGS) $(DEP_FLAGS) $(WARN_FLAGS) $(OWN_INCLUDES) $(THIRDPARTY_INCLUDES) $(GLFW_CFLAGS)
THIRDPARTY_CFLAGS := $(STD_FLAGS) $(OPT_FLAGS) $(DEP_FLAGS) $(THIRDPARTY_WARN_FLAGS) $(OWN_INCLUDES) $(THIRDPARTY_INCLUDES) $(GLFW_CFLAGS)

LDFLAGS := $(GLFW_LIBS) $(SYS_LIBS)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Our sources: strict flags.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Thirdparty sources: lenient flags. This pattern rule is listed AFTER
# the general one above but matches more specifically (it's anchored
# under src/thirdparty/), and GNU make prefers the more specific stem
# match, so thirdparty .c files are routed here instead of the strict
# rule.
$(OBJ_DIR)/thirdparty/%.o: $(SRC_DIR)/thirdparty/%.c
	@mkdir -p $(dir $@)
	$(CC) $(THIRDPARTY_CFLAGS) -c $< -o $@

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

run: all
	./$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

-include $(DEPS)
