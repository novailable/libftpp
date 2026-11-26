
CXX = c++
RM = rm -f

INCLUDES = -I.. \
           -I../data_structures \
           -I../design_patterns \
           -I../iostream \
           -I../mathematics \
           -I../threading \
		   -I../network

CXXFLAGS = $(INCLUDES) -Wall -Wextra -Werror -std=c++20 -pthread

LDFLAGS = -pthread

TSAN_FLAGS = -g -O1 -fsanitize=thread -fno-omit-frame-pointer

LIB = ../libftpp.a

LIB_SRCS = ../data_structures/data_buffer.cpp \
           ../design_patterns/memento.cpp \
           ../iostream/thread_safe_iostream.cpp \
           ../mathematics/ivector2.cpp \
           ../mathematics/ivector3.cpp \
           ../mathematics/random_2D_coordinate_generator.cpp \
           ../mathematics/perlin_noise_2D.cpp \
           ../threading/thread.cpp \
           ../threading/worker_pool.cpp \
		   ../threading/presistent_worker.cpp \
		   ../network/message.cpp

TESTS = thread_safe_iostream_test \
        thread_safe_queue_test \
        thread_test \
		worker_pool_test \
		presistent_worker_test \
		message_test

BUILD_DIR = build

TEST_BINS = $(addprefix $(BUILD_DIR)/,$(TESTS))
TSAN_BINS = $(addprefix $(BUILD_DIR)/tsan/,$(TESTS))

all: $(TEST_BINS)

# Normal test compilation
$(BUILD_DIR)/%: %.cpp $(LIB) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< $(LIB) $(LDFLAGS) -o $@

# ThreadSanitizer compilation
$(BUILD_DIR)/tsan/%: %.cpp $(LIB_SRCS) | $(BUILD_DIR)/tsan
	$(CXX) $(CXXFLAGS) $(TSAN_FLAGS) $< $(LIB_SRCS) $(LDFLAGS) -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/tsan:
	mkdir -p $@

# Execute all normal tests
run: all
	@set -e; \
	for test in $(TEST_BINS); do \
		echo ""; \
		echo "========================================"; \
		echo "Running $$test"; \
		echo "========================================"; \
		./$$test; \
	done; \
	echo ""; \
	echo "All tests passed!"

# Execute all tests with ThreadSanitizer
tsan: $(TSAN_BINS)
	@set -e; \
	for test in $(TSAN_BINS); do \
		echo ""; \
		echo "========================================"; \
		echo "Running $$test (ThreadSanitizer)"; \
		echo "========================================"; \
		./$$test; \
	done; \
	echo ""; \
	echo "All ThreadSanitizer tests passed!"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run tsan clean
