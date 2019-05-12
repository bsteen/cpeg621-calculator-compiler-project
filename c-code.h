// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#define NUM_ITERS 100000	 // For timing analysis, the number of time the generated code will be looped
#define MAX_NUM_TEMP_VARS 1024

void init_c_code();
void track_user_var(char *var, int assigned);
void gen_c_code(char * input, char * output, int timing);
