################################################################################

NAME := webserv

CC := c++

CFLAGS := -Wall -Wextra -Werror -Wpedantic -Wfatal-errors -Wshadow -Wswitch -Wimplicit-fallthrough -Wno-c99-designator -Werror=type-limits -fstandalone-debug -std=c++2b

################################################################################

ifdef O3
CFLAGS += -Ofast
endif
ifdef DEBUG
CFLAGS += -g3
CFLAGS += -Wfatal-errors
endif
ifdef SAN
CFLAGS += -fsanitize=address,undefined
endif
ifdef SUPPRESS_LOGGING
CFLAGS += -DSUPPRESS_LOGGING
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
