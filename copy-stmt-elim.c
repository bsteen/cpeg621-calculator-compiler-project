// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calc.h"
#include "copy-stmt-elim.h"

# define MAX_CPY_STATEMENT 	512
# define LINE_BUF_SIZE 		(MAX_USR_VAR_NAME_LEN * 4) + 16

typedef struct Copystatement{

	char assigned[MAX_USR_VAR_NAME_LEN + 1];
	char expr_1[MAX_USR_VAR_NAME_LEN + 1];
	char op[MAX_USR_VAR_NAME_LEN + 1];
	char expr_2[MAX_USR_VAR_NAME_LEN + 1];

	int statement_type;		// Is it in the form a = b (ASSIGN)
							// or a = b op  c (OP)
							// or a = !b (OP)

	int depth_created_in;	// When the depth is left, the expression becomes invalid
							// (no guarantee path was taken to spot where it was declared)

	int valid;				// Is the expression still valid (made invalid when one of the values written to)

} Cpystmnt;

Cpystmnt cp_st_table[MAX_CPY_STATEMENT];
int num_cp_stmnts;
int cp_st_changes_made;

int cp_st_line_num;		// NOTE: the line number is for to the input tac file, NOT THE OUTPUT!!!
int cp_st_ifelse_depth;

FILE * cp_st_temp_tac_ptr;
FILE * cp_st_opt_tac_ptr;

// Create a new entry in the subexpression table
// return the value of the temp variable that will now hold this subexpression
// This function IS expecting the ! operator to be in the op string
void _cp_st_record_cpy_stmnt(char* assigned, char *expr_1, char *op, char *expr_2)
{
	if(num_cp_stmnts >= MAX_CPY_STATEMENT)
	{
		printf("Copy statement module exceeded max number of recorded statement (MAX=%d)\n", MAX_CPY_STATEMENT);
		exit(1);
	}

	cp_st_table[num_cp_stmnts].depth_created_in = cp_st_ifelse_depth;
	cp_st_table[num_cp_stmnts].valid = 1;

	strcpy(cp_st_table[num_cp_stmnts].assigned, assigned);
	cp_st_table[num_cp_stmnts].statement_type = ASSIGN;	 // Assume form of a = b unless operator is present

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
		cp_st_table[num_cp_stmnts].statement_type = OP;

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
		printf("%s", expr_2);
		strcpy(cp_st_table[num_cp_stmnts].expr_2, expr_2);
	}
	else
	{
		strcpy(cp_st_table[num_cp_stmnts].expr_2, "");
	}

	printf("; on line %d at depth=%d (type=%d)\n", cp_st_line_num, cp_st_ifelse_depth, cp_st_table[num_cp_stmnts].statement_type);

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
			return i;
		}
	}

	return -1;
}

