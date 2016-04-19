all: src
	make -C lib

src: src/config.mk
	make -C src

deps:
	make -Csrc deps

src/config.mk:
	echo ucc needs configuring >&2; exit 1

clean:
	make -C src clean
	make -C lib clean

cleanall: clean
	./configure clean

check: all
	cd test && ./run_tests -q -i ignores .

.PHONY: all clean cleanall src
