%{
// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "basic-block.h"
#include "calc.h"
#include "c-code.h"

int yylex(void);					// Will be generated in lex.yy.c by flex

// Following are defined below in sub-routines section
void my_free(char *ptr);
char* lc(char *str);
void gen_tac_assign(char *var, char *expr);
char* gen_tac_expr(char *one, char *op, char *three);
void gen_tac_if(char *cond_expr);
void gen_tac_else(char *expr);
void yyerror(const char *);

int inside_if1 = 0;					// For tracking nest of level of if-statements
int inside_if2 = 0;
int do_gen_else = 0;				// When set do the else part of the if/else statement
int if_depth = 0;					// Amount of nested if-statements (only finite amount allowed)
int temp_var_ctr = 0;				// Number of temp vars in use

int flex_line_num = 1;		// Used for debugging
FILE * yyin;				// Input calc program file pointer
FILE * tac_file;			// Three address code file pointer
FILE * c_code_file;			// C code produced by backend file pointer
%}

%define parse.error verbose		// Enable verbose errors
%token INTEGER POWER VARIABLE	// bison adds these #defines in calc.tab.h for use in flex
								// Tells flex what the tokens are

// Union defines all possible values a token can have associated with it
// Allow yylval to hold either an integer or a string
%union
{
	int dval;
	char * str;
}

// When %union is used to specify multiple value types, must declare the
// value type of each token for which values are used
// In this case, all values associated with tokens are to be strings
%type <str> INTEGER POWER VARIABLE

// Conditional expressions and expressions values are also string type
%type <str> expr

// Make grammar unambiguous
// Low to high precedence and associativity within a precedent rank
// https://en.cppreference.com/w/c/language/operator_precedence
%right '='
%right '?'
%left '+' '-'
%left '*' '/'
%precedence '!'		// Unary bitwise not; No associativity b/c it is unary
%right POWER		// ** exponent operator

%start calc

%%

calc :
	calc expr '\n'		{ my_free($2); gen_tac_else(NULL); }
	|
	;

expr :
	INTEGER				{ $$ = $1; }
	| VARIABLE        	{ $$ = lc($1); track_user_var(lc($1), 0); }
	| VARIABLE '=' expr	{ $$ = lc($1); gen_tac_assign(lc($1), $3); my_free($3); }
	| expr '+' expr		{ $$ = gen_tac_expr($1, "+", $3); my_free($1); my_free($3); }
	| expr '-' expr		{ $$ = gen_tac_expr($1, "-", $3); my_free($1); my_free($3); }
	| expr '*' expr		{ $$ = gen_tac_expr($1, "*", $3); my_free($1); my_free($3); }
	| expr '/' expr		{ $$ = gen_tac_expr($1, "/", $3); my_free($1); my_free($3); }
	| '!' expr			{ $$ = gen_tac_expr(NULL, "!", $2); my_free($2); }		// Bitwise-not in calc lang
	| expr POWER expr	{ $$ = gen_tac_expr($1, "**", $3); my_free($1); my_free($3);}
	| '(' expr ')'		{ $$ = $2; }					// Will give syntax error for unmatched parens
	| '(' expr ')' '?' { gen_tac_if($2); } '(' expr ')'
						{
							$$ = $7;
							do_gen_else++;	// Keep track of how many closing elses are need for
							my_free($2);	// nested if/else cases
						}
	;

%%

// Used for debugging
// Print out token being freed
void my_free(char * ptr)
{
	if(ptr == NULL)
	{
		yyerror("Tried to free null pointer!");
	}
	else
	{
		// printf("Freed token: %s\n", ptr);
		free(ptr);
	}

	return;
}

// Convert a string to lower case
// Use to this to help enforce variable names being case insensitive
char* lc(char *str)
{
	int i;
	for (i = 0; i < strlen(str); i++)
	{
		str[i] = tolower(str[i]);
	}

	return str;
}

// For case where variable is being assigned an expression
void gen_tac_assign(char * var, char * expr)
{
	track_user_var(var, 1);

	char tac_buf[MAX_USR_VAR_NAME_LEN * 4];
	sprintf(tac_buf, "%s = %s;\n", var, expr);

	fprintf(tac_file, tac_buf);
	bb_print_tac(tac_buf);

	gen_tac_else(var);

	return;
}

