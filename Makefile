# Benjamin Steenkamer
# CPEG 621 Project - Calculator Compiler

# To create the compiler middle end, type `make`
# The binary `calc` will be created in the main folder
# Then type `./calc Tests/<test_file_name> to run the compiler on an 
# 	input calculator program
# All output files from the middle end are placed in `Output/`

# Generate calculator compiler with
calc: calc.l calc.y calc.h c-code.c
	bison -d calc.y
	flex calc.l
	gcc -O3 -Wall lex.yy.c calc.tab.c c-code.c basic-block.c ssa.c -o calc

# Create calc.output for debugging
debug:
	bison -v calc.y

# Compile the outputted C code for the front-end TAC and basic block TAC
ccode: Output/c-frontend.c Output/c-basic-block.c
	gcc -O0 -o Output/prog-tacf Output/c-frontend.c -lm
	gcc -O0 -o Output/prog-tacbb Output/c-basic-block.c -lm

# Same as above, but with warnings (will see unused labels)
ccodew: Output/tac-frontend.c Output/tac-basic-block.c
	gcc -O0 -Wall -o Output/prog-tacf Output/c-frontend.c -lm
	gcc -O0 -Wall -o Output/prog-tacbb Output/c-basic-block.c -lm

clean:
	rm -f calc.tab.* lex.yy.c calc.output calc
	rm -f Output/*.txt Output/*.c Output/prog-*
