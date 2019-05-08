# Benjamin Steenkamer
# CPEG 621 Project - Calculator Compiler

# TO DO:
# What are common subexpr and copy-statement
#	Do they needed full data dependency graph?
# Data dependency
#	Implement
# 	Test
# Optimizations
#	Implement common subexpr
#		make heuristic
#	Implement copy-statement
#		make heuristic
#	Determine fix point
#		when both have an equal number of changes for 3 times in a row?
# 			what if weird oscillating pattern?

# To create the compiler middle end, type `make`
# The binary `calc` will be created in the main folder
# Then type `./calc Tests/<test_file_name> to run the compiler on an 
# 	input calculator program
# All output files from the middle end are placed in `Output/`

# Generate calculator compiler with ....
calc: calc.l calc.y calc.h c-code.c c-code.h
	bison -d calc.y
	flex calc.l
	gcc -O3 -Wall lex.yy.c calc.tab.c c-code.c -o calc

# Create calc.output for debugging
bison-dbg:
	bison -v calc.y

# Compile the outputted C code
# Disable gcc optimizations to see how effective my optimizations are
ccode: Output/backend.c Output/backend-timing.c
	gcc -O0 -o Output/prog Output/backend.c -lm
	gcc -O0 -o Output/prog-time Output/backend-timing.c -lm

# Same as above, but with warnings (will see unused labels)
ccodew: Output/backend.c Output/backend-timing.c
	gcc -Wall -O0 -o Output/prog Output/backend.c -lm
	gcc -Wall -O0 -o Output/prog-time Output/backend-timing.c -lm

clean:
	rm -f calc.tab.* lex.yy.c calc.output calc
	rm -f Output/*.txt Output/*.c Output/prog*
