#include "system.h"

// NeuralC - Tiny C Compiler/Interpreter for NeuralOS
// Based on 'c4' by Robert Swierczek (C in 4 functions)
// Adapted for bare-metal OS context (no stdlib, simple print mapping)

char *nc_p, *nc_lp, *nc_data;
int *nc_e, *nc_le, *nc_id, *nc_sym;
int nc_tk, nc_ival, nc_ty, nc_loc, nc_line, nc_src, nc_debug, nc_error;

enum {
  Num = 128,
  Fun,
  Sys,
  Glo,
  Loc,
  Id,
  Char,
  Else,
  Enum,
  If,
  Int,
  Return,
  Sizeof,
  While,
  Assign,
  Cond,
  Lor,
  Lan,
  Or,
  Xor,
  And,
  Eq,
  Ne,
  Lt,
  Gt,
  Le,
  Ge,
  Shl,
  Shr,
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Inc,
  Dec,
  Brak
};

enum {
  LEA,
  IMM,
  JMP,
  JSR,
  BZ,
  BNZ,
  ENT,
  ADJ,
  LEV,
  LI,
  LC,
  SI,
  SC,
  PSH,
  OR,
  XOR,
  AND,
  EQ,
  NE,
  LT,
  GT,
  LE,
  GE,
  SHL,
  SHR,
  ADD,
  SUB,
  MUL,
  DIV,
  MOD,
  SYS_PRINTSTR,
  SYS_PRINTNUM,
  SYS_MALLOC,
  SYS_MSET,
  SYS_MCMP,
  SYS_GETCHAR,
  EXIT
};

enum { CHAR, INT, PTR };

enum { Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, Idsz };

