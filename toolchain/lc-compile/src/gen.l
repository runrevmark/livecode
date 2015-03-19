%{
#if defined(__GNUC__) && defined(__APPLE__)
#  ifdef BOOTSTRAP
#    include "gen-bootstrap.h"
#  else
#    include "gen-full.h"
#  endif
#else
#  include "gen.h"
#endif
extern YYSTYPE yylval;
#include "position.h"
#define yysetpos() { GetCurrentPosition(&yylval.attr[0]); AdvanceCurrentPosition(yyleng); }
#include "literal.h"
#include "report.h"
%}
%s SPECIAL
%x COMMENT
%%
