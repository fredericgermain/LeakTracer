#///////////////////////////////////////////////////////
#
# LeakTracer
# Contribution to original project by Erwin S. Andreasen
# site: http://www.andreasen.org/LeakTracer/
#
# Added by Michael Gopshtein, 2006
# mgopshtein@gmail.com
# 
# Any comments/suggestions are welcome
# 
#///////////////////////////////////////////////////////

CC = g++

# Common flags
C_FLAGS = -Wall -pthread -ggdb

# File names
LTLIB = libleaktrace.a
TESTAPP = testLeaktrace

# Library
all: $(LTLIB)

$(LTLIB): AllocationHandlers.o MemoryTrace.o
	ar cr $@ AllocationHandlers.o MemoryTrace.o

%.o: %.cpp
	$(CC) $(C_FLAGS) -c $(C_FLAGS) -o $@ $<

test: $(TESTAPP)
	./$(TESTAPP)
	./parse_leaktracer_out.pl testLeaktrace leaks.out

$(TESTAPP): test.cc $(LTLIB)
	$(CC) -o $(TESTAPP) test.cc $(C_FLAGS) -L. -lleaktrace

clean:
	rm -f *.o $(LTLIB) $(TESTAPP) *~ *.out


