#include <stdio.h>
#include <stdarg.h>

#include "../../util/util.h"

#include "../decl.h"
#include "../op.h"
#include "../macros.h"

#include "val.h"
#include "asm.h"
#include "impl.h"
#include "write.h"

void impl_comment(out_ctx *octx, const char *fmt, va_list l)
{
	out_asm2(octx, SECTION_TEXT, P_NO_NL, "/* ");
	out_asmv(octx, SECTION_TEXT, P_NO_INDENT | P_NO_NL, fmt, l);
	out_asm2(octx, SECTION_TEXT, P_NO_INDENT, " */");
}

enum flag_cmp op_to_flag(enum op_type op)
{
	switch(op){
#define OP(x) case op_ ## x: return flag_ ## x
		OP(eq);
		OP(ne);
		OP(le);
		OP(lt);
		OP(ge);
		OP(gt);
#undef OP

		default:
			break;
	}

	ICE("invalid op");
	return -1;
}

const char *flag_cmp_to_str(enum flag_cmp cmp)
{
	switch(cmp){
		CASE_STR_PREFIX(flag, eq);
		CASE_STR_PREFIX(flag, ne);
		CASE_STR_PREFIX(flag, le);
		CASE_STR_PREFIX(flag, lt);
		CASE_STR_PREFIX(flag, ge);
		CASE_STR_PREFIX(flag, gt);
		CASE_STR_PREFIX(flag, overflow);
		CASE_STR_PREFIX(flag, no_overflow);
	}
	return NULL;
}

int impl_reg_is_callee_save(type *fnty, const struct vreg *r)
{
	unsigned i, n;
	const int *csaves;

	if(r->is_float)
		return 0;

	csaves = impl_callee_save_regs(fnty, &n);

	for(i = 0; i < n; i++)
		if(r->idx == csaves[i])
			return 1;
	return 0;
}

const char *impl_val_str(const out_val *vs, int deref)
{
	static char buf[VAL_STR_SZ];
	return impl_val_str_r(buf, vs, deref);
}