static void next(void) {
  char *pp;
  while ((nc_tk = *nc_p)) {
    ++nc_p;
    if (nc_tk == '\n') {
      ++nc_line;
    } else if (nc_tk == '#') {
      while (*nc_p != 0 && *nc_p != '\n')
        ++nc_p;
    } else if ((nc_tk >= 'a' && nc_tk <= 'z') ||
               (nc_tk >= 'A' && nc_tk <= 'Z') || nc_tk == '_') {
      pp = (char *)nc_p - 1;
      while ((*nc_p >= 'a' && *nc_p <= 'z') || (*nc_p >= 'A' && *nc_p <= 'Z') ||
             (*nc_p >= '0' && *nc_p <= '9') || *nc_p == '_')
        nc_tk = nc_tk * 147 + *nc_p++;
      nc_tk = (nc_tk << 6) + (nc_p - pp);
      nc_id = nc_sym;
      while (nc_id[Tk]) {
        if (nc_tk == nc_id[Hash]) {
          int match = 1;
          char *id_name = (char *)nc_id[Name];
          int len = (char *)nc_p - pp;
          for (int j = 0; j < len; j++) {
            if (id_name[j] != pp[j])
              match = 0;
          }
          if (match) {
            nc_tk = nc_id[Tk];
            return;
          }
        }
        nc_id = nc_id + Idsz;
      }
      nc_id[Name] = (int)pp;
      nc_id[Hash] = nc_tk;
      nc_tk = nc_id[Tk] = Id;
      return;
    } else if (nc_tk >= '0' && nc_tk <= '9') {
      if ((nc_ival = nc_tk - '0')) {
        while (*nc_p >= '0' && *nc_p <= '9')
          nc_ival = nc_ival * 10 + *nc_p++ - '0';
      } else if (*nc_p == 'x' || *nc_p == 'X') {
        while ((nc_tk = *++nc_p) && ((nc_tk >= '0' && nc_tk <= '9') ||
                                     (nc_tk >= 'a' && nc_tk <= 'f') ||
                                     (nc_tk >= 'A' && nc_tk <= 'F')))
          nc_ival = nc_ival * 16 + (nc_tk & 15) + (nc_tk >= 'A' ? 9 : 0);
      } else {
        while (*nc_p >= '0' && *nc_p <= '7')
          nc_ival = nc_ival * 8 + *nc_p++ - '0';
      }
      nc_tk = Num;
      return;
    } else if (nc_tk == '/') {
      if (*nc_p == '/') {
        ++nc_p;
        while (*nc_p != 0 && *nc_p != '\n')
          ++nc_p;
      } else {
        nc_tk = Div;
        return;
      }
    } else if (nc_tk == '\'' || nc_tk == '"') {
      pp = (char *)nc_data;
      while (*nc_p != 0 && *nc_p != nc_tk) {
        if ((nc_ival = *nc_p++) == '\\') {
          if ((nc_ival = *nc_p++) == 'n')
            nc_ival = '\n';
        }
        if (nc_tk == '"')
          *((char *)nc_data++) = nc_ival;
      }
      ++nc_p;
      if (nc_tk == '"')
        nc_ival = (int)pp;
      else
        nc_tk = Num;
      return;
    } else if (nc_tk == '=') {
      if (*nc_p == '=') {
        ++nc_p;
        nc_tk = Eq;
      } else
        nc_tk = Assign;
      return;
    } else if (nc_tk == '+') {
      if (*nc_p == '+') {
        ++nc_p;
        nc_tk = Inc;
      } else
        nc_tk = Add;
      return;
    } else if (nc_tk == '-') {
      if (*nc_p == '-') {
        ++nc_p;
        nc_tk = Dec;
      } else
        nc_tk = Sub;
      return;
    } else if (nc_tk == '!') {
      if (*nc_p == '=') {
        ++nc_p;
        nc_tk = Ne;
      }
      return;
    } else if (nc_tk == '<') {
      if (*nc_p == '=') {
        ++nc_p;
        nc_tk = Le;
      } else if (*nc_p == '<') {
        ++nc_p;
        nc_tk = Shl;
      } else
        nc_tk = Lt;
      return;
    } else if (nc_tk == '>') {
      if (*nc_p == '=') {
        ++nc_p;
        nc_tk = Ge;
      } else if (*nc_p == '>') {
        ++nc_p;
        nc_tk = Shr;
      } else
        nc_tk = Gt;
      return;
    } else if (nc_tk == '|') {
      if (*nc_p == '|') {
        ++nc_p;
        nc_tk = Lor;
      } else
        nc_tk = Or;
      return;
    } else if (nc_tk == '&') {
      if (*nc_p == '&') {
        ++nc_p;
        nc_tk = Lan;
      } else
        nc_tk = And;
      return;
    } else if (nc_tk == '^') {
      nc_tk = Xor;
      return;
    } else if (nc_tk == '%') {
      nc_tk = Mod;
      return;
    } else if (nc_tk == '*') {
      nc_tk = Mul;
      return;
    } else if (nc_tk == '[') {
      nc_tk = Brak;
      return;
    } else if (nc_tk == '?') {
      nc_tk = Cond;
      return;
    } else if (nc_tk == '~' || nc_tk == ';' || nc_tk == '{' || nc_tk == '}' ||
               nc_tk == '(' || nc_tk == ')' || nc_tk == ']' || nc_tk == ',' ||
               nc_tk == ':')
      return;
  }
}

