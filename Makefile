CC_64  ?= x86_64-w64-mingw32-gcc
BOFLINT ?= python3 scripts/boflint.py

-include config.mk

GITHUB_OWNER ?= example
GITHUB_REPO  ?= c2-channel
GITHUB_PAT   ?= ghp_dummy_token_for_ci

DEFINES = -DGITHUB_OWNER=\"$(GITHUB_OWNER)\" \
          -DGITHUB_REPO=\"$(GITHUB_REPO)\" \
          -DGITHUB_PAT=\"$(GITHUB_PAT)\"

INCLUDES = -Iinclude -Isrc

CFLAGS     = -std=c11 -Wall -Wextra -O2 $(INCLUDES) $(DEFINES)
BOF_CFLAGS = -std=c11 -Wall -Wextra -Os -c -DBOF \
             -fno-stack-protector -fno-builtin -fno-asynchronous-unwind-tables \
             $(INCLUDES) $(DEFINES)

EXE_SRCS = src/main.c src/github.c src/json.c src/b64.c src/util.c
EXE_LIBS = -lwinhttp -ladvapi32

.PHONY: all exe bof lint lint-bof clean

all: exe bof

bin:
	mkdir -p bin

exe: bin
	$(CC_64) $(CFLAGS) $(EXE_SRCS) -o bin/github-c2.exe $(EXE_LIBS)

bof: bin
	$(CC_64) $(BOF_CFLAGS) src/udc2_bof.c -o bin/github-c2.x64.o

lint:
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --language=c --std=c11 --enable=warning --error-exitcode=1 --force \
			--inline-suppr --suppress=missingIncludeSystem --suppress=missingInclude \
			--suppress=unusedFunction src include; \
	else \
		echo "cppcheck not installed; skipping"; \
	fi

lint-bof: bof
	$(BOFLINT) --loader cs bin/github-c2.x64.o

clean:
	rm -rf bin
