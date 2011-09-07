#include <stdio.h>
#include <stdarg.h>

#include "tokenise.h"
#include "tree.h"
#include "parse.h"
#include "alloc.h"
#include "tokconv.h"
#include "util.h"


/*
 * order goes:
 *
 * parse_expr_unary_op   [-+!~]`parse_expr_single`
 * parse_expr_binary_op  above [/%*]    above
 * parse_expr_sum        above [+-]     above
 * parse_expr_bit_op     above [&|]     above
 * parse_expr_cmp_op     above [><==!=] above
 * parse_expr_logical_op above [&&||]   above
 */
#define parse_expr() parse_expr_logical_op()

extern enum token curtok;

tree  *parse_code();
expr **parse_funcargs();
expr  *parse_expr();
decl  *parse_decl(enum type type, enum type_spec spec, int need_spel);

expr *parse_expr_binary_op();

/* generalised recursive descent */
expr *parse_expr_join(
		expr *(*above)(), expr *(*this)(),
		enum token accept, ...
		)
{
	va_list l;
	expr *e = above();

	va_start(l, accept);
	if(curtok == accept || curtok_in_list(l)){
		expr *join;

		va_end(l);

		join       = expr_new();
		join->type = expr_op;
		join->op   = curtok_to_op();
		join->lhs  = e;

		EAT(curtok);
		join->rhs = this();

		return join;
	}else{
		va_end(l);
		return e;
	}
}

expr *parse_expr_unary_op()
{
	extern int currentval;
	expr *e;

	switch(curtok){
		case token_sizeof:
			EAT(token_sizeof);
			e = expr_new();
			e->type = expr_sizeof;
			if(curtok == token_identifier){
				e->spel = token_current_spel();
				EAT(token_identifier);
			}else if(curtok_is_type()){
				e->vartype.type = curtok_to_type();
				EAT(curtok);
			}else{
				EAT(token_identifier); /* raise error */
			}
			return e;

		case token_integer:
		case token_character:
			e = expr_new();
			e->type = expr_val;
			e->val = currentval;
			EAT(curtok);
			return e;

		case token_string:
			e = expr_new();
			e->type = expr_str;
			token_get_current_str(&e->spel, &e->val);
			EAT(token_string);
			return e;

		case token_open_paren:
			EAT(token_open_paren);
			e = parse_expr();
			EAT(token_close_paren);
			return e;

		case token_multiply: /* deref! */
			EAT(token_multiply);
			e = expr_new();
			e->type = expr_op;
			e->op   = op_deref;
			e->lhs  = parse_expr();
			return e;

		case token_and:
			EAT(token_and);
			if(curtok != token_identifier)
				EAT(token_identifier); /* raise error */
			e = expr_new();
			e->type = expr_addr;
			e->spel = token_current_spel();
			EAT(token_identifier);
			return e;

		case token_plus:
			EAT(token_plus);
			return parse_expr();

		case token_minus:
			e = expr_new();
			e->type = expr_op;
			e->op = curtok_to_op();

			EAT(token_minus);
			e->lhs = parse_expr_binary_op();
			return e;

		case token_not:
		case token_bnot:
			e = expr_new();
			e->type = expr_op;
			e->op = curtok_to_op();

			EAT(curtok);
			e->lhs = parse_expr_binary_op();
			return e;

		case token_increment:
		case token_decrement:
			fprintf(stderr, "TODO: ");
			break;

		case token_identifier:
			e = expr_new();

			e->spel = token_current_spel();
			EAT(token_identifier);

			if(curtok == token_assign){
				EAT(token_assign);

				e->type = expr_assign;
				e->expr = parse_expr();
				return e;

			}else if(curtok == token_open_paren){
				EAT(token_open_paren);
				e->type = expr_funcall;
				e->funcargs = parse_funcargs();
				EAT(token_close_paren);
				return e;
			}else{
				e->type = expr_identifier;
				return e;
			}

		default:
			break;
	}
	fprintf(stderr, "warning: parse_expr_unary_op() returning NULL @ %s\n", token_to_str(curtok));
	return NULL;
}

expr *parse_expr_div()
{
	/* above [/] above */
	return parse_expr_join(
			parse_expr_unary_op, parse_expr_div,
				token_divide, token_unknown);
}

expr *parse_expr_binary_op()
{
	/* above [%*] above */
	return parse_expr_join(
			parse_expr_div, parse_expr_binary_op,
				token_multiply, token_modulus,
				token_unknown);
}

expr *parse_expr_neg()
{
	/* above [-] above */
	return parse_expr_join(
			parse_expr_binary_op, parse_expr_neg,
				token_minus, token_unknown);
}

expr *parse_expr_sum()
{
	/* above [+] above */
	return parse_expr_join(
			parse_expr_neg, parse_expr_sum,
				token_plus, token_unknown);
}

expr *parse_expr_bit_op()
{
	/* above [&|] above */
	return parse_expr_join(
			parse_expr_sum, parse_expr_bit_op,
				token_and, token_or, token_unknown);
}

expr *parse_expr_cmp_op()
{
	/* above [><==!=] above */
	return parse_expr_join(
			parse_expr_bit_op, parse_expr_cmp_op,
				token_eq, token_ne,
				token_le, token_lt,
				token_ge, token_gt,
				token_unknown);
}

expr *parse_expr_logical_op()
{
	/* above [&&||] above */
	return parse_expr_join(
			parse_expr_cmp_op, parse_expr_logical_op,
			token_orsc, token_andsc, token_unknown);
}

tree *parse_if()
{
	tree *t = tree_new();
	EAT(token_if);
	EAT(token_open_paren);

	t->type = stat_if;

	t->expr = parse_expr();

	EAT(token_close_paren);

	t->lhs = parse_code();

	if(curtok == token_else){
		EAT(token_else);
		t->rhs = parse_code();
	}

	return t;
}

