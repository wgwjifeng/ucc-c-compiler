#include <stdlib.h>

#include "ops.h"
#include "stmt_while.h"
#include "stmt_if.h"
#include "../out/lbl.h"

const char *str_stmt_while()
{
	return "while";
}

void fold_stmt_while(stmt *s)
{
	symtable *stab = s->symtab;

	flow_fold(s->flow, &stab);

	s->lbl_break    = out_label_flow(b_from, "while_break");
	s->lbl_continue = out_label_flow(b_from, "while_cont");

	FOLD_EXPR(s->expr, stab);
	fold_check_expr(
			s->expr,
			FOLD_CHK_NO_ST_UN | FOLD_CHK_BOOL,
			s->f_str());

	fold_stmt(s->lhs);
}

void gen_stmt_while(stmt *s)
{
	out_label(b_from, s->lbl_continue);

	flow_gen(s->flow, s->symtab);
	gen_expr(s->expr);

	out_op_unary(b_from, op_not);
	out_jtrue(b_from, s->lbl_break);

	gen_stmt(s->lhs);

	out_push_lbl(b_from, s->lbl_continue, 0);
	out_jmp(b_from);

	out_label(b_from, s->lbl_break);
}

void style_stmt_while(stmt *s)
{
	stylef("while(");
	gen_expr(s->expr);
	stylef(")");
	gen_stmt(s->lhs);
}

int while_passable(stmt *s)
{
	if(const_expr_and_non_zero(s->expr))
		return fold_code_escapable(s); /* while(1) */

	return 1; /* fold_passable(s->lhs) - doesn't depend on this */
}

void mutate_stmt_while(stmt *s)
{
	s->f_passable = while_passable;
}
