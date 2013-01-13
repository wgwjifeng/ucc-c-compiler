decl_init *parse_initialisation(void)
{
	decl_init *di;

	if(accept(token_open_block)){
		decl_init **exps = NULL;

		di = decl_init_new(decl_init_brace);

#ifdef DEBUG_DECL_INIT
		fprintf(stderr, "new brace %p\n", (void *)di);
#endif

		while(curtok != token_close_block){
			decl_init *sub;
			struct desig *desig = NULL;

			if(curtok == token_open_square || curtok == token_dot){
				/* parse as many as we need */
				struct desig **plast = &desig;

				do{
					struct desig *d = *plast = umalloc(sizeof *d);
					plast = &d->next;

					if(accept(token_dot)){
						d->type = desig_struct;
						d->bits.name = token_current_spel();
						EAT(token_identifier);

					}else if(accept(token_open_square)){
						d->type = desig_ar;
						d->bits.ar = parse_expr_exp();
						EAT(token_close_square);

					}else{
						ICE("unreachable");
					}
				}while(curtok == token_dot || curtok == token_open_square);

				EAT(token_assign);
			}

			sub = parse_initialisation();
			sub->desig = desig;

			if(desig)
				ICW("TODO: impl of designated initialiser");

			dynarray_add((void ***)&exps, sub);

			if(!accept(token_comma))
				break;
		}

		di->bits.inits = exps;

		EAT(token_close_block);

	}else{
		di = decl_init_new(decl_init_scalar);
		di->bits.expr = parse_expr_no_comma();

#ifdef DEBUG_DECL_INIT
		fprintf(stderr, "new scalar %p\n", (void *)di);
#endif
	}

	return di;
}
