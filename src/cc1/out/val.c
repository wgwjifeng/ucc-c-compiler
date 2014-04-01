#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../../util/alloc.h"

#include "../type.h"
#include "../type_nav.h"

#include "../macros.h"

#include "val.h"
#include "ctx.h"

#include "../op.h"
#include "asm.h"
#include "impl.h"
#include "out.h" /* retain/release prototypes */

#include "../cc1.h" /* cc1_type_nav */

const char *v_store_to_str(enum out_val_store store)
{
	switch(store){
		CASE_STR(V_CONST_I);
		CASE_STR(V_REG);
		CASE_STR(V_REG_SAVE);
		CASE_STR(V_LBL);
		CASE_STR(V_CONST_F);
		CASE_STR(V_FLAG);
	}
	return NULL;
}

static void v_register(out_ctx *octx, out_val *v)
{
	assert(!v->next);

	/* double link */
	v->next = octx->val_head;
	if(octx->val_head)
		octx->val_head->prev = v;

	/* store in val_head */
	octx->val_head = v;

	if(!octx->val_tail)
		octx->val_tail = v;
}

static void v_init(out_val *v, type *ty)
{
	v->retains = 1;
	v->t = ty;
}

out_val *v_new(out_ctx *octx, type *ty)
{
	out_val *v = umalloc(sizeof *v);

	v_init(v, ty);
	v_register(octx, v);

	return v;
}

out_val *v_new_from(out_ctx *octx, out_val *from, type *ty)
{
	if(!from)
		return v_new(octx, ty);

	out_val_consume(octx, from);

	if(from->retains == 0){
		v_init(from, ty);
		return from;
	}else{
		out_val *v = v_new(octx, ty);
		memcpy_safe(v, from);
		v_init(v, ty);
		return v;
	}
}

out_val *v_new_flag(
		out_ctx *octx, out_val *from,
		enum flag_cmp cmp, enum flag_mod mod)
{
	out_val *v = v_new_from(octx, from,
			type_nav_btype(cc1_type_nav, type__Bool));

	v->type = V_FLAG;
	v->bits.flag.cmp = cmp;
	v->bits.flag.mods = mod;
	return v;
}

out_val *v_new_reg(
		out_ctx *octx, out_val *from,
		type *ty, const struct vreg *reg)
{
	out_val *v = v_new_from(octx, from, ty);
	v->type = V_REG;
	memcpy_safe(&v->bits.regoff.reg, reg);
	return v;
}

out_val *v_new_sp(out_ctx *octx, out_val *from)
{
	struct vreg r;

	r.is_float = 0;
	r.idx = REG_SP;

	return v_new_reg(octx, from, type_nav_voidptr(cc1_type_nav), &r);
}

out_val *v_new_sp3(out_ctx *octx, out_val *from, type *ty, long stack_pos)
{
	out_val *v = v_new_sp(octx, from);
	v->t = ty;
	v->bits.regoff.offset = stack_pos;
	return v;
}

out_val *out_val_release(out_ctx *octx, out_val *v)
{
	(void)octx;
	assert(v->retains > 0 && "double release");
	if(--v->retains == 0)
		return NULL;
	return v;
}

out_val *out_val_retain(out_ctx *octx, out_val *v)
{
	(void)octx;
	v->retains++;
	return v;
}

int vreg_eq(const struct vreg *a, const struct vreg *b)
{
	return a->idx == b->idx && a->is_float == b->is_float;
}
