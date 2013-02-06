#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "../../util/util.h"
#include "../../util/alloc.h"
#include "../data_structs.h"
#include "out.h"
#include "vstack.h"
#include "impl.h"
#include "../../util/platform.h"
#include "../cc1.h"

#define v_check_type(t) if(!t) t = type_ref_new_VOID_PTR()

static int calc_ptr_step(type_ref *t);

/*
 * This entire stack-output idea was inspired by tinycc, and improved somewhat
 */

#define N_VSTACK 1024
struct vstack vstack[N_VSTACK];
struct vstack *vtop = NULL;

static int stack_sz;

static int reserved_regs[N_SCRATCH_REGS];

int out_vcount(void)
{
	return vtop ? 1 + (int)(vtop - vstack) : 0;
}

static void vpush(type_ref *t)
{
	v_check_type(t);

	if(!vtop){
		vtop = vstack;
	}else{
		UCC_ASSERT(vtop < vstack + N_VSTACK - 1,
				"vstack overflow, vtop=%p, vstack=%p, diff %d",
				(void *)vtop, (void *)vstack, out_vcount());

		if(vtop->type == FLAG)
			v_to_reg(vtop);

		vtop++;
	}

	vtop_clear(t);
}

void v_clear(struct vstack *vp, type_ref *t)
{
	v_check_type(t);

	memset(vp, 0, sizeof *vp);
	vp->t = t;
}

void vtop_clear(type_ref *t)
{
	v_clear(vtop, t);
}

void vpop(void)
{
	UCC_ASSERT(vtop, "NULL vtop for vpop");

	if(vtop == vstack){
		vtop = NULL;
	}else{
		UCC_ASSERT(vtop < vstack + N_VSTACK - 1, "vstack underflow");
		vtop--;
	}
}

void out_flush_volatile(void)
{
	if(vtop)
		v_to_reg(vtop);
}

void out_assert_vtop_null(void)
{
	UCC_ASSERT(!vtop, "vtop not null (%d entries)", out_vcount());
}

void out_dump(void)
{
	int i;

	for(i = 0; &vstack[i] <= vtop; i++)
		fprintf(stderr, "vstack[%d] = { %d, %d }\n",
				i, vstack[i].type, vstack[i].bits.reg);
}

void vswap(void)
{
	struct vstack tmp;
	memcpy_safe(&tmp, vtop);
	memcpy_safe(vtop, &vtop[-1]);
	memcpy_safe(&vtop[-1], &tmp);
}

void out_swap(void)
{
	vswap();
}

void v_prepare_op(struct vstack *vp)
{
	switch(vp->type){
		case STACK:
		case LBL:
			/* need to pull the values from the stack */

		case STACK_SAVE:
			/* ditto, impl handles pulling from stack */

		case FLAG:
			/* obviously can't have a flag in cmp/mov code */
			v_to_reg(vp);

		case REG:
		case CONST:
			break;
	}
}

void vtop2_prepare_op(void)
{
#if 0
	/* this assume we're assigning back */
	/* special case - const and stack/lbl is fine */
	if((vtop->type == CONST && vtop[-1].type == STACK)
	|| (vtop->type == STACK && vtop[-1].type == CONST))
	{
		return;
	}
#endif


	/* attempt to give this a higher reg,
	 * since it'll be used more,
	 * e.g. return and idiv */
	v_prepare_op(&vtop[-1]);

	v_prepare_op(vtop);

	if((vtop->type == vtop[-1].type && vtop->type != REG) || vtop[-1].type != REG){
		/* prefer putting vtop[-1] in a reg, since that's returned after */
		v_to_reg(&vtop[-1]);
	}
}

int v_unused_reg(int stack_as_backup)
{
	static int used[N_SCRATCH_REGS];
	struct vstack *it, *first;
	int i;

	memcpy(used, reserved_regs, sizeof used);
	first = NULL;

	for(it = vstack; it <= vtop; it++){
		if(it->type == REG){
			if(!first)
				first = it;
			used[it->bits.reg - FIRST_SCRATCH_REG] = 1;
		}
	}

	for(i = 0; i < N_SCRATCH_REGS; i++)
		if(!used[i])
			return i + FIRST_SCRATCH_REG;

	if(stack_as_backup){
		/* no free regs, move `first` to the stack and claim its reg */
		int reg = first->bits.reg;

		v_freeup_regp(first);

		return reg;
	}
	return -1;
}

