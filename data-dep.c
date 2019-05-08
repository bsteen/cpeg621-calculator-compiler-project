// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <stdio.h>

#define MAX_NUM_STATEMENTS	256
#define MAX_NUM_DEPENDS		256

typedef struct Statement_struct
{
	int flow_deps[MAX_NUM_DEPENDS];
	int num_flow_deps;
	
	int anti_deps[MAX_NUM_DEPENDS];
	int num_anti_deps;
	
	int write_deps[MAX_NUM_DEPENDS];
	int num_write_deps;
} Statement;

// Each index in the array corresponds to the statement in the TAC
Statement stmt_dep_array[MAX_NUM_STATEMENTS];
int num_statements = 0;

void print_out_dependencies()
{
	
}