static void expr(int lev) {
  int t, *d;
  if (nc_error)
    return;
  if (!nc_tk) {
    print_string("Error: eof in exp\n", 0x0C);
    nc_error = 1;
    return;
  } else if (nc_tk == Num) {
    *++nc_e = IMM;
    *++nc_e = nc_ival;
    next();
    nc_ty = INT;
  } else if (nc_tk == '"') {
    *++nc_e = IMM;
    *++nc_e = nc_ival;
    next();
    while (nc_tk == '"')
      next();
    nc_data = (char *)(((int)nc_data + sizeof(int)) & ~(sizeof(int) - 1));
    nc_ty = PTR;
  } else if (nc_tk == Sizeof) {
    next();
    if (nc_tk == '(')
      next();
    nc_ty = INT;
    if (nc_tk == Int)
      next();
    else if (nc_tk == Char) {
      next();
      nc_ty = CHAR;
    }
    while (nc_tk == Mul) {
      next();
      nc_ty = nc_ty + PTR;
    }
    if (nc_tk == ')')
      next();
    *++nc_e = IMM;
    *++nc_e = (nc_ty == CHAR) ? sizeof(char) : sizeof(int);
    nc_ty = INT;
  } else if (nc_tk == Id) {
    d = nc_id;
    next();
    if (nc_tk == '(') {
      next();
      t = 0;
      while (nc_tk != ')') {
        expr(Assign);
        if (nc_error)
          break;
        *++nc_e = PSH;
        ++t;
        if (nc_tk == ',')
          next();
      }
      next();
      if (d[Class] == Sys)
        *++nc_e = d[Val];
      else if (d[Class] == Fun) {
        *++nc_e = JSR;
        *++nc_e = d[Val];
      }
      if (t) {
        *++nc_e = ADJ;
        *++nc_e = t;
      }
      nc_ty = d[Type];
    } else if (d[Class] == Num) {
      *++nc_e = IMM;
      *++nc_e = d[Val];
      nc_ty = INT;
    } else {
      if (d[Class] == Loc) {
        *++nc_e = LEA;
        *++nc_e = nc_loc - d[Val];
      } else if (d[Class] == Glo) {
        *++nc_e = IMM;
        *++nc_e = d[Val];
      }
      *++nc_e = ((nc_ty = d[Type]) == CHAR) ? LC : LI;
    }
  } else if (nc_tk == '(') {
    next();
    if (nc_tk == Int || nc_tk == Char) {
      t = (nc_tk == Int) ? INT : CHAR;
      next();
      while (nc_tk == Mul) {
        next();
        t = t + PTR;
      }
      if (nc_tk == ')')
        next();
      expr(Inc);
      nc_ty = t;
    } else {
      expr(Assign);
      if (nc_tk == ')')
        next();
    }
  } else if (nc_tk == Mul) {
    next();
    expr(Inc);
    if (nc_ty > INT)
      nc_ty = nc_ty - PTR;
    *++nc_e = (nc_ty == CHAR) ? LC : LI;
  } else if (nc_tk == And) {
    next();
    expr(Inc);
    if (*nc_e == LC || *nc_e == LI)
      --nc_e;
    nc_ty = nc_ty + PTR;
  } else if (nc_tk == '!') {
    next();
    expr(Inc);
    *++nc_e = PSH;
    *++nc_e = IMM;
    *++nc_e = 0;
    *++nc_e = EQ;
    nc_ty = INT;
  } else if (nc_tk == '~') {
    next();
    expr(Inc);
    *++nc_e = PSH;
    *++nc_e = IMM;
    *++nc_e = -1;
    *++nc_e = XOR;
    nc_ty = INT;
  } else if (nc_tk == Add) {
    next();
    expr(Inc);
    nc_ty = INT;
  } else if (nc_tk == Sub) {
    next();
    *++nc_e = IMM;
    if (nc_tk == Num) {
      *++nc_e = -nc_ival;
      next();
    } else {
      *++nc_e = -1;
      *++nc_e = PSH;
      expr(Inc);
      *++nc_e = MUL;
    }
    nc_ty = INT;
  } else if (nc_tk == Inc || nc_tk == Dec) {
    t = nc_tk;
    next();
    expr(Inc);
    if (*nc_e == LC) {
      *nc_e = PSH;
      *++nc_e = LC;
    } else if (*nc_e == LI) {
      *nc_e = PSH;
      *++nc_e = LI;
    }
    *++nc_e = PSH;
    *++nc_e = IMM;
    *++nc_e = (nc_ty > PTR) ? sizeof(int) : sizeof(char);
    *++nc_e = (t == Inc) ? ADD : SUB;
    *++nc_e = (nc_ty == CHAR) ? SC : SI;
  } else {
    print_string("Error: Unexpected token '", 0x0C);
    print_char(nc_tk < 128 ? nc_tk : '?', 0x0C);
    print_string("'\n", 0x0C);
    nc_error = 1;
    return;
  }

  while (nc_tk >= lev) {
    t = nc_ty;
    if (nc_tk == Assign) {
      next();
      if (*nc_e == LC || *nc_e == LI)
        *nc_e = PSH;
      expr(Assign);
      *++nc_e = ((nc_ty = t) == CHAR) ? SC : SI;
    } else if (nc_tk == Cond) {
      next();
      *++nc_e = BZ;
      d = ++nc_e;
      expr(Assign);
      if (nc_tk == ':')
        next();
      *d = (int)(nc_e + 3);
      *++nc_e = JMP;
      d = ++nc_e;
      expr(Cond);
      if (nc_error)
        break;
      *d = (int)(nc_e + 1);
    } else if (nc_tk == Lor) {
      next();
      *++nc_e = BNZ;
      d = ++nc_e;
      expr(Lan);
      *d = (int)(nc_e + 1);
      nc_ty = INT;
    } else if (nc_tk == Lan) {
      next();
      *++nc_e = BZ;
      d = ++nc_e;
      expr(Or);
      *d = (int)(nc_e + 1);
      nc_ty = INT;
    } else if (nc_tk == Or) {
      next();
      *++nc_e = PSH;
      expr(Xor);
      *++nc_e = OR;
      nc_ty = INT;
    } else if (nc_tk == Xor) {
      next();
      *++nc_e = PSH;
      expr(And);
      *++nc_e = XOR;
      nc_ty = INT;
    } else if (nc_tk == And) {
      next();
      *++nc_e = PSH;
      expr(Eq);
      *++nc_e = AND;
      nc_ty = INT;
    } else if (nc_tk == Eq) {
      next();
      *++nc_e = PSH;
      expr(Lt);
      *++nc_e = EQ;
      nc_ty = INT;
    } else if (nc_tk == Ne) {
      next();
      *++nc_e = PSH;
      expr(Lt);
      *++nc_e = NE;
      nc_ty = INT;
    } else if (nc_tk == Lt) {
      next();
      *++nc_e = PSH;
      expr(Shl);
      *++nc_e = LT;
      nc_ty = INT;
    } else if (nc_tk == Gt) {
      next();
      *++nc_e = PSH;
      expr(Shl);
      *++nc_e = GT;
      nc_ty = INT;
    } else if (nc_tk == Le) {
      next();
      *++nc_e = PSH;
      expr(Shl);
      *++nc_e = LE;
      nc_ty = INT;
    } else if (nc_tk == Ge) {
      next();
      *++nc_e = PSH;
      expr(Shl);
      *++nc_e = GE;
      nc_ty = INT;
    } else if (nc_tk == Shl) {
      next();
      *++nc_e = PSH;
      expr(Add);
      *++nc_e = SHL;
      nc_ty = INT;
    } else if (nc_tk == Shr) {
      next();
      *++nc_e = PSH;
      expr(Add);
      *++nc_e = SHR;
      nc_ty = INT;
    } else if (nc_tk == Add) {
      next();
      *++nc_e = PSH;
      expr(Mul);
      if ((nc_ty = t) > PTR) {
        *++nc_e = PSH;
        *++nc_e = IMM;
        *++nc_e = sizeof(int);
        *++nc_e = MUL;
      }
      *++nc_e = ADD;
    } else if (nc_tk == Sub) {
      next();
      *++nc_e = PSH;
      expr(Mul);
      if (t > PTR && t == nc_ty) {
        *++nc_e = SUB;
        *++nc_e = PSH;
        *++nc_e = IMM;
        *++nc_e = sizeof(int);
        *++nc_e = DIV;
        nc_ty = INT;
      } else if ((nc_ty = t) > PTR) {
        *++nc_e = PSH;
        *++nc_e = IMM;
        *++nc_e = sizeof(int);
        *++nc_e = MUL;
        *++nc_e = SUB;
      } else
        *++nc_e = SUB;
    } else if (nc_tk == Mul) {
      next();
      *++nc_e = PSH;
      expr(Inc);
      *++nc_e = MUL;
      nc_ty = INT;
    } else if (nc_tk == Div) {
      next();
      *++nc_e = PSH;
      expr(Inc);
      *++nc_e = DIV;
      nc_ty = INT;
    } else if (nc_tk == Mod) {
      next();
      *++nc_e = PSH;
      expr(Inc);
      *++nc_e = MOD;
      nc_ty = INT;
    } else if (nc_tk == Inc || nc_tk == Dec) {
      if (*nc_e == LC) {
        *nc_e = PSH;
        *++nc_e = LC;
      } else if (*nc_e == LI) {
        *nc_e = PSH;
        *++nc_e = LI;
      }
      *++nc_e = PSH;
      *++nc_e = IMM;
      *++nc_e = (nc_ty > PTR) ? sizeof(int) : sizeof(char);
      *++nc_e = (nc_tk == Inc) ? ADD : SUB;
      *++nc_e = (nc_ty == CHAR) ? SC : SI;
      *++nc_e = PSH;
      *++nc_e = IMM;
      *++nc_e = (nc_ty > PTR) ? sizeof(int) : sizeof(char);
      *++nc_e = (nc_tk == Inc) ? SUB : ADD;
      next();
    } else if (nc_tk == Brak) {
      next();
      *++nc_e = PSH;
      expr(Assign);
      if (nc_tk == ']')
        next();
      if (t > PTR) {
        *++nc_e = PSH;
        *++nc_e = IMM;
        *++nc_e = sizeof(int);
        *++nc_e = MUL;
      }
      *++nc_e = ADD;
      *++nc_e = ((nc_ty = t - PTR) == CHAR) ? LC : LI;
    } else {
      print_string("Error: Compiler error\n", 0x0C);
      nc_error = 1;
      return;
    }
  }
}

