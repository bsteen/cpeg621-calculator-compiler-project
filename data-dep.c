// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calc.h"
#include "data-dep.h"

#define MAX_NUM_STATEMENTS	350
#define MAX_NUM_DEPENDS		128

typedef struct Statement_struct
{
	// For the statement, store the variables being assigned or read from
	// Read value(s) will be NULL if constants was used or in case where a=(!)b;
	char written[MAX_USR_VAR_NAME_LEN + 1];	// Will be NULL for if statement
	char read1[MAX_USR_VAR_NAME_LEN + 1];
	char read2[MAX_USR_VAR_NAME_LEN + 1];

	int flow_deps[MAX_NUM_DEPENDS];
	int num_flow_deps;

	int anti_deps[MAX_NUM_DEPENDS];
	int num_anti_deps;

	int write_deps[MAX_NUM_DEPENDS];
	int num_write_deps;

	int dd_ifelse_id;
	int dd_ifelse_depth;
	int dd_inside_if;	// If not in if then in else OR outside of next
} Statement;

// Each index in the array corresponds to the statement in the TAC
Statement stmt_dep_array[MAX_NUM_STATEMENTS];
int num_stmts = 0;	// Number of statements in the TAC

int if_else_id_counter = 0;

// Given an index to stmt_dep_array, add to_append to the end of the specific
// dependency array specified by the type
void _dd_append_to_depend_array(int index, int to_append, int type)
{
	// Should NOT be num_stmts-1 because of calls from _dd_check_for_dependecies
	// which can write to the next element in the array
	if(index > num_stmts)
	{
		printf("Index %d out of bounds for num_stmts\n", index);
		return;
	}

	int end;

	switch(type)
	{
		case FLOW:
			if(stmt_dep_array[index].num_flow_deps >= MAX_NUM_DEPENDS)
			{
				printf("Statement %d exceeded max number of flow dependencies\n", index);
				exit(1);
			}

			end = stmt_dep_array[index].num_flow_deps;
			stmt_dep_array[index].flow_deps[end] = to_append;
			stmt_dep_array[index].num_flow_deps++;

			break;
		case ANTI:
			if(stmt_dep_array[index].num_anti_deps >= MAX_NUM_DEPENDS)
			{
				printf("Statement %d exceeded max number of anti dependencies\n", index);
				exit(1);
			}

			end = stmt_dep_array[index].num_anti_deps;
			stmt_dep_array[index].anti_deps[end] = to_append;
			stmt_dep_array[index].num_anti_deps++;

			break;
		case WRITE:
			if(stmt_dep_array[index].num_write_deps >= MAX_NUM_DEPENDS)
			{
				printf("Statement %d exceeded max number of write dependencies\n", index);
				exit(1);
			}

			end = stmt_dep_array[index].num_write_deps;
			stmt_dep_array[index].write_deps[end] = to_append;
			stmt_dep_array[index].num_write_deps++;
			break;
		default:
			printf("ERROR Unknown case for _dd_append_to_depend_array\n");
	}

	// printf("ADDED S%d to S%d for type %d\n", to_append, index, type);

	return;
}

