#include <stdio.h>
#include <assert.h>

#include "../../util/dynmap.h"

#include "../as_cfg.h"
#include "macros.h"

#include "ctx.h"

static struct
{
	const char *desc, *lbl;
} default_sections[] = {
	{ "text", QUOTE(SECTION_NAME_TEXT) },
	{ "data", QUOTE(SECTION_NAME_DATA) },
	{ "bss",  QUOTE(SECTION_NAME_BSS) },
	{ "rodata", QUOTE(SECTION_NAME_RODATA) },
	{ "dbg_abrv", QUOTE(SECTION_NAME_DBG_ABBREV) },
	{ "dbg_info", QUOTE(SECTION_NAME_DBG_INFO) },
	{ "dbg_line", QUOTE(SECTION_NAME_DBG_LINE) },
};

typedef struct section sect;
struct section
{
	char *name, *lbl;
	FILE *f;
};

struct section *out_section_new(
		out_ctx *octx,
		const char *name, const char *lbl,
		FILE *file)
{
	sect *section, *prev;

	if(!octx->sections)
		octx->sections = dynmap_new(char *, strcmp, dynmap_strhash);

	sect = dynmap_value(const char *, sect *, octx->sections, name);
	if(sect)
		return sect;

	sect = umalloc(sizeof *sect);
	sect->name = ustrdup(name);
	sect->lbl = ustrdup(name);
	sect->f = file;

	prev = dynmap_set(char *, sect *, octx->sections, sect->name, sect);
	assert(!prev);

	return sect;
}

struct section *out_section_get(out_ctx *octx, const char *name)
{
	return dynmap_value(const char *, sect *, octx->sections, name);
}

struct section *out_section_default(out_ctx *octx, enum section_default d)
{

}
