// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#define NUM_ITERS 100000	 // For timing analysis, the number of time the generated code will be looped

void init_c_code();
void track_user_var(char *var, int assigned);
void gen_c_code(char * input, char * output, int num_temp_vars, int timing);
