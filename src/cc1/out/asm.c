#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "../../util/util.h"
#include "../../util/platform.h"
#include "../../util/alloc.h"
#include "../../util/dynarray.h"
#include "../../util/math.h"

#include "../type.h"
#include "../type_nav.h"
#include "../decl.h"
#include "../strings.h"

#include "asm.h"
#include "out.h"

#include "../fopt.h"
#include "../cc1.h"
#include "../sym.h"
#include "../expr.h"
#include "../stmt.h"
#include "../type_is.h"

#include "../sue.h"
#include "../const.h"
#include "../gen_asm.h"
#include "../decl_init.h"
#include "../pack.h"
#include "../str.h"

#include "../ops/expr_compound_lit.h"

#include "../../config_as.h" /* weak directive */

#define ASSERT_SCALAR(di)                  \
	UCC_ASSERT(di->type == decl_init_scalar, \
			"scalar expected for bitfield init")

#define ASM_COMMENT "#"

#define INIT_DEBUG 0

#define DEBUG(s, ...) do{ \
	if(INIT_DEBUG) fprintf(stderr, "\033[35m" s "\033[m\n", __VA_ARGS__); \
}while(0)


struct bitfield_val
{
	integral_t val;
	unsigned offset;
	unsigned width;
};

int asm_table_lookup(type *r)
{
	int sz;
	int i;

	if(!r)
		sz = type_primitive_size(type_long); /* or ptr */
	else if(type_is(r, type_array) || type_is(r, type_func))
		/* special case for funcs and arrays */
		sz = platform_word_size();
	else
		sz = type_size(r, NULL);

	for(i = 0; i < ASM_TABLE_LEN; i++)
		if(asm_type_table[i].sz == sz)
			return i;

	ICE("no asm type index for byte size %d", sz);
	return -1;
}

const char *asm_type_directive(type *r)
{
	return asm_type_table[asm_table_lookup(r)].directive;
}

int asm_type_size(type *r)
{
	return asm_type_table[asm_table_lookup(r)].sz;
}

static void asm_declare_pad(enum section_type sec, unsigned pad, const char *why)
{
	if(pad)
		asm_out_section(sec, ".space %u " ASM_COMMENT " %s\n", pad, why);
}

static void asm_declare_init_type(enum section_type sec, type *ty)
{
	asm_out_section(sec, ".%s ", asm_type_directive(ty));
}

static void asm_declare_init_bitfields(
		enum section_type sec,
		struct bitfield_val *vals, unsigned n,
		type *ty)
{
#define BITFIELD_DBG(...) /*fprintf(stderr, __VA_ARGS__)*/
	integral_t v = 0;
	unsigned width = 0;
	unsigned i;

	BITFIELD_DBG("bitfield out -- new\n");
	for(i = 0; i < n; i++){
		integral_t this = integral_truncate_bits(
				vals[i].val, vals[i].width, NULL);

		width += vals[i].width;

		BITFIELD_DBG("bitfield out: 0x%llx << %u gives ",
				this, vals[i].offset);

		v |= this << vals[i].offset;

		BITFIELD_DBG("0x%llx\n", v);
	}

	BITFIELD_DBG("bitfield done with 0x%llx\n", v);

	if(width > 0){
		asm_declare_init_type(sec, ty);
		asm_out_section(sec, "%" NUMERIC_FMT_D "\n", v);
	}else{
		asm_out_section(sec,
				ASM_COMMENT " skipping zero length bitfield%s init\n",
				n == 1 ? "" : "s");
	}
}

static void bitfields_out(
		enum section_type sec,
		struct bitfield_val *bfs, unsigned *pn,
		type *ty)
{
	asm_declare_init_bitfields(sec, bfs, *pn, ty);
	*pn = 0;
}

static void bitfield_val_set(
		struct bitfield_val *bfv, expr *kval, expr *field_w)
{
	bfv->val = kval ? const_fold_val_i(kval) : 0;
	bfv->offset = 0;
	bfv->width = const_fold_val_i(field_w);
}

