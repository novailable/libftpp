ROOT = ..

CXX = c++
AR = ar rcs
RM = rm -rf

BUILD_DIR = build
NAME = test_tsan
LIB = libftpp_tsan.a

INCLUDES = -I$(ROOT)/data_structures \
           -I$(ROOT)/design_patterns \
           -I$(ROOT)/iostream \
           -I$(ROOT)/mathematics

CXXFLAGS = $(INCLUDES) -Wall -Wextra -Werror -std=c++20
TSAN_FLAGS = -g -O1 -fsanitize=thread -fno-omit-frame-pointer -pthread

SRCS = $(ROOT)/data_structures/data_buffer.cpp \
       $(ROOT)/design_patterns/memento.cpp \
       $(ROOT)/iostream/thread_safe_iostream.cpp \
       $(ROOT)/mathematics/ivector2.cpp \
       $(ROOT)/mathematics/ivector3.cpp \
       $(ROOT)/mathematics/random_2D_coordinate_generator.cpp \
       $(ROOT)/mathematics/perlin_noise_2D.cpp

OBJS = $(patsubst $(ROOT)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

TEST_SRC = thread_safe_iostream_test.cpp

all: $(NAME)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(ROOT)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(TSAN_FLAGS) -c $< -o $@

$(LIB): $(OBJS)
	$(AR) $(LIB) $(OBJS)

$(NAME): $(TEST_SRC) $(LIB) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(TSAN_FLAGS) $(TEST_SRC) $(LIB) -o $(NAME)

run: $(NAME)
	./$(NAME)

clean:
	$(RM) $(BUILD_DIR) *.dSYM

fclean: clean
	$(RM) $(NAME) $(LIB)

re: fclean all

.PHONY: all run clean fclean re