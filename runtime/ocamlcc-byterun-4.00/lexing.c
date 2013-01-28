/***********************************************************************/
/*                                                                     */
/*                                OCaml                                */
/*                                                                     */
/*            Xavier Leroy, projet Cristal, INRIA Rocquencourt         */
/*                                                                     */
/*  Copyright 1996 Institut National de Recherche en Informatique et   */
/*  en Automatique.  All rights reserved.  This file is distributed    */
/*  under the terms of the GNU Library General Public License, with    */
/*  the special exception on linking described in file ../LICENSE.     */
/*                                                                     */
/***********************************************************************/

/* $Id: lexing.c 11156 2011-07-27 14:17:02Z doligez $ */

/* The table-driven automaton for lexers generated by camllex. */

#include "fail.h"
#include "mlvalues.h"
#include "stacks.h"

struct lexer_buffer {
  value refill_buff;
  value lex_buffer;
  value lex_buffer_len;
  value lex_abs_pos;
  value lex_start_pos;
  value lex_curr_pos;
  value lex_last_pos;
  value lex_last_action;
  value lex_eof_reached;
  value lex_mem;
  value lex_start_p;
  value lex_curr_p;
};

struct lexing_table {
  value lex_base;
  value lex_backtrk;
  value lex_default;
  value lex_trans;
  value lex_check;
  value lex_base_code;
  value lex_backtrk_code;
  value lex_default_code;
  value lex_trans_code;
  value lex_check_code;
  value lex_code;
};

#if defined(ARCH_BIG_ENDIAN) || SIZEOF_SHORT != 2
#define Short(tbl,n) \
  (*((unsigned char *)((tbl) + (n) * 2)) + \
          (*((schar *)((tbl) + (n) * 2 + 1)) << 8))
#else
#define Short(tbl,n) (((short *)(tbl))[(n)])
#endif

/* OCamlCC: caml_lex_engine arguments as value (to remove CC warning) */

CAMLprim value old_caml_lex_engine(struct lexing_table *tbl, value start_state,
                                   struct lexer_buffer *lexbuf);

CAMLprim value caml_lex_engine(value tbl, value start_state, value lexbuf){
  return old_caml_lex_engine((struct lexing_table *) tbl, start_state,
                             (struct lexer_buffer *) lexbuf);
}

CAMLprim value old_caml_lex_engine(struct lexing_table *tbl, value start_state,
                                   struct lexer_buffer *lexbuf)
{
  int state, base, backtrk, c;

  state = Int_val(start_state);
  if (state >= 0) {
    /* First entry */
    lexbuf->lex_last_pos = lexbuf->lex_start_pos = lexbuf->lex_curr_pos;
    lexbuf->lex_last_action = Val_int(-1);
  } else {
    /* Reentry after refill */
    state = -state - 1;
  }
  while(1) {
    /* Lookup base address or action number for current state */
    base = Short(tbl->lex_base, state);
    if (base < 0) return Val_int(-base-1);
    /* See if it's a backtrack point */
    backtrk = Short(tbl->lex_backtrk, state);
    if (backtrk >= 0) {
      lexbuf->lex_last_pos = lexbuf->lex_curr_pos;
      lexbuf->lex_last_action = Val_int(backtrk);
    }
    /* See if we need a refill */
    if (lexbuf->lex_curr_pos >= lexbuf->lex_buffer_len){
      if (lexbuf->lex_eof_reached == Val_bool (0)){
        return Val_int(-state - 1);
      }else{
        c = 256;
      }
    }else{
      /* Read next input char */
      c = Byte_u(lexbuf->lex_buffer, Long_val(lexbuf->lex_curr_pos));
      lexbuf->lex_curr_pos += 2;
    }
    /* Determine next state */
    if (Short(tbl->lex_check, base + c) == state)
      state = Short(tbl->lex_trans, base + c);
    else
      state = Short(tbl->lex_default, state);
    /* If no transition on this char, return to last backtrack point */
    if (state < 0) {
      lexbuf->lex_curr_pos = lexbuf->lex_last_pos;
      if (lexbuf->lex_last_action == Val_int(-1)) {
        caml_failwith("lexing: empty token");
      } else {
        return lexbuf->lex_last_action;
      }
    }else{
      /* Erase the EOF condition only if the EOF pseudo-character was
         consumed by the automaton (i.e. there was no backtrack above)
       */
      if (c == 256) lexbuf->lex_eof_reached = Val_bool (0);
    }
  }
}

/***********************************************/
/* New lexer engine, with memory of positions  */
/***********************************************/

