#ifndef OUT_H
#define OUT_H

void out_pop(void);
void out_pop_func_ret(type_ref *) ucc_nonnull((1));

void out_push_iv(type_ref *, intval *iv) ucc_nonnull((1));
void out_push_i( type_ref *, int i) ucc_nonnull((1));
void out_push_lbl(char *s, int pic);

void out_dup(void); /* duplicate top of stack */
void out_normalise(void); /* change to 0 or 1 */

void out_push_sym(sym *);
void out_push_sym_val(sym *);
void out_store(void); /* store stack[1] into *stack[0] */

void out_op(      enum op_type); /* binary ops and comparisons */
void out_op_unary(enum op_type); /* unary ops */
void out_deref(void);
void out_swap(void);
void out_flush_volatile(void);

void out_cast(type_ref *from, type_ref *to) ucc_nonnull((1, 2));
void out_change_type(type_ref *) ucc_nonnull((1));

void out_call(int nargs, type_ref *rt, type_ref *f) ucc_nonnull((2, 3));

void out_jmp(void); /* jmp to *pop() */
void out_jtrue( const char *);
void out_jfalse(const char *);

void out_func_prologue(decl *); /* push rbp, sub rsp, ... */
void out_func_epilogue(decl *); /* mov rsp, rbp; ret */
void out_label(const char *);

void out_comment(const char *, ...);

void out_assert_vtop_null(void);
void out_dump(void);
void out_undefined(void);

void out_push_frame_ptr(int nframes);

int out_n_call_regs(void);

int out_vcount(void);

#endif
