# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: aoo <aoo@student.42.fr>                    +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/03/19 23:28:02 by aoo               #+#    #+#              #
#    Updated: 2026/10/12 19:45:11 by aoo              ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = libftpp.a

CXX = c++
INCLUDES = -Imathematics
CXXFLAGS = $(INCLUDES) -Wall -Wextra -Werror -std=c++20

AR = ar rcs
RM = rm -f

MATHEMATICS = mathematics/ivector2.cpp \
              mathematics/ivector3.cpp \
              mathematics/random_2D_coordinate_generator.cpp

SRCS = $(MATHEMATICS)
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