expr **parse_funcargs()
{
	expr **args = NULL;

	while(curtok != token_close_paren){
		dynarray_add((void ***)&args, parse_expr());

		if(curtok == token_close_paren)
			break;
		EAT(token_comma);
	}

	return args;
}


tree *expr_to_tree(expr *e)
{
	tree *t = tree_new();
	t->type = stat_expr;
	t->expr = e;
	return t;
}

tree *parse_switch(){return NULL;}

tree *parse_do()
{
	tree *t = tree_new();

	EAT(token_do);

	t->lhs = parse_code();

	EAT(token_while);
	EAT(token_open_paren);
	t->expr = parse_expr();
	EAT(token_close_paren);
	EAT(token_semicolon);

	t->type = stat_do;

	return t;
}

tree *parse_while()
{
	tree *t = tree_new();

	EAT(token_while);
	EAT(token_open_paren);

	t->expr = parse_expr();
	EAT(token_close_paren);
	t->lhs = parse_code();

	t->type = stat_while;

	return t;
}

tree *parse_for()
{
	tree *t = tree_new();
	tree_flow *tf;

	EAT(token_for);
	EAT(token_open_paren);

	tf = t->flow = tree_flow_new();

	tf->for_init  = parse_expr();
	EAT(token_semicolon);
	tf->for_while = parse_expr();
	EAT(token_semicolon);
	tf->for_inc   = parse_expr();
	EAT(token_close_paren);

	t->lhs = parse_code();

	t->type = stat_for;

	return t;
}

int parse_type(enum type *t, enum type_spec *s)
{
	int is_spec = 0;

	if(curtok_is_type() || (is_spec = curtok_is_type_specifier())){
		*s = spec_none;

		if(is_spec){
			do{
				*s |= curtok_to_type_specifier();
				EAT(curtok);
			}while(curtok_is_type_specifier());
		}

		if((*t = curtok_to_type()) == type_unknown)
			*t = type_int; /* default to int */
		else
			EAT(curtok);

		return 0;
	}
	return 1;
}

tree *parse_code_declblock()
{
	tree *t = tree_new();
	enum type curtype;
	enum type_spec curspec;

	t->type = stat_code;

	EAT(token_open_block);

	while(!parse_type(&curtype, &curspec)){
next_decl:
		dynarray_add((void ***)&t->decls, parse_decl(curtype, curspec, 1));

		if(curtok == token_comma){
			EAT(token_comma);
			goto next_decl; /* don't read another type */
		}
		EAT(token_semicolon);
	}

	/* main read loop */
	do{
		tree *sub = parse_code();

		if(sub)
			dynarray_add((void ***)&t->codes, sub);
		else
			break;
	}while(curtok != token_close_block);

	EAT(token_close_block);
	return t;
}

tree *parse_code()
{
	tree *t;

	switch(curtok){
		case token_semicolon:
			t = tree_new();
			t->type = stat_noop;
			EAT(token_semicolon);
			return t;


		case token_break:
		case token_return:
			t = tree_new();
			if(curtok == token_break){
				t->type = stat_break;
				EAT(token_break);
			}else{
				t->type = stat_return;
				EAT(token_return);
				if(curtok != token_semicolon)
					t->expr = parse_expr();
			}
			EAT(token_semicolon);
			return t;

		case token_switch: return parse_switch();
		case token_if:     return parse_if();
		case token_while:  return parse_while();
		case token_do:     return parse_do();
		case token_for:    return parse_for();

		case token_open_block: return parse_code_declblock();

		default: break;
	}

	t = expr_to_tree(parse_expr());
	EAT(token_semicolon);
	return t;
}

decl *parse_decl(enum type type, enum type_spec spec, int need_spel)
{
	decl *d = decl_new();

	if(type == type_unknown){
		parse_type(&d->type, &d->spec);
	}else{
		d->type = type;
		d->spec = spec;
	}

	while(curtok == token_multiply){
		EAT(token_multiply);
		d->ptr_depth++;
	}

	if(curtok == token_identifier){
		d->spel = token_current_spel();
		EAT(token_identifier);
	}else if(need_spel){
		EAT(token_identifier); /* raise error */
	}

	/* array parsing */
	while(curtok == token_open_square){
		expr *size;
		int fin;

		fin = 0;

		EAT(token_open_square);
		if(curtok != token_close_square)
			size = parse_expr(); /* fold.c checks for const-ness */
		else
			fin = 1;
		EAT(token_close_square);

		if(fin){
			d->ptr_depth++;
			break;
		}

		dynarray_add((void ***)&d->arraysizes, size);
	}

	return d;
}

function *parse_function_proto()
{
	function *f = function_new();

	f->func_decl = parse_decl(type_unknown, spec_none, 1);
	f->func_decl->func = 1;

	EAT(token_open_paren);

	while((curtok_is_type_prething())){
		dynarray_add((void ***)&f->args, parse_decl(type_unknown, spec_none, 0));

		if(curtok == token_close_paren)
			break;

		EAT(token_comma);
		/* continue loop */
	}

	EAT(token_close_paren);

	return f;
}

function *parse_function()
{
	function *f = parse_function_proto();

	if(curtok == token_semicolon)
		EAT(token_semicolon);
	else
		f->code = parse_code();

	return f;
}

function **parse()
{
	function **f;
	int i, n;

	n = 10;
	i = 0;
	f = umalloc(n * sizeof *f);

	do{
		f[i++] = parse_function();

		if(i == n)
			f = urealloc(f, (n += 10) * sizeof *f);
	}while(curtok != token_eof);

	return f;
}