static void stmt() {
  int *a, *b;
  if (nc_error)
    return;
  if (nc_tk == If) {
    next();
    if (nc_tk == '(')
      next();
    expr(Assign);
    if (nc_tk == ')')
      next();
    *++nc_e = BZ;
    b = ++nc_e;
    stmt();
    if (nc_tk == Else) {
      *b = (int)(nc_e + 3);
      *++nc_e = JMP;
      b = ++nc_e;
      next();
      stmt();
    }
    *b = (int)(nc_e + 1);
  } else if (nc_tk == While) {
    next();
    a = nc_e + 1;
    if (nc_tk == '(')
      next();
    expr(Assign);
    if (nc_tk == ')')
      next();
    *++nc_e = BZ;
    b = ++nc_e;
    stmt();
    *++nc_e = JMP;
    *++nc_e = (int)a;
    *b = (int)(nc_e + 1);
  } else if (nc_tk == Return) {
    next();
    if (nc_tk != ';')
      expr(Assign);
    *++nc_e = LEV;
    if (nc_tk == ';')
      next();
  } else if (nc_tk == '{') {
    next();
    while (nc_tk != '}' && nc_tk != 0) {
      stmt();
      if (nc_error)
        break;
    }
    next();
  } else if (nc_tk == ';') {
    next();
  } else {
    expr(Assign);
    if (nc_tk == ';')
      next();
  }
}

