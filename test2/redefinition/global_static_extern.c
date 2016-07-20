// RUN: %ucc -S -o %t %s '-DERROR(...)='
//
// RUN: ! grep '\.globl.*x' %t
// RUN: grep '\.globl.*y' %t
// RUN: grep '\.globl.*z' %t
//
// RUN: ! grep '\.globl.*f' %t
// RUN: grep '\.globl.*g' %t
// RUN: grep '\.globl.*h' %t
// RUN: %check -e %s '-DERROR(...)=__VA_ARGS__'

typedef int T;

// ---------- variables ------------

static T x;                                            // CHECK: !/error/
static T x;                                            // CHECK: !/error/
extern T x;                                            // CHECK: !/error/
ERROR(T x;)        // CHECK: /error.*redef/

T y;                                                   // CHECK: !/error/
extern T y;                                            // CHECK: !/error/
T y;                                                   // CHECK: !/error/
ERROR(static T y;) // CHECK: /error.*redef/

extern T z;                                            // CHECK: !/error/
T z;                                                   // CHECK: !/error/
extern T z;                                            // CHECK: !/error/
ERROR(static T z;) // CHECK: /error.*redef/

// ---------- functions ------------

static T f();                                          // CHECK: !/error/
static T f();                                          // CHECK: !/error/
extern T f();                                          // CHECK: !/error/
T f();                                                 // CHECK: !/error/

T g();                                                 // CHECK: !/error/
extern T g();                                          // CHECK: !/error/
T g();                                                 // CHECK: !/error/
ERROR(static T g();) // CHECK: /error.*redef/

extern T h();                                          // CHECK: !/error/
T h();                                                 // CHECK: !/error/
extern T h();                                          // CHECK: !/error/
ERROR(static T h();) // CHECK: /error.*redef/
