// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calc.h"
#include "cse.h"

# define MAX_SUB_EXPRESSIONS 512

typedef struct Subexprssion{
	char expr_1[MAX_USR_VAR_NAME_LEN + 1];
	char op[MAX_USR_VAR_NAME_LEN + 1];
	char expr_2[MAX_USR_VAR_NAME_LEN + 1];

	int temp_var;

	int valid;				// Is the expression still valid (made invalid when one of the values written to)
	int depth_created_in;	// When the depth is left, the expression becomes invalid
							// (no guarantee path was taken to spot where it was declared)

} Subexpr;

Subexpr subexpr_table[MAX_SUB_EXPRESSIONS];
int num_sub_exprs = 0;

int cse_next_temp_var_name;
FILE *cse_temp_tac_ptr;
FILE * cse_opt_tac_ptr;




// Go through the input file and find the last assigned temporary variable.
// (will have the largest value in the form _t#); when doing CSE, can the
// insert temporary variables that have a number one larger than the  last one
// already assigned to avoid name conflicts
void _cse_init_first_temp_var_name(char *input_file)
{
	FILE *input_file_ptr = fopen(input_file, "r");
	if (input_file_ptr == NULL)
	{
		printf("CSE couldn't open %s to find last assigned temp variable\n", input_file);
		exit(1);
	}

	// Initialize the next temp variable counter
	cse_next_temp_var_name = 0;

	char line[MAX_USR_VAR_NAME_LEN * 4];
	while(fgets(line, MAX_USR_VAR_NAME_LEN * 4, input_file_ptr) != NULL)
	{
		if(strstr(line, "}") != NULL)
		{
			continue;
		}

		char *tok = strtok(line, " +-*/!=();\n");

		while(tok != NULL)
		{
			if(tok[0] == '_')
			{
				int temp_name = atoi(tok+2);

				if(temp_name >= cse_next_temp_var_name)
				{
					cse_next_temp_var_name = temp_name + 1;
				}

			}

			tok = strtok(NULL, " +-*/!=();\n");
		}
	}

	fclose(input_file_ptr);

	// printf("Starting CSE with temp var of _t%d\n", cse_next_temp_var_name);

	return;
}

// Create a new entry in the subexpression table
// return the value of the temp variable that will now hold this subexpression
int _cse_record_subexpression(char *expr_1, char *op, char *expr_2)
{
	if(num_sub_exprs >= MAX_SUB_EXPRESSIONS)
	{
		printf("CSE module exceeded max number of recorded subexpressions (MAX=%d)\n", MAX_SUB_EXPRESSIONS);
		exit(1);
	}

	strcpy(subexpr_table[num_sub_exprs].expr_1, expr_1);
	strcpy(subexpr_table[num_sub_exprs].op, op);
	strcpy(subexpr_table[num_sub_exprs].expr_2, expr_2);
	subexpr_table[num_sub_exprs].temp_var = cse_next_temp_var_name;
	subexpr_table[num_sub_exprs].valid = 1;

	num_sub_exprs++;
	cse_next_temp_var_name++;

	return cse_next_temp_var_name - 1;
}

// Checks if the subexpression is in the table of subexpressions
// If it is, it returns the index of the expression in the table
// Returns -1 otherwise
int _cse_get_expression_index(char *expr_1, char *op, char *expr_2)
{
	int i;
	for(i = 0; i < num_sub_exprs; i++)
	{
		// If table entry not valid or they don't have matching operator, ignore
		if(!subexpr_table[i].valid || strcmp(subexpr_table[i].op, op) != 0)
		{
			continue;
		}

		int one_one = strcmp(subexpr_table[i].expr_1, expr_1) == 0;
		int two_two = strcmp(subexpr_table[i].expr_2, expr_2) == 0;

		if(one_one && two_two)
		{
			return i;
		}
		else if(strcmp(op, "+") == 0 || strcmp(op, "*") == 0)
		{
			// Check for commutative operation => a +* b == b +* a
			int one_two = strcmp(subexpr_table[i].expr_1, expr_2) == 0;
			int two_one = strcmp(subexpr_table[i].expr_2, expr_1) == 0;

			if(one_two && two_one)
			{
				return i;
			}
		}
	}

	return -1;
}

