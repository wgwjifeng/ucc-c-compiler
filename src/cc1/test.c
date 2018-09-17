#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "../util/where.h"
#include "cc1.h"
#include "cc1_where.h"
#include "out/asm.h"
#include "fopt.h"

enum cc1_backend cc1_backend = BACKEND_ASM;
int cc1_error_limit = 16;
char *cc1_first_fname;
enum debug_level cc1_gdebug = DEBUG_OFF;
int cc1_gdebug_columninfo;
int cc1_mstack_align;
enum c_std cc1_std = STD_C99;
struct cc1_warning cc1_warning;
FILE *cc_out[NUM_SECTIONS];     /* temporary section files */
struct cc1_fopt cc1_fopt;
enum mopt mopt_mode;
struct section sections[NUM_SECTIONS];
int show_current_line;
enum san_opts cc1_sanitize;
char *cc1_sanitize_handler_fn;
enum visibility cc1_visibility_default;
enum stringop_strategy cc1_mstringop_strategy = STRINGOP_STRATEGY_THRESHOLD;
unsigned cc1_mstringop_threshold = 16;

int where_in_sysheader(const where *w)
{
	(void)w;
	return 1;
}

/* ------------ */

#include "type_nav.h"

static int ec;

static void test(int cond, const char *expr)
{
	if(!cond){
		ec = 1;
		fprintf(stderr, "test failed: %s\n", expr);
	}
}
#define test(exp) test((exp), #exp)

static void test_quals(void)
{
	type *tint = type_nav_btype(cc1_type_nav, type_int);
	type *tconstint = type_qualify(tint, qual_const);

	test(tconstint == type_qualify(tconstint, qual_const));
}

int main(void)
{
	cc1_type_nav = type_nav_init();

	test_quals();

	return ec;
}