static struct bitfield_val *bitfields_add(
		struct bitfield_val *bfs, unsigned *pn,
		decl *mem, decl_init *di)
{
	const unsigned i = *pn;

	bfs = urealloc1(bfs, (*pn += 1) * sizeof *bfs);

	if(di)
		ASSERT_SCALAR(di);

	bitfield_val_set(&bfs[i],
			di ? di->bits.expr : NULL,
			mem->bits.var.field_width);

	bfs[i].offset = mem->bits.var.struct_offset_bitfield;

	return bfs;
}

void asm_out_fp(enum section_type sec, type *ty, floating_t f)
{
	switch(type_primitive(ty)){
		case type_float:
			{
				union { float f; unsigned u; } u;
				u.f = f;
				asm_out_section(sec, ".long %u # float %f\n", u.u, u.f);
				break;
			}

		case type_double:
			{
				union { double d; unsigned long ul; } u;
				u.d = f;
				asm_out_section(sec, ".quad %lu # double %f\n", u.ul, u.d);
				break;
			}
		case type_ldouble:
			ICE("TODO");
		default:
			ICE("bad float type");
	}
}

static void static_val(enum section_type sec, type *ty, expr *e)
{
	consty k;

	memset(&k, 0, sizeof k);

	const_fold(e, &k);

	switch(k.type){
		case CONST_NEED_ADDR:
		case CONST_NO:
			ICE("non-constant expr-%s const=%d%s",
					e->f_str(),
					k.type,
					k.type == CONST_NEED_ADDR ? " (needs addr)" : "");
			break;

		case CONST_NUM:
			if(K_FLOATING(k.bits.num)){
				/* asm fp const */
				asm_out_fp(sec, ty, k.bits.num.val.f);
			}else{
				char buf[INTEGRAL_BUF_SIZ];
				asm_declare_init_type(sec, ty);
				/* use 'ty' here - e->tree_type will be the casted-from type,
				 * e.g.
				 * char x[] = { 5ull };
				 * we want 'char', not 'unsigned long long' */
				integral_str(buf, sizeof buf, k.bits.num.val.i, ty);
				asm_out_section(sec, "%s", buf);
			}
			break;

		case CONST_ADDR:
			asm_declare_init_type(sec, ty);
			if(k.bits.addr.is_lbl)
				asm_out_section(sec, "%s", k.bits.addr.bits.lbl);
			else
				asm_out_section(sec, "%ld", k.bits.addr.bits.memaddr);
			break;

		case CONST_STRK:
			stringlit_use(k.bits.str->lit); /* must be before the label access */
			asm_declare_init_type(sec, ty);
			asm_out_section(sec, "%s", k.bits.str->lit->lbl);
			break;
	}

	/* offset in bytes, no mul needed */
	if(k.offset)
		asm_out_section(sec, " + %ld", k.offset);
	asm_out_section(sec, "\n");
}

