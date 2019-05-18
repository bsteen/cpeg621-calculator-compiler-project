# Benjamin Steenkamer
# CPEG 621 Project - Calculator Compiler
#	Heuristic
#		Need good tests to demonstrate them working together
#
# Remove printout and stop points; enable DD
# Check everything up to now
# Start report
#
# Get 3 deep to work
#	Test
#	Test with dependency
# 	Test with optimizations

# To create the compiler middle end, type `make`
# The binary `calc` will be created in the main folder
# Then type `./calc Tests/<test_file_name> to run the compiler on an 
# 	input calculator program
# All output files from the middle end are placed in `Output/`

# Generate calculator compiler with ....
calc: calc.l calc.y calc.h c-code.c c-code.h data-dep.c data-dep.h cse.c cse.h copy-stmt-elim.c copy-stmt-elim.h
	bison -d calc.y
	flex calc.l
	gcc -O3 -Wall lex.yy.c calc.tab.c c-code.c data-dep.c cse.c copy-stmt-elim.c -o calc

# Create calc.output for debugging
bison-dbg:
	bison -v calc.y

# Compile the outputted C code
# Disable gcc optimizations to see how effective my optimizations are
ccode: Output/backend.c Output/backend-opt.c Output/backend-timing.c Output/backend-opt-timing.c
	gcc -O0 -o Output/prog Output/backend.c -lm
	gcc -O0 -o Output/prog-opt Output/backend-opt.c -lm
	gcc -O0 -o Output/prog-time Output/backend-timing.c -lm
	gcc -O0 -o Output/prog-opt-time Output/backend-opt-timing.c -lm

# Same as above, but with warnings (will see unused labels)
ccodew: Output/backend.c Output/backend-opt.c Output/backend-timing.c Output/backend-opt-timing.c
	gcc -Wall -O0 -o Output/prog Output/backend.c -lm
	gcc -Wall -O0 -o Output/prog-opt Output/backend-opt.c -lm
	gcc -Wall -O0 -o Output/prog-time Output/backend-timing.c -lm
	gcc -Wall -O0 -o Output/prog-opt-time Output/backend-opt-timing.c -lm
	

clean:
	rm -f calc.tab.* lex.yy.c calc.output calc
	rm -f Output/*.txt Output/*.c Output/prog*
