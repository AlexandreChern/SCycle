all: main

DEBUG_MODULES   = -DVERBOSE=1 -DODEPRINT=0 -DDEBUG=0
CFLAGS          = $(DEBUG_MODULES)
FFLAGS	        = -I${PETSC_DIR}/include/finclude
CLINKER		      = openmpicc

OBJECTS := domain.o debuggingFuncs.o fault.o genFuncs.o linearElastic.o\
 maxwellViscoelastic.o odeSolver.o rootFinder.o sbpOps.o spmat.o testOdeSolver.o

include ${PETSC_DIR}/conf/variables
include ${PETSC_DIR}/conf/rules

main:  main.o $(OBJECTS)
	-${CLINKER} $^ -o $@ ${PETSC_SYS_LIB}
	-rm main.o

testMain: testMain.o $(OBJECTS)
	-${CLINKER} $^ -o $@ ${PETSC_SYS_LIB}
	-rm testMain.o

helloWorld: helloWorld.o
	-${CLINKER} $^ -o $@ ${PETSC_SYS_LIB}
	-rm helloWorld.o


#.PHONY : clean
clean::
	-rm -f *.o main helloWorld

depend:
	-g++ -MM *.c*

include ${PETSC_DIR}/conf/test

#=========================================================
# Dependencies
#=========================================================

fault.o: fault.cpp fault.hpp genFuncs.hpp domain.hpp \
 rootFinderContext.hpp rootFinder.hpp
genFuncs.o: genFuncs.cpp genFuncs.hpp
helloWorld.o: helloWorld.cpp
linearElastic.o: linearElastic.cpp linearElastic.hpp \
 integratorContext.hpp odeSolver.hpp genFuncs.hpp domain.hpp sbpOps.hpp \
 debuggingFuncs.hpp spmat.hpp fault.hpp rootFinderContext.hpp \
 rootFinder.hpp
main.o: main.cpp genFuncs.hpp spmat.hpp domain.hpp sbpOps.hpp \
 debuggingFuncs.hpp fault.hpp rootFinderContext.hpp rootFinder.hpp \
 linearElastic.hpp integratorContext.hpp odeSolver.hpp \
 maxwellViscoelastic.hpp
maxwellViscoelastic.o: maxwellViscoelastic.cpp maxwellViscoelastic.hpp \
 integratorContext.hpp odeSolver.hpp genFuncs.hpp domain.hpp \
 linearElastic.hpp sbpOps.hpp debuggingFuncs.hpp spmat.hpp fault.hpp \
 rootFinderContext.hpp rootFinder.hpp
odeSolver.o: odeSolver.cpp odeSolver.hpp integratorContext.hpp \
 genFuncs.hpp
rootFinder.o: rootFinder.cpp rootFinder.hpp rootFinderContext.hpp
sbpOps_arrays.o: sbpOps_arrays.cpp sbpOps.hpp domain.hpp genFuncs.hpp \
 debuggingFuncs.hpp spmat.hpp
sbpOps.o: sbpOps.cpp sbpOps.hpp domain.hpp genFuncs.hpp \
 debuggingFuncs.hpp spmat.hpp
spmat.o: spmat.cpp spmat.hpp
#~testMain.o: testMain.cpp genFuncs.hpp domain.hpp sbpOps.hpp \
#~ debuggingFuncs.hpp spmat.hpp testOdeSolver.hpp integratorContext.hpp \
#~ odeSolver.hpp
testMain.o: testMain.cpp genFuncs.hpp domain.hpp sbpOps.hpp \
 testOdeSolver.hpp odeSolver.hpp
testOdeSolver.o: testOdeSolver.cpp testOdeSolver.hpp \
 integratorContext.hpp odeSolver.hpp genFuncs.hpp





