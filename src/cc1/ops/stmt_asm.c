#include <stdlib.h>

#include "ops.h"
#include "stmt_asm.h"
#include "../out/__asm.h"

const char *str_stmt_asm()
{
	return "asm";
}

static void check_constraint(asm_inout *io, symtable *stab, int output)
{
	if(output)
		fold_inc_writes_if_sym(io->exp, stab);

	if(output)
		fold_expr_no_decay (io->exp, stab);
	else
		FOLD_EXPR(io->exp, stab);

	out_constraint_check(&io->exp->where, io->constraints, output);
}

void fold_stmt_asm(stmt *s)
{
	asm_inout **it;
	int n_inouts = 0;

	for(it = s->bits.asm_args->ios; it && *it; it++, n_inouts++){
		asm_inout *io = *it;

		check_constraint(io, s->symtab, io->is_output);

		if(io->is_output && !expr_is_lval(io->exp))
			die_at(&io->exp->where, "asm output not an lvalue");
	}

	/* validate asm string - s->bits.asm_args->cmd */
	if(s->bits.asm_args->extended){
		char *str;

		for(str = s->bits.asm_args->cmd; *str; str++)
			if(*str == '%'){
				if(str[1] == '%'){
					str++;

				}else if(str[1] == '['){
					ICE("TODO: named constraint");

				}else{
					char *ep;
					long pos = strtol(str + 1, &ep, 0);

					if(ep == str + 1)
						die_at(&s->where, "invalid register character '%c', number expected", *ep);

					if(pos >= n_inouts)
						die_at(&s->where, "invalid register index %ld / %d", pos, n_inouts);

					str = ep - 1;
				}
			}
	}
}

void gen_stmt_asm(stmt *s)
{
	asm_inout **ios;
	int npops = 0;

	for(ios = s->bits.asm_args->ios; ios && *ios; ios++, npops++){
		asm_inout *io = *ios;

		(io->is_output ? lea_expr : gen_expr)(io->exp);
	}

	out_comment("### begin asm(%s) from %s",
			s->bits.asm_args->extended ? ":::" : "",
			where_str(&s->where));

	out_asm_inline(s->bits.asm_args, &s->where);

	out_comment("### end asm()");
}

void init_stmt_asm(stmt *s)
{
	s->f_passable = fold_passable_yes;
}

void style_stmt_asm(stmt *s)
{
	stylef(
			"asm%s(%s",
			s->bits.asm_args->is_volatile ? " volatile" : "",
			s->bits.asm_args->cmd);

	stylef(")\n");
}