static void run_mem(char *pc, value mem, value curr_pos) {
  for (;;) {
    unsigned char dst, src ;

    dst = *pc++ ;
    if (dst == 0xff)
      return ;
    src = *pc++ ;
    if (src == 0xff) {
      /*      fprintf(stderr,"[%hhu] <- %d\n",dst,Int_val(curr_pos)) ;*/
      Field(mem,dst) = curr_pos ;
    } else {
      /*      fprintf(stderr,"[%hhu] <- [%hhu]\n",dst,src) ; */
      Field(mem,dst) = Field(mem,src) ;
    }
  }
}

static void run_tag(char *pc, value mem) {
  for (;;) {
    unsigned char dst, src ;

    dst = *pc++ ;
    if (dst == 0xff)
      return ;
    src = *pc++ ;
    if (src == 0xff) {
      /*      fprintf(stderr,"[%hhu] <- -1\n",dst) ; */
      Field(mem,dst) = Val_int(-1) ;
    } else {
      /*      fprintf(stderr,"[%hhu] <- [%hhu]\n",dst,src) ; */
      Field(mem,dst) = Field(mem,src) ;
    }
  }
}

/* OCamlCC: caml_new_lex_engine arguments as value (to remove CC warning) */

CAMLprim value old_caml_new_lex_engine(struct lexing_table *tbl,
                                       value start_state,
                                       struct lexer_buffer *lexbuf);

CAMLprim value caml_new_lex_engine(value tbl, value start_state, value lexbuf){
  return old_caml_new_lex_engine((struct lexing_table *) tbl, start_state,
                                 (struct lexer_buffer *) lexbuf);
}

CAMLprim value old_caml_new_lex_engine(struct lexing_table *tbl,
                                       value start_state,
                                       struct lexer_buffer *lexbuf)
{
  int state, base, backtrk, c, pstate ;
  state = Int_val(start_state);
  if (state >= 0) {
    /* First entry */
    lexbuf->lex_last_pos = lexbuf->lex_start_pos = lexbuf->lex_curr_pos;
    lexbuf->lex_last_action = Val_int(-1);
  } else {
    /* Reentry after refill */
    state = -state - 1;
  }
  while(1) {
    /* Lookup base address or action number for current state */
    base = Short(tbl->lex_base, state);
    if (base < 0) {
      int pc_off = Short(tbl->lex_base_code, state) ;
      run_tag(Bp_val(tbl->lex_code) + pc_off, lexbuf->lex_mem);
      /*      fprintf(stderr,"Perform: %d\n",-base-1) ; */
      return Val_int(-base-1);
    }
    /* See if it's a backtrack point */
    backtrk = Short(tbl->lex_backtrk, state);
    if (backtrk >= 0) {
      int pc_off =  Short(tbl->lex_backtrk_code, state);
      run_tag(Bp_val(tbl->lex_code) + pc_off, lexbuf->lex_mem);
      lexbuf->lex_last_pos = lexbuf->lex_curr_pos;
      lexbuf->lex_last_action = Val_int(backtrk);

    }
    /* See if we need a refill */
    if (lexbuf->lex_curr_pos >= lexbuf->lex_buffer_len){
      if (lexbuf->lex_eof_reached == Val_bool (0)){
        return Val_int(-state - 1);
      }else{
        c = 256;
      }
    }else{
      /* Read next input char */
      c = Byte_u(lexbuf->lex_buffer, Long_val(lexbuf->lex_curr_pos));
      lexbuf->lex_curr_pos += 2;
    }
    /* Determine next state */
    pstate=state ;
    if (Short(tbl->lex_check, base + c) == state)
      state = Short(tbl->lex_trans, base + c);
    else
      state = Short(tbl->lex_default, state);
    /* If no transition on this char, return to last backtrack point */
    if (state < 0) {
      lexbuf->lex_curr_pos = lexbuf->lex_last_pos;
      if (lexbuf->lex_last_action == Val_int(-1)) {
        caml_failwith("lexing: empty token");
      } else {
        return lexbuf->lex_last_action;
      }
    }else{
      /* If some transition, get and perform memory moves */
      int base_code = Short(tbl->lex_base_code, pstate) ;
      int pc_off ;
      if (Short(tbl->lex_check_code, base_code + c) == pstate)
        pc_off = Short(tbl->lex_trans_code, base_code + c) ;
      else
        pc_off = Short(tbl->lex_default_code, pstate) ;
      if (pc_off > 0)
        run_mem(Bp_val(tbl->lex_code) + pc_off, lexbuf->lex_mem, lexbuf->lex_curr_pos) ;
      /* Erase the EOF condition only if the EOF pseudo-character was
         consumed by the automaton (i.e. there was no backtrack above)
       */
      if (c == 256) lexbuf->lex_eof_reached = Val_bool (0);
    }
  }
}

/* OCamlCC: undef Short (conflicts with parsing.c) */

#undef Short