static void asm_declare_init(enum section_type sec, decl_init *init, type *tfor)
{
	type *r;

	if(init == DYNARRAY_NULL)
		init = NULL;

	if(!init){
		/* don't initialise flex-arrays */
		if(!type_is_incomplete_array(tfor)){
			asm_declare_pad(sec, type_size(tfor, NULL),
					"null init"/*, type_to_str(tfor)*/);
		}else{
			asm_out_section(sec, ASM_COMMENT " flex array init skipped\n");
		}

	}else if((r = type_is_primitive(tfor, type_struct))){
		/* array of stmts for each member
		 * assumes the ->bits.inits order is member order
		 */
		struct_union_enum_st *const sue = r->bits.type->sue;
		sue_member **mem;
		decl_init **i;
		unsigned end_of_last = 0;
		struct bitfield_val *bitfields = NULL;
		unsigned nbitfields = 0;
		decl *first_bf = NULL;
		expr *copy_from_exp;

		UCC_ASSERT(init->type == decl_init_brace, "unbraced struct");

		i = init->bits.ar.inits;

		/* check for compound-literal copy-init */
		if((copy_from_exp = decl_init_is_struct_copy(init, sue))){
			decl_init *copy_from_init;

			copy_from_exp = expr_skip_lval2rval(copy_from_exp);

			/* the only struct-expression that's possible
			 * in static context is a compound literal */
			assert(expr_kind(copy_from_exp, compound_lit)
					&& "unhandled expression init");

			copy_from_init = copy_from_exp->bits.complit.decl->bits.var.init.dinit;
			assert(copy_from_init->type == decl_init_brace);

			i = copy_from_init->bits.ar.inits;
		}

		/* iterate using members, not inits */
		for(mem = sue->members;
				mem && *mem;
				mem++)
		{
			decl *d_mem = (*mem)->struct_member;
			decl_init *di_to_use = NULL;

			if(i){
				int inc = 1;

				if(*i == NULL)
					inc = 0;
				else if(*i != DYNARRAY_NULL)
					di_to_use = *i;

				if(inc){
					i++;
					if(!*i)
						i = NULL; /* reached end */
				}
			}

			DEBUG("init for %ld/%s, %s",
					mem - sue->members, d_mem->spel,
					di_to_use && di_to_use->type == decl_init_scalar
					? di_to_use->bits.expr->f_str()
					: NULL);

			/* only pad if we're not on a bitfield or we're on the first bitfield */
			if(!d_mem->bits.var.field_width || !first_bf){
				DEBUG("prev padding, offset=%d, end_of_last=%d",
						d_mem->bits.var.struct_offset, end_of_last);

				UCC_ASSERT(
						d_mem->bits.var.struct_offset >= end_of_last,
						"negative struct pad, %s::%s @ %u >= end_of_last @ %u",
						sue->spel, decl_to_str(d_mem),
						d_mem->bits.var.struct_offset, end_of_last);

				asm_declare_pad(sec,
						d_mem->bits.var.struct_offset - end_of_last,
						"prev struct padding");
			}

			if(d_mem->bits.var.field_width){
				const int zero_width = const_fold_val_i(d_mem->bits.var.field_width) == 0;

				if(!first_bf || d_mem->bits.var.first_bitfield){
					if(first_bf){
						DEBUG("new bitfield group (%s is new boundary), old:",
								d_mem->spel);
						/* next bitfield group - store the current */
						bitfields_out(sec, bitfields, &nbitfields, first_bf->ref);
					}
					if(!zero_width)
						first_bf = d_mem;
				}

				if(!zero_width){
					bitfields = bitfields_add(
							bitfields, &nbitfields,
							d_mem, di_to_use);
				}

			}else{
				if(nbitfields){
					DEBUG("at non-bitfield, prev-bitfield out:", 0);

					bitfields_out(sec, bitfields, &nbitfields, decl_type_for_bitfield(first_bf));
					first_bf = NULL;
				}

				DEBUG("normal init for %s:", d_mem->spel);
				asm_declare_init(sec, di_to_use, d_mem->ref);
			}

			if(type_is_incomplete_array(d_mem->ref)){
				UCC_ASSERT(!mem[1], "flex-arr not at end");
			}else if(!d_mem->bits.var.field_width || d_mem->bits.var.first_bitfield){
				unsigned sz, align;

				decl_size_align_inc_bitfield(d_mem, &sz, &align);

				end_of_last = d_mem->bits.var.struct_offset + sz;
				DEBUG("done with member \"%s\", end_of_last = %d",
						d_mem->spel, end_of_last);
			}
		}

		if(nbitfields)
			bitfields_out(sec, bitfields, &nbitfields, first_bf->ref);
		free(bitfields);

		/* need to pad to struct size */
		asm_declare_pad(sec,
				sue_size(sue, NULL) - end_of_last,
				"struct tail");

	}else if((r = type_is(tfor, type_array))){
		size_t i, len;
		decl_init **p;
		type *next = type_next(tfor);

		UCC_ASSERT(init->type == decl_init_brace, "unbraced struct");

		if(type_is_incomplete_array(tfor)){
			len = dynarray_count(init->bits.ar.inits);
		}else{
			UCC_ASSERT(type_is_complete(tfor), "incomplete array/type init");
			len = type_array_len(tfor);
		}

		for(i = len, p = init->bits.ar.inits;
				i > 0;
				i--)
		{
			decl_init *this = NULL;
			if(*p){
				this = *p++;

				if(this != DYNARRAY_NULL && this->type == decl_init_copy){
					/*fprintf(f, "# copy from %lu\n", DECL_INIT_COPY_IDX(this, init));*/
					struct init_cpy *icpy = *this->bits.range_copy;
					/* resolve the copy */
					this = icpy->range_init;
				}
			}

			asm_declare_init(sec, this, next);
		}

	}else if((r = type_is_primitive(tfor, type_union))){
		/* union inits are decl_init_brace with spaces up to the first union init,
		 * then NULL/end of the init-array */
		struct_union_enum_st *sue = type_is_s_or_u(r);
		unsigned i, sub = 0;
		decl_init *u_init;

		UCC_ASSERT(init->type == decl_init_brace, "brace init expected");

		/* skip the empties until we get to one */
		for(i = 0; init->bits.ar.inits[i] == DYNARRAY_NULL; i++);

		if((u_init = init->bits.ar.inits[i])){
			decl *mem = sue->members[i]->struct_member;
			type *mem_r = mem->ref;

			/* union init, member at index `i' */
			if(mem->bits.var.field_width){
				/* we know it's integral */
				struct bitfield_val bfv;

				ASSERT_SCALAR(u_init);

				bitfield_val_set(&bfv, u_init->bits.expr, mem->bits.var.field_width);

				asm_declare_init_bitfields(sec, &bfv, 1, mem_r);
			}else{
				asm_declare_init(sec, u_init, mem_r);
			}

			sub = type_size(mem_r, NULL);
		} /* else null union init */

		asm_declare_pad(sec,
				type_size(r, NULL) - sub,
				"union extra");

	}else{
		/* scalar */
		expr *exp = init->bits.expr;

		UCC_ASSERT(init->type == decl_init_scalar, "scalar init expected");

		/* exp->tree_type should match tfor */
		{
			char buf[TYPE_STATIC_BUFSIZ];

			UCC_ASSERT(
					type_cmp(exp->tree_type, tfor, TYPE_CMP_ALLOW_TENATIVE_ARRAY) != TYPE_NOT_EQUAL,
					"mismatching init types: %s and %s",
					type_to_str_r(buf, exp->tree_type),
					type_to_str(tfor));
		}

		/* use tfor, since "abc" has type (char[]){(int)'a', (int)'b', ...} */
		DEBUG("  scalar init for %s:", type_to_str(tfor));
		static_val(sec, tfor, exp);
	}
}

