// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calc.h"
#include "cse.h"

int cse_next_temp_var_name;
FILE *cse_temp_tac_ptr;

// Go through the input file and find the last assigned temporary variable.
// (will have the largest value in the form _t####); when doing CSE, can the 
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

// write optimizations to tac-optimized template
void cse_do_optimization(char *opt_tac_name, char *temp_tac_name)
{
	cse_temp_tac_ptr = fopen(temp_tac_name, "w"); // Clear contents of temp file for next opt iteration
	if (cse_temp_tac_ptr == NULL)
	{
		printf("CSE couldn't open %s for writing next optimization to\n", temp_tac_name);
		exit(1);
	}
	
	_cse_init_first_temp_var_name(opt_tac_name);
	
	
	
	fclose(cse_temp_tac_ptr);
	
	// Copy contents from temp file back to main file
	copy_to_file(opt_tac_name, temp_tac_name);
}