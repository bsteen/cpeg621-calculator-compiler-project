// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calc.h"
#include "cse.h"

#define MAX_SUB_EXPRESSIONS 512

typedef struct Subexprssion{
	char expr_1[MAX_USR_VAR_NAME_LEN + 1];
	char op[MAX_USR_VAR_NAME_LEN + 1];
	char expr_2[MAX_USR_VAR_NAME_LEN + 1];

	int temp_var;			// Temp variable (_c#) that holds the expression

	int depth_created_in;	// When the depth is left, the expression becomes invalid
							// (no guarantee path was taken to spot where it was declared)

	int valid;				// Is the expression still valid (made invalid when one of the values written to)

} Subexpr;

Subexpr subexpr_table[MAX_SUB_EXPRESSIONS];
int num_sub_exprs;

int cse_next_temp_var_name;
FILE *cse_temp_tac_ptr;
FILE * cse_opt_tac_ptr;

int cse_line_num;		// NOTE: the line number is for to the input tac file, NOT THE OUTPUT!!!
int cse_ifelse_depth;
int cse_changes_made;

// Go through the input file and find the last assigned temporary variable.
// (will have the largest value in the form _c#); when doing CSE, can the
// insert temporary variables that have a number one larger than the last one
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
			// Ignore constants, user vars, normal temps, and the "if" in "if(...)"
			if(tok[0] == '_' && tok[1] == 'c')
			{
				int temp_name = atoi(tok + 2);	// Skip over the "_c" part and convert last part to an int

				if(temp_name >= cse_next_temp_var_name)
				{
					cse_next_temp_var_name = temp_name + 1;
				}

			}

			tok = strtok(NULL, " +-*/!=();\n");
		}
	}

	fclose(input_file_ptr);

	printf("Starting CSE with temp var of _c%d\n", cse_next_temp_var_name);

	return;
}

