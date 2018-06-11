CC = ./ucc
CFLAGS = -mstackrealign

unalign: unalign_thunk.o unalign.o
	${CC} -o $@ $^ ${LDFLAGS}

unalign.o: unalign.s
	${CC} -c -o $@ $< ${ASFLAGS}
unalign.s: unalign.c
	${CC} -S -o $@ $< ${CFLAGS}

clean:
	rm -f unalign unalign.o unalign.s unalign_thunk.o
