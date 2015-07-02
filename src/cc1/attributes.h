#ifndef ATTRIBUTES_H
#define ATTRIBUTES_H

enum attribute_category
{
	attribute_cat_type_funconly = 1 << 0,
	attribute_cat_type_enumonly = 1 << 1,
	attribute_cat_type_ptronly = 1 << 2,
	attribute_cat_type_structonly = 1 << 3,

	attribute_cat_type =
		attribute_cat_type_funconly |
		attribute_cat_type_enumonly |
		1 << 4,

	attribute_cat_decl_funconly = 1 << 5,
	attribute_cat_decl_varonly = 1 << 6,

	attribute_cat_decl =
		attribute_cat_decl_funconly |
		1 << 7,

	attribute_cat_label = 1 << 8,

	attribute_cat_any
};

#define ATTRIBUTES                                                                                    \
		NAME(format, attribute_cat_type_funconly)                                                         \
		NAME(unused, attribute_cat_decl | attribute_cat_label)                                            \
		NAME(warn_unused, attribute_cat_type_funconly)                                                    \
		NAME(section, attribute_cat_decl)                                                                 \
		NAME(enum_bitmask, attribute_cat_type_enumonly)                                                   \
		NAME(noreturn, attribute_cat_type_funconly)                                                       \
		NAME(noderef, attribute_cat_type_ptronly)                                                         \
		NAME(nonnull, attribute_cat_type_funconly | attribute_cat_type_ptronly)                           \
		NAME(packed, attribute_cat_type_structonly)                                                       \
		NAME(sentinel, attribute_cat_type_funconly)                                                       \
		NAME(aligned, attribute_cat_decl_varonly)                                                         \
		NAME(weak, attribute_cat_decl)                                                                    \
		NAME(cleanup, attribute_cat_decl_varonly)                                                         \
		NAME(always_inline, attribute_cat_decl_funconly)                                                  \
		NAME(noinline, attribute_cat_decl_funconly)                                                       \
		ALIAS("designated_init", desig_init, attribute_cat_type_structonly)                               \
		ALIAS("__ucc_debug", ucc_debug, attribute_cat_any) /* logs out a message when handled */          \
		ALIAS("__cdecl", call_conv, attribute_cat_type_funconly)                                          \
		EXTRA_ALIAS("cdecl", call_conv, attribute_cat_type_funconly)                                      \
		EXTRA_ALIAS("stdcall", call_conv, attribute_cat_type_funconly)                                    \
		EXTRA_ALIAS("fastcall", call_conv, attribute_cat_type_funconly)                                   \
		EXTRA_ALIAS("warn_unused_result", warn_unused, attribute_cat_type_funconly | attribute_cat_label)

#endif
