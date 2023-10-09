################################################################################

NAME := config_fuzzing

CC := c++

CFLAGS := -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -Wconversion

OBJDIR := obj

################################################################################

ifdef DEBUG
CFLAGS += -g3
endif
# ifdef SAN
# CFLAGS += -fsanitize=address,undefined
# endif
ifdef AFL
NAME := config_fuzzing_afl
CC := afl-clang-lto++
CFLAGS += -Wno-gnu-statement-expression -DAFL=1
OBJDIR := obj_afl
endif
ifdef CTMIN
NAME := config_fuzzing_ctmin
CC := afl-clang-lto++
CFLAGS += -DCTMIN=1
OBJDIR := obj_ctmin
endif
ifdef GCOV
NAME := config_fuzzing_gcov
CC := afl-g++-fast
CFLAGS += -Wno-gnu-statement-expression -fprofile-arcs -ftest-coverage -DGCOV=1
OBJDIR := obj_gcov
endif

################################################################################

CFILES := src/Config.cpp fuzzing/src/main.cpp

################################################################################

OBJFILES := $(addprefix $(OBJDIR)/,$(CFILES:cpp=o))

################################################################################

.PHONY: all
all: $(NAME)

$(NAME): $(OBJFILES)
	$(CC) $(CFLAGS) $(OBJFILES) -o $(NAME)

$(OBJDIR)/%.o : ../%.cpp
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o : %.cpp
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

################################################################################

.PHONY: clean
clean:
	rm -rf afl/

	rm -rf obj/
	rm -rf obj_afl/
	rm -rf obj_ctmin/
	rm -rf obj_gcov/

	rm -f config_fuzzing
	rm -f config_fuzzing_afl
	rm -f config_fuzzing_ctmin
	rm -f config_fuzzing_gcov

.PHONY: fclean
fclean: clean

.PHONY: re
re: clean all

################################################################################
