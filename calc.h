// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#define MAX_NUM_VARS 			256
#define MAX_USR_VAR_NAME_LEN	64
#define MAX_IFELSE_DEPTH		3

void copy_to_file(char *source_name, char *dest_name);

// Max if depth of 3
/*
if(...)
{
	if(...)
	{
		if(...)
		{
			
		}
		else
		{
			
		}
		...
	}
	else
	{
		
	}
	...
}
else
{
	
}
...
*/