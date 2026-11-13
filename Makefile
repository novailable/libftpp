# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: aoo <aoo@student.42singapore.sg>           +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/03/19 23:28:02 by aoo               #+#    #+#              #
#    Updated: 2026/11/14 03:13:09 by aoo              ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = libftpp.a

CXX = c++
AR = ar
ARFLAGS = rcs
RM = rm -f

INCLUDES = -Idata_structures \
           -Idesign_patterns \
           -Iiostream \
           -Imathematics \
           -Ithreading

CXXFLAGS = $(INCLUDES) -Wall -Wextra -Werror -std=c++20 -pthread

DATA_STRUCTURES = data_structures/data_buffer.cpp

DESIGN_PATTERNS = design_patterns/memento.cpp

IOSTREAM = iostream/thread_safe_iostream.cpp

MATHEMATICS = mathematics/ivector2.cpp \
              mathematics/ivector3.cpp \
              mathematics/random_2D_coordinate_generator.cpp \
              mathematics/perlin_noise_2D.cpp

THREADING = threading/thread.cpp \
            threading/worker_pool.cpp

SRCS = $(DATA_STRUCTURES) \
       $(DESIGN_PATTERNS) \
       $(IOSTREAM) \
       $(MATHEMATICS) \
       $(THREADING)

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(NAME)
	$(MAKE) -C test run

test-tsan:
	$(MAKE) -C test tsan

clean:
	$(RM) $(OBJS)
	$(MAKE) -C test clean

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all test test-tsan clean fclean re
