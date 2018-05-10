all: main

DEBUG_MODULES   = -DVERBOSE=1 -DODEPRINT=0
CFLAGS          = $(DEBUG_MODULES)
CPPFLAGS        = $(CFLAGS) -std=c++11
FFLAGS	        = -I${PETSC_DIR}/include/finclude
CLINKER		      = openmpicc

OBJECTS := domain.o fault.o genFuncs.o\
 odeSolver.o rootFinder.o \
 spmat.o powerLaw.o heatEquation.o \
 sbpOps_c.o sbpOps_fc.o sbpOps_sc.o  sbpOps_fc_coordTrans.o \
 odeSolverImex.o odeSolver_WaveEq.o pressureEq.o \
 strikeSlip_linearElastic_qd.o linearElastic.o \
 strikeSlip_powerLaw_qd.o powerLaw.o \
 iceStream_linearElastic_qd.o strikeSlip_linearElastic_dyn.o strikeSlip_linearElastic_switch.o \
 newFault.o


include ${PETSC_DIR}/lib/petsc/conf/variables
include ${PETSC_DIR}/lib/petsc/conf/rules


main:  main.o $(OBJECTS)
	-${CLINKER} $^ -o $@ ${PETSC_SYS_LIB}
	-rm main.o

FDP: FDP.o
	-${CLINKER} $^ -o $@ ${PETSC_SYS_LIB}

helloWorld: helloWorld.o
	-${CLINKER} $^ -o $@ ${PETSC_SYS_LIB}
	-rm helloWorld.o

test: test.o
	-${CLINKER} $^ -o $@ ${PETSC_SYS_LIB}
	-rm test.o

#.PHONY : clean
clean::
	-rm -f *.o main

depend:
	-g++ -MM *.c*

include ${PETSC_DIR}/lib/petsc/conf/test

#=========================================================
# Dependencies
#=========================================================
domain.o: domain.cpp domain.hpp genFuncs.hpp
fault.o: fault.cpp fault.hpp genFuncs.hpp domain.hpp heatEquation.hpp \
 sbpOps.hpp sbpOps_c.hpp spmat.hpp sbpOps_fc.hpp sbpOps_fc_coordTrans.hpp \
 integratorContextEx.hpp odeSolver.hpp integratorContextImex.hpp \
 odeSolverImex.hpp rootFinderContext.hpp rootFinder.hpp
newFault.o: newFault.cpp newFault.hpp genFuncs.hpp domain.hpp heatEquation.hpp \
 sbpOps.hpp sbpOps_c.hpp spmat.hpp sbpOps_fc.hpp sbpOps_fc_coordTrans.hpp \
 rootFinderContext.hpp rootFinder.hpp
genFuncs.o: genFuncs.cpp genFuncs.hpp
heatEquation.o: heatEquation.cpp heatEquation.hpp genFuncs.hpp domain.hpp \
 sbpOps.hpp sbpOps_c.hpp spmat.hpp sbpOps_fc.hpp sbpOps_fc_coordTrans.hpp \
 integratorContextEx.hpp odeSolver.hpp integratorContextImex.hpp \
 odeSolverImex.hpp
helloWorld.o: helloWorld.cpp
iceStream_linearElastic_qd.o: iceStream_linearElastic_qd.cpp \
 iceStream_linearElastic_qd.hpp integratorContextEx.hpp genFuncs.hpp \
 odeSolver.hpp integratorContextImex.hpp odeSolverImex.hpp domain.hpp \
 sbpOps.hpp sbpOps_c.hpp spmat.hpp sbpOps_fc.hpp sbpOps_fc_coordTrans.hpp \
 fault.hpp heatEquation.hpp rootFinderContext.hpp rootFinder.hpp \
 pressureEq.hpp linearElastic.hpp
linearElastic.o: linearElastic.cpp linearElastic.hpp genFuncs.hpp \
 domain.hpp sbpOps.hpp sbpOps_c.hpp spmat.hpp sbpOps_fc.hpp \
 sbpOps_fc_coordTrans.hpp
main.o: main.cpp genFuncs.hpp spmat.hpp domain.hpp sbpOps.hpp fault.hpp \
 heatEquation.hpp sbpOps_c.hpp sbpOps_fc.hpp sbpOps_fc_coordTrans.hpp \
 integratorContextEx.hpp odeSolver.hpp integratorContextImex.hpp \
 odeSolverImex.hpp rootFinderContext.hpp rootFinder.hpp linearElastic.hpp \
 powerLaw.hpp pressureEq.hpp strikeSlip_linearElastic_qd.hpp \
 strikeSlip_powerLaw_qd.hpp newFault.hpp strikeSlip_linearElastic_switch.hpp
mainLinearElastic.o: mainLinearElastic.cpp genFuncs.hpp spmat.hpp \
 domain.hpp sbpOps.hpp sbpOps_fc.hpp sbpOps_c.hpp sbpOps_sc.hpp \
 sbpOps_fc_coordTrans.hpp fault.hpp heatEquation.hpp \
 integratorContextEx.hpp odeSolver.hpp integratorContextImex.hpp \
 odeSolverImex.hpp rootFinderContext.hpp rootFinder.hpp linearElastic.hpp
