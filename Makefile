all: main

DEBUG_MODULES   = -DVERBOSE=2 -DODEPRINT=0 -DDEBUG=0
CFLAGS          = $(DEBUG_MODULES)
FFLAGS	        = -I${PETSC_DIR}/include/finclude
CLINKER		      = openmpicc

include ${PETSC_DIR}/conf/variables
include ${PETSC_DIR}/conf/rules

main:  main.o genFuncs.o debuggingFuncs.o odeSolver.o sbpOps.o linearElastic.o fault.o\
 domain.o rootFinder.o debuggingFuncs.o spmat.o maxwellViscoelastic.o
	-${CLINKER} $^ -o $@ ${PETSC_SYS_LIB}
	-rm main.o

testMain: testMain.o testOdeSolver.o odeSolver.o
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

genFuncs.o: genFuncs.cpp genFuncs.hpp
testOdeSolver.o: testOdeSolver.cpp integratorContext.hpp odeSolver.hpp testOdeSolver.hpp
domain.o: domain.cpp domain.hpp
sbpOps.o: sbpOps.cpp sbpOps.hpp genFuncs.hpp domain.hpp debuggingFuncs.hpp spmat.hpp
fault.o: fault.cpp fault.hpp genFuncs.hpp domain.hpp rootFinderContext.hpp rootFinder.hpp
linearElastic.o: linearElastic.cpp linearElastic.hpp genFuncs.hpp domain.hpp sbpOps.hpp \
 debuggingFuncs.hpp fault.hpp integratorContext.hpp
maxwellViscoelastic.o: maxwellViscoelastic.cpp maxwellViscoelastic.hpp genFuncs.hpp domain.hpp linearElastic.hpp
main.o: main.cpp linearElastic.hpp domain.hpp spmat.hpp sbpOps.hpp
debuggingFuncs.o: debuggingFuncs.cpp debuggingFuncs.hpp genFuncs.hpp
odeSolver.o: odeSolver.cpp odeSolver.hpp genFuncs.hpp integratorContext.hpp
rootFinder.o: rootFinder.cpp rootFinder.hpp rootFinderContext.hpp
spmat.o: spmat.cpp spmat.hpp

