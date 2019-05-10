// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calc.h"
#include "data-dep.h"

#define MAX_NUM_STATEMENTS	256
#define MAX_NUM_DEPENDS		256

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

	int ifelse_id;
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

	return;
}

// Verify dependency of variable (var_name) from the given statement (statement_num)
// that was found inside an if/else-statement
// Look forward from the statement with the potential dependency and see if
// there are any future assignments in the same if/else statement that would
// block this dependency
// If there is an assignment that blocks this dependency, the function returns
// Otherwise, it appends statement_num to the appropriate dependency array for the current statement
void _dd_verify_ifelse_dependence(char *var_name, int statement_num, int type)
{

	printf("%s sn=%d, num_stmts=%d\n", var_name, statement_num, num_stmts);

	// Start after statement_num (statement with the potential dep.) and look forward
	// all the way up to just before the current statement that initiated the dependence check
	int i;
	for(i = statement_num + 1; i < num_stmts; i++)
	{
		// Determine attributes for the next statement
		int is_assigned_later = strcmp(stmt_dep_array[i].written, var_name) == 0;
		int is_in_same_ifelse = stmt_dep_array[i].ifelse_id == stmt_dep_array[statement_num].ifelse_id;
		int has_less_eq_depth = stmt_dep_array[i].dd_ifelse_depth <= stmt_dep_array[statement_num].dd_ifelse_depth;
		int is_in_else = !stmt_dep_array[i].dd_inside_if;
		
		// Check for assignments that are guaranteed to block
		if(is_assigned_later && is_in_same_ifelse && has_less_eq_depth)
		{
			if(is_in_else)
			{
				// If future assignment is in an else-statement, it won't interfere
				// with this dependency
				printf("Future assignment in S%d WON'T block %s in S%d\n", i, var_name, statement_num);
				continue;
			}
			else
			{
				printf("Future assignment in S%d blocks %s in S%d\n", i, var_name, statement_num);
				return;
			}
		}

		int is_one_deeper = (stmt_dep_array[i].dd_ifelse_depth - 1) == stmt_dep_array[statement_num].dd_ifelse_depth;
		if(is_in_else && is_one_deeper)
		{
			// If the future assignment is in an else-statement one nest deeper,
			// then it will also have an assignment in the matching if and they will
			// block this dependency
			printf("Future assignment in S%d inside else (and the matching if) blocks %s in S%d\n", i, var_name, statement_num);
			return;
		}
	}

	// Cases where statements are right next to each other, so loop was skipped
	// This isn't necessarily bad, but there are special cases that need to be caught
	int back_to_back = statement_num + 1 == num_stmts;
	if(back_to_back)
	{
		printf("Statements S%d and S%d were back to back, loop was skipped\n", statement_num, num_stmts);
	}
	
	// Statement checking for dependences is in else, it will have an assignment right before
	// these two assignments won't have dependence between each other
	int same_if_statement = stmt_dep_array[statement_num].ifelse_id == stmt_dep_array[num_stmts].ifelse_id;
	int checker_in_else = !stmt_dep_array[num_stmts].dd_inside_if;
	int other_in_if = stmt_dep_array[statement_num].dd_inside_if;
	
	if(same_if_statement && back_to_back && checker_in_else && other_in_if)
	{
		printf("For %s, S%d and S%d not dependent because in different paths\n", var_name, statement_num, num_stmts);
		return;
	}
	
	_dd_append_to_depend_array(num_stmts, statement_num, type);

	return;
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
					// Case where previous assignment done in outer else (and therefore outer if also)
					// will block all previous dependencies, so we can exit loop
					_dd_append_to_depend_array(num_stmts, i - 1, WRITE);
					_dd_append_to_depend_array(num_stmts, i, WRITE);
					break;
				}
				else
				{	// Case where previous write found in outer/inner if or inner else
					_dd_verify_ifelse_dependence(var_name, i, WRITE);
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
					// Case where previous assignment done in outer else (and therefore also outer if)
					// will block all previous dependencies, so we can exit loop
					_dd_append_to_depend_array(num_stmts, i - 1, FLOW);
					_dd_append_to_depend_array(num_stmts, i, FLOW);
					break;
				}
				else
				{
					_dd_verify_ifelse_dependence(var_name, i, FLOW);
				}
			}
		}
	}

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
		stmt_dep_array[num_stmts].ifelse_id = if_else_id_counter;
	}
	else // Outside if/else statement
	{
		stmt_dep_array[num_stmts].ifelse_id = -1;
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
