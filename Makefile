################################################################################

NAME := webserv

CC := c++

CFLAGS := -Wall -Wextra -Werror -Wpedantic -Wshadow

################################################################################

ifdef O3
CFLAGS += -Ofast
endif
ifdef DEBUG
CFLAGS += -g3
CFLAGS += -Wfatal-errors
endif
# ifdef COVERAGE
# CFLAGS += --coverage -O0
# endif
ifdef SAN
CFLAGS += -fsanitize=address,undefined
endif

################################################################################

CFILES := $(wildcard src/*.cpp src/**/*.cpp)

################################################################################

OBJDIR := obj
OBJFILES := $(addprefix $(OBJDIR)/,$(CFILES:cpp=o))

################################################################################

.PHONY: all
all: $(NAME)

$(NAME): $(OBJFILES)
	@$(CC) $(CFLAGS) $(OBJFILES) -o $(NAME)
	@echo Created executable \'$(NAME)\'

.PHONY: run
run: all
	clear
	./webserv

.PHONY: drun
drun:
	@$(MAKE) DEBUG=1 SAN=1 run

################################################################################

.PHONY: clean
clean:
	@$(RM) -rf $(OBJDIR)
	@echo Deleted \'obj/\'

.PHONY: fclean
fclean: clean
	@$(RM) -f $(NAME)
	@echo Deleted executable \'$(NAME)\'

.PHONY: re
re: fclean all

# TODO: Use coverage
# .PHONY: coverage
# coverage: re
# 	./$(NAME); cd obj; /Users/sbos/Library/Python/3.9/bin/gcovr --html-nested coverage.html -r ..

################################################################################

# Source and explanation: https://stackoverflow.com/a/52036564/13279557

DEPENDS := $(patsubst %.o,%.d,$(OBJFILES))

-include $(DEPENDS)

$(OBJDIR)/%.o : %.cpp Makefile
	@mkdir -p $(@D)
	@$(call tidy_compilation,$(CC) $(CFLAGS) -MMD -MP -c $< -o $@)

define tidy_compilation
	@printf "%s\e[K\n" "$(1)"
	@$(1)
	@printf "\e[A\e[K"
endef

################################################################################
