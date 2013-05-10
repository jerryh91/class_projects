TURNIN   := /lusr/bin/turnin
GRADER   := imr
LAB      ?= lab8
LAB_NAME := cs439w-$(LAB)
TARBALL  := $(LAB)-handin.tar.gz

CC = gcc
CFLAGS = -Wall -g -O2
LDFLAGS = -lpthread -lrt

OBJS = proxy.o csapp.o sthread.o

all: proxy

proxy: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

# Most of the next two rules ripped from MIT JOS.
tarball:
	@if ! git diff-files --quiet || ! git diff-index --quiet --cached HEAD; then \
		git status; \
		echo; \
		echo "You have uncomitted changes.  Please commit or stash them."; \
		false; \
	fi
	@if test -n "`git ls-files -o --exclude-standard`"; then \
		git status; \
		read -p "Untracked files will not be handed in.  Continue? [y/N] " r; \
		test "$$r" = y; \
	fi
	tar -cf - `git ls-files` './.git' | gzip > $(TARBALL)

turnin: tarball
	@echo
	@echo "Are you sure you want to turn in this lab to \"$(LAB_NAME)\"? If Yes, press any key (and Enter) to continue. Else, press Ctrl-C to abort"
	@read p
	$(TURNIN) --submit $(GRADER) $(LAB_NAME) $(TARBALL)

turnin-part%:
	$(MAKE) turnin "LAB=$(LAB)`echo '$*' | tr \[A-Z\] \[a-z\]`"

clean:
	rm -f *~ *.o proxy core

.PHONY: clean tarball tidy

tidy:
	git clean -dff