// Try to substitute the expressions in the current statement with the expression(s)
// from its previous copy statement (if it exists)
// If no previous copy statement was found in a compatible form, the current expression
// is used as normal
// This function will not edit the input strings and only edit the output buffer
// by adding onto the end of it
// This function assume "!" is NOT in the op string and that is in the
void _cpy_st_insert_copies(char *output, char *expr_1, char *op, char *expr_2)
{
	int index;

	if(op != NULL)
	{
		// Current expression is in form var = var/const op var/const TYPE 3
		if(expr_1[0] < 'A')	// Ignore constants
		{
			strcat(output, expr_1);
		}
		else
		{
			// Try to replace expr_1
			index = _cp_st_get_statement_index(expr_1);

			if(index != -1 && cp_st_table[index].statement_type == ASSIGN)
			{
				// a = b		(use b as substitute)
				// x = a op c	(looking at a in current line)

				strcat(output, cp_st_table[index].expr_1);
				printf("REPLACED %s with %s in %s\n", expr_1, cp_st_table[index].expr_1, output);
				cp_st_changes_made++;
			}
			else
			{
				strcat(output, expr_1);
			}
		}

		strcat(output, " ");	// Add the operator back in
		strcat(output, op);
		strcat(output, " ");

		if(expr_2[0] < 'A')	// Ignore constants
		{
			strcat(output, expr_2);
		}
		else
		{
			// Try to replace expr_2
			index = _cp_st_get_statement_index(expr_2);

			if(index != -1 && cp_st_table[index].statement_type == ASSIGN)
			{
				// a = b		(use b as substitute)
				// x = c op a	(looking at a in current line)

				strcat(output, cp_st_table[index].expr_1);
				printf("REPLACED %s with %s in %s\n", expr_2, cp_st_table[index].expr_1, output);
				cp_st_changes_made++;
			}
			else
			{
				strcat(output, expr_2);
			}
		}
	}
	else if(expr_1[0] == '!')
	{
		// Current expression is of form var = !var/!const TYPE OP

		assert((op == NULL) && (expr_2 == NULL)); 	// Sanity check

		if(expr_1[1] < 'A')	// Ignore constants, they won't have an entry
		{
			strcat(output, expr_1);
			return;
		}

		index = _cp_st_get_statement_index(expr_1 + 1);	// Skip over the !

		if(index != -1 && cp_st_table[index].statement_type == ASSIGN)
		{
			strcat(output, "!");
			strcat(output, cp_st_table[index].expr_1);
			printf("REPLACED %s with !%s in %s\n", expr_1, cp_st_table[index].expr_1, output);
			cp_st_changes_made++;
		}
		else
		{
			strcat(output, expr_1);
		}
	}
	else
	{
		// Current expression is in form var = var/const; TYPE ASSIGN

		if(expr_1[0] < 'A')	// Ignore constants, they won't have an entry
		{
			strcat(output, expr_1);
			return;
		}

		index = _cp_st_get_statement_index(expr_1);

		if(index != -1)
		{
			if(cp_st_table[index].statement_type == ASSIGN)
			{
				// ASSIGN TO ASSIGN operation
				// a = b	(use b as substitute)
				// c = a	(current line, looking at a)

				strcat(output, cp_st_table[index].expr_1);
				cp_st_changes_made++;

				printf("REPLACED %s with %s in %s\n", expr_1, cp_st_table[index].expr_1, output);
			}
			else
			{
				// OP TO ASSIGN operation
				if(strcmp(cp_st_table[index].op, "!") == 0)
				{
					// a = !v (use !v as substitute)
					// x = a  (current line)

					strcat(output, cp_st_table[index].op);
					strcat(output, cp_st_table[index].expr_2);
					cp_st_changes_made++;

					printf("REPLACED %s with !%s in %s\n", expr_1, cp_st_table[index].expr_2, output);
				}
				else
				{
					// a = b op c	(use b op c as substitute)
					// x = a  		(current line)

					strcat(output, cp_st_table[index].expr_1);
					strcat(output, " ");
					strcat(output, cp_st_table[index].op);
					strcat(output, " ");
					strcat(output, cp_st_table[index].expr_2);
					cp_st_changes_made++;

					printf("REPLACED %s with %s %s %s in %s\n", expr_1, cp_st_table[index].expr_1,
							cp_st_table[index].op, cp_st_table[index].expr_2, output);
				}
			}
		}
		else
		{
			strcat(output, expr_1);
		}
	}

	return;
}

// Record when the if/else level is changed (when a context is entered or left)
// When a context is left, go through the copy statement table and invalidate
// any copy statement that was created inside that context
void _cpy_st_ifelse_context_invalidator(char *ifelse_line)
{
	if(strstr(ifelse_line, "if(") != NULL)
	{
		cp_st_ifelse_depth++;
		// printf("Depth=%d on line %d b/c of %s\n", cp_st_ifelse_depth, cp_st_line_num, ifelse_line);
	}
	else if(strstr(ifelse_line, "}") != NULL) // "} else {" or "}"
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

			if(cp_st_table[i].depth_created_in >= cp_st_ifelse_depth)
			{
				cp_st_table[i].valid = 0;
				printf("Invalidated copy statement for %s since the context it was created in exited at line %d\n",
					cp_st_table[i].assigned, cp_st_line_num);
			}
		}

		if (strcmp(ifelse_line, "}\n") == 0)
		{
			cp_st_ifelse_depth--;
			// printf("Depth=%d on line %d b/c of %s\n", cp_st_ifelse_depth, cp_st_line_num, ifelse_line);
		}
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
			cp_st_table[i].expr_2, assigned, cp_st_line_num);
		}
	}

	return;
}

