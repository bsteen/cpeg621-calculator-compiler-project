%{
// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "c-code.h"
#include "cse.h"
#include "data-dep.h"

int yylex(void);					// Will be generated in lex.yy.c by flex

// Following are defined below in sub-routines section
void my_free(char *ptr);
char* lc(char *str);
void gen_tac_assign(char *var, char *expr);
char* gen_tac_expr(char *one, char *op, char *three);
void gen_tac_if(char *cond_expr);
void gen_tac_else(char *expr);
void yyerror(const char *);

int do_gen_else = 0;				// When set do the else part of the if/else statement
int ifelse_depth = 0;					// Amount of nested if-statements (only finite amount allowed)
int inside_if = 0;					// Is TAC currently inside an if-statement at the given level
int temp_var_ctr = 0;				// Number of temp vars in use

int flex_line_num = 1;		// Used for debugging
FILE * yyin;				// Input calc program file pointer
FILE * tac_file;			// Three address code file pointer
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
							// printf("do_gen_else incremented by \"%s\": %d\n", $7, do_gen_else);
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
void gen_tac_assign(char *var, char *expr)
{
	track_user_var(var, 1);

	fprintf(tac_file, "%s = %s;\n", var, expr);
	// printf("WROTE OUT: %s", tac_buf);
	
	dd_record_and_process(var, expr, NULL, ifelse_depth, inside_if);

	gen_tac_else(var);

	return;
}

// Generates and writes out string of three address code
// Returns temporary variable's name (that must be freed later)
char* gen_tac_expr(char *one, char *op, char *three)
{
	char tmp_var_name[16]; 	// temp var names: _t0123456789

	// Create the temp variable name
	sprintf(tmp_var_name, "_t%d", temp_var_ctr);
	temp_var_ctr++;

	if (one != NULL)
	{
		// Write out three address code
		fprintf(tac_file, "%s = %s %s %s;\n", tmp_var_name, one, op, three);
		dd_record_and_process(tmp_var_name, one, three, ifelse_depth, inside_if);
	}
	else	// Unary operator case
	{
		fprintf(tac_file, "%s = %s%s;\n", tmp_var_name, op, three);
		dd_record_and_process(tmp_var_name, NULL, three, ifelse_depth, inside_if);
	}

	return strdup(tmp_var_name);
}

// Print out the if part of the if/else statement
// Track if-statement depth
void gen_tac_if(char *cond_expr)
{
	char buf[MAX_USR_VAR_NAME_LEN * 2];
	sprintf(buf, "if(%s) {\n", cond_expr);
	fprintf(tac_file, buf);
	
	dd_record_and_process(NULL, cond_expr, NULL, ifelse_depth, inside_if);
	
	// Increase if-statement depth after TAC written out
	ifelse_depth++;
	inside_if = 1;
	// printf("Inside IF at depth=%d\n", ifelse_depth);
	
	if(ifelse_depth > MAX_IFELSE_DEPTH)
	{
		char err_buf[128];
		sprintf(err_buf, "Max depth of if-statements exceeded (MAX=%d)", MAX_IFELSE_DEPTH);
		yyerror(err_buf);

		exit(1);
	}
	
	return;
}

// Print out closing brace of if-statement and the whole else statement
// else will be a variable being assigned to a value of zero
// If the result of the conditional expression is not being written to a variable
// the else part will be empty (when expr is NULL)
void gen_tac_else(char *expr)
{
	// printf("do_gen_else=%d, ifelse_depth=%d\n", do_gen_else, ifelse_depth);
	
	for (; do_gen_else > 0; do_gen_else--)
	{
		inside_if = 0;
		// printf("Leaving IF, entering ELSE at depth=%d\n", ifelse_depth);
		
		if(expr != NULL)
		{
			fprintf(tac_file, "} else {\n%s = 0;\n}\n", expr);
			// printf("WROTE OUT: %s = 0;\n", expr);
			dd_record_and_process(expr, NULL, NULL, ifelse_depth, inside_if);
		}
		else
		{
			fprintf(tac_file, "} else {\n}\n");
		}
		
		// printf("Left ELSE at depth=%d\n", ifelse_depth);
		ifelse_depth--;
		
		if(ifelse_depth > 0)
		{
			inside_if = 1;
			// printf("Back in IF at depth=%d\n", ifelse_depth);
		}
		else
		{
			// Reached end of if/else nest, next loop with exit before starting
			inside_if = 0;
			// printf("Outside all IF/ELSE (depth=%d)\n\n", ifelse_depth);
			dd_left_ifelse_nest();
		}
	}

	return;
}

void yyerror(const char *s)
{
	printf("%s\n", s);
}

// Copy contents of source file into destination file
void copy_to_file(char *dest_name, char *source_name)
{
	FILE *source_ptr = fopen(source_name, "r");
	FILE *dest_ptr = fopen(dest_name, "w");
	
	if(source_ptr == NULL)
	{
		printf("Couldn't open %s for copying to %s\n", source_name, dest_name);
		exit(1);
	}
	
	if(dest_ptr == NULL)
	{
		printf("Couldn't create %s\n", dest_name);
		exit(1);
	}
	
	char line[MAX_USR_VAR_NAME_LEN * 4];
	while(fgets(line, MAX_USR_VAR_NAME_LEN * 4, source_ptr) != NULL)
	{
		fprintf(dest_ptr, "%s", line);
	}
	
	fclose(source_ptr);
	fclose(dest_ptr);
	
	return;
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
	char *frontend_tac_name = "Output/tac-frontend.txt";
	tac_file = fopen(frontend_tac_name, "w");

	if (tac_file == NULL)
	{
		yyerror("Couldn't create TAC file");
		exit(1);
	}

	init_c_code();	// Initialize counters for var tracking (tracking results only used in C code gen)

	yyparse();	// Read in the input program and parse the tokens, writes out frontend TAC to file

	// Close the files from initial TAC generation
	fclose(yyin);
	fclose(tac_file);

	// dd_print_out_dependencies();	// Print out data dependencies based on the internal TAC
	
	// Setup for optimization process
	// Do the initial copy of the front end TAC to the file for optimizations
	char *opt_tac_name = "Output/tac-opt.txt";
	copy_to_file(opt_tac_name, frontend_tac_name);
	
	char *temp_tac_name = "Output/tac-opt-temp.txt";
	
	// Start the optimization loop
	// while()
	// {
		cse_do_optimization(opt_tac_name, temp_tac_name);
		// copy_stmnt_do_optimization(opt_temp_name, opt_temp_name);
	// }

	// Generate runnable C code for unoptimized and and optimized, with and
	// without timing
	gen_c_code(frontend_tac_name, "Output/backend.c", 0);
	gen_c_code(frontend_tac_name, "Output/backend-timing.c", 1);
	// gen_c_code(opt_tac_name, "Output/backend-opt.c", 0);
	// gen_c_code(opt_tac_name, "Output/backend-opt-timing.c", 1);

	return 0;
}