// Checks if the cs used is used again after the current line
// Only worth creating a temp variable for elimination if that cs is used more
// than once
int _cse_used_again(char *expr_1, char *op, char *expr_2)
{
	// TO DO!!!
	// use dup()?

	return 1;
}

// For an expression to be considered an acceptable subexpression,
// it must be in the form a = b op c;
// That means the following are ignored:
// if(var or const) {, var = !var or !const, end of if/else statements
void _cse_process_tac_line(char *tac_line)
{
	// Ignore and just copy in "if() {", ""} else {", and "}"
	if(strstr(tac_line, "{") != NULL || strstr(tac_line, "}") != NULL)
	{
		fprintf(cse_temp_tac_ptr, "%s", tac_line);

		// TO DO Update context tracker
		// TO DO invalidate all CSE temps that were created in the nest scope when it is left

		return;
	}

	char temp[MAX_USR_VAR_NAME_LEN * 4];
	strcpy(temp, tac_line);

	char *assigned = strtok(temp, " =;\n");
	char *expr_1 = strtok(NULL, " =;\n");
	char *op = strtok(NULL, " =;\n");
	char *expr_2 = strtok(NULL, " =;\n");

	// Verify sub-expression is in correct form before recording and/or inserting
	if(expr_1 != NULL && op != NULL && expr_2 != NULL)
	{
		int index = _cse_get_expression_index(expr_1, op, expr_2);

		if(index == -1 && _cse_used_again(expr_1, op, expr_2))
		{
			int temp_var_to_use = _cse_record_subexpression(expr_1, op, expr_2);

			// Assign new temp variable with the cs
			fprintf(cse_temp_tac_ptr, "_t%d = %s %s %s;\n", temp_var_to_use, expr_1, op, expr_2);

			// Give the assigned value of the statement this temp variable
			fprintf(cse_temp_tac_ptr, "%s = _t%d;\n", assigned, temp_var_to_use);
		}
		else if(index != -1)
		{
			// Insert the already defined temp variable for the already recorded cs
			int temp_var_to_use = subexpr_table[index].temp_var;

			fprintf(cse_temp_tac_ptr, "%s = _t%d;\n", assigned, temp_var_to_use);
		}
		else
		{
			// Statement does not have a common subexpression, just copy statement over
			fprintf(cse_temp_tac_ptr, "%s", tac_line);
		}
	}
	else
	{
		// Statement not in the form of a subexpression, just copy statement over
		fprintf(cse_temp_tac_ptr, "%s", tac_line);
	}

	// Go through table and invalidate any subexpression that contains "assigned"
	// bases on the depth and path
	// TO DO


}

// write optimizations to tac-optimized template
void cse_do_optimization(char *opt_tac_name, char *temp_tac_name)
{
	cse_temp_tac_ptr = fopen(temp_tac_name, "w"); // Clear contents of temp file for next opt iteration
	if (cse_temp_tac_ptr == NULL)
	{
		printf("CSE couldn't open %s for writing next optimization to\n", temp_tac_name);
		exit(1);
	}

	cse_opt_tac_ptr = fopen(opt_tac_name, "r");
	if (cse_opt_tac_ptr == NULL)
	{
		printf("CSE couldn't open %s for reading in TAC\n", opt_tac_name);
		exit(1);
	}

	_cse_init_first_temp_var_name(opt_tac_name);

	char line[MAX_USR_VAR_NAME_LEN * 4];
	while(fgets(line, MAX_USR_VAR_NAME_LEN * 4, cse_opt_tac_ptr) != NULL)
	{
		_cse_process_tac_line(line);
	}

	fclose(cse_temp_tac_ptr);
	fclose(cse_opt_tac_ptr);

	// Copy contents from temp file back to main file
	copy_to_file(opt_tac_name, temp_tac_name);
}