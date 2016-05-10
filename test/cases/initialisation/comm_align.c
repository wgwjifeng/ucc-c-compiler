// RUN: %ucc -fno-leading-underscore -S -o- %s | grep -F .comm | %stdoutcheck %s
// STDOUT: .comm i,1,8
// STDOUT-NEXT: .comm j,1,4
// STDOUT-NEXT: .comm k,1,2

char i __attribute((aligned(8)));
_Alignas(int) char j;
_Alignas(2) char k;