// Tokenize the TAC line, track if/else contexts, insert copy statements into
// the TAC line where possible, record the assignment for future copy statement
// elimination, and invalidate recorded copy statements that are no longer valid
void _cpy_st_process_tac_line(char *tac_line)
{
	// Ignore and just copy "} else {" and "}"
	if(strstr(tac_line, "} else {") != NULL || strstr(tac_line, "}") != NULL)
	{
		fprintf(cp_st_temp_tac_ptr, "%s", tac_line);

		// Process if/else context change
		_cpy_st_ifelse_context_invalidator(tac_line);

		return;
	}

	char temp[LINE_BUF_SIZE];	// Make buffer larger than needed to be safe
	strcpy(temp, tac_line);

	char new_statment[LINE_BUF_SIZE];
	strcpy(new_statment, "");

	if(strstr(temp, "if(") != NULL)
	{
		strtok(temp, " (){\n");		// Skip past the if
		char *expr_1 = strtok(NULL, " (){\n");
		char *op = strtok(NULL, " (){\n");
		char *expr_2 = strtok(NULL, " (){\n");

		char if_temp[LINE_BUF_SIZE];
		strcpy(if_temp, "");

		_cpy_st_insert_copies(if_temp, expr_1, op, expr_2);

		strcpy(new_statment, "if(");
		strcat(new_statment, if_temp);
		strcat(new_statment, ") {\n");

		fprintf(cp_st_temp_tac_ptr, "%s", new_statment);

		_cpy_st_ifelse_context_invalidator(tac_line);

		return;
	}

	// Replace any expressions that have copy statements depending on the typed
	// Line is in the form a = b op c, a = b, or a = !b
	char *assigned = strtok(temp, " =;\n");
	char *expr_1 = strtok(NULL, " =;\n");
	char *op = strtok(NULL, " =;\n");
	char *expr_2 = strtok(NULL, " =;\n");

	strcpy(new_statment, assigned);
	strcat(new_statment, " = ");
	_cpy_st_insert_copies(new_statment, expr_1, op, expr_2);
	strcat(new_statment, ";\n");

	fprintf(cp_st_temp_tac_ptr, "%s", new_statment);	// Print out finished line, with any replacements

	// Go through table and invalidate any copy statement that contains "assigned"
	_cpy_st_inval_with_assigned_var(assigned);

	// Record the assignment statement in the table
	int index = _cp_st_get_statement_index(assigned);

	if(index == -1)
	{
		// DON'T RECORD _c = ... to prevent "infinite stack" with CSE insertions
		if(assigned[0] == '_' && assigned[1] == 'c')
		{
			printf("Not recording line %d because %s = ...\n", cp_st_line_num, assigned);
		}
		else if(strcmp(assigned, expr_1) == 0 && op == NULL && expr_2 == NULL)
		{
			printf("Not recording line %d because %s = %s (SELF ASSIGN)\n", cp_st_line_num, assigned, expr_1);
		}
		else if(strstr(expr_1, "!") != NULL)
		{
			assert((op == NULL) && (expr_2 == NULL));	// Sanity check

			_cp_st_record_cpy_stmnt(assigned, NULL, "!", expr_1 + 1);	// Separate the ! sign
		}
		else
		{
			_cp_st_record_cpy_stmnt(assigned, expr_1, op, expr_2);
		}
	}

	return;
}

