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
TEST_LINK_STATIC = 0
TEST_HARD_LINK = 1

ifneq ($(CROSS_COMPILE), "")
CXX=$(CROSS_COMPILE)g++
else
ifeq ($(CXX), "")
CXX ?= g++
endif
endif
SRCDIR ?= $(PWD)
OBJDIR ?= $(PWD)/build/$(shell $(CXX) -dumpmachine)/$(shell $(CXX) -dumpversion)
ifneq ($(PREFIX), "")
PREFIX := "/usr"
endif

ifeq ($(DEBUG), 1)
NOOPT := 1
endif

LIBLEAKTRACERPATH := libleaktracer

# Common flags
CXXFLAGS = -Wall -pthread
ifeq ($(NOOPT), 1)
# with -O0, functions are not inlined, so it's harder to get the backtrace on some architecture
# but you can use it if you want to debug leaktracer (if some could find the right optim option to pass to gcc to
# use with O0, it would be nice !)
#CXXFLAGS += -O0 -finline-functions-called-once 
CXXFLAGS += -O1
CXXFLAGS += -g3 -DLOGGER
else
CXXFLAGS += -O3
endif

# some architecture generate a lot more instuction than on x86 (mips, arm...), this make the functions not inlined
# make inline-limit big to force inline
CXXFLAGS += -finline-limit=10000
CPP_FLAGS += -I$(LIBLEAKTRACERPATH)/include -I$(LIBLEAKTRACERPATH)/src
DYNLIB_FLAGS=-fpic -DSHARED -Wl,-z,defs

CXXFLAGS += $(EXTRA_CXXFLAGS)

# File names
LTLIB = $(OBJDIR)/libleaktracer.a
LTLIBSO = $(OBJDIR)/libleaktracer.so

# Source files
SRCS := AllocationHandlers.cpp  MemoryTrace.cpp LeakTracerC.c
HEADERS := $(wildcard $(LIBLEAKTRACERPATH)/include/*) $(wildcard $(LIBLEAKTRACERPATH)/src/*hpp)

OBJS   := $(SRCS)
OBJS   := $(patsubst %.cpp,$(OBJDIR)/%.o,$(OBJS))
OBJS   := $(patsubst %.c,$(OBJDIR)/%.o,$(OBJS))
SHOBJS := $(SRCS)
SHOBJS := $(patsubst %.cpp,$(OBJDIR)/%.os,$(SHOBJS))
SHOBJS := $(patsubst %.c,$(OBJDIR)/%.os,$(SHOBJS))

TESTSSRC := $(wildcard tests/*.cc)
TESTSBIN := $(patsubst tests/%.cc,$(OBJDIR)/%.bin,$(TESTSSRC))

VPATH := $(LIBLEAKTRACERPATH)/src

# Library
all: $(LTLIB) $(LTLIBSO)

VPATH := $(LIBLEAKTRACERPATH)/src tests/
$(LTLIB): $(OBJS)
	ar rcs $(LTLIB) $(OBJS)

$(LTLIBSO): $(SHOBJS)
	$(CXX) $(LD_FLAGS) -shared $(DYNLIB_FLAGS) -o $(LTLIBSO) $(SHOBJS) -ldl -lpthread

$(OBJDIR)/%.os: %.c $(HEADERS)
	@[ -d $(OBJDIR) ] || mkdir -p $(OBJDIR)
	$(CXX) $(CPP_FLAGS) $(CXXFLAGS) $(DYNLIB_FLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.c $(HEADERS)
	@[ -d $(OBJDIR) ] || mkdir -p $(OBJDIR)
	$(CXX) $(CPP_FLAGS) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/%.os: %.cpp $(HEADERS)
	@[ -d $(OBJDIR) ] || mkdir -p $(OBJDIR)
	$(CXX) $(CPP_FLAGS) $(CXXFLAGS) $(DYNLIB_FLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.cpp $(HEADERS)
	@[ -d $(OBJDIR) ] || mkdir -p $(OBJDIR)
	$(CXX) $(CPP_FLAGS) $(CXXFLAGS) -c -o $@ $<


TESTRUNENV := LEAKTRACER_NOBANNER=1
ifeq ($(TEST_LINK_STATIC), 1)
TESTLINKDEP := $(LTLIB)
TESTLINKARGS := -L$(OBJDIR) $(LTLIB)
TESTRUNENV += LD_LIBRARY_PATH=$(OBJDIR) 
else
ifeq ($(TEST_HARD_LINK), 1)
TESTLINKDEP := $(LTLIBSO)
TESTLINKARGS := -L$(OBJDIR) -lleaktracer
TESTRUNENV += LD_LIBRARY_PATH=$(OBJDIR) 
else
TESTLINKDEP := 
TESTLINKARGS := 
TESTRUNENV += LD_PRELOAD=$(LTLIBSO) LD_LIBRARY_PATH=$(OBJDIR)
endif
endif

runtests: $(TESTSBIN)
ifeq ($(CROSS_COMPILE), "")
	@echo "Run tests not available when cross compiling for $(CROSS_COMPILE)"
else
	@[ -d $(OBJDIR)/tests ] || mkdir -p $(OBJDIR)/tests
	for testbin in $(TESTSBIN); do \
	  cd $(OBJDIR)/tests && $(TESTRUNENV) $${testbin}; \
	  $(SRCDIR)/helpers/leak-analyze-addr2line $${testbin} $(OBJDIR)/tests/leaks.out; \
	done
endif

tests: $(TESTSBIN)


$(OBJDIR)/%.bin: %.cc $(TESTLINKDEP) $(HEADERS)
	$(CXX) -o $@ $< -g2 -I$(LIBLEAKTRACERPATH)/include $(CXXFLAGS) -O0 $(TESTLINKARGS) -ldl -lpthread

clean:
	rm -f $(SHOBJS) $(LTLIBSO) $(OBJS) $(LTLIB) $(TESTSBIN) *~ *.out

install:
	install -d $(DESTDIR)/$(PREFIX)/include
	install -d $(DESTDIR)/$(PREFIX)/lib
	install -d $(DESTDIR)/$(PREFIX)/bin
	install -d $(DESTDIR)/$(PREFIX)/share/doc/leaktracer
	install -m 660 $(LIBLEAKTRACERPATH)/include/* $(DESTDIR)/$(PREFIX)/include
	install -m 770 $(LTLIBSO) $(DESTDIR)/$(PREFIX)/lib
	install -m 770 helpers/* $(DESTDIR)/$(PREFIX)/bin
	install -m 660 $(LTLIB) $(DESTDIR)/$(PREFIX)/lib
	install -m 660 README $(DESTDIR)/$(PREFIX)/share/doc/leaktracer
