// RUN: %caret_check %s -pedantic

int i = (void *)0;
// CARETS:
//    ^ note: cast expr
//      ^ warning

// not a top-level init:
int x[] = { (void *)0, 2 };
// CARETS:
//      ^ note: cast expr
//          ^ warning
