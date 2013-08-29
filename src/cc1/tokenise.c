#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>

#include "../util/util.h"
#include "data_structs.h"
#include "tokenise.h"
#include "../util/alloc.h"
#include "../util/str.h"
#include "str.h"
#include "cc1.h"

#define KEYWORD(x) { #x, token_ ## x }

#define KEYWORD__(x, t) \
	{ "__" #x,      t },  \
	{ "__" #x "__", t }

#define KEYWORD__ALL(x) KEYWORD(x), KEYWORD__(x, token_ ## x)

struct statement
{
	const char *str;
	enum token tok;
} statements[] = {
#ifdef BRITISH
	{ "perchance", token_if      },
	{ "otherwise", token_else    },

	{ "what_about",        token_switch  },
	{ "perhaps",           token_case    },
	{ "on_the_off_chance", token_default },

	{ "splendid",    token_break    },
	{ "goodday",     token_return   },
	{ "as_you_were", token_continue },

	{ "tallyho",     token_goto     },
#else
	KEYWORD(if),
	KEYWORD(else),

	KEYWORD(switch),
	KEYWORD(case),
	KEYWORD(default),

	KEYWORD(break),
	KEYWORD(return),
	KEYWORD(continue),

	KEYWORD(goto),
#endif

	KEYWORD(do),
	KEYWORD(while),
	KEYWORD(for),

	KEYWORD__ALL(asm),

	KEYWORD(void),
	KEYWORD(char),
	KEYWORD(int),
	KEYWORD(short),
	KEYWORD(long),
	KEYWORD(float),
	KEYWORD(double),
	KEYWORD(_Bool),

	KEYWORD(auto),
	KEYWORD(static),
	KEYWORD(extern),
	KEYWORD(register),

	KEYWORD__ALL(inline),
	KEYWORD(_Noreturn),

	KEYWORD__ALL(const),
	KEYWORD__ALL(volatile),
	KEYWORD__ALL(restrict),

	KEYWORD__ALL(signed),
	KEYWORD__ALL(unsigned),

	KEYWORD(typedef),
	KEYWORD(struct),
	KEYWORD(union),
	KEYWORD(enum),

	KEYWORD(_Alignof),
	KEYWORD__(alignof, token__Alignof),
	KEYWORD(_Alignas),
	KEYWORD__(alignas, token__Alignas),

	{ "__builtin_va_list", token___builtin_va_list },

	KEYWORD(sizeof),
	KEYWORD(_Generic),
	KEYWORD(_Static_assert),

	KEYWORD__ALL(typeof),

	KEYWORD__(attribute, token_attribute),
};

static tokenise_line_f *in_func;
int buffereof = 0;
int parse_finished = 0;

#define FNAME_STACK_N 32
static struct fnam_stack
{
	char *fnam;
	int    lno;
} current_fname_stack[FNAME_STACK_N];

static int current_fname_stack_cnt;
char *current_fname;
int current_fname_used;

static char *buffer, *bufferpos;
static int ungetch = EOF;

static struct line_list
{
	char *line;
	struct line_list *next;
} *store_lines, **store_line_last = &store_lines;

/* -- */
enum token curtok, curtok_uneat;

numeric currentval = { { 0 } }; /* an integer literal */

char *currentspelling = NULL; /* e.g. name of a variable */

char *currentstring   = NULL; /* a string literal */
int   currentstringlen = 0;
int   currentstringwide = 0;

/* -- */
int current_line = 0;
int current_chr  = 0;
char *current_line_str = NULL;
int current_line_str_used = 0;

#define SET_CURRENT(ty, new) do{\
	if(!current_ ## ty ## _used)  \
		free(current_ ## ty);       \
	current_## ty = new;          \
	current_## ty ##_used = 0; }while(0)

#define SET_CURRENT_LINE_STR(new) SET_CURRENT(line_str, new)

static void push_fname(char *fn, int lno)
{
	current_fname = fn;
	if(current_fname_stack_cnt < FNAME_STACK_N){
		struct fnam_stack *p = &current_fname_stack[current_fname_stack_cnt++];
		p->fnam = ustrdup(fn);
		p->lno = lno;
	}
}

static void pop_fname(void)
{
	if(current_fname_stack_cnt > 0){
		struct fnam_stack *p = &current_fname_stack[--current_fname_stack_cnt];
		free(p->fnam);
	}
}

static void handle_line_file_directive(char *fnam, int lno)
{
	/*
# 1 "inc.c"
# 5 "yo.h"  // include "yo.h"
            // if we get an error here,
            // we want to know we're included from inc.c:1
# 2 "inc.c" // include end - the line no. doesn't have to be prev+1
	 */

	/* logic for knowing when to pop and when to push */
	int i;

	for(i = current_fname_stack_cnt - 1; i >= 0; i--){
		struct fnam_stack *stk = &current_fname_stack[i];

		if(!strcmp(fnam, stk->fnam)){
			/* found another "inc.c" */
			/* pop `n` stack entries, then push our new one */
			while(current_fname_stack_cnt > i)
				pop_fname();
			break;
		}
	}

	push_fname(fnam, lno);
}

void include_bt(FILE *f)
{
	int i;
	for(i = 0; i < current_fname_stack_cnt - 1; i++){
		struct fnam_stack *stk = &current_fname_stack[i];

		fprintf(f, "%s:%d: included from here\n",
				stk->fnam, stk->lno);
	}
}

static void add_store_line(char *l)
{
	struct line_list *new = umalloc(sizeof *new);
	new->line = l;

	*store_line_last = new;
	store_line_last = &new->next;
}

static void tokenise_read_line()
{
	char *l;

	if(buffereof)
		return;

	if(buffer){
		if((fopt_mode & FOPT_SHOW_LINE) == 0)
			free(buffer);
		buffer = NULL;
	}

	l = in_func();
	if(!l){
		buffereof = 1;
	}else{
		/* check for preprocessor line info */
		/* but first - add to store_lines */
		if(fopt_mode & FOPT_SHOW_LINE)
			add_store_line(l);

		/* format is # line? [0-9] "filename" ([0-9])* */
		if(*l == '#'){
			int lno;
			char *ep;

			l = str_spc_skip(l + 1);
			if(!strncmp(l, "line", 4))
				l += 4;

			lno = strtol(l, &ep, 0);
			if(ep == l)
				die("couldn't parse number for #line directive (%s)", ep);

			if(lno < 0)
				die("negative #line directive argument");

			current_line = lno - 1; /* inc'd below */

			ep = str_spc_skip(ep);

			switch(*ep){
				case '"':
				{
					char *p = str_quotefin(++ep);
					if(!p)
						die("no terminating quote to #line directive (%s)", l);
					handle_line_file_directive(ustrdup2(ep, p), lno);
					/*l = str_spc_skip(p + 1);
					if(*l)
						die("characters after #line?");
						- gcc puts characters after the string */
					break;
				}
				case '\0':
					break;

				default:
					die("expected '\"' or nothing after #line directive (%s)", ep);
			}

			tokenise_read_line();
			return;
		}

		current_chr = -1;
		current_line++;
		if(current_fname_stack_cnt > 0)
			current_fname_stack[current_fname_stack_cnt - 1].lno = current_line;
	}

	if(l)
		SET_CURRENT_LINE_STR(ustrdup(l));

	bufferpos = buffer = l;
}

void tokenise_set_input(tokenise_line_f *func, const char *nam)
{
	char *nam_dup = ustrdup(nam);
	in_func = func;

	if(fopt_mode & FOPT_TRACK_INITIAL_FNAM)
		push_fname(nam_dup, 1);
	else
		current_fname = nam_dup;

	SET_CURRENT_LINE_STR(NULL);

	current_line = buffereof = parse_finished = 0;
	nexttoken();
}

static int rawnextchar()
{
	if(buffereof)
		return EOF;

	while(!bufferpos || !*bufferpos){
		tokenise_read_line();
		if(buffereof)
			return EOF;
	}

	current_chr++;
	return *bufferpos++;
}

static int nextchar()
{
	int c;
	do
		c = rawnextchar();
	while(isspace(c) || c == '\f'); /* C allows ^L aka '\f' anywhere in the code */
	return c;
}

static int peeknextchar()
{
	/* doesn't ignore isspace() */
	if(!bufferpos)
		tokenise_read_line();

	if(buffereof)
		return EOF;

	return *bufferpos;
}

static void read_number(enum base mode)
{
	int read_suffix = 1;
	int nlen;
	char c;
	enum numeric_suffix suff = 0;

	char_seq_to_iv(bufferpos, &currentval, &nlen, mode);

	if(nlen == 0)
		die_at(NULL, "%s-number expected (got '%c')",
				base_to_str(mode), peeknextchar());

	bufferpos += nlen;

	/* accept either 'U' 'L' or 'LL' as atomic parts (i.e. not LUL) */
	/* fine using nextchar() since we peeknextchar() first */
	do switch((c = peeknextchar())){
		case 'U':
		case 'u':
			if(suff & VAL_UNSIGNED)
				die_at(NULL, "duplicate U suffix");
			suff |= VAL_UNSIGNED;
			nextchar();
			break;
		case 'L':
		case 'l':
			if(suff & (VAL_LLONG | VAL_LONG))
				die_at(NULL, "already have a L/LL suffix");

			nextchar();
			if(peeknextchar() == c){
				C99_LONGLONG();
				suff |= VAL_LLONG;
				nextchar();
			}else{
				suff |= VAL_LONG;
			}
			break;
		default:
			read_suffix = 0;
	}while(read_suffix);

	/* don't touch cv.suffix until after
	 * - it may already have ULL from an
	 * overflow in parsing
	 */
	currentval.suffix |= suff;
}

static enum token curtok_to_xequal(void)
{
#define MAP(x) case x: return x ## _assign
	switch(curtok){
		MAP(token_plus);
		MAP(token_minus);
		MAP(token_multiply);
		MAP(token_divide);
		MAP(token_modulus);
		MAP(token_not);
		MAP(token_bnot);
		MAP(token_and);
		MAP(token_or);
		MAP(token_xor);
		MAP(token_shiftl);
		MAP(token_shiftr);
#undef MAP

		default:
			break;
	}
	return token_unknown;
}

static int curtok_is_xequal()
{
	return curtok_to_xequal() != token_unknown;
}

static void read_string(char **sptr, int *plen)
{
	char *const start = bufferpos;
	char *const end = terminating_quote(start);
	int size;

	if(!end){
		char *p;
		if((p = strchr(bufferpos, '\n')))
			*p = '\0';
		die_at(NULL, "Couldn't find terminating quote to \"%s\"", bufferpos);
	}

	size = end - start + 1;

	*sptr = umalloc(size);
	*plen = size;

	strncpy(*sptr, start, size);
	(*sptr)[size-1] = '\0';

	escape_string(*sptr, plen);

	bufferpos += size;
}

static void read_string_multiple(const int is_wide)
{
	/* TODO: read in "hello\\" - parse string char by char, rather than guessing and escaping later */
	char *str;
	int len;

	read_string(&str, &len);

	curtok = token_string;

	for(;;){
		int c = nextchar();
		if(c == '"'){
			/* "abc" "def"
			 *       ^
			 */
			char *new, *alloc;
			int newlen;

			read_string(&new, &newlen);

			alloc = umalloc(newlen + len);

			memcpy(alloc, str, len);
			memcpy(alloc + len - 1, new, newlen);

			free(str);
			free(new);

			str = alloc;
			len += newlen - 1;
		}else{
			if(ungetch != EOF)
				ICE("ungetch");
			ungetch = c;
			break;
		}
	}

	currentstring    = str;
	currentstringlen = len;
	currentstringwide = is_wide;
}

static void read_char(const int is_wide)
{
	/* TODO: merge with read_string escape code */
	int c = rawnextchar();

	if(c == EOF){
		die_at(NULL, "Invalid character");
	}else if(c == '\\'){
		char esc = tolower(peeknextchar());

		if(esc == 'x' || esc == 'b' || isoct(esc)){

			if(esc == 'x' || esc == 'b')
				nextchar();

			read_number(esc == 'x' ? HEX : esc == 'b' ? BIN : OCT);

			if(currentval.suffix & ~VAL_PREFIX_MASK)
				die_at(NULL, "invalid character sequence: suffix given");

			if(!is_wide && currentval.val.i > 0xff)
				warn_at(NULL,
						"invalid character sequence: too large (parsed 0x%" NUMERIC_FMT_X ")",
						currentval.val.i);

			c = currentval.val.i;
		}else{
			/* special parsing */
			c = escape_char(esc);

			if(c == -1)
				die_at(NULL, "invalid escape character '%c'", esc);

			nextchar();
		}
	}

	currentval.val.i = c;
	currentval.suffix = 0;

	if((c = nextchar()) != '\'')
		die_at(NULL, "no terminating \"'\" for character (got '%c')", c);

	curtok = token_character;
}

void nexttoken()
{
	int c;

	if(buffereof){
		/* delay this until we are asked for token_eof */
		parse_finished = 1;
		curtok = token_eof;
		return;
	}

	if(ungetch != EOF){
		c = ungetch;
		ungetch = EOF;
	}else{
		c = nextchar(); /* no numbers, no more peeking */
		if(c == EOF){
			curtok = token_eof;
			return;
		}
	}

	if(isdigit(c)){
		char *const num_start = bufferpos - 1;
		enum base mode;

		if(c == '0'){
			switch(tolower(c = peeknextchar())){
				case 'x':
					mode = HEX;
					nextchar();
					break;
				case 'b':
					mode = BIN;
					nextchar();
					break;
				default:
					if(!isoct(c)){
						if(isdigit(c))
							die_at(NULL, "invalid oct character '%c'", c);
						else
							mode = DEC; /* just zero */

						bufferpos--; /* have the zero */
					}else{
						mode = OCT;
					}
					break;
			}
		}else{
			mode = DEC;
			bufferpos--; /* rewind */
		}

		read_number(mode);

#if 0
		if(tolower(peeknextchar()) == 'e'){
			/* 5e2 */
			int n = currentval.val;

			if(!isdigit(peeknextchar())){
				curtok = token_unknown;
				return;
			}
			read_number();

			currentval.val = n * pow(10, currentval.val);
			/* cv = n * 10 ^ cv */
		}
#endif

		if(peeknextchar() == '.'){
			/* floating point */

			currentval.val.f = strtold(num_start, &bufferpos);

			if(peeknextchar() == 'f'){
				currentval.suffix = VAL_FLOAT;
				nextchar();
			}else if(toupper(peeknextchar()) == 'L'){
				currentval.suffix = VAL_LDOUBLE;
				nextchar();
			}else{
				currentval.suffix = VAL_DOUBLE;
			}

			curtok = token_floater;

		}else{
			curtok = token_integer;
		}

		return;
	}

	if(c == '.'){
		curtok = token_dot;

		if(peeknextchar() == '.'){
			nextchar();
			if(peeknextchar() == '.'){
				nextchar();
				curtok = token_elipsis;
			}
			/* else leave it at token_dot and next as token_dot;
			 * parser will get an error */
		}
		return;
	}

	switch(c == 'L' ? peeknextchar() : 0){
		case '"':
			/* wchar_t string */
			nextchar();
			read_string_multiple(1);
			return;
		case '\'':
			nextchar();
			read_char(1);
			return;
	}

	if(isalpha(c) || c == '_' || c == '$'){
		unsigned int len = 1, i;
		char *const start = bufferpos - 1; /* regrab the char we switched on */

		do{ /* allow numbers */
			c = peeknextchar();
			if(isalnum(c) || c == '_' || c == '$'){
				nextchar();
				len++;
			}else
				break;
		}while(1);

		/* check for a built in statement - while, if, etc */
		for(i = 0; i < sizeof(statements) / sizeof(statements[0]); i++)
			if(strlen(statements[i].str) == len && !strncmp(statements[i].str, start, len)){
				curtok = statements[i].tok;
				return;
			}


		/* not found, wap into currentspelling */
		free(currentspelling);
		currentspelling = umalloc(len + 1);

		strncpy(currentspelling, start, len);
		currentspelling[len] = '\0';
		curtok = token_identifier;
		return;
	}

	switch(c){
		case '"':
			read_string_multiple(0);
			break;

		case '\'':
			read_char(0);
			break;

		case '(':
			curtok = token_open_paren;
			break;
		case ')':
			curtok = token_close_paren;
			break;
		case '+':
			if(peeknextchar() == '+'){
				nextchar();
				curtok = token_increment;
			}else{
				curtok = token_plus;
			}
			break;
		case '-':
			switch(peeknextchar()){
				case '-':
					nextchar();
					curtok = token_decrement;
					break;
				case '>':
					nextchar();
					curtok = token_ptr;
					break;

				default:
					curtok = token_minus;
			}
			break;
		case '*':
			curtok = token_multiply;
			break;
		case '/':
			if(peeknextchar() == '*'){
				/* comment */
				for(;;){
					int c = rawnextchar();
					if(c == '*' && *bufferpos == '/'){
						rawnextchar(); /* eat the / */
						nexttoken();
						return;
					}
				}
				die_at(NULL, "No end to comment");
				return;
			}else if(peeknextchar() == '/'){
				tokenise_read_line();
				nexttoken();
				return;
			}
			curtok = token_divide;
			break;
		case '%':
			curtok = token_modulus;
			break;

		case '<':
			if(peeknextchar() == '='){
				nextchar();
				curtok = token_le;
			}else if(peeknextchar() == '<'){
				nextchar();
				curtok = token_shiftl;
			}else{
				curtok = token_lt;
			}
			break;

		case '>':
			if(peeknextchar() == '='){
				nextchar();
				curtok = token_ge;
			}else if(peeknextchar() == '>'){
				nextchar();
				curtok = token_shiftr;
			}else{
				curtok = token_gt;
			}
			break;

		case '=':
			if(peeknextchar() == '='){
				nextchar();
				curtok = token_eq;
			}else
				curtok = token_assign;
			break;

		case '!':
			if(peeknextchar() == '='){
				nextchar();
				curtok = token_ne;
			}else
				curtok = token_not;
			break;

		case '&':
			if(peeknextchar() == '&'){
				nextchar();
				curtok = token_andsc;
			}else
				curtok = token_and;
			break;

		case '|':
			if(peeknextchar() == '|'){
				nextchar();
				curtok = token_orsc;
			}else
				curtok = token_or;
			break;

		case ',':
			curtok = token_comma;
			break;

		case ':':
			curtok = token_colon;
			break;

		case '?':
			curtok = token_question;
			break;

		case ';':
			curtok = token_semicolon;
			break;

		case ']':
			curtok = token_close_square;
			break;

		case '[':
			curtok = token_open_square;
			break;

		case '}':
			curtok = token_close_block;
			break;

		case '{':
			curtok = token_open_block;
			break;

		case '~':
			curtok = token_bnot;
			break;

		case '^':
			curtok = token_xor;
			break;

		default:
			die_at(NULL, "unknown character %c 0x%x %d", c, c, buffereof);
			curtok = token_unknown;
	}

	if(curtok_is_xequal() && peeknextchar() == '='){
		nextchar();
		curtok = curtok_to_xequal(); /* '+' '=' -> "+=" */
	}
}
