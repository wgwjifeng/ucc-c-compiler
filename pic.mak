.PHONY: all

all: lbl.gcc.32.pic.s lbl.gcc.32.no-pic.s lbl.gcc.64.pic.s lbl.gcc.64.no-pic.s lbl.clang.32.pic.s lbl.clang.32.no-pic.s lbl.clang.64.pic.s lbl.clang.64.no-pic.s

lbl.%.s: lbl.c
	$(shell echo $@ | cut -d. -f2 -) \
		-m$(shell echo $@ | cut -d. -f3 -) \
		-f$(shell echo $@ | cut -d. -f4 -) \
		-S -o $@ $<
