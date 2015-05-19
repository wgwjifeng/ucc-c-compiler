#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../util/dynarray.h"

#include "sym.h"
#include "str.h"
#include "expr.h"
#include "decl_init.h"
#include "stmt.h"
#include "type_is.h"
#include "sue.h"

#include "gen_dump.h"

struct dump
{
	FILE *fout;
	unsigned indent;
};

static void dump_decl(decl *d, dump *ctx);

#if 0
#define ENGLISH_PRINT_ARGLIST

#define PRINT_IF(x, sub, fn) \
	if(x->sub){ \
		idt_printf(#sub ":\n"); \
		gen_str_indent++; \
		fn(x->sub); \
		gen_str_indent--; \
	}

void idt_print()
{
	int i;

	for(i = gen_str_indent; i > 0; i--)
		fputs("  ", cc1_out);
}

void idt_printf(const char *fmt, ...)
{
	va_list l;

	idt_print();

	va_start(l, fmt);
	vfprintf(cc1_out, fmt, l);
	va_end(l);
}

static void print_expr_val(expr *e)
{
	consty k;

	const_fold(e, &k);

	UCC_ASSERT(k.type == CONST_NUM, "val expected");
	UCC_ASSERT((k.bits.num.suffix & VAL_UNSIGNED) == 0, "TODO: unsigned");

	if(K_INTEGRAL(k.bits.num))
		fprintf(cc1_out, NUMERIC_FMT_D, k.bits.num.val.i);
	else
		fprintf(cc1_out, NUMERIC_FMT_LD, k.bits.num.val.f);
}

static void print_decl_init(decl_init *di)
{
	switch(di->type){
		case decl_init_scalar:
			idt_printf("scalar:\n");
			gen_str_indent++;
			print_expr(di->bits.expr);
			gen_str_indent--;
			break;

		case decl_init_copy:
			ICE("copy in print");
			break;

		case decl_init_brace:
		{
			decl_init *s;
			int i;

			idt_printf("brace\n");

			gen_str_indent++;
			for(i = 0; (s = di->bits.ar.inits[i]); i++){
				if(s == DYNARRAY_NULL){
					idt_printf("[%d] = <zero init>\n", i);
				}else if(s->type == decl_init_copy){
					idt_printf("[%d] = copy from range_store[%ld]\n",
							i, (long)DECL_INIT_COPY_IDX(s, di));
				}else{
					const int need_brace = s->type == decl_init_brace;

					/* ->member not printed */
#ifdef DINIT_WITH_STRUCT
					if(s->spel)
						idt_printf(".%s", s->spel);
					else
#endif
						idt_printf("[%d]", i);

					fprintf(cc1_out, " = %s\n", need_brace ? "{" : "");

					gen_str_indent++;
					print_decl_init(s);
					gen_str_indent--;

					if(need_brace)
						idt_printf("}\n");
				}
			}
			gen_str_indent--;

			if(di->bits.ar.range_inits){
				struct init_cpy *icpy;

				idt_printf("range store:\n");
				gen_str_indent++;

				for(i = 0; (icpy = di->bits.ar.range_inits[i]); i++){
					idt_printf("store[%d]:\n", i);
					gen_str_indent++;
					print_decl_init(icpy->range_init);
					gen_str_indent--;
					if(icpy->first_instance){
						idt_printf("first expr:\n");
						gen_str_indent++;
						print_expr(icpy->first_instance);
						gen_str_indent--;
					}
				}
				gen_str_indent--;
			}
		}
	}
}

static void print_type_eng(type *ref)
{
	if(!ref)
		return;

	print_type_eng(ref->ref);

	switch(ref->type){
		case type_auto:
			ICE("__auto_type");

		case type_cast:
			fprintf(cc1_out, "%s", type_qual_to_str(ref->bits.cast.qual, 1));
			break;

		case type_ptr:
			fprintf(cc1_out, "pointer to ");
			break;

		case type_block:
			fprintf(cc1_out, "block returning ");
			break;

		case type_func:
		{
#ifdef ENGLISH_PRINT_ARGLIST
			funcargs *fargs = ref->bits.func.args;
			decl **iter;
#endif

			fputs("function", cc1_out);

#ifdef ENGLISH_PRINT_ARGLIST
			fputc('(', cc1_out);
			if(fargs->arglist){

				for(iter = fargs->arglist; iter && *iter; iter++){
					print_decl(*iter, PDECL_NONE);
					if(iter[1])
						fputs(", ", cc1_out);
				}

				if(fargs->variadic)
					fputs("variadic", cc1_out);

			}else{
				fprintf(cc1_out, "taking %s arguments", fargs->args_void ? "no" : "unspecified");
			}
			fputc(')', cc1_out);
#endif
			fputs(" returning ", cc1_out);

			break;
		}

		case type_array:
			fputs("array[", cc1_out);
			if(ref->bits.array.size)
				print_expr_val(ref->bits.array.size);
			fputs("] of ", cc1_out);
			break;

		case type_btype:
			fprintf(cc1_out, "%s", btype_to_str(ref->bits.type));
			break;

		case type_tdef:
		case type_attr:
			ICE("TODO");
		case type_where:
			break;
	}
}

static void print_decl_eng(decl *d)
{
	if(d->spel)
		fprintf(cc1_out, "\"%s\": ", d->spel);

	print_type_eng(d->ref);
}

void print_type(type *ref, decl *d)
{
	char buf[TYPE_STATIC_BUFSIZ];

	fprintf(cc1_out, "%s",
			type_to_str_r_spel(buf, ref, d ? d->spel : NULL));

	if(ref->type == type_attr){
		attribute *da;
		for(da = ref->bits.attr; da; da = da->next){
			fprintf(cc1_out, " __attribute__((%s))",
					attribute_to_str(da));
		}
	}
}

static void print_attribute(attribute *da)
{
	for(; da; da = da->next){
		idt_printf("__attribute__((%s))\n", attribute_to_str(da));

		gen_str_indent++;
		switch(da->type){
			case attr_section:
				idt_printf("section \"%s\"\n", da->bits.section);
				break;
			case attr_nonnull:
			{
				unsigned long l = da->bits.nonnull_args;

				idt_printf("nonnull: ");
				if(l == ~0UL){
					fprintf(cc1_out, "all");
				}else{
					const char *sep = "";
					int i;

					for(i = 0; i <= 32; i++)
						if(l & (1 << i)){
							fprintf(cc1_out, "%s%d", sep, i);
							sep = ", ";
						}
				}

				fputc('\n', cc1_out);
				break;
			}

			default:
				break;
		}
		gen_str_indent--;
	}
}

static void print_type_attr(type *r)
{
	enum attribute_type i;

	for(i = 0; i < attr_LAST; i++){
		attribute *da;
		if((da = type_attr_present(r, i)))
			print_attribute(da);
	}
}

void print_decl(decl *d, enum pdeclargs mode)
{
	if(mode & PDECL_INDENT)
		idt_print();

	if(d->store)
		fprintf(cc1_out, "%s ", decl_store_to_str(d->store));

	if(fopt_mode & FOPT_ENGLISH){
		print_decl_eng(d);
	}else{
		print_type(d->ref, d);
	}

	if(mode & PDECL_SYM_OFFSET){
		if(d->sym){
			fprintf(cc1_out, " (sym %s)", sym_to_str(d->sym->type));
		}else{
			fprintf(cc1_out, " (no sym)");
		}
	}

	if(mode & PDECL_SIZE && !type_is(d->ref, type_func)){
		if(type_is_complete(d->ref)){
			const unsigned sz = decl_size(d);
			const unsigned align = decl_align(d);

			fprintf(cc1_out, " size %u, align %u", sz, align);
		}else{
			fprintf(cc1_out, " incomplete decl");
		}
	}

	if(mode & PDECL_NEWLINE)
		fputc('\n', cc1_out);

	if(!type_is(d->ref, type_func)
	&& d->bits.var.init.dinit
	&& mode & PDECL_PINIT)
	{
		gen_str_indent++;
		print_decl_init(d->bits.var.init.dinit);
		gen_str_indent--;
	}

	if(mode & PDECL_ATTR){
		gen_str_indent++;
		if(!type_is(d->ref, type_func) && d->bits.var.align)
			idt_printf("[align={as_int=%d, resolved=%d}]\n",
					d->bits.var.align->as_int, d->bits.var.align->resolved);
		print_attribute(d->attr);
		print_type_attr(d->ref);
		gen_str_indent--;
	}

	if((mode & PDECL_FUNC_DESCEND) && DECL_HAS_FUNC_CODE(d)){
		gen_str_indent++;

		print_stmt(d->bits.func.code);

		gen_str_indent--;
	}
}

void print_expr(expr *e)
{
	idt_printf("expr: %s\n", e->f_str());
	if(e->tree_type){ /* might be a label */
		idt_printf("tree_type: ");
		gen_str_indent++;
		print_type(e->tree_type, NULL);
		gen_str_indent--;
		fputc('\n', cc1_out);
	}
	gen_str_indent++;
	if(e->f_gen)
		IGNORE_PRINTGEN(e->f_gen(e, NULL));
	else
		idt_printf("builtin/%s::%s\n", e->f_str(),
				e->expr->bits.ident.bits.ident.spel);
	gen_str_indent--;
}

static void print_struct(struct_union_enum_st *sue)
{
	sue_member **iter;

	if(!sue_complete(sue)){
		idt_printf("incomplete %s %s\n", sue_str(sue), sue->spel);
		return;
	}

	idt_printf("%s %s (size %d):\n", sue_str(sue), sue->spel, sue_size(sue, &sue->where));

	gen_str_indent++;
	for(iter = sue->members; iter && *iter; iter++){
		decl *d = (*iter)->struct_member;

		idt_printf("decl %s:\n", d->spel ? d->spel : "<anon>");
		gen_str_indent++;
		print_decl(d, PDECL_INDENT | PDECL_NEWLINE | PDECL_ATTR);

		if(!type_is(d->ref, type_func)){
#define SHOW_FIELD(nam) idt_printf("." #nam " = %u\n", d->bits.var.nam)
			SHOW_FIELD(struct_offset);

			if(d->bits.var.field_width){
				integral_t v = const_fold_val_i(d->bits.var.field_width);

				gen_str_indent++;

				idt_printf(".field_width = %" NUMERIC_FMT_D "\n", v);

				SHOW_FIELD(struct_offset_bitfield);

				gen_str_indent--;
			}
		}

		gen_str_indent--;
	}
	gen_str_indent--;
}

static void print_enum(struct_union_enum_st *et)
{
	sue_member **mi;

	idt_printf("enum %s:\n", et->spel);

	gen_str_indent++;
	for(mi = et->members; *mi; mi++){
		enum_member *m = (*mi)->enum_member;

		idt_printf("member %s = %" NUMERIC_FMT_D "\n",
				m->spel, const_fold_val_i(m->val));
	}
	gen_str_indent--;
}

static void print_sues_static_asserts(symtable *stab)
{
	struct_union_enum_st **sit;
	static_assert **stati;
	int nl = 0;

	for(sit = stab->sues; sit && *sit; sit++){
		struct_union_enum_st *sue = *sit;
		(sue->primitive == type_enum ? print_enum : print_struct)(sue);
		nl = 1;
	}

	for(stati = stab->static_asserts; stati && *stati; stati++){
		static_assert *sa = *stati;

		idt_printf("static assertion: %s\n", sa->s);
		gen_str_indent++;
		print_expr(sa->e);
		gen_str_indent--;

		nl = 1;
	}

	if(nl)
		fputc('\n', cc1_out);
}

static void print_stmt_flow(stmt_flow *t)
{
	idt_printf("for parts:\n");

	gen_str_indent++;
	PRINT_IF(t, for_init,      print_expr);
	PRINT_IF(t, for_while,     print_expr);
	PRINT_IF(t, for_inc,       print_expr);
	gen_str_indent--;
}

void print_stmt(stmt *t)
{
	idt_printf("statement: %s\n", t->f_str());

	if(t->flow){
		gen_str_indent++;
		print_stmt_flow(t->flow);
		gen_str_indent--;
	}

	PRINT_IF(t, expr, print_expr);
	PRINT_IF(t, lhs,  print_stmt);
	PRINT_IF(t, rhs,  print_stmt);
	PRINT_IF(t, rhs,  print_stmt);

	if(stmt_kind(t, code)){
		idt_printf("structs/unions/enums:\n");
		gen_str_indent++;
		print_sues_static_asserts(t->symtab);
		gen_str_indent--;

		if(t->symtab){
			decl **iter;

			idt_printf("decls:\n");

			for(iter = symtab_decls(t->symtab); iter && *iter; iter++){
				decl *d = *iter;

				gen_str_indent++;
				print_decl(d, PDECL_INDENT
						| PDECL_NEWLINE
						| PDECL_SYM_OFFSET
						| PDECL_ATTR
						| PDECL_PINIT);
				gen_str_indent--;
			}
		}

		if(t->bits.code.stmts){
			stmt **iter;

			idt_printf("code:\n");

			for(iter = t->bits.code.stmts; *iter; iter++){
				gen_str_indent++;
				print_stmt(*iter);
				gen_str_indent--;
			}
		}
	}
}
#endif

static void dump_indent(dump *ctx)
{
	unsigned i;
	for(i = ctx->indent; i; i--)
		fputc(' ', ctx->fout);
}

static void dump_newline(dump *ctx, int newline)
{
	if(newline)
		fputc('\n', ctx->fout);
}

static void dump_desc_newline(
		dump *ctx,
		const char *desc, const void *uniq, const where *loc,
		int newline)
{
	dump_indent(ctx);

	fprintf(ctx->fout, "%s %p <%s>", desc, uniq, where_str(loc));

	dump_newline(ctx, newline);
}

void dump_desc(
		dump *ctx,
		const char *desc, const void *uniq, const where *loc)
{
	dump_desc_newline(ctx, desc, uniq, loc, 1);
}

void dump_desc_expr_newline(
		dump *ctx, const char *desc, const struct expr *e,
		int newline)
{
	dump_desc_newline(ctx, desc, e, &e->where, 0);

	if(e->tree_type)
		fprintf(ctx->fout, " '%s'", type_to_str(e->tree_type));

	dump_newline(ctx, newline);
}

void dump_desc_stmt(dump *ctx, const char *desc, const struct stmt *s)
{
	dump_desc(ctx, desc, s, &s->where);
}

void dump_desc_expr(dump *ctx, const char *desc, const expr *e)
{
	dump_desc_expr_newline(ctx, desc, e, 1);
}

void dump_strliteral(dump *ctx, const char *str, size_t len)
{
	fprintf(ctx->fout, "\"");
	literal_print(ctx->fout, str, len);
	fprintf(ctx->fout, "\"\n");
}

void dump_expr(expr *e, dump *ctx)
{
	e->f_dump(e, ctx);
}

void dump_stmt(stmt *s, dump *ctx)
{
	s->f_dump(s, ctx);
}

void dump_init(dump *ctx, decl_init *dinit)
{
	if(dinit == DYNARRAY_NULL){
		dump_printf(ctx, "<null init>\n");
		return;
	}

	switch(dinit->type){
		case decl_init_scalar:
		{
			dump_expr(dinit->bits.expr, ctx);
			break;
		}

		case decl_init_brace:
		{
			decl_init **i;

			dump_desc(ctx, "brace init", dinit, &dinit->where);

			dump_inc(ctx);

			for(i = dinit->bits.ar.inits; i && *i; i++)
				dump_init(ctx, *i);

			dump_dec(ctx);
			break;
		}

		case decl_init_copy:
		{
			struct init_cpy *cpy = *dinit->bits.range_copy;
			dump_init(ctx, cpy->range_init);
			break;
		}
	}
}

void dump_inc(dump *ctx)
{
	ctx->indent++;
}

void dump_dec(dump *ctx)
{
	ctx->indent--;
}

static void dump_vprintf_indent(
		dump *ctx, int indent, const char *fmt, va_list l)
{
	if(indent)
		dump_indent(ctx);

	vfprintf(ctx->fout, fmt, l);
}

void dump_printf_indent(dump *ctx, int indent, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
	dump_vprintf_indent(ctx, indent, fmt, l);
	va_end(l);
}

void dump_printf(dump *ctx, const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
	dump_vprintf_indent(ctx, 1, fmt, l);
	va_end(l);
}

static void dump_gasm(symtable_gasm *gasm, dump *ctx)
{
	dump_desc(ctx, "global asm", gasm, &gasm->where);

	dump_inc(ctx);
	dump_strliteral(ctx, gasm->asm_str, strlen(gasm->asm_str));
	dump_dec(ctx);
}

static void dump_sue(dump *ctx, type *ty)
{
	struct_union_enum_st *sue = type_is_s_or_u_or_e(ty);
	sue_member **mi;

	if(!sue)
		return;

	dump_inc(ctx);

	for(mi = sue->members; mi && *mi; mi++){
		if(sue->primitive == type_enum){
			enum_member *emem = (*mi)->enum_member;

			dump_desc(ctx, emem->spel, emem, &emem->where);

		}else{
			decl *d = (*mi)->struct_member;

			dump_decl(d, ctx);

			dump_sue(ctx, d->ref);
		}
	}

	dump_dec(ctx);
}

static void dump_decl(decl *d, dump *ctx)
{
	int is_func = !!type_is(d->ref, type_func);
	const char *desc;

	if(d->spel){
		desc = is_func ? "function" : "variable";
	}else{
		desc = "type";
	}

	dump_desc_newline(ctx, desc, d, &d->where, 0);

	if(d->proto)
		dump_printf_indent(ctx, 0, " prev %p", (void *)d->proto);

	if(d->spel)
		dump_printf_indent(ctx, 0, " %s", d->spel);

	dump_printf_indent(ctx, 0, " '%s'", type_to_str(d->ref));

	if(d->store)
		dump_printf_indent(ctx, 0, " %s", decl_store_to_str(d->store));

	dump_printf_indent(ctx, 0, "\n");

	if(is_func){
		if(d->bits.func.code){
			dump_inc(ctx);
			dump_stmt(d->bits.func.code, ctx);
			dump_dec(ctx);
		}
	}else if(!d->spel){
		dump_sue(ctx, d->ref);
	}else if(d->bits.var.init.dinit){
		dump_inc(ctx);
		dump_init(ctx, d->bits.var.init.dinit);
		dump_dec(ctx);
	}
}

void gen_dump(symtable_global *globs)
{
	dump dump = { 0 };
	symtable_gasm **iasm = globs->gasms;
	decl **diter;

	dump.fout = stdout;

	for(diter = symtab_decls(&globs->stab); diter && *diter; diter++){
		decl *d = *diter;

		while(iasm && d == (*iasm)->before){
			dump_gasm(*iasm, &dump);

			if(!*++iasm)
				iasm = NULL;
		}

		dump_decl(d, &dump);
	}
}
