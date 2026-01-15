CXX      := g++
CXXFLAGS := -std=c++20 -O2 -g -Wall -Wextra -Wpedantic
LDFLAGS  :=
LDLIBS   :=

# libcurl Flags (falls pkg-config verfügbar ist)
CURL_CFLAGS := $(shell pkg-config --cflags libcurl 2>/dev/null)
CURL_LIBS   := $(shell pkg-config --libs libcurl 2>/dev/null)

# Falls pkg-config nichts liefert, fallback:
ifeq ($(strip $(CURL_LIBS)),)
  CURL_LIBS := -lcurl
endif

CXXFLAGS += $(CURL_CFLAGS)
LDLIBS   += $(CURL_LIBS)

TARGET := buggy

SRCS := \
  main.cpp \
  argumentParser.cpp \
  ProcessRunner.cpp \
  ShellQuote.cpp \
  GdbRunner.cpp \
  ValgrindRunner.cpp \
  ReportJson.cpp \
  ClaudeClient.cpp

BUILD_DIR := build
OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

-include $(DEPS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
