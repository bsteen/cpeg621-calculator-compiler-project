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
} Statement;

// Each index in the array corresponds to the statement in the TAC
Statement stmt_dep_array[MAX_NUM_STATEMENTS];
int num_stmts = 0;	// Number of statements in the TAC

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

// For a given variable on the current TAC line, check for dependence with
// previous lines; if there is, update the dependence of the previous line
// (if needed) and the dependence of the current line
void _dd_check_for_dependecies(char *var, int written)
{
	// Find the most recent dependencies
	int i;
	for(i = num_stmts - 1; i >= 0 ; i--)
	{
		// If var was written to this TAC line
		if(written)
		{
			// Write dependence (write after write)
			if(strcmp(var, stmt_dep_array[i].written) == 0)
			{
				// If found outside of if/else statement
				_dd_append_to_depend_array(num_stmts, i, WRITE);
				break;
			}
			else if(strcmp(var, stmt_dep_array[i].read1) == 0	
				 || strcmp(var, stmt_dep_array[i].read2) == 0)	// Anti-dependence (write after read)
			{	
				// If found outside of if/else statement
				_dd_append_to_depend_array(num_stmts, i, ANTI);
			}
		}
		else
		{
			// Can ignore previous reads, since no write in this case
			// Flow dependence (read after write)
			if(strcmp(var, stmt_dep_array[i].written) == 0)
			{
				// If found outside of if/else statement
				_dd_append_to_depend_array(num_stmts, i, FLOW);
				break;
			}
		}
	}

	return;
}

// Called by calc.y every time a new TAC line is generated
// Recorded all variable names that appeared in the TAC line then call a 
// functions to check for dependence with the previous lines
void dd_record_and_process(char *written, char *read1, char *read2)
{
	if(num_stmts >= MAX_NUM_STATEMENTS)
	{
		printf("Max number of statements for data dependence exceeded (MAX=%d)\n", MAX_NUM_STATEMENTS);
		exit(1);
	}

	stmt_dep_array[num_stmts].num_flow_deps = 0;
	stmt_dep_array[num_stmts].num_anti_deps = 0;
	stmt_dep_array[num_stmts].num_write_deps = 0;

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
