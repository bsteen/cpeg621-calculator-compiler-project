// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <stdio.h>
#include <stdlib.h>
#include "calc.h"
#include "copy-stmt-elim.h"

FILE *copy_stmnt_temp_tac_ptr;

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