// Generates and writes out string of three address code
// Returns temporary variable's name (that must be freed later)
char* gen_tac_expr(char * one, char * op, char * three)
{
	char tmp_var_name[16]; 	// temp var names: _t0123456789
	char tac_buf[MAX_USR_VAR_NAME_LEN * 4];

	// Create the temp variable name
	sprintf(tmp_var_name, "_t%d", temp_var_ctr);
	temp_var_ctr++;

	if (one != NULL)
	{
		// Write out three address code
		sprintf(tac_buf, "%s = %s %s %s;\n", tmp_var_name, one, op, three);
		fprintf(tac_file, tac_buf);

	}
	else	// Unary operator case
	{
		sprintf(tac_buf, "%s = %s%s;\n", tmp_var_name, op, three);
		fprintf(tac_file, tac_buf);
	}

	bb_print_tac(tac_buf);

	return strdup(tmp_var_name);
}

// Print out the if part of the if/else statement
void gen_tac_if(char * cond_expr)
{
	if_depth++;
	if(if_depth > MAX_NESTED_IFS)
	{
		char err_buf[128];
		sprintf(err_buf, "Max number of nested if-statements exceeded (MAX=%d)", MAX_NESTED_IFS);
		yyerror(err_buf);

		exit(1);
	}

	// Track entering of if-statements
	if(inside_if1)
	{
		inside_if2 = 1;
	}
	else
	{
		inside_if1 = 1;
	}

	char buf[MAX_USR_VAR_NAME_LEN * 2];
	sprintf(buf, "if(%s) {\n", cond_expr);

	fprintf(tac_file, buf);
	bb_print_if_else_block_end(buf, inside_if2);

	return;
}

// Print out closing brace of if-statement and the whole else statement
// else will be a variable being assigned to a value of zero
// If the result of the conditional expression is not being written to a variable
// the else part will be empty (when expr is NULL)
void gen_tac_else(char *expr)
{
	for (; do_gen_else > 0; do_gen_else--)
	{
		if_depth--;

		if(expr != NULL)
		{
			fprintf(tac_file, "} else {\n%s = 0;\n}\n", expr);
		}
		else
		{
			fprintf(tac_file, "} else {\n}\n");
		}

		// Track exiting of if-statements
		if(inside_if2 == 1)
		{
			inside_if2 = 0;
		}
		else
		{
			inside_if1 = 0;
		}

		bb_print_else_block(expr, !inside_if1);
	}

	return;
}

void yyerror(const char *s)
{
	printf("%s\n", s);
}

int main(int argc, char *argv[])
{
	// Open the input program file
	if (argc != 2)
	{
		yyerror("Need to provide input file");
		exit(1);
	}
	else
	{
		yyin = fopen(argv[1], "r");
		if(yyin == NULL)
		{
			yyerror("Couldn't open input file");
			exit(1);
		}
	}

	// Open the output file where the three address codes will be written
	char * frontend_tac_name = "Output/tac-frontend.txt";
	tac_file = fopen(frontend_tac_name, "w");

	if (tac_file == NULL)
	{
		yyerror("Couldn't create TAC file");
		exit(1);
	}

	char * bb_file_name = "Output/tac-basic-block.txt";
	char * ssa_file_name = "Output/tac-ssa.txt";
	bb_init_files(bb_file_name, ssa_file_name);

	init_c_code();	// Initialize counters for var tracking (tracking results only used in C code gen)

	// Read in the input program and parse the tokens, writes out frontend TAC to file
	// and writes out basic block form of TAC
	// At the same time, SSA form is also generated from the basic block form
	yyparse();

	// Close the files from initial TAC generation
	fclose(yyin);
	fclose(tac_file);
	bb_close_files(bb_file_name, ssa_file_name);

	// Generate runnable C code from frontend TAC and basic block code
	gen_c_code(frontend_tac_name, "Output/c-frontend.c", temp_var_ctr);
	gen_c_code(bb_file_name, "Output/c-basic-block.c", temp_var_ctr);

	return 0;
}