void out_load(struct vstack *from, int reg)
{
	type_ref *const save = from->t;
	int lea = 0;

	switch(from->type){
		case STACK:
		case LBL:
			lea = 1;
		case FLAG:
		case STACK_SAVE: /* voila */
		case CONST:
		case REG:
			break;
	}

	(lea ? impl_lea : impl_load)(from, reg);

	vtop_clear(save);
	from->type = REG;
	from->bits.reg = reg;
}

int v_to_reg(struct vstack *conv)
{
	if(conv->type != REG)
		out_load(conv, v_unused_reg(1));

	return conv->bits.reg;
}

struct vstack *v_find_reg(int reg)
{
	struct vstack *vp;
	if(!vtop)
		return NULL;

	for(vp = vstack; vp <= vtop; vp++)
		if(vp->type == REG && vp->bits.reg == reg)
			return vp;

	return NULL;
}

void v_freeup_regp(struct vstack *vp)
{
	/* freeup this reg */
	int r;

	UCC_ASSERT(vp->type == REG, "not reg");

	/* attempt to save to a register first */
	r = v_unused_reg(0);

	if(r >= 0){
		impl_reg_cp(vp, r);

		v_clear(vp, NULL);
		vp->type = REG;
		vp->bits.reg = r;

	}else{
		v_save_reg(vp);
	}
}

static int out_alloc_stack(int sz)
{
	static int word_size;
	/* sz must be a multiple of word_size */

	if(!word_size)
		word_size = platform_word_size();

	if(sz){
		const int extra = sz % word_size ? word_size - sz % word_size : 0;

		vpush(NULL);
		vtop->type = REG;
		vtop->bits.reg = REG_SP;

		out_push_i(type_ref_new_INTPTR_T(), sz += extra);
		out_op(op_minus);
		out_pop();
	}

	return stack_sz += sz;
}

void v_save_reg(struct vstack *vp)
{
	struct vstack store;

	UCC_ASSERT(vp->type == REG, "not reg");

	memset(&store, 0, sizeof store);

	store.type = STACK;
	store.t = type_ref_ptr_depth_inc(vp->t);

	/* the following gen two instructions - subq and movq
	 * instead/TODO: impl_save_reg(vp) -> "pushq %%rax"
	 * -O1?
	 */
	store.bits.off_from_bp = -out_alloc_stack(type_ref_size(store.t, NULL));
	impl_store(vp, &store);

	store.type = STACK_SAVE;

	memcpy_safe(vp, &store);

	/* no need for copy */
	vp->t = type_ref_ptr_depth_dec(vp->t);
}

void v_freeup_reg(int r, int allowable_stack)
{
	struct vstack *vp = v_find_reg(r);

	if(vp && vp < &vtop[-allowable_stack + 1])
		v_freeup_regp(vp);
}

void v_freeup_regs(int a, int b)
{
	reserved_regs[a] = 1;
	reserved_regs[b] = 1;

	v_freeup_reg(a, 2);
	v_freeup_reg(b, 2);

	reserved_regs[a] = 0;
	reserved_regs[b] = 0;
}

void v_inv_cmp(struct vstack *vp)
{
	switch(vp->bits.flag.cmp){
#define OPPOSITE(from, to) case flag_ ## from: vp->bits.flag.cmp = flag_ ## to; return
		OPPOSITE(eq, ne);
		OPPOSITE(ne, eq);

		OPPOSITE(le, gt);
		OPPOSITE(gt, le);

		OPPOSITE(lt, ge);
		OPPOSITE(ge, lt);

		/*OPPOSITE(z, nz);
		OPPOSITE(nz, z);*/
#undef OPPOSITE
	}
	ICE("invalid op");
}

void out_pop(void)
{
	vpop();
}

void out_push_iv(type_ref *t, intval *iv)
{
	vpush(t);

	vtop->type = CONST;
	vtop->bits.val = iv->val; /* TODO: unsigned */
}

void out_push_i(type_ref *t, int i)
{
	intval iv = {
		.val = i,
		.suffix = 0
	};

	out_push_iv(t, &iv);
}

void out_push_lbl(char *s, int pic)
{
	vpush(NULL);

	vtop->bits.lbl.str = s;
	vtop->bits.lbl.pic = pic;

	vtop->type = LBL;
}

void vdup(void)
{
	vpush(NULL);
	memcpy_safe(&vtop[0], &vtop[-1]);
}

void out_dup(void)
{
	vdup();
}

void out_store()
{
	struct vstack *store, *val;

	val   = &vtop[0];
	store = &vtop[-1];

	impl_store(val, store);

	/* swap, popping the store, but not the value */
	vswap();
	vpop();
}

void out_normalise(void)
{
	if(vtop->type == CONST)
		vtop->bits.val = !!vtop->bits.val;
	else
		impl_normalise();
}

