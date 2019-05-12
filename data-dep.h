// Benjamin Steenkamer
// CPEG 621 Project - Calculator Compiler

#define FLOW 	0
#define ANTI 	1
#define WRITE	2 

void dd_print_out_dependencies();
void dd_record_and_process(char *written, char *read1, char *read2, int dd_ifelse_depth, int dd_inside_if);
void dd_left_ifelse_nest();
