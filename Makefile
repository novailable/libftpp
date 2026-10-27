# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: aoo <aoo@student.42.fr>                    +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/03/19 23:28:02 by aoo               #+#    #+#              #
#    Updated: 2026/10/27 14:25:06 by aoo              ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = libftpp.a

CXX = c++
INCLUDES = -Idata_structures -Idesign_patterns -Imathematics 
CXXFLAGS = $(INCLUDES) -Wall -Wextra -Werror -std=c++20

AR = ar rcs
RM = rm -f

DATA_STRUCTURES = data_structures/data_buffer.cpp

DESIGN_PATTERNS = design_patterns/memento.cpp

MATHEMATICS = mathematics/ivector2.cpp \
              mathematics/ivector3.cpp \
              mathematics/random_2D_coordinate_generator.cpp \
			  mathematics/perlin_noise_2D.cpp			  

SRCS = $(DATA_STRUCTURES) $(DESIGN_PATTERNS) $(MATHEMATICS) 
OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(AR) $(NAME) $(OBJS)

clean:
	$(RM) $(OBJS)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