void out_push_sym(sym *s)
{
	decl *const d = s->decl;

	vpush(type_ref_ptr_depth_inc(d->ref));

	switch(s->type){
		case sym_local:
			if(DECL_IS_FUNC(d))
				goto label;

			if((d->store & STORE_MASK_STORE) == store_register && d->spel_asm)
				ICW("TODO: %s asm(\"%s\")", decl_to_str(d), d->spel_asm);

			vtop->type = STACK;
			/* sym offsetting takes into account the stack growth direction */
			vtop->bits.off_from_bp = -s->offset;
			break;

		case sym_arg:
			vtop->type = STACK;
			/*
			 * if it's less than #call regs, it's below rbp, otherwise it's above
			 */
			vtop->bits.off_from_bp = (s->offset < N_CALL_REGS
					? -(s->offset + 1)
					:   s->offset - N_CALL_REGS + 2)
				* platform_word_size();
			break;

		case sym_global:
label:
			vtop->type = LBL;
			vtop->bits.lbl.str = decl_asm_spel(d);
			vtop->bits.lbl.pic = 1;
			break;
	}
}

void out_push_sym_val(sym *s)
{
	out_push_sym(s);
	out_deref();
}

static void vtop2_are(
		enum vstore a,
		enum vstore b,
		struct vstack **pa, struct vstack **pb)
{
	if(vtop->type == a)
		*pa = vtop;
	else if(vtop[-1].type == a)
		*pa = &vtop[-1];
	else
		*pa = NULL;

	if(vtop->type == b)
		*pb = vtop;
	else if(vtop[-1].type == b)
		*pb = &vtop[-1];
	else
		*pb = NULL;
}

static int calc_ptr_step(type_ref *t)
{
	/* we are calculating the sizeof *t */
	int sz;

	if(type_ref_is_type(type_ref_is_ptr(t), type_void))
		return type_primitive_size(type_void);

	sz = type_ref_size(type_ref_next(t), NULL);

	return sz;
}

void out_op(enum op_type op)
{
	/*
	 * the implementation does a vpop() and
	 * sets vtop sufficiently to describe how
	 * the result is returned
	 */

	struct vstack *t_const, *t_stack;

	/* check for adding or subtracting to stack */
	vtop2_are(CONST, STACK, &t_const, &t_stack);

	if(t_const && t_stack){
		/* t_const == vtop... should be */
		t_stack->bits.off_from_bp += t_const->bits.val * calc_ptr_step(t_stack->t);

		goto ignore_const;

	}else if(t_const){
		/* TODO: -O1, constant folding here */

		switch(op){
			case op_plus:
			case op_minus:
			case op_or:
			case op_xor:
				if(t_const->bits.val == 0)
					goto ignore_const;
			default:
				break;

			case op_multiply:
			case op_divide:
				if(t_const->bits.val == 1)
					goto ignore_const;
				break;

			case op_and:
				if(t_const->bits.val == -1)
					goto ignore_const;
				break;
		}

		goto def;

ignore_const:
		if(t_const != vtop)
			vswap(); /* need t_const on top for discarding */

		vpop();

	}else{
		int div;

def:
		div = 0;

		switch(op){
			case op_plus:
			case op_minus:
			{
				int l_ptr, r_ptr;

				l_ptr = !!type_ref_is(vtop->t   , type_ref_ptr);
				r_ptr = !!type_ref_is(vtop[-1].t, type_ref_ptr);

				if(l_ptr || r_ptr){
					const int ptr_step = calc_ptr_step(l_ptr ? vtop->t : vtop[-1].t);

					if(l_ptr ^ r_ptr){
						/* ptr +/- int, adjust the non-ptr by sizeof *ptr */
						struct vstack *val = &vtop[l_ptr ? -1 : 0];

						switch(val->type){
							case CONST:
								val->bits.val *= ptr_step;
								break;

							default:
								v_to_reg(val);

							case REG:
							{
								int swap;
								if((swap = (val != vtop)))
									vswap();

								out_push_i(type_ref_new_VOID_PTR(), ptr_step);
								out_op(op_multiply);

								if(swap)
									vswap();
							}
						}

					}else if(l_ptr && r_ptr){
						/* difference - divide afterwards */
						div = ptr_step;
					}
				}
				break;
			}

			default:
				break;
		}

		impl_op(op);

		if(div){
			out_push_i(type_ref_new_VOID_PTR(), div);
			out_op(op_divide);
		}
	}
}

void v_deref_decl(struct vstack *vp)
{
	/* XXX: memleak */
	vp->t = type_ref_ptr_depth_dec(vp->t);
}