static void asm_out_align(enum section_type sec, unsigned align)
{
	if(mopt_mode & MOPT_ALIGN_IS_POW2){
		align = log2i(align);
	}

	if(align)
		asm_out_section(sec, ".align %u\n", align);
}

void asm_nam_begin3(enum section_type sec, const char *lbl, unsigned align)
{
	asm_out_align(sec, align);
	asm_out_section(sec, "%s:\n", lbl);
}

static void asm_nam_begin(enum section_type sec, decl *d)
{
	asm_nam_begin3(sec, decl_asm_spel(d), decl_align(d));
}

static void asm_reserve_bytes(enum section_type sec, unsigned nbytes)
{
	/*
	 * TODO: .comm buf,512,5
	 * or    .zerofill SECTION_NAME,buf,512,5
	 */
	asm_declare_pad(sec, nbytes, "object space");
}

static void asm_predecl(const char *type, decl *d)
{
	asm_out_section(SECTION_TEXT, ".%s %s\n", type, decl_asm_spel(d));
}

void asm_predeclare_extern(decl *d)
{
	(void)d;
	/*
	asm_comment("extern %s", d->spel);
	asm_out_section(SECTION_BSS, "extern %s", d->spel);
	*/
}

void asm_predeclare_global(decl *d)
{
	asm_predecl("globl", d);
}

void asm_predeclare_weak(decl *d)
{
	asm_predecl(ASM_WEAK_DIRECTIVE, d);
}

