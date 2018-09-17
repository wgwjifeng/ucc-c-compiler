/*
 * NOTE: fopts that take a value,
 * e.g. -ferror-limit=...
 * are handled manually in cc1
 */

/* tag_anon_struct_ext = fopt_ms_extensions | fopt_plan9_extensions */
#define FOPT_TAG_ANON_STRUCT_EXT(fopt) \
	((fopt)->ms_extensions || (fopt)->plan9_extensions)

/* frontend language */
X("asm", ext_keywords)
ALIAS("gnu-keywords", ext_keywords)
X("builtin", builtin)
X("freestanding", freestanding)
X("ms-extensions", ms_extensions)
X("plan9-extensions", plan9_extensions)
X("signed-char", signed_char)
INVERT("unsigned-char", signed_char)

/* frontend output */
X("dump-bblocks", dump_basic_blocks)
X("dump-init", dump_init)
X("dump-record-layouts", dump_layouts)
X("dump-symtab", dump_symtab)
X("dump-type-tree", dump_type_tree)
X("english", english)
X("print-aka", print_aka)
X("print-typedefs", print_typedefs)
X("show-inlined", show_inlined)
X("show-line", show_line)
X("show-static-asserts", show_static_asserts)
X("show-warning-option", show_warning_option)
X("track-initial-fname", track_initial_fnam)
X("verbose-asm", verbose_asm)

/* codegen */
X("cast-with-builtin-types", cast_w_builtin_types)
X("common", common)
X("const-fold", const_fold)
X("finite-math-only", finite_math_only)
X("fold-const-vlas", fold_const_vlas)
X("inline-functions", inline_functions)
X("integral-float-load", integral_float_load)
X("leading-underscore", leading_underscore)
X("omit-frame-pointer", omit_frame_pointer)
X("pic", pic)
ALIAS("PIC", pic)
X("pie", pie)
ALIAS("PIE", pie)
X("plt", plt)
X("rounding-math", rounding_math)
X("short-enums", short_enums)
X("symbol-arith", symbol_arith)
X("thread-jumps", thread_jumps)
X("trapv", trapv)
