// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calc.h"
#include "copy-stmt-elim.h"

# define MAX_CPY_STATEMENT 512

typedef struct Copystatement{

	char assigned[MAX_USR_VAR_NAME_LEN + 1]
	char expr_1[MAX_USR_VAR_NAME_LEN + 1];
	char op[MAX_USR_VAR_NAME_LEN + 1];
	char expr_2[MAX_USR_VAR_NAME_LEN + 1];

	int depth_created_in;	// When the depth is left, the expression becomes invalid
							// (no guarantee path was taken to spot where it was declared)

	int valid;				// Is the expression still valid (made invalid when one of the values written to)

} Cpystmnt;

Cpystmnt cp_st_table[MAX_CPY_STATEMENT];
int num_cp_stmnts;

FILE * cp_st_temp_tac_ptr;
FILE * cp_st_opt_tac_ptr;

int cpy_st_line_num;		// NOTE: the line number is for to the input tac file, NOT THE OUTPUT!!!
int cpy_st_ifelse_depth;

// Create a new entry in the subexpression table
// return the value of the temp variable that will now hold this subexpression
void _cp_st_record_cpy_stmnt(char* assigned, char *expr_1, char *op, char *expr_2)
{
	if(num_cp_stmnts >= MAX_CPY_STATEMENT)
	{
		printf("Copy statement module exceeded max number of recorded statement (MAX=%d)\n", MAX_CPY_STATEMENT);
		exit(1);
	}

	strcpy(cp_st_table[num_cp_stmnts].assigned, assigned);

	printf("Recorded %s = ", assigned);

	if(expr_1 != NULL)
	{
		strcpy(cp_st_table[num_cp_stmnts].expr_1, expr_1);
		printf("%s", expr_1);
	}
	else
	{
		strcpy(cp_st_table[num_cp_stmnts].expr_1, "");
	}

	if(op != NULL)
	{
		strcpy(cp_st_table[num_cp_stmnts].op, op);

		if(expr_1 != NULL)
		{
			printf(" %s ", op);
		}
		else
		{
			printf("%s", op);	// Case for var = !var or !const
		}
	}
	else
	{
		strcpy(cp_st_table[num_cp_stmnts].op, "");
	}

	if(expr_2 != NULL)
	{
		printf("%s;", op);	// Case for var = !var or !const
		strcpy(cp_st_table[num_cp_stmnts].expr_2, expr_2);
	}
	else
	{
		strcpy(cp_st_table[num_cp_stmnts].expr_2, "");
	}

	cp_st_table[num_cp_stmnts].depth_created_in = cpy_st_ifelse_depth;
	cp_st_table[num_cp_stmnts].valid = 1;

	printf(" on line %d at depth=%d\n", cpy_st_line_num, cpy_st_ifelse_depth);

	num_cp_stmnts++;

	return;
}

// Checks if the expression has a valid copy statement in the table
// If it is, it returns the index of the copy statement in the table
// Returns -1 otherwise
int _cp_st_get_statement_index(char *expr)
{
	int i;
	for(i = 0; i < num_cp_stmnts; i++)
	{
		// If table entry not valid, ignore
		if(!cp_st_table[i].valid)
		{
			continue;
		}
		else if(strcmp(expr, cp_st_table[i].assigned) == 0)
		{
			return 1;
		}
	}

	return -1;
}

// Record when the if/else level is changed (when a context is entered or left)
// When a context is left, go through the copy statement table and invalidate
// any copy statent that was created inside that context
void _cpy_st_ifelse_context_invalidator(char *ifelse_line)
{
	if(strstr(ifelse_line, "if(") != NULL)
	{
		cpy_st_ifelse_depth++;
		// printf("Depth=%d on line %d b/c of %s", cpy_st_ifelse_depth, cpy_st_line_num, ifelse_line);
	}
	else if(strstr(ifelse_line, "} else {") != NULL)
	{
		// If leaving a context, go through the copy statement table and invalidate as needed
		int i;
		for(i = 0; i < num_cp_stmnts; i++)
		{
			// Skip over already invalidated copy statements
			if(!cp_st_table[i].valid)
			{
				continue;
			}

			if(cp_st_table[i].depth_created_in >= cpy_st_ifelse_depth)
			{
				cp_st_table[i].valid = 0;
				printf("Invalidated copy statement for %s since the context it was created in exited at line %d\n",
					cp_st_table[i].assigned, cpy_st_line_num);
			}
		}

		cpy_st_ifelse_depth--;
		// printf("Depth=%d on line %d b/c of %s", cpy_st_ifelse_depth, cpy_st_line_num, ifelse_line);
	}
	else
	{
		// Don't need to care about closing "}" for else statements
		// else statements will never have a CS since always either empty or has var = 0;
		// Can decrement depth and invalidate when end of if is reached.
	}

	return;
}

