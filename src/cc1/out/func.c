#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include "../type.h"

#include "val.h"
#include "out.h" /* this file defs */
#include "asm.h"
#include "impl.h"
#include "ctx.h"
#include "virt.h"
#include "blk.h"
#include "dbg.h"

#include "../cc1.h" /* mopt_mode */
#include "../../util/platform.h"

out_val *out_call(out_ctx *octx,
		out_val *fn, out_val **args,
		type *fnty)
{
	return impl_call(octx, fn, args, fnty);
}

void out_func_epilogue(out_ctx *octx, type *ty, char *end_dbg_lbl)
{
	impl_func_epilogue(octx, ty);

	out_dbg_label(octx, end_dbg_lbl);

	blk_flushall(octx);

	octx->current_blk = NULL;

	octx->stack_local_offset = octx->stack_sz = 0;
}

void out_func_prologue(
		out_ctx *octx, const char *sp,
		type *rf,
		int stack_res, int nargs, int variadic,
		int arg_offsets[], int *local_offset)
{
	assert(octx->stack_sz == 0 && "non-empty stack for new func");

	assert(!octx->current_blk);
	octx->first_blk = octx->current_blk = out_blk_new(octx, sp);

	impl_lbl(octx, sp);

	impl_func_prologue_save_fp(octx);

	if(mopt_mode & MOPT_STACK_REALIGN)
		v_stack_align(octx, cc1_mstack_align, 1);

	impl_func_prologue_save_call_regs(octx, rf, nargs, arg_offsets);

	if(variadic) /* save variadic call registers */
		impl_func_prologue_save_variadic(octx, rf);

	/* setup "pointers" to the right place in the stack */
	octx->stack_variadic_offset = octx->stack_sz - platform_word_size();
	octx->stack_local_offset = octx->stack_sz;
	*local_offset = octx->stack_local_offset;

	if(stack_res)
		v_alloc_stack(octx, stack_res, "local variables");
}
