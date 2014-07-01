#ifndef OUT_SECTIONS_H
#define OUT_SECTIONS_H

#include "forwards.h"

enum section_default
{
	SECTION_DEFAULT_TEXT,
	SECTION_DEFAULT_DATA,
	SECTION_DEFAULT_BSS,
	SECTION_DEFAULT_RODATA,

	SECTION_DEFAULT_DBG_ABBREV,
	SECTION_DEFAULT_DBG_INFO,
	SECTION_DEFAULT_DBG_LINE
};

struct section;

/* name used as .section <lbl>
 * lbl used as Lsection_begin_<name> and Lsection_end_<name>
 */
struct section *out_section_new(
		out_ctx *,
		const char *lbl, const char *name,
		FILE *);

struct section *out_section_get(out_ctx *, const char *name);
struct section *out_section_default(out_ctx *, enum section_default);

#endif