odeSolver.o: odeSolver.cpp odeSolver.hpp integratorContextEx.hpp \
 genFuncs.hpp
odeSolverImex.o: odeSolverImex.cpp odeSolverImex.hpp \
 integratorContextImex.hpp genFuncs.hpp odeSolver.hpp \
 integratorContextEx.hpp
odeSolver_WaveEq.o: odeSolver_WaveEq.cpp odeSolver_WaveEq.hpp \
 integratorContextWave.hpp genFuncs.hpp domain.hpp odeSolver.hpp \
 integratorContextEx.hpp
powerLaw.o: powerLaw.cpp powerLaw.hpp genFuncs.hpp domain.hpp \
 heatEquation.hpp sbpOps.hpp sbpOps_c.hpp spmat.hpp sbpOps_fc.hpp \
 sbpOps_fc_coordTrans.hpp integratorContextEx.hpp odeSolver.hpp \
 integratorContextImex.hpp odeSolverImex.hpp
pressureEq.o: pressureEq.cpp pressureEq.hpp genFuncs.hpp domain.hpp \
 fault.hpp heatEquation.hpp sbpOps.hpp sbpOps_c.hpp spmat.hpp \
 sbpOps_fc.hpp sbpOps_fc_coordTrans.hpp integratorContextEx.hpp \
 odeSolver.hpp integratorContextImex.hpp odeSolverImex.hpp \
 rootFinderContext.hpp rootFinder.hpp
rootFinder.o: rootFinder.cpp rootFinder.hpp rootFinderContext.hpp
sbpOps_arrays.o: sbpOps_arrays.cpp sbpOps.hpp domain.hpp genFuncs.hpp
sbpOps_c.o: sbpOps_c.cpp sbpOps_c.hpp domain.hpp genFuncs.hpp spmat.hpp \
 sbpOps.hpp
sbpOps_fc_coordTrans.o: sbpOps_fc_coordTrans.cpp sbpOps_fc_coordTrans.hpp \
 domain.hpp genFuncs.hpp spmat.hpp sbpOps.hpp
sbpOps_fc.o: sbpOps_fc.cpp sbpOps_fc.hpp domain.hpp genFuncs.hpp \
 spmat.hpp sbpOps.hpp
sbpOps_sc.o: sbpOps_sc.cpp sbpOps_sc.hpp domain.hpp genFuncs.hpp \
 spmat.hpp sbpOps.hpp
spmat.o: spmat.cpp spmat.hpp
strikeSlip_linearElastic_qd.o: strikeSlip_linearElastic_qd.cpp \
 strikeSlip_linearElastic_qd.hpp integratorContextEx.hpp genFuncs.hpp \
 odeSolver.hpp integratorContextImex.hpp odeSolverImex.hpp domain.hpp \
 sbpOps.hpp sbpOps_c.hpp spmat.hpp sbpOps_fc.hpp sbpOps_fc_coordTrans.hpp \
 fault.hpp heatEquation.hpp rootFinderContext.hpp rootFinder.hpp \
 pressureEq.hpp linearElastic.hpp newFault.hpp
strikeSlip_linearElastic_dyn.o: strikeSlip_linearElastic_dyn.cpp \
 strikeSlip_linearElastic_qd.hpp integratorContextEx.hpp genFuncs.hpp \
 odeSolver.hpp integratorContextImex.hpp odeSolverImex.hpp domain.hpp \
 sbpOps.hpp sbpOps_c.hpp spmat.hpp sbpOps_fc.hpp sbpOps_fc_coordTrans.hpp \
 fault.hpp heatEquation.hpp rootFinderContext.hpp rootFinder.hpp \
 pressureEq.hpp linearElastic.hpp
strikeSlip_linearElastic_switch.o: strikeSlip_linearElastic_dyn.cpp \
 strikeSlip_linearElastic_qd.hpp integratorContextEx.hpp genFuncs.hpp \
 odeSolver.hpp integratorContextImex.hpp odeSolverImex.hpp domain.hpp \
 sbpOps.hpp sbpOps_c.hpp spmat.hpp sbpOps_fc.hpp sbpOps_fc_coordTrans.hpp \
 fault.hpp heatEquation.hpp rootFinderContext.hpp rootFinder.hpp \
 pressureEq.hpp linearElastic.hpp
strikeSlip_powerLaw_qd.o: strikeSlip_powerLaw_qd.cpp \
 strikeSlip_powerLaw_qd.hpp integratorContextEx.hpp genFuncs.hpp \
 odeSolver.hpp integratorContextImex.hpp odeSolverImex.hpp domain.hpp \
 sbpOps.hpp sbpOps_c.hpp spmat.hpp sbpOps_fc.hpp sbpOps_fc_coordTrans.hpp \
 fault.hpp heatEquation.hpp rootFinderContext.hpp rootFinder.hpp \
 pressureEq.hpp powerLaw.hpp newFault.hpp
