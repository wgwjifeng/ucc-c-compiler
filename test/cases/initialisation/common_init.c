// RUN: %ucc -fno-leading-underscore -S -o- %s | grep '\(common\|space\)' | grep -vF .globl | %stdoutcheck %s
// STDOUT: nocommon:
// STDOUT-NEXT: .space 4 # object space
// STDOUT-NEXT: .comm common,4,4

int nocommon = 0;
int common;
