#include <stddef.h>

#include "../type.h"
#include "../type_nav.h"
#include "../pack.h"

#include "macros.h"

#include "forwards.h"

#include "val.h"
#include "ctx.h"
#include "blk.h"

#include "out.h"

#include "stack.h"

void v_stack_adj(out_ctx *octx, v_stackt amt, int sub)
{
	out_flush_volatile(
			octx,
			out_op(
				octx, sub ? op_minus : op_plus,
				v_new_sp(octx, NULL),
				out_new_l(
					octx,
					type_nav_btype(cc1_type_nav, type_intptr_t),
					amt)));
}

static void align_sz(unsigned *psz, unsigned align)
{
	/* align greater than size - we increase
	 * size so it can be aligned to 'align' */
	if(align > *psz)
		*psz = pack_to_align(*psz, align);
}

const out_val *out_aalloc(
		out_ctx *octx, unsigned sz, unsigned align, type *in_ty)
{
	type *ty = type_ptr_to(in_ty
		? in_ty : type_nav_btype(cc1_type_nav, type_nchar));

	align_sz(&sz, align);

	/* packing takes care of everything */
	pack_next(&octx->cur_stack_sz, NULL, sz, align);

	return v_new_bp3_below(octx, NULL, ty, octx->cur_stack_sz);
}

void out_adealloc(out_ctx *octx, const out_val **val)
{
	/* TODO: reclaim stack */
	out_val_release(octx, *val);
	*val = NULL;
}

void v_aalloc_noop(out_ctx *octx, unsigned sz, unsigned align, const char *why)
{
	(void)why;

	align_sz(&sz, align);

	octx->cur_stack_sz += sz;
}

void v_stack_needalign(out_ctx *octx, unsigned align)
{
	/* aligning the stack isn't sufficient here - if the stack is adjusted after,
	 * it might not be at a 16-byte alignment */
	octx->max_align = MAX(octx->max_align, align);
}
