#include <string.h>

#include "visibility.h"
#include "../config_as.h"

int visibility_parse(enum visibility *const e, const char *s)
{
	if(!strcmp(s, "default"))
		*e = VISIBILITY_DEFAULT;
	else if(!strcmp(s, "hidden"))
		*e = VISIBILITY_HIDDEN;
	else if(!strcmp(s, "protected") && AS_SUPPORTS_VISIBILITY_PROTECTED)
		*e = VISIBILITY_PROTECTED;
	else
		return 0;

	return 1;
}

const char *visibility_to_str(enum visibility v)
{
	switch(v){
		case VISIBILITY_DEFAULT: return "default";
		case VISIBILITY_HIDDEN: return "hidden";
		case VISIBILITY_PROTECTED: return "protected";
	}
	return NULL;
}