void asm_predeclare_visibility(decl *d, attribute *attr)
{
	enum visibility v = cc1_visibility_default;

	if(decl_linkage(d) == linkage_internal)
		return;

	if(attr){
		assert(attr->type == attr_visibility);
		v = attr->bits.visibility;
	}

	switch(v){
		case VISIBILITY_DEFAULT:
			break;
		case VISIBILITY_HIDDEN:
			asm_predecl(ASM_VISIBILITY_HIDDEN_DIRECTIVE, d);
			break;
		case VISIBILITY_PROTECTED:
			assert(AS_SUPPORTS_VISIBILITY_PROTECTED);
			asm_predecl("protected", d);
			break;
	}
}

static void asm_declare_ctor_dtor(decl *d, enum section_type sec)
{
	type *intptr_ty = type_nav_btype(cc1_type_nav, type_intptr_t);
	const char *directive = asm_type_directive(intptr_ty);

	if(asm_section_empty(sec))
		asm_out_align(sec, type_align(intptr_ty, NULL));
	asm_out_section(sec, ".%s %s\n", directive, decl_asm_spel(d));
}

void asm_declare_constructor(decl *d)
{
	asm_declare_ctor_dtor(d, SECTION_CTORS);
}

void asm_declare_destructor(decl *d)
{
	asm_declare_ctor_dtor(d, SECTION_DTORS);
}

void asm_declare_stringlit(enum section_type sec, const stringlit *lit)
{
	FILE *const f = cc_out[sec];

	/* could be SECTION_RODATA */
	asm_nam_begin3(sec, lit->lbl, /*align:*/1);

	switch(lit->cstr->type){
		case CSTRING_WIDE:
		{
			const char *join = "";
			size_t i;
			fprintf(f, ".long ");
			for(i = 0; i < lit->cstr->count; i++){
				fprintf(f, "%s%d", join, lit->cstr->bits.wides[i]);
				join = ", ";
			}
			break;
		}

		case CSTRING_RAW:
			assert(0 && "raw string in code gen");

		case CSTRING_ASCII:
			fprintf(f, ".ascii \"");
			literal_print(f, lit->cstr);
			fputc('"', f);
			break;
	}

	fputc('\n', f);
}

void asm_declare_decl_init(decl *d)
{
	int is_const, nonzero_init;
	enum section_type sec;

	if((d->store & STORE_MASK_STORE) == store_extern){
		asm_predeclare_extern(d);
		return;
	}

	is_const = type_is_const(d->ref);
	nonzero_init = d->bits.var.init.dinit && !decl_init_is_zero(d->bits.var.init.dinit);

	if(is_const){
		sec = SECTION_RODATA;
	}else if(nonzero_init){
		sec = SECTION_DATA;
	}else{
		sec = SECTION_BSS;
	}

	if(nonzero_init){
		asm_nam_begin(sec, d);
		asm_declare_init(sec, d->bits.var.init.dinit, d->ref);
		asm_out_section(sec, "\n");

	}else if(d->bits.var.init.compiler_generated && cc1_fopt.common){
		const char *common_prefix = "comm ";
		unsigned align;

		if(decl_linkage(d) == linkage_internal){
			if(AS_SUPPORTS_LOCAL_COMMON){
				asm_out_section(sec, ".local %s\n", decl_asm_spel(d));
			}else{
				common_prefix = "zerofill __DATA,__bss,";
			}
		}

		align = decl_align(d);
		if(mopt_mode & MOPT_ALIGN_IS_POW2){
			align = log2i(align);
		}

		asm_out_section(sec, ".%s%s,%u,%u\n",
				common_prefix,
				decl_asm_spel(d), decl_size(d), align);

	}else{
		/* always resB, since we use decl_size() */
		asm_nam_begin(sec, d);
		asm_reserve_bytes(sec, decl_size(d));
	}
}

void asm_out_sectionv(enum section_type t, const char *fmt, va_list l)
{
	vfprintf(cc_out[t], fmt, l);
}

void asm_out_section(enum section_type t, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
	asm_out_sectionv(t, fmt, l);
	va_end(l);
}

int asm_section_empty(enum section_type t)
{
	return ftell(cc_out[t]) == 0;
}