// If a variable is assigned a value on any path, all the copy statements
// it was used in are now invalid
void _cpy_st_inval_with_assigned_var(char *assigned)
{
	int i;
	for(i = 0; i < num_cp_stmnts; i++)
	{
		if(!cp_st_table[i].valid)
		{
			continue;
		}

		int match1 = strcmp(assigned, cp_st_table[i].assigned) == 0;
		int match2 = strcmp(assigned, cp_st_table[i].expr_1) == 0;
		int match3 = strcmp(assigned, cp_st_table[i].expr_2) == 0;

		if(match1 || match2 || match3)
		{
			cp_st_table[i].valid = 0;

			printf("Invalidated copy statement for %s = %s %s %s since %s was assigned value on line %d\n",
			cp_st_table[i].assigned, cp_st_table[i].expr_1, cp_st_table[i].op,
			cp_st_table[i].expr_2, assigned, cpy_st_line_num);
		}
	}

	return;
}

void _cpy_process_tac_line(char *tac_line, char *opt_tac_name)
{
	// Ignore and just copy "} else {" and "}"
	if(strstr(tac_line, "} else {") != NULL || strstr(tac_line, "}") != NULL)
	{
		fprintf(cp_st_temp_tac_ptr, "%s", tac_line);

		// Process if/else context change
		_cpy_st_ifelse_context_invalidator(tac_line);

		return;
	}
	else if(strstr(tac_line, "if(") != NULL)
	{
		/// See if contents can be replaced
		if()
		{

		}
		else
		{
			fprintf(cp_st_temp_tac_ptr, "%s", tac_line);
		}

		return
	}

	char temp[MAX_USR_VAR_NAME_LEN * 4];
	char new_statment[MAX_USR_VAR_NAME_LEN * 4];
	strcpy(temp, tac_line);

	char *assigned = strtok(temp, " =;\n");
	char *expr_1 = strtok(NULL, " =;\n");
	char *op = strtok(NULL, " =;\n");
	char *expr_2 = strtok(NULL, " =;\n");
	
	
	
	// Replace any expressions that have copy statements depending on the typed
	
	// Record the new statement in the table

	int index = _cp_st_get_statement_index(assigned);
	if(index == -1)
	{
		// Only record subexpression if it is used at least once more in the
		// program following the current point (before written to or context left)
		int used_again = _cse_used_again(opt_tac_name, expr_1, op, expr_2);
		if(used_again)
		{
			int temp_var_to_use = _cse_record_subexpression(expr_1, op, expr_2);

			// Assign new temp variable with the CS
			fprintf(cp_st_temp_tac_ptr, "_t%d = %s %s %s;\n", temp_var_to_use, expr_1, op, expr_2);

			// Give the assigned value of the statement this temp variable
			fprintf(cp_st_temp_tac_ptr, "%s = _t%d;\n", assigned, temp_var_to_use);
		}
		else
		{
			// If not used more than once, just copy statement over
			fprintf(cp_st_temp_tac_ptr, "%s", tac_line);
		}
	}
	else // (index != -1)
	{
		// Insert the assigned temp variable for the already recorded CS
		int temp_var_to_use = cp_st_table[index].temp_var;

		fprintf(cp_st_temp_tac_ptr, "%s = _t%d;\n", assigned, temp_var_to_use);
		printf("Used _t%d on line %d\n", temp_var_to_use, cpy_st_line_num);
	}

	// Go through table and invalidate any copy statement that contains "assigned"
	_cpy_st_inval_with_assigned_var(assigned);
	
	// Print out the final statement
	fprintf(cp_st_temp_tac_ptr "%s", new_statment);

	return;
}

// Write optimizations to tac-optimized template
void cse_do_optimization(char *opt_tac_name, char *temp_tac_name)
{
	// Reset these values for the next iteration of optimizations
	num_cp_stmnts = 0;
	cse_next_temp_var_name = 0;
	cpy_st_line_num = 0;
	cpy_st_ifelse_depth = 0;

	cp_st_temp_tac_ptr = fopen(temp_tac_name, "w"); // Clear contents of temp file for next opt iteration
	if (cp_st_temp_tac_ptr == NULL)
	{
		printf("CSE couldn't open %s for writing next optimization to\n", temp_tac_name);
		exit(1);
	}

	cp_st_temp_tac_ptr = fopen(opt_tac_name, "r");
	if (cp_st_temp_tac_ptr == NULL)
	{
		printf("CSE couldn't open %s for reading in TAC\n", opt_tac_name);
		exit(1);
	}

	_cse_init_first_temp_var_name(opt_tac_name);

	char line[MAX_USR_VAR_NAME_LEN * 4];
	while(fgets(line, MAX_USR_VAR_NAME_LEN * 4, cp_st_temp_tac_ptr) != NULL)
	{
		_cse_process_tac_line(line, opt_tac_name);
		cpy_st_line_num++;
	}

	fclose(cp_st_temp_tac_ptr);
	fclose(cp_st_temp_tac_ptr);

	// Copy contents from temp file back to main file
	copy_to_file(opt_tac_name, temp_tac_name);
}

void copy_stmnt_do_optimization(char *opt_tac_name, char *temp_tac_name)
{
	copy_stmnt_temp_tac_ptr = fopen(temp_tac_name, "w"); // Clear contents of temp file for next opt iteration
	if (copy_stmnt_temp_tac_ptr == NULL)
	{
		printf("Copy-statement couldn't open %s for writing next optimization to\n", temp_tac_name);
		exit(1);
	}

	fclose(copy_stmnt_temp_tac_ptr);

	// Copy contents from temp file back to main file
	copy_to_file(opt_tac_name, temp_tac_name);

	return;
}