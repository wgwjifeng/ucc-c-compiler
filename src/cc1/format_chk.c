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

#include "format_chk.h"

enum printf_attr
{
	printf_attr_long = 1 << 0,
	printf_attr_llong = 1 << 1,
	printf_attr_size_t = 1 << 2
};

static const char *printf_attr_to_str(enum printf_attr attr)
{
	if(attr & printf_attr_size_t)
		return "z";
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

static void format_check_printf_1(char fmt, type *const t_in,
		where *w, enum printf_attr attr)
{
	char expected[BTYPE_STATIC_BUFSIZ];

	expected[0] = '\0';

	switch(fmt){
		enum type_primitive prim;
		type *tt;

		case 's': prim = type_nchar; goto ptr;
		case 'p': prim = type_void; goto ptr;
		case 'n': prim = type_int;  goto ptr;
ptr:
			tt = type_is_primitive(type_is_ptr(t_in), prim);
			if(!tt){
				snprintf(expected, sizeof expected,
						"'%s *'", type_primitive_to_str(prim));
			}else if(attr){
				warn_printf_attr(fmt, w, attr);
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
#define ATTR_CHECK(suff, str)                   \
			if((attr & printf_attr_##suff)            \
			&& !type_is_primitive(t_in, type_##suff)) \
			{                                         \
				strcpy(expected, str);                  \
			}

			ATTR_CHECK(llong, "'long long'")
			else
			ATTR_CHECK(long, "'long'")
			else
			ATTR_CHECK(long, "'size_t'")
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
				warn_printf_attr(fmt, w, attr);
			break;

		default:
			cc1_warn_at(w, attr_printf_unknown,
					"unknown conversion character 0x%x", fmt);
			return;
	}

	if(*expected){
		cc1_warn_at(w, attr_printf_bad,
				"format %%%s%c expects %s argument (got %s)",
				attr & printf_attr_long ? "l" : "", fmt,
				expected, type_to_str(t_in));
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
			attr |= printf_attr_size_t;
			++*index;
			break;

		default:
			return attr;
	}
}

static void format_check_printf_str(
		expr **args,
		const char *fmt, const int len,
		int var_idx, where *w)
{
	int n_arg = 0;
	int i;

	for(i = 0; i < len && fmt[i];){
		if(fmt[i++] == '%'){
			if(i == len){
				where strloc = *w;
				strloc.chr += i + 1; /* +1 since we start on the '(' */
				cc1_warn_at(&strloc, attr_printf_bad, "incomplete format specifier");
				return;
			}

			if(fmt[i] == '%'){
				i++;
				continue;
			}

			/* don't check for format(printf, ..., 0) */
			if(var_idx != -1){
				expr *e = args[var_idx + n_arg++];
				enum printf_attr attr = printf_modifiers(fmt, &i);

				if(!e){
					cc1_warn_at(w, attr_printf_bad,
							"too few arguments for format (%%%c)", fmt[i]);
					break;
				}

				format_check_printf_1(fmt[i], e->tree_type, &e->where, attr);
			}

			/*if(fmt[i] == '*'){
				i++;
				goto recheck;
			}*/
		}
	}

	if(var_idx != -1 && (!fmt[i] || i == len) && args[var_idx + n_arg])
		cc1_warn_at(w, attr_printf_toomany, "too many arguments for format");
}

static void format_check_printf(
		expr *str_arg,
		expr **args,
		unsigned var_idx,
		where *w)
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
						args, var_idx, w);

				format_check_printf(str_arg->rhs, args, var_idx, w);
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

	{
		const char *fmt = fmt_str->str;
		const int   len = fmt_str->len - 1;

		if(len <= 0)
			;
		else if(k.offset >= len)
			cc1_warn_at(&str_arg->where, attr_printf_bad,
					"undefined printf-format argument");
		else
			format_check_printf_str(args, fmt + k.offset, len, var_idx, w);
	}
}

void format_check_call(
		where *w, type *ref,
		expr **args, const int variadic)
{
	attribute *attr = type_attr_present(ref, attr_format);
	int n, fmt_idx, var_idx;

	if(!attr || !variadic)
		return;
	switch(attr->bits.format.validity){
		case fmt_unchecked:
			ICW("unchecked __attribute__((format...))");
		case fmt_invalid:
			return;
		case fmt_valid:
			break;
	}

	fmt_idx = attr->bits.format.fmt_idx;
	var_idx = attr->bits.format.var_idx;

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
			format_check_printf(args[fmt_idx], args, var_idx, w);
			break;

		case attr_fmt_scanf:
			ICW("scanf check");
			break;
	}
}

void format_check_decl(decl *d, attribute *da)
{
	type *r_func;
	funcargs *fargs;
	int fmt_idx, var_idx, nargs;

	if(!cc1_warning.format)
		return;

	if(da->bits.format.validity != fmt_unchecked){
		/* i.e. checked */
		return;
	}

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
		if(da->bits.format.var_idx >= 0){
			cc1_warn_at(&da->where, attr_printf_bad,
					"variadic function required for format attribute");
		}
		goto invalid;
	}

	nargs = dynarray_count(fargs->arglist);
	fmt_idx = da->bits.format.fmt_idx;
	var_idx = da->bits.format.var_idx;

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