// Create a new entry in the subexpression table
// return the value of the temp variable that will now hold this subexpression
int _cse_record_subexpression(char *expr_1, char *op, char *expr_2, int reuse_this_temp)
{
	if(num_sub_exprs >= MAX_SUB_EXPRESSIONS)
	{
		printf("CSE module exceeded max number of recorded subexpressions (MAX=%d)\n", MAX_SUB_EXPRESSIONS);
		exit(1);
	}

	strcpy(subexpr_table[num_sub_exprs].expr_1, expr_1);
	strcpy(subexpr_table[num_sub_exprs].op, op);
	strcpy(subexpr_table[num_sub_exprs].expr_2, expr_2);
	subexpr_table[num_sub_exprs].depth_created_in = cse_ifelse_depth;
	subexpr_table[num_sub_exprs].valid = 1;

	if(reuse_this_temp == -1)
	{
		subexpr_table[num_sub_exprs].temp_var = cse_next_temp_var_name;
		cse_next_temp_var_name++;
	}
	else
	{
		printf("Reusing _c%d for cs %s %s %s\n", reuse_this_temp, expr_1, op, expr_2);
		subexpr_table[num_sub_exprs].temp_var = reuse_this_temp;
	}

	printf("Recorded %s %s %s, stored in _c%d at depth %d\n",
			expr_1, op, expr_2, subexpr_table[num_sub_exprs].temp_var, cse_ifelse_depth);

	num_sub_exprs++;

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
			// Check for commutative operation => a (+*) b == b (+*) a
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

// Checks if the cs used is used again after the current line before being invalidated
// Only worth creating a temp variable for cs elimination if that cs is used AT LEAST once
// before being invalidated by assignment
int _cse_used_again(char *opt_tac_name, char *expr_1, char *op, char *expr_2)
{
	FILE * temp_file_ptr = fopen(opt_tac_name, "r");
	if(temp_file_ptr == NULL)
	{
		printf("Couldn't open %s for checking number of cs usages after line %d\n", opt_tac_name, cse_line_num);
	}

	char line[MAX_USR_VAR_NAME_LEN * 4];
	int i = 0;

	// Move pasts the current line
	while(fgets(line, MAX_USR_VAR_NAME_LEN * 4, temp_file_ptr) != NULL)
	{
		if(i == cse_line_num)
		{
			i++; // Need to increment because the next while will be reading the next line
			break;
		}

		i++;
	}

	// cse_ifelse_depth represent the depth the CS would be created at
	// temp_ifelse_context is the future depth after the line this subexpression is in
	int temp_ifelse_context = cse_ifelse_depth;

	// Once past the current line, look for another instance of the subexpression
	while(fgets(line, MAX_USR_VAR_NAME_LEN * 4, temp_file_ptr) != NULL)
	{
		if(strstr(line, "if(") != NULL )
		{
			temp_ifelse_context++;
			i++;
			continue;
		}
		else if(strstr(line, "}") != NULL)
		{
			if(strstr(line, "} else {") != NULL)
			{
				if(cse_ifelse_depth >= temp_ifelse_context)
				{
					// Have left the context the expression was first used in
					break;
				}

				temp_ifelse_context--;
			}

			i++;
			continue;
		}

		char *temp_assigned = strtok(line, " =;\n");
		int match1 = strcmp(temp_assigned, expr_1) == 0;
		int match2 = strcmp(temp_assigned, expr_2) == 0;

		// If one of the variables is written to before finding another uses of the
		// expression, it is not worth creating a new variable for it
		if(match1 || match2)
		{
			break;
		}

		char *temp_expr_1 = strtok(NULL, " =;\n");
		char *temp_op = strtok(NULL, " =;\n");
		char *temp_expr_2 = strtok(NULL, " =;\n");

		if(temp_expr_1 != NULL && temp_op != NULL && temp_expr_2 != NULL)
		{
			// If the next line has different operator, then they can't be be the
			// same subexpression
			if(strcmp(op, temp_op) != 0)
			{
				i++;
				continue;
			}

			int one_one = strcmp(expr_1, temp_expr_1) == 0;
			int two_two = strcmp(expr_2, temp_expr_2) == 0;

			if(one_one && two_two)
			{
				fclose(temp_file_ptr);
				// printf("CS %s %s %s (from line %d) used again at line %d\n",
					// expr_1, op, expr_2, cse_line_num, i);
				return 1;
			}
			else if(strcmp(op, "+") == 0 || strcmp(op, "*") == 0)
			{
				// Check for commutative operation => a (+*) b == b (+*) a
				int one_two = strcmp(expr_1, temp_expr_2) == 0;
				int two_one = strcmp(expr_2, temp_expr_1) == 0;

				if(one_two && two_one)
				{
					fclose(temp_file_ptr);
					// printf("CS %s %s %s (from line %d) used again at line %d\n",
						// expr_1, op, expr_2, cse_line_num, i);
					return 1;
				}
			}
		}

		i++;
	}

	// At this point, it was found that the sub expression was either:
	// 		never used again after this point
	// 		before being used again, one of its variables was written to
	// 		the if/else context it was created in was left before another use was found
	// In any of these cases, it is not worth inserting a variable to substitute it

	fclose(temp_file_ptr);
	return 0;
}

// Record when the if/else level is changed (when a context is entered or left)
// When a context is left, go through the subexpression table and invalidate
// any subexpression that was created inside that context
void _cse_ifelse_context_invalidator(char *ifelse_line)
{
	if(strstr(ifelse_line, "if(") != NULL)
	{
		cse_ifelse_depth++;
		// printf("Depth=%d on line %d b/c of %s", cse_ifelse_depth, cse_line_num, ifelse_line);
	}
	else if(strstr(ifelse_line, "} else {") != NULL)
	{
		// If leaving a context, go through the subexpression table and invalidate as needed
		int i;
		for(i = 0; i < num_sub_exprs; i++)
		{
			// Skip over already invalidated CSs
			if(!subexpr_table[i].valid)
			{
				continue;
			}

			if(subexpr_table[i].depth_created_in >= cse_ifelse_depth)
			{
				subexpr_table[i].valid = 0;
				printf("Invalidated %s %s %s since the context it was created in exited at line %d\n",
					subexpr_table[i].expr_1, subexpr_table[i].op, subexpr_table[i].expr_2, cse_line_num);
			}
		}

		cse_ifelse_depth--;
		// printf("Depth=%d on line %d b/c of %s", cse_ifelse_depth, cse_line_num, ifelse_line);
	}
	else
	{
		// Don't need to care about closing "}" for else statements
		// else statements will never have a CS since always either empty or has var = 0;
		// Can decrement depth and invalidate when end of if is reached.
	}

	return;
}

// If a variable is assigned a value on any path, all the CSs it was used in
// are now invalid
void _cse_invalidate_cs_with_assigned_var(char *assigned)
{
	int i;
	for(i = 0; i < num_sub_exprs; i++)
	{
		if(!subexpr_table[i].valid)
		{
			continue;
		}

		int match1 = strcmp(assigned, subexpr_table[i].expr_1) == 0;
		int match2 = strcmp(assigned, subexpr_table[i].expr_2) == 0;

		if(match1 || match2)
		{
			subexpr_table[i].valid = 0;
			printf("Invalidated %s %s %s since %s was assigned value on line %d\n",
			subexpr_table[i].expr_1, subexpr_table[i].op, subexpr_table[i].expr_2, assigned, cse_line_num);
		}
	}

	return;
}

// For an expression to be considered an acceptable subexpression,
// it must be in the form a = b op c;
// That means the following are ignored:
// if(var or const) {, var = !var or !const, end of if/else statements
void _cse_process_tac_line(char *tac_line, char *opt_tac_name)
{
	// Ignore and just copy in "if() {", ""} else {", and "}"
	if(strstr(tac_line, "{") != NULL || strstr(tac_line, "}") != NULL)
	{
		fprintf(cse_temp_tac_ptr, "%s", tac_line);

		// Process if/else context change
		_cse_ifelse_context_invalidator(tac_line);

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

		if(index == -1)
		{
			// Only record subexpression if it is used at least once more in the
			// program following the current point (before written to or context left)
			int used_again = _cse_used_again(opt_tac_name, expr_1, op, expr_2);
			if(used_again)
			{
				int temp_var_to_use;

				// If it finds _c# = a op c is used again, don't insert a new temp,
				// just reuse the old temp (still need to record the subexpression)
				// This can happen when CSE already did a cycle and copy statement opened
				// up a new CS of a previously found from
				if(strstr(assigned, "_c") != NULL)
				{
					temp_var_to_use = atoi(assigned + 2);	// Skip past the "_c"
					_cse_record_subexpression(expr_1, op, expr_2, temp_var_to_use);
					
					// Just copy the input line out as nothing has changed
					fprintf(cse_temp_tac_ptr, "%s", tac_line);

					// Don't increment cse_changes_made because the TAC has not been altered yet
				}
				else
				{
					temp_var_to_use = _cse_record_subexpression(expr_1, op, expr_2, -1);

					// Assign new temp variable with the CS
					fprintf(cse_temp_tac_ptr, "_c%d = %s %s %s;\n", temp_var_to_use, expr_1, op, expr_2);

					// Give the assigned value of the statement this temp variable
					fprintf(cse_temp_tac_ptr, "%s = _c%d;\n", assigned, temp_var_to_use);
					
					cse_changes_made++;
				}
			}
			else
			{
				// If not used more than once, just copy statement over
				fprintf(cse_temp_tac_ptr, "%s", tac_line);
			}
		}
		else // (index != -1)
		{
			// Insert the assigned temp variable for the already recorded CS
			int temp_var_to_use = subexpr_table[index].temp_var;

			fprintf(cse_temp_tac_ptr, "%s = _c%d;\n", assigned, temp_var_to_use);
			printf("Used _c%d on line %d\n", temp_var_to_use, cse_line_num);
			cse_changes_made++;
		}
	}
	else
	{
		// Statement not in the form of a subexpression, just copy statement over
		fprintf(cse_temp_tac_ptr, "%s", tac_line);
	}

	// Go through table and invalidate any subexpression that contains "assigned"
	_cse_invalidate_cs_with_assigned_var(assigned);

	return;
}

// Write optimizations to tac-optimized template
int cse_do_optimization(char *opt_tac_name, char *temp_tac_name)
{
	// Reset these values for the next iteration of optimizations
	num_sub_exprs = 0;
	cse_next_temp_var_name = 0;
	cse_line_num = 0;
	cse_ifelse_depth = 0;
	cse_changes_made = 0;

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
		_cse_process_tac_line(line, opt_tac_name);
		cse_line_num++;
	}

	fclose(cse_temp_tac_ptr);
	fclose(cse_opt_tac_ptr);

	// Copy contents from temp file back to main file
	copy_to_file(opt_tac_name, temp_tac_name);

	printf("CSE changes made: %d\n", cse_changes_made);

	return cse_changes_made;
}