// Global VM pools
static int *sym_pool, *text_pool, *data_pool, *stack_pool;
static int pools_init = 0;

void run_neuralc(const char *source) {
  int bt, i, *idmain;
  int *pc, *sp, *bp, a = 0;

  int poolsz = 128 * 1024; // 128KB pools

  if (!pools_init) {
    sym_pool = mem_alloc(poolsz);
    text_pool = mem_alloc(poolsz);
    data_pool = mem_alloc(poolsz);
    stack_pool = mem_alloc(poolsz);
    pools_init = 1;
  }

  nc_sym = sym_pool;
  nc_e = text_pool;
  nc_data = (char *)data_pool;
  sp = stack_pool;
  memset(nc_sym, 0, poolsz);
  memset(nc_e, 0, poolsz);
  memset(nc_data, 0, poolsz);
  nc_error = 0;

  // Setup keywords
  const char *kwd =
      "char else enum if int return sizeof while "
      "print_string print_number mem_alloc memset string_compare get_char "
      "exit void main";
  nc_p = (char *)kwd;
  i = Char;
  while (i <= While) {
    next();
    nc_id[Tk] = i++;
  }
  i = SYS_PRINTSTR;
  while (i <= EXIT) {
    next();
    nc_id[Class] = Sys;
    nc_id[Type] = INT;
    nc_id[Val] = i++;
  }
  next();
  nc_id[Tk] = Char;
  next();
  idmain = nc_id; // main

  // Parse source
  nc_p = (char *)source;
  nc_line = 1;
  next();

  while (nc_tk) {
    bt = INT;
    if (nc_tk == Int)
      next();
    else if (nc_tk == Char) {
      next();
      bt = CHAR;
    } else if (nc_tk == Enum) {
      next();
      if (nc_tk == '{') {
        next();
        i = 0;
        while (nc_tk != '}' && nc_tk != 0) {
          next();
          if (nc_tk == Assign) {
            next();
            i = nc_ival;
            next();
          }
          nc_id[Class] = Num;
          nc_id[Type] = INT;
          nc_id[Val] = i++;
          if (nc_tk == ',')
            next();
        }
        next();
      }
    }
    while (nc_tk != ';' && nc_tk != '}' && nc_tk != 0) {
      nc_ty = bt;
      while (nc_tk == Mul) {
        next();
        nc_ty = nc_ty + PTR;
      }
      next();
      nc_id[Type] = nc_ty;
      if (nc_tk == '(') {
        nc_id[Class] = Fun;
        nc_id[Val] = (int)(nc_e + 1);
        next();
        i = 0;
        while (nc_tk != ')' && nc_tk != 0) {
          nc_ty = INT;
          if (nc_tk == Int)
            next();
          else if (nc_tk == Char) {
            next();
            nc_ty = CHAR;
          }
          while (nc_tk == Mul) {
            next();
            nc_ty = nc_ty + PTR;
          }
          nc_id[HClass] = nc_id[Class];
          nc_id[Class] = Loc;
          nc_id[HType] = nc_id[Type];
          nc_id[Type] = nc_ty;
          nc_id[HVal] = nc_id[Val];
          nc_id[Val] = i++;
          next();
          if (nc_tk == ',')
            next();
        }
        next();
        if (nc_tk == '{') {
          nc_loc = ++i;
          next();
          while (nc_tk == Int || nc_tk == Char) {
            bt = (nc_tk == Int) ? INT : CHAR;
            next();
            while (nc_tk != ';' && nc_tk != 0) {
              nc_ty = bt;
              while (nc_tk == Mul) {
                next();
                nc_ty = nc_ty + PTR;
              }
              nc_id[HClass] = nc_id[Class];
              nc_id[Class] = Loc;
              nc_id[HType] = nc_id[Type];
              nc_id[Type] = nc_ty;
              nc_id[HVal] = nc_id[Val];
              nc_id[Val] = ++i;
              next();
              if (nc_tk == ',')
                next();
            }
            next();
          }
          *++nc_e = ENT;
          *++nc_e = i - nc_loc;
          while (nc_tk != '}' && nc_tk != 0) {
            stmt();
            if (nc_error)
              break;
          }
          *++nc_e = LEV;
          nc_id = nc_sym;
          while (nc_id[Tk]) {
            if (nc_id[Class] == Loc) {
              nc_id[Class] = nc_id[HClass];
              nc_id[Type] = nc_id[HType];
              nc_id[Val] = nc_id[HVal];
            }
            nc_id = nc_id + Idsz;
          }
        }
      } else {
        nc_id[Class] = Glo;
        nc_id[Val] = (int)nc_data;
        nc_data = (char *)((int)nc_data + sizeof(int));
      }
      if (nc_tk == ',')
        next();
    }
    next();
  }

  if (nc_error) {
    print_string("Compilation Aborted due to syntax errors.\n", 0x0C);
    return;
  }

  if (!(pc = (int *)idmain[Val])) {
    print_string("Error: main() not defined\n", 0x0C);
    return;
  }

  bp = sp = (int *)((int)stack_pool + poolsz);
  *--sp = EXIT;
  *--sp = PSH;
  int *t_sp = sp;
  *--sp = 0; // argc
  *--sp = 0; // argv
  *--sp = (int)t_sp;

  print_string("[TCC] Bytecode assembled. Executing...\n", 0x0D);

  // VM loop
  while (1) {
    i = *pc++;
    if (i == LEA)
      a = (int)(bp + *pc++);
    else if (i == IMM)
      a = *pc++;
    else if (i == JMP)
      pc = (int *)*pc;
    else if (i == JSR) {
      *--sp = (int)(pc + 1);
      pc = (int *)*pc;
    } else if (i == BZ)
      pc = a ? pc + 1 : (int *)*pc;
    else if (i == BNZ)
      pc = a ? (int *)*pc : pc + 1;
    else if (i == ENT) {
      *--sp = (int)bp;
      bp = sp;
      sp = sp - *pc++;
    } else if (i == ADJ)
      sp = sp + *pc++;
    else if (i == LEV) {
      sp = bp;
      bp = (int *)*sp++;
      pc = (int *)*sp++;
    } else if (i == LI)
      a = *(int *)a;
    else if (i == LC)
      a = *(char *)a;
    else if (i == SI)
      *(int *)*sp++ = a;
    else if (i == SC)
      a = *(char *)*sp++ = a;
    else if (i == PSH)
      *--sp = a;
    else if (i == OR)
      a = *sp++ | a;
    else if (i == XOR)
      a = *sp++ ^ a;
    else if (i == AND)
      a = *sp++ & a;
    else if (i == EQ)
      a = *sp++ == a;
    else if (i == NE)
      a = *sp++ != a;
    else if (i == LT)
      a = *sp++ < a;
    else if (i == GT)
      a = *sp++ > a;
    else if (i == LE)
      a = *sp++ <= a;
    else if (i == GE)
      a = *sp++ >= a;
    else if (i == SHL)
      a = *sp++ << a;
    else if (i == SHR)
      a = *sp++ >> a;
    else if (i == ADD)
      a = *sp++ + a;
    else if (i == SUB)
      a = *sp++ - a;
    else if (i == MUL)
      a = *sp++ * a;
    else if (i == DIV)
      a = *sp++ / a;
    else if (i == MOD)
      a = *sp++ % a;

    // System mappings
    else if (i == SYS_PRINTSTR) {
      print_string((char *)sp[1], sp[0]);
      a = 0;
    } else if (i == SYS_PRINTNUM) {
      print_number(sp[1], sp[0]);
      a = 0;
    } else if (i == SYS_MALLOC) {
      a = (int)mem_alloc(*sp);
    } else if (i == SYS_MSET) {
      a = (int)memset((void *)sp[2], sp[1], *sp);
    } else if (i == SYS_MCMP) {
      a = string_compare((char *)sp[2], (char *)sp[1]);
    } else if (i == SYS_GETCHAR) {
      char kc = 0;
      while (kc == 0) {
        kc = keyboard_poll_char();
      }
      char tmp_str[2];
      tmp_str[0] = kc;
      tmp_str[1] = '\0';
      print_string(tmp_str, 0x0F); // Echo ke layar
      a = kc;
    } else if (i == EXIT) {
      print_string("[TCC] Program return.\n", 0x0A);
      return;
    } else {
      print_string("TCC Fatal Error. Unknown Opcode: ", 0x0C);
      print_number(i, 0x0C);
      print_string("\n", 0x0C);
      return;
    }
  }
}