// After the main copy statement logic is done, go through and remove any temporary
// variables that are assigned a value but then never used again
void _cp_st_remove_dead_temp(char *opt_tac_name, char *temp_tac_name)
{
	cp_st_temp_tac_ptr = fopen(temp_tac_name, "w");
	if (cp_st_temp_tac_ptr == NULL)
	{
		printf("Copy statement couldn't open %s for writing out and removing dead temps to\n", temp_tac_name);
		exit(1);
	}

	cp_st_opt_tac_ptr = fopen(opt_tac_name, "r");
	if (cp_st_opt_tac_ptr == NULL)
	{
		printf("Copy statement couldn't open %s for reading in to remove dead temps\n", opt_tac_name);
		exit(1);
	}

	char line[LINE_BUF_SIZE];

	// Go through the TAC and try to find and remove dead temps
	while(fgets(line, LINE_BUF_SIZE, cp_st_opt_tac_ptr) != NULL)
	{
		if(strstr(line, "if(") != NULL || strstr(line, "{") != NULL)
		{
			// if statement conditional will never have an assignment in it
			// also ignore "} else {" and "}"
			fprintf(cp_st_temp_tac_ptr, "%s", line);
			continue;
		}

		char temp[LINE_BUF_SIZE];
		strcpy(temp, line);
		char *assigned = strtok(temp, " ");

		if(strstr(assigned, "_t") != NULL)
		{
			long int saved_pos = ftell(cp_st_opt_tac_ptr);
			char future_line[LINE_BUF_SIZE];
			int remove_line = 1;

			// Look forward from the current line to see if the temp is used again
			while(fgets(future_line, LINE_BUF_SIZE, cp_st_opt_tac_ptr) != NULL)
			{
				// If the temp variable appears again in the program from the
				// current line, don't remove it
				if(strstr(future_line, assigned) != NULL)
				{
					remove_line = 0;
					break;
				}
			}

			fseek(cp_st_opt_tac_ptr, saved_pos, SEEK_SET); // Restore the file pointer position

			if(remove_line)
			{
				// Remove the line by not printing to the TAC file
				cp_st_changes_made++;
				printf("REMOVED dead temp assignment: %s", line);
			}
			else
			{
				fprintf(cp_st_temp_tac_ptr, "%s", line);
			}
		}
		else
		{
			// Statement doesn't contain temp as assignment, continue on
			fprintf(cp_st_temp_tac_ptr, "%s", line);
		}
	}

	fclose(cp_st_temp_tac_ptr);
	fclose(cp_st_opt_tac_ptr);

	copy_to_file(opt_tac_name, temp_tac_name);

	return;
}

// Main optimization loop for copy statement elimination
int cp_st_do_optimization(char *opt_tac_name, char *temp_tac_name)
{
	// Reset these values for the next iteration of optimizations
	num_cp_stmnts = 0;
	cp_st_line_num = 0;
	cp_st_ifelse_depth = 0;
	cp_st_changes_made = 0;

	cp_st_temp_tac_ptr = fopen(temp_tac_name, "w"); // Clear contents of temp file for next opt iteration
	if (cp_st_temp_tac_ptr == NULL)
	{
		printf("Copy statement couldn't open %s for writing next optimization to\n", temp_tac_name);
		exit(1);
	}

	cp_st_opt_tac_ptr = fopen(opt_tac_name, "r");
	if (cp_st_opt_tac_ptr == NULL)
	{
		printf("Copy statement couldn't open %s for reading in TAC\n", opt_tac_name);
		exit(1);
	}

	char line[LINE_BUF_SIZE];
	while(fgets(line, LINE_BUF_SIZE, cp_st_opt_tac_ptr) != NULL)
	{
		_cpy_st_process_tac_line(line);
		cp_st_line_num++;
	}

	fclose(cp_st_temp_tac_ptr);
	fclose(cp_st_opt_tac_ptr);

	// Copy contents from temp file back to main file
	copy_to_file(opt_tac_name, temp_tac_name);

	// Perform one last optimization: delete dead temps
	_cp_st_remove_dead_temp(opt_tac_name, temp_tac_name);

	printf("Copy-statement Elim. changes made: %d\n", cp_st_changes_made);

	return cp_st_changes_made;
}
