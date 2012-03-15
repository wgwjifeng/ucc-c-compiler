#ifndef OP_H
#define OP_H

typedef int         func_op_exec(op *, int *bad);
typedef int         func_op_optimise(op *);
typedef void        func_op_compare(op *);
typedef void        func_op_fold(op *);
typedef void        func_op_gen(op *);
typedef void        func_op_gen_store(op *);

struct op
{
	func_op_exec       *f_exec;
	func_op_optimise   *f_optimise; /* optional */

	func_op_fold       *f_fold;
	func_op_gen        *f_gen, *f_gen_str;
	func_op_gen_store  *f_store;   /* optional */
	func_op_compare    *f_compare; /* optional */

	func_str      *f_str;

	expr *lhs, *rhs;
};


op   *op_new(expr *lhs, expr *rhs, func_op_exec *f_exec, func_op_fold *f_fold, func_op_gen *f_gen, func_op_gen *f_gen_str, func_str *f_str);
void  op_mutate(op *e,             func_op_exec *f_exec, func_op_fold *f_fold, func_op_gen *f_gen, func_op_gen *f_gen_str, func_str *f_str);

#include "ops/op_deref.h"
#include "ops/op_divide.h"
#include "ops/op_eq.h"
#include "ops/op_ge.h"
#include "ops/op_le.h"
#include "ops/op_minus.h"
#include "ops/op_modulus.h"
#include "ops/op_multiply.h"
#include "ops/op_not.h"
#include "ops/op_or.h"
#include "ops/op_orsc.h"
#include "ops/op_plus.h"
#include "ops/op_shiftl.h"
#include "ops/op_struct_ptr.h"
#include "ops/op_xor.h"
#include "ops/op_and.h"
#include "ops/op_andsc.h"
#include "ops/op_bnot.h"
#include "ops/op_gt.h"
#include "ops/op_lt.h"
#include "ops/op_ne.h"
#include "ops/op_shiftr.h"
#include "ops/op_struct_dot.h"

#define op_new_wrapper(type, l, r)  op_new(l, r, exec_op_ ## type, fold_op_ ## type, gen_op_ ## type, gen_str_op_ ## type, str_op_ ## type)
#define op_mutate_wrapper(e, type)  op_mutate(e, exec_op_ ## type, fold_op_ ## type, gen_op_ ## type, gen_str_op_ ## type, str_op_ ## type)

#define OP_NEW(x) op *op_new_ ## x(expr *l, expr *r)
OP_NEW(divide);
OP_NEW(eq);
OP_NEW(ge);
OP_NEW(le);
OP_NEW(minus);
OP_NEW(modulus);
OP_NEW(multiply);
OP_NEW(or);
OP_NEW(orsc);
OP_NEW(plus);
OP_NEW(shiftl);
OP_NEW(struct_ptr);
OP_NEW(xor);
OP_NEW(and);
OP_NEW(andsc);
OP_NEW(gt);
OP_NEW(lt);
OP_NEW(ne);
OP_NEW(shiftr);
OP_NEW(struct_dot);
#undef OP_NEW
#define OP_NEW(x) op *op_new_ ## x(expr *)
OP_NEW(not);
OP_NEW(bnot);
OP_NEW(deref);
#undef OP_NEW

#if 0
enum op_type curtok_to_augmented_op()
{
#define CASE(x) case token_ ## x ## _assign: return op_ ## x
	switch(curtok){
		CASE(plus);
		CASE(minus);
		CASE(multiply);
		CASE(divide);
		CASE(modulus);
		CASE(not);
		CASE(bnot);
		CASE(and);
		CASE(or);
		CASE(xor);
		CASE(shiftl);
		CASE(shiftr);
		default:
			break;
	}
	return op_unknown;
#undef CASE
}
#endif


#endif
