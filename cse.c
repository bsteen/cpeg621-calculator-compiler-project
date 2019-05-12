// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#include <stdio.h>
#include <stdlib.h>
#include "calc.h"
#include "cse.h"

void copy_to_file(char *source_name, char *dest_name)
{
	FILE *source = fopen(source_name, "r");
	FILE *dest = fopen(dest_name, "w");
	
	if(source == NULL)
	{
		printf("Couldn't open %s for copying to %s\n", source_name, dest_name);
		exit(1);
	}
	
	if(dest == NULL)
	{
		printf("Couldn't create %s\n", dest_name);
		exit(1);
	}
	
	char line[MAX_USR_VAR_NAME_LEN * 4];
	while(fgets(line, MAX_USR_VAR_NAME_LEN * 4, source) != NULL)
	{
		fprintf(dest, "%s", line);
	}
	
	fclose(source);
	fclose(dest);
	
	return;
}

// write optimizations to tac-optimized template