// Verify dependency of variable (var_name) from the given statement (statement_num)
// that was found inside an if/else-statement
// The statement that called this function (in num_stmts index) is the checker can
// either be inside or outside an if/else statement
// This function looks forward from the statement with the potential dependency (statement_num)
// and sees if there are any future assignments in the same if/else statement that would
// block this dependency
// If there is an assignment that blocks this dependency, the function returns
// otherwise, it appends statement_num to the appropriate dependency array for the checker statement
// If there was an assignment after statement_num outside the if/else, previous rules
// would have already caught it and this function wouldn't be called
// Returns 1 if dependence will block all previous dependencies
int _dd_verify_ifelse_dependence(char *var_name, int statement_num, int type)
{
	// printf("%s in S%d looking back at S%d\n", var_name, num_stmts, statement_num);

	int is_checker_in_else = (stmt_dep_array[num_stmts].dd_ifelse_depth != 0) && !stmt_dep_array[num_stmts].dd_inside_if;
	int other_is_equal_or_deeper = stmt_dep_array[statement_num].dd_ifelse_depth >= stmt_dep_array[num_stmts].dd_ifelse_depth;
	int in_same_nest = stmt_dep_array[statement_num].dd_ifelse_id == stmt_dep_array[num_stmts].dd_ifelse_id;

	if(in_same_nest && is_checker_in_else && other_is_equal_or_deeper)
	{
		// printf("%s in S%d (ELSE) and S%d (equal or deeper down) not dependent => in different paths\n", var_name, num_stmts, statement_num);
		return 0;
	}

	int assigned_in_matching_ifelse = 0;
	// int matching_else_statement = -1;

	// Start after statement_num (statement with the potential dep.) and look forward
	// all the way up to just before the current statement that initiated the dependence check
	int i;
	for(i = statement_num + 1; i < num_stmts; i++)
	{
		// Determine attributes for the next statement
		int is_assigned_later = strcmp(stmt_dep_array[i].written, var_name) == 0;
		in_same_nest = stmt_dep_array[i].dd_ifelse_id == stmt_dep_array[statement_num].dd_ifelse_id;
		int has_less_eq_depth = stmt_dep_array[i].dd_ifelse_depth <= stmt_dep_array[statement_num].dd_ifelse_depth;
		int is_in_else = (stmt_dep_array[i].dd_ifelse_depth != 0) && !stmt_dep_array[i].dd_inside_if;

		// Check for assignments that are guaranteed to block
		if(is_assigned_later && in_same_nest && has_less_eq_depth)
		{
			int on_same_level = stmt_dep_array[i].dd_ifelse_depth == stmt_dep_array[statement_num].dd_ifelse_depth;
			
			if(is_in_else && on_same_level)
			{
				assigned_in_matching_ifelse = 1;
				// matching_else_statement = i;
			}
			
			if(is_in_else)
			{
				// If future assignment is in an else-statement at a depth equal 
				// or higher than statement being checked, it won't interfere with this dependency
				// printf("%s in S%d won't block be blocked by future assignment in S%d (else)\n", var_name, statement_num, i);
				continue;
			}
			else
			{
				// Is in the same if statement and further on at the same or higher up level => will block this statement being checked
				// printf("Future assignment in S%d blocks %s in S%d\n", i, var_name, statement_num);
				return 0;
			}
		}

		if(is_assigned_later && in_same_nest && is_in_else)
		{
			int is_one_deeper = (stmt_dep_array[i].dd_ifelse_depth - 1) == stmt_dep_array[statement_num].dd_ifelse_depth;

			if(is_one_deeper)
			{
				// If the future assignment is in an else-statement one nest deeper,
				// then it will ALSO have an assignment in the matching if and they will
				// block this dependency
				// printf("Future assignments in S%d and S%d inside else and the matching if will block %s in S%d\n", i, i-1, var_name, statement_num);
				return 0;
			}
		}
	}

	_dd_append_to_depend_array(num_stmts, statement_num, type);

	in_same_nest = stmt_dep_array[statement_num].dd_ifelse_id == stmt_dep_array[num_stmts].dd_ifelse_id;
	if((type == FLOW || type == WRITE) && in_same_nest)
	{
		// If flow or write dep was found inside if/else nest AND CHECKER IS ALSO IN
		// THE SAME NEST, this dependence can block the checker from all previous deps
		int dependency_is_le_depth = stmt_dep_array[statement_num].dd_ifelse_depth <= stmt_dep_array[num_stmts].dd_ifelse_depth;

		if(dependency_is_le_depth)
		{
			// If dependency is at higher depth it will block checker statement in lower depth
			// printf("For %s, S%d blocks prev. dep. for S%d\n", var_name, statement_num, num_stmts);
			return 1;
		}
		else if(assigned_in_matching_ifelse)
		{
			// printf("%s in S%d (IF) and %d (ELSE) blocks prev. dep. for S%d\n", var_name, statement_num, matching_else_statement, num_stmts);
			return 1;
		}
	}

	return 0;
}

