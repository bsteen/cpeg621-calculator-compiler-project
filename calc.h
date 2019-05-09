// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#define MAX_NUM_VARS 			128
#define MAX_USR_VAR_NAME_LEN	64
#define MAX_IF_DEPTH			3

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