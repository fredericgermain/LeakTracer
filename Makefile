CC = g++

# Source files
SRC := LeakTracer.cc

# Comment out to disable thread safetly
THREAD=-DTHREAD_SAVE -D_REENTRANT -D_THREAD_SAFE -pthread 

# Common flags
C_FLAGS = -g -pipe -Wall -W $(THREAD)
O_FLAGS = $(C_FLAGS)

# Object files
OBJ_DIR = .
OBJ   := $(patsubst %.cc,$(OBJ_DIR)/%.o,$(SRC))
SHOBJ := $(patsubst %.o,$(OBJ_DIR)/%.so,$(OBJ))

.PHONY: all clean tidy distrib test

all: $(OBJ) $(SHOBJ)

clean:	tidy 
	rm -f $(OBJ) leak.out

tidy:
	rm -f *~ *orig *bak *rej

tags:	$(SRC) $(INCL)
	ctags $(SRC) $(INCL)

distrib: clean all README.html
	(cd .. && tar cvfz  /u/erwin/drylock/LeakTracer/LeakTracer.tar.gz -X LeakTracer/.tarexcl  LeakTracer/)

$(OBJ_DIR)/%.o: %.cc
	$(CC) -fPIC -c $(C_FLAGS) $< -o $@

$(OBJ_DIR)/%.so : $(OBJ_DIR)/%.o
	$(CC) $(O_FLAGS) -shared -o $@ $<

README.html: README
	/u/erwin/ed/mcl/util/htmlize.pl README

test:
	$(CC) $(C_FLAGS) test.cc -o test
	./test
	./LeakCheck ./test
	./leak-analyze ./test
#	./compare-test test.template test.result
