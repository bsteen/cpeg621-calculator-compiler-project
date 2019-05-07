// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "calc.h"		// To get calc compiler global constants/defines
#include "c-code.h"

int num_user_vars;				// Number of user variables in use
int num_user_vars_wo_def;		// Number of user variables that didn't have declarations
char user_vars[MAX_NUM_VARS][MAX_USR_VAR_NAME_LEN + 1];			// List of all unique user vars in proper
char user_vars_wo_def[MAX_NUM_VARS][MAX_USR_VAR_NAME_LEN + 1];	// List of user vars used w/o definition

void init_c_code()
{
	num_user_vars = 0;
	num_user_vars_wo_def = 0;
	
	return;
}

// Records all first appearances of user variables for use in C code generation
// If variable is not being defined and hasn't been used before, add it to list of uninitialized variables
void track_user_var(char *var, int assigned)
{
	// Check if variable has been recorded before
	int i;
	for(i = 0; i < num_user_vars; i++)
	{
		if(strcmp(user_vars[i], var) == 0)
		{
			return; // If the variable was already recorded, don't need to record it again
		}
	}

	// Check if variable is valid
	if(num_user_vars >= MAX_NUM_VARS)
	{
		printf("Max number of user variables reached (%d)\n", MAX_NUM_VARS);
		exit(1);	// Exit since variable (and therefor the entire program) is not valid
	}
	else if (strlen(var) > MAX_USR_VAR_NAME_LEN)
	{
		printf("Variable name (%s) too long: %lu > MAX_USR_VAR_NAME_LEN (%d)\n", var, strlen(var), MAX_USR_VAR_NAME_LEN);
		exit(1); 	// Exit since variable (and therefor the entire program) is not valid
	}

	// If the variable hasn't been seen before, need to record its first appearance
	if(!assigned)	// If variable is not being assigned a value, then it's first use is without a definition
	{
		strcpy(user_vars_wo_def[num_user_vars_wo_def], var);
		num_user_vars_wo_def++;
	}

	strcpy(user_vars[num_user_vars], var);
	num_user_vars++;

	return;
}

// Take the TAC and generate a valid C program code
void gen_c_code(char * input, char * output, int num_temp_vars)
{
	// Open files for reading TAC and writing C code
	FILE * tac_file = fopen(input, "r");
	FILE * c_code_file = fopen(output, "w");
	
	if (tac_file == NULL)
	{
		printf("Couldn't open TAC file in C code generation step\n");
		exit(1);
	}
	if (c_code_file == NULL)
	{
		printf("Couldn't create C code output file\n");
		exit(1);
	}

	int i;
	fprintf(c_code_file, "#include <stdio.h>\n#include <math.h>\n\nint main() {\n");

	// Declare all user variables and initialize them to 0
	if (num_user_vars > 0)
	{
		fprintf(c_code_file, "\tint ");
	}
	for(i = 0; i < num_user_vars; i++)
	{
		if (i != num_user_vars - 1)
		{
			fprintf(c_code_file, "%s = 0, ", user_vars[i]);
		}
		else
		{
			fprintf(c_code_file, "%s = 0;\n", user_vars[i]);
		}
	}

	// Declare all temp variables and initialize them to 0
	if (num_temp_vars > 0)
	{
		fprintf(c_code_file, "\tint ");
	}
	for(i = 0; i < num_temp_vars; i++)
	{
		if(i < num_temp_vars - 1)
		{
			fprintf(c_code_file, "_t%d = 0, ", i);
		}
		else
		{
			fprintf(c_code_file, "_t%d = 0;\n", i);
		}
	}

	fprintf(c_code_file, "\n");

	// Initialize user variables not assigned (ask user input for variables)
	for (i = 0; i < num_user_vars_wo_def; i++)
	{
		fprintf(c_code_file, "\tprintf(\"%s=\");\n", user_vars_wo_def[i]);
		fprintf(c_code_file, "\tscanf(\"%%d\", &%s);\n\n", user_vars_wo_def[i]);
	}

	// Read in TAC file, write to c file with line labels
	// Convert lines with ** or ! and replace with pow or ~
	char line_buf[MAX_USR_VAR_NAME_LEN * 4];
	char *bitwise;
	char *pow;
	i = 0;
	while(fgets(line_buf, MAX_USR_VAR_NAME_LEN * 4, tac_file) != NULL)
	{
		// Don't print label if line is a closing }, else statements, or labels
		if(strcmp(line_buf, "\n") == 0 || strstr(line_buf, "}") != NULL || strstr(line_buf, ":") != NULL)
		{
			fprintf(c_code_file, "\t\t\t%s", line_buf);
			continue;
		}

		bitwise = strstr(line_buf, "!");
		pow = strstr(line_buf, "**");

		if(bitwise != NULL) 		// Replace ! with ~
		{
			*bitwise = '~';
		}
		else if(pow != NULL)		// Split up the line with a ** and reformat it with a pow() func
		{
			char temp[MAX_USR_VAR_NAME_LEN * 4];
			strcpy(temp, line_buf);

			char *first = strtok(temp, " \t=*;");		// Lines with ** will always have 3 operands
			char *second = strtok(NULL, " \t=*;");
			char *third = strtok(NULL, " \t=*;");
			
			if(line_buf[0] == '\t')		// Case for basic block code to maintain leading tab
			{
				sprintf(line_buf, "\t%s = (int)pow(%s, %s);\n", first, second, third);
			}
			else{
				sprintf(line_buf, "%s = (int)pow(%s, %s);\n", first, second, third);
			}
		}

		// Print c code line with line # label
		if(i < 10)
		{
			fprintf(c_code_file, "\tS%d:\t\t%s", i, line_buf);
		}
		else
		{
			fprintf(c_code_file, "\tS%d:\t%s", i, line_buf);
		}

		i++;	// Increment line number
	}

	fprintf(c_code_file, "\n");

	// Print out user variable final values
	for(i = 0; i < num_user_vars; i++)
	{
		fprintf(c_code_file, "\tprintf(\"%s=%%d\\n\", %s);\n", user_vars[i], user_vars[i]);
	}

	fprintf(c_code_file, "\n\treturn 0;\n}\n");

	// Close files from C code generation
	fclose(tac_file);
	fclose(c_code_file);

	return;
}
