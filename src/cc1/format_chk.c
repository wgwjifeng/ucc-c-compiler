#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "../util/where.h"
#include "../util/util.h"
#include "../util/dynarray.h"

#include "expr.h"
#include "decl.h"
#include "const.h"
#include "funcargs.h"
#include "type_is.h"
#include "warn.h"
#include "str.h"

#include "format_chk.h"

#include "ops/expr_if.h"

enum printf_attr
{
	printf_attr_long = 1 << 0,
	printf_attr_llong = 1 << 1,
	printf_attr_size_t = 1 << 2,
	printf_attr_ptrdiff_t = 1 << 3
};

static const char *printf_attr_to_str(enum printf_attr attr)
{
	if(attr & printf_attr_size_t)
		return "z";
	if(attr & printf_attr_ptrdiff_t)
		return "t";
	if(attr & printf_attr_llong)
		return "ll";
	if(attr & printf_attr_long)
		return "l";
	return NULL;
}

static void warn_printf_attr(char fmt, where *w, enum printf_attr attr)
{
	cc1_warn_at(w, attr_printf_bad,
			"unexpected printf modifier '%s' for %%%c",
			printf_attr_to_str(attr), fmt);
}

static int attr_check(
		enum printf_attr attr, enum printf_attr mask,
		enum type_primitive primitive, type *ty,
		char expected[BTYPE_STATIC_BUFSIZ], const char *str)
{
	int present = !!(attr & mask);

	if(present && !type_is_primitive_anysign(ty, primitive))
		strcpy(expected, str);

	return present;
}

static void format_check_printf_1(char fmt, type *const t_in,
		where *loc_expr, where *loc_str, enum printf_attr attr)
{
	unsigned char *const default_warningp = &cc1_warning.attr_printf_bad;
	unsigned char *warningp = default_warningp;
	char expected[BTYPE_STATIC_BUFSIZ];

	expected[0] = '\0';

	switch(fmt){
		enum type_primitive prim;
		type *tt;

		case 'p':
			prim = type_void;

			if(cc1_warning.attr_printf_voidp){
				/* strict %p / void* check - emitted with voidp flag */
				warningp = &cc1_warning.attr_printf_voidp;
			}else{
				/* allow any* */
				if(type_is_ptr(t_in))
					break;

				/* not a pointer - emit with the default-warning flag */
			}
			goto ptr;

		case 's': prim = type_nchar; goto ptr;
		case 'n': prim = type_int;  goto ptr;
ptr:
			tt = type_is_primitive_anysign(type_is_ptr(t_in), prim);
			if(!tt){
				if(prim == type_unknown)
					prim = type_void;

				snprintf(expected, sizeof expected,
						"'%s *'", type_primitive_to_str(prim));
			}else if(attr){
				warningp = default_warningp;
				warn_printf_attr(fmt, loc_str, attr);
			}
			break;

		case 'x':
		case 'X':
		case 'u':
		case 'o':
			/* unsigned ... */
		case '*':
		case 'c':
		case 'd':
		case 'i':
			if(!type_is_integral(t_in)){
				strcpy(expected, "integral");
				break;
			}

			if(attr & (printf_attr_size_t | printf_attr_ptrdiff_t)){
				/* just do size checks for size_t, since it
				 * could be long, or long-long */

				if(type_size(t_in, loc_expr) != type_primitive_size(type_intptr_t))
					strcpy(expected, "'size_t/intptr_t'");

				break;
			}

			/* check %ld and %lld */
#define ATTR_CHECK(suff, str) \
			attr_check(attr, printf_attr_##suff, type_##suff, t_in, expected, str)

			if(ATTR_CHECK(llong, "'long long'"))
				break;
			if(ATTR_CHECK(long, "'long'"))
				break;

#undef ATTR_CHECK

			/* check int doesn't have anything greater */
			if(!type_is_primitive_anysign(t_in, type_int))
				strcpy(expected, "'int'");
			break;

		case 'e':
		case 'E':
		case 'f':
		case 'F':
		case 'g':
		case 'G':
		case 'a':
		case 'A':
			if(!type_is_floating(t_in))
				strcpy(expected, "'double'");
			else if(attr)
				warn_printf_attr(fmt, loc_str, attr);
			break;

		default:
			if(fmt){
				cc1_warn_at(loc_str, attr_printf_unknown,
						"unknown conversion character '%c' (0x%x)",
						fmt, fmt);
			}else{
				cc1_warn_at(loc_str, attr_printf_bad,
						"missing conversion character");
			}
			return;
	}

	if(*expected){
		cc1_warn_at_w(loc_expr, warningp,
				"format %%%s%c expects %s argument (got %s)",
				attr & printf_attr_llong ? "ll" : attr & printf_attr_long ? "l" : "",
				fmt, expected, type_to_str(t_in));
	}
}

static enum printf_attr printf_modifiers(
		const char *fmt, int *index)
{
	enum printf_attr attr = 0;

	for(;;) switch(fmt[*index]){
		case 'l':
			if(attr & printf_attr_long)
				attr |= printf_attr_llong;
			else
				attr |= printf_attr_long;

		case '1': case '2': case '3':
		case '4': case '5': case '6':
		case '7': case '8': case '9':

		case '0': case '#': case '-':
		case ' ': case '+': case '.':

		case 'h': case 'L':
			++*index;
			break;

		case 'z':
		case 't':
			attr |= (fmt[*index] == 'z'
					? printf_attr_size_t
					: printf_attr_ptrdiff_t);

			++*index;
			break;

		default:
			return attr;
	}
}

static void format_check_printf_arg(
		char fmt,
		where *strloc,
		expr ***current_arg, enum printf_attr attr)
{
	expr *e = **current_arg;

	if(!e){
		cc1_warn_at(strloc, attr_printf_bad,
				"too few arguments for format (%%%c)", fmt);
		return;
	}

	/* place us on the format char */
	strloc->chr++;

	format_check_printf_1(fmt, e->tree_type, &e->where, strloc, attr);

	++*current_arg;
}

static void format_check_printf_str(
		expr **args,
		const char *fmt, const int len,
		const int var_idx,
		where *quote_loc)
{
	expr **current_arg = args;
	int i;

	for(i = var_idx; *current_arg && i > 0; i--)
		current_arg++;

	for(i = 0; i < len && fmt[i];){
		if(fmt[i++] == '%'){
			where strloc = *quote_loc;
			strloc.chr += i + 1; /* +1 since we start on the '"' */

			if(i == len){
				cc1_warn_at(&strloc, attr_printf_bad, "incomplete format specifier");
				return;
			}

			if(fmt[i] == '%'){
				i++;
				continue;
			}

			/* don't check for format(printf, ..., 0) */
			if(var_idx != -1){
				enum printf_attr attr = printf_modifiers(fmt, &i);

				format_check_printf_arg(
						fmt[i],
						&strloc,
						&current_arg,
						attr);
				i++;
				if(i >= len)
					continue;
			}

			if(fmt[i] == '*'){
				if(var_idx != -1){
					enum printf_attr attr = printf_modifiers(fmt, &i);

					format_check_printf_arg(
							fmt[i],
							&strloc,
							&current_arg,
							attr);
				}
				i++;
			}
		}
	}

	if(i > len)
		i = len;

	if(var_idx != -1 && (!fmt[i] || i == len) && *current_arg){
		cc1_warn_at(&(*current_arg)->where, attr_printf_toomany,
				"too many arguments for format");
	}
}

static void format_check_printf(
		expr *str_arg,
		expr **args,
		unsigned var_idx)
{
	stringlit *fmt_str;
	consty k;

	const_fold(str_arg, &k);

	switch(k.type){
		case CONST_NO:
		case CONST_NEED_ADDR:
			/* check for the common case printf(x?"":"", ...) */
			if(expr_kind(str_arg, if)){

				format_check_printf(
						str_arg->lhs ? str_arg->lhs : str_arg->expr,
						args, var_idx);

				format_check_printf(str_arg->rhs, args, var_idx);

				return;
			}
			goto not_string;

		case CONST_NUM:
			if(K_INTEGRAL(k.bits.num) && k.bits.num.val.i == 0)
				return; /* printf(NULL, ...) */
			/* fall - printf(5, ...) or printf(2.3f, ...) */

		case CONST_ADDR:
not_string:
			cc1_warn_at(&str_arg->where, attr_printf_bad,
					"format argument isn't a string constant");
			return;

		case CONST_STRK:
			fmt_str = k.bits.str->lit;
			break;
	}

	if(fmt_str->cstr->type != CSTRING_WIDE){
		const char *fmt = fmt_str->cstr->bits.ascii;
		const int   len = fmt_str->cstr->count - 1;

		if(len <= 0)
			;
		else if(k.offset >= len)
			cc1_warn_at(&str_arg->where, attr_printf_bad,
					"undefined printf-format argument");
		else
			format_check_printf_str(args, fmt + k.offset, len,
					var_idx, &str_arg->where);
	}
}

void format_check_call(
		type *fnty, expr **args, const int variadic)
{
	attribute *attr = type_attr_present(fnty, attr_format);
	int n, fmt_idx, var_idx;

	if(!attr || !variadic)
		return;
	switch(attr->bits.format.validity){
		case fmt_unchecked:
			/*ICW("unchecked __attribute__((format...))");*/
			/* printf checking is disabled/warning is off - return */
			/* fall */
		case fmt_invalid:
			return;
		case fmt_valid:
			break;
	}

	fmt_idx = const_fold_val_i(attr->bits.format.fmt_idx) - 1;
	var_idx = const_fold_val_i(attr->bits.format.var_idx) - 1;

	n = dynarray_count(args);

	/* do bounds checks here, but no warnings
	 * warnings are on the decl
	 *
	 * if var_idx is zero we only check the format string,
	 * not the arguments
	 */
	if(fmt_idx >= n
	|| var_idx > n
	|| (var_idx > -1 && var_idx <= fmt_idx))
	{
		return;
	}

	switch(attr->bits.format.fmt_func){
		case attr_fmt_printf:
			if(cc1_warning.attr_printf_bad
			|| cc1_warning.attr_printf_toomany
			|| cc1_warning.attr_printf_unknown)
			{
				format_check_printf(args[fmt_idx], args, var_idx);
			}
			break;

		case attr_fmt_scanf:
			ICW("scanf check");
			break;
	}
}

static int extract_const(expr *e, int *const out)
{
	consty k;

	const_fold(e, &k);

	if(k.type != CONST_NUM || k.bits.num.suffix & VAL_FLOATING)
		return 0;

	*out = k.bits.num.val.i;
	return 1;
}

void format_check_decl(decl *d, attribute *da)
{
	type *r_func;
	funcargs *fargs;
	int fmt_idx, var_idx, nargs;

	if(da->bits.format.validity != fmt_unchecked){
		/* i.e. checked */
		return;
	}

	if(!extract_const(da->bits.format.fmt_idx, &fmt_idx)
	|| !extract_const(da->bits.format.var_idx, &var_idx))
	{
		cc1_warn_at(&da->where, attr_printf_bad,
				"format/variadic argument indexes must be integer constant expressions");
		goto invalid;
	}
	fmt_idx--;
	var_idx--;

	r_func = type_is_func_or_block(d->ref);
	assert(r_func);
	fargs = r_func->bits.func.args;

	if(!fargs->variadic){
		/* if the index is zero, we ignore it, e.g.
		 * vprintf(char *, va_list) __attribute((format(printf, 1, 0)));
		 *                                                         ^
		 *
		 * (-1, not zero, since we subtract one for format indexes)
		 */
		if(var_idx >= 0){
			cc1_warn_at(&da->where, attr_printf_bad,
					"variadic function required for format attribute");
		}
		goto invalid;
	}

	nargs = dynarray_count(fargs->arglist);

	/* format string index must be < nargs */
	if(fmt_idx >= nargs){
		cc1_warn_at(&da->where,
				attr_format_baddecl,
				"format argument out of bounds (%d >= %d)",
				fmt_idx, nargs);
		goto invalid;
	}

	if(var_idx != -1){
		/* variadic index must be nargs */
		if(var_idx != nargs){
			cc1_warn_at(&da->where,
					attr_printf_bad,
					"variadic argument out of bounds (should be %d)",
					nargs + 1); /* +1 to get to the "..." index as 1-based */
			goto invalid;
		}

		assert(var_idx > fmt_idx);
	}

	if(type_str_type(fargs->arglist[fmt_idx]->ref) != type_str_char){
		cc1_warn_at(&da->where, attr_printf_bad,
				"format argument not a string type");
		goto invalid;
	}

	da->bits.format.validity = fmt_valid;
	return;
invalid:
	da->bits.format.validity = fmt_invalid;
}