// For a given variable on the current TAC line, check for dependence with
// previous lines; if there is, update the dependence of the previous line
// (if needed) and the dependence of the current line
void _dd_check_for_dependecies(char *var_name, int written)
{
	// Find the most recent dependencies
	int i;
	for(i = num_stmts - 1; i >= 0 ; i--)
	{
		// If var_name was written to this TAC line
		if(written)
		{
			// WRITE DEPENDENCE (write after write)
			if(strcmp(var_name, stmt_dep_array[i].written) == 0)
			{
				// If previous write found outside of if/else statement, it will
				// will block all previous dependencies, so we can exit loop
				if(stmt_dep_array[i].dd_ifelse_depth == 0)
				{
					_dd_append_to_depend_array(num_stmts, i, WRITE);
					break;
				}
				else if(stmt_dep_array[i].dd_ifelse_depth == 1 && !stmt_dep_array[i].dd_inside_if)
				{
					_dd_append_to_depend_array(num_stmts, i, WRITE);
					
					int same_var = strcmp(var_name, stmt_dep_array[i - 1].written) == 0;
					int same_depth = stmt_dep_array[i - 1].dd_ifelse_depth == 1;
					int same_nest =  stmt_dep_array[i].dd_ifelse_id == stmt_dep_array[i - 1].dd_ifelse_id;
					
					if(same_var && same_depth && same_nest)
					{
						// Case where previous assignment done in outer else end of outer if
						// will block all previous dependencies, so we can exit loop
						_dd_append_to_depend_array(num_stmts, i - 1, WRITE);
						break;
					}
				}
				else
				{
					int blocking = _dd_verify_ifelse_dependence(var_name, i, WRITE);
					if(blocking)
					{
						break;
					}
				}
			} // ANTI-DEPENDENCE (write after read)
			else if(strcmp(var_name, stmt_dep_array[i].read1) == 0
				 || strcmp(var_name, stmt_dep_array[i].read2) == 0)
			{
				// If previous read found outside of if/else statement
				if(stmt_dep_array[i].dd_ifelse_depth == 0)
				{
					_dd_append_to_depend_array(num_stmts, i, ANTI);
				}
				else
				{
					_dd_verify_ifelse_dependence(var_name, i, ANTI);
				}
				// We don't break from the loop in this case because we are finding previous reads,
				// not previous writes, and reads won't block dependencies
			}
		}
		else	// Variable is being read from
		{
			// FLOW DEPENDENCE (read after write)
			if(strcmp(var_name, stmt_dep_array[i].written) == 0)
			{
				// If previous write found outside of if/else statement, it will
				// will block all previous dependencies, so we can exit loop
				if(stmt_dep_array[i].dd_ifelse_depth == 0)
				{
					_dd_append_to_depend_array(num_stmts, i, FLOW);
					break;
				}
				else if(stmt_dep_array[i].dd_ifelse_depth == 1 && !stmt_dep_array[i].dd_inside_if)
				{
					_dd_append_to_depend_array(num_stmts, i, FLOW);
					
					int same_var = strcmp(var_name, stmt_dep_array[i - 1].written) == 0;
					int same_depth = stmt_dep_array[i - 1].dd_ifelse_depth == 1;
					int same_nest =  stmt_dep_array[i].dd_ifelse_id == stmt_dep_array[i - 1].dd_ifelse_id;
					
					if(same_var && same_depth && same_nest)
					{
						// Case where previous assignment done in outer else end of outer if
						// will block all previous dependencies, so we can exit loop
						_dd_append_to_depend_array(num_stmts, i - 1, FLOW);
						break;
					}
				}
				else
				{
					int blocking = _dd_verify_ifelse_dependence(var_name, i, FLOW);
					if(blocking)
					{
						break;
					}
				}
			}
		}
	}
	// printf("\n");

	return;
}

