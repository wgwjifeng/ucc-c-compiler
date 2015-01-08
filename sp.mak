CC = ./ucc

sp: sp.s
	${CC} -o $@ $<

sp.s: sp.s_
	sed -e 's/__stack_chk_guard/&@GOTPCREL/' < $< > $@

sp.s_: sp.c
	${CC} -S -o $@ $<

clean:
	rm -f sp sp.s sp.s_