void out_deref()
{
#define DEREF_CHECK
#ifdef DEREF_CHECK
	type_ref *const vtop_t = vtop->t;
#endif
	type_ref *indir;
	/* if the pointed-to object is not an lvalue, don't deref */

	indir = type_ref_ptr_depth_dec(vtop->t);

	if(type_ref_is(indir, type_ref_array)
	|| type_ref_is(type_ref_is_ptr(vtop->t), type_ref_func)){
		out_change_type(indir);
		return; /* noop */
	}

	/* optimisation: if we're dereffing a pointer to stack/lbl, just do a mov */
	switch(vtop->type){
		case FLAG:
			ICE("deref of flag");

		default:
			v_to_reg(vtop);
		case REG:
			impl_deref_reg();
			break;

		case LBL:
		case STACK:
		case CONST:
		{
			int r = v_unused_reg(1);

			v_deref_decl(vtop);

			/* impl_load, since we don't want a lea switch */
			impl_load(vtop, r);

			vtop->type = REG;
			vtop->bits.reg = r;
			break;
		}
	}

#ifdef DEREF_CHECK
	UCC_ASSERT(vtop_t != vtop->t, "no depth change");
#endif
}


void out_op_unary(enum op_type op)
{
	switch(op){
		case op_plus:
			return;

		default:
			/* special case - reverse the flag if possible */
			switch(vtop->type){
				case FLAG:
					if(op == op_not){
						v_inv_cmp(vtop);
						return;
					}
					break;

				case CONST:
					switch(op){
#define OP(t, o) case op_ ## t: vtop->bits.val = o vtop->bits.val; return
						OP(not, !);
						OP(minus, -);
						OP(bnot, ~);
#undef OP

						default:
							ICE("invalid unary op");
					}
					break;

				case REG:
				case STACK:
				case STACK_SAVE:
				case LBL:
					break;
			}
	}

	impl_op_unary(op);
}

void out_cast(type_ref *from, type_ref *to)
{
	/* casting vtop - don't bother if it's a constant, just change the size */
	if(vtop->type != CONST)
		impl_cast(from, to);

	out_change_type(to);
}

void out_change_type(type_ref *t)
{
	v_check_type(t);
	/* XXX: memleak */
	vtop->t = t;
}

void out_call(int nargs, type_ref *r_ret, type_ref *r_func)
{
	impl_call(nargs, r_ret, r_func);

	/* return type */
	vtop_clear(r_ret);
	vtop->type = REG;
	vtop->bits.reg = REG_RET;
}

void out_jmp(void)
{
	if(vtop > vstack){
		/* flush the stack-val we need to generate before the jump */
		vtop--;
		out_flush_volatile();
		vtop++;
	}

	switch(vtop->type){
		default:
			v_to_reg(vtop);
		case REG:
			impl_jmp_reg(vtop->bits.reg);
			break;

		case LBL:
			impl_jmp_lbl(vtop->bits.lbl.str);
	}

	vpop();
}

void out_jtrue(const char *lbl)
{
	impl_jcond(1, lbl);

	vpop();
}

void out_jfalse(const char *lbl)
{
	int cond = 0;

	if(vtop->type == FLAG){
		v_inv_cmp(vtop);
		cond = 1;
	}

	impl_jcond(cond, lbl);

	vpop();
}

void out_label(const char *lbl)
{
	/* if we have volatile data, ensure it's in a register */
	out_flush_volatile();

	impl_lbl(lbl);
}

void out_comment(const char *fmt, ...)
{
	va_list l;

	va_start(l, fmt);
	impl_comment(fmt, l);
	va_end(l);
}

void out_func_prologue(int stack_res, int nargs, int variadic)
{
	UCC_ASSERT(stack_sz == 0, "non-empty stack for new func");

	stack_sz = MIN(nargs, N_CALL_REGS) * platform_word_size();

	impl_func_prologue(nargs);

	if(stack_res)
		stack_sz = out_alloc_stack(stack_res);

	if(variadic)
		impl_func_save_variadic(nargs);
}

void out_func_epilogue()
{
	impl_func_epilogue();
	stack_sz = 0;
}

void out_pop_func_ret(type_ref *t)
{
	impl_pop_func_ret(t);
}

void out_undefined(void)
{
	out_flush_volatile();
	impl_undefined();
}

void out_push_frame_ptr(int nframes)
{
	out_flush_volatile();

	vpush(NULL);
	vtop->type = REG;
	vtop->bits.reg = impl_frame_ptr_to_reg(nframes);
}