// Called by calc.y when if/else nest is left
// Create ID for next if/else statement
void dd_left_ifelse_nest()
{
	if_else_id_counter++;

	return;
}

// Called by calc.y every time a new TAC line is generated
// Recorded all variable names that appeared in the TAC line then call a
// functions to check for dependence with the previous lines
void dd_record_and_process(char *written, char *read1, char *read2, int dd_ifelse_depth, int dd_inside_if)
{
	if(num_stmts >= MAX_NUM_STATEMENTS)
	{
		printf("Max number of statements for data dependence exceeded (MAX=%d)\n", MAX_NUM_STATEMENTS);
		exit(1);
	}

	stmt_dep_array[num_stmts].num_flow_deps = 0;
	stmt_dep_array[num_stmts].num_anti_deps = 0;
	stmt_dep_array[num_stmts].num_write_deps = 0;
	stmt_dep_array[num_stmts].dd_ifelse_depth = dd_ifelse_depth;
	stmt_dep_array[num_stmts].dd_inside_if = dd_inside_if;		// Inside an if or else statement

	if(dd_ifelse_depth > 0)
	{
		stmt_dep_array[num_stmts].dd_ifelse_id = if_else_id_counter;
	}
	else // Outside if/else statement
	{
		stmt_dep_array[num_stmts].dd_ifelse_id = -1;
		stmt_dep_array[num_stmts].dd_inside_if = -1;	// Since it is outside an if/else, it is neither in an if (1) nor else (0)
	}

	if(written != NULL)		// Value written to will always be a variable
	{
		strcpy(stmt_dep_array[num_stmts].written, written);
		_dd_check_for_dependecies(written, 1);
	}
	else
	{
		strcpy(stmt_dep_array[num_stmts].written, "");
	}

	// Filter out constant values
	if(read1 != NULL && read1[0] >= 'A')
	{
		strcpy(stmt_dep_array[num_stmts].read1, read1);
		_dd_check_for_dependecies(read1, 0);
	}
	else
	{
		strcpy(stmt_dep_array[num_stmts].read1, "");
	}

	// Filter out constant values
	if(read2 != NULL && read2[0] >= 'A')
	{
		strcpy(stmt_dep_array[num_stmts].read2, read2);
		_dd_check_for_dependecies(read2, 0);
	}
	else
	{
		strcpy(stmt_dep_array[num_stmts].read2, "");
	}

	num_stmts++;

	return;
}

// Print out the dependencies of each statement
void dd_print_out_dependencies()
{
	printf("Printing dependencies:\n");
	int i;
	for(i = 0; i < num_stmts; i++)
	{
		printf("S%d:\nFlow-Dependence: ", i);
		if (stmt_dep_array[i].num_flow_deps == 0){
			printf("None");
		}
		else
		{
			int j;
			for(j = 0; j < stmt_dep_array[i].num_flow_deps; j++)
			{
				printf("S%d ", stmt_dep_array[i].flow_deps[j]);
			}
		}

		printf("\nAnti-dependence: ");
		if (stmt_dep_array[i].num_anti_deps == 0){
			printf("None");
		}
		else
		{
			int j;
			for(j = 0; j < stmt_dep_array[i].num_anti_deps; j++)
			{
				printf("S%d ", stmt_dep_array[i].anti_deps[j]);
			}
		}

		printf("\nWrite-dependence: ");
		if (stmt_dep_array[i].num_write_deps == 0){
			printf("None");
		}
		else
		{
			int j;
			for(j = 0; j < stmt_dep_array[i].num_write_deps; j++)
			{
				printf("S%d ", stmt_dep_array[i].write_deps[j]);
			}
		}

		printf("\n");
	}

	return;
}
