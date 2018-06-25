#include "odeSolver_WaveImex.hpp"

using namespace std;

OdeSolver_WaveEq_Imex::OdeSolver_WaveEq_Imex(PetscInt maxNumSteps,PetscScalar initT,PetscScalar finalT,PetscScalar deltaT)
: _initT(initT),_finalT(finalT),_currT(initT),_deltaT(deltaT),
  _maxNumSteps(maxNumSteps),_stepCount(0),
  _lenVar(0),_runTime(0)
{
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Starting OdeSolver_WaveEq_Imex constructor in odeSolver_waveImex.cpp.\n");
#endif
  double startTime = MPI_Wtime();

  _runTime += MPI_Wtime() - startTime;
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Ending OdeSolver_WaveEq_Imex constructor in odeSolver_waveImex.cpp.\n");
#endif
}

PetscErrorCode OdeSolver_WaveEq_Imex::setStepSize(const PetscReal deltaT)
{
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Starting OdeSolver_WaveEq_Imex::setStepSize in odeSolver_waveImex.cpp.\n");
#endif
  double startTime = MPI_Wtime();
  _deltaT = deltaT;
  _runTime += MPI_Wtime() - startTime;
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Ending OdeSolver_WaveEq_Imex::setStepSize in odeSolver_waveImex.cpp.\n");
#endif
  return 0;
}


PetscErrorCode OdeSolver_WaveEq_Imex::view()
{
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Starting OdeSolver_WaveEq_Imex::view in odeSolver_WaveImex.cpp.\n");
#endif
  PetscErrorCode ierr = 0;

  ierr = PetscPrintf(PETSC_COMM_WORLD,"\nTimeSolver summary:\n");CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   integration algorithm: wave equation\n");CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   time interval: %g to %g\n",
                     _initT,_finalT);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   total number of steps taken: %i/%i\n",
                     _stepCount,_maxNumSteps);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   final time reached: %g\n",
                     _currT);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   total run time: %g\n",
                     _runTime);CHKERRQ(ierr);
  return 0;
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Ending WaveEq::view in odeSolver_waveImex.cpp.\n");
#endif
}

PetscErrorCode OdeSolver_WaveEq_Imex::setInitialConds(std::map<string,Vec>& var, std::map<string,Vec>& varIm)
{
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Starting WaveEq::setInitialConds in odeSolver_waveImex.cpp.\n");
#endif
  double startTime = MPI_Wtime();
  PetscErrorCode ierr = 0;

  // explicitly integrated variables
  for (map<string,Vec>::iterator it = var.begin(); it != var.end(); it++ ) {

    // allocate n: var
    VecDuplicate(var[it->first],&_var[it->first]); VecCopy(var[it->first],_var[it->first]);

    // allocate n-1: varPrev
    VecDuplicate(var[it->first],&_varPrev[it->first]); VecCopy(var[it->first],_varPrev[it->first]);

    // allocate n+1: varNext
    VecDuplicate(var[it->first],&_varNext[it->first]); VecSet(_varNext[it->first],0.);
  }

  // implicitly integrated variables
  for (map<string,Vec>::iterator it = var.begin(); it != var.end(); it++ ) {

    // allocate n: var
    VecDuplicate(var[it->first],&_varIm[it->first]); VecCopy(var[it->first],_varIm[it->first]);

    // allocate n-1: varPrev
    VecDuplicate(var[it->first],&_varImPrev[it->first]); VecCopy(var[it->first],_varImPrev[it->first]);
  }

  _runTime += MPI_Wtime() - startTime;
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Ending WaveEq::setInitialConds in odeSolver_waveImex.cpp.\n");
#endif
  return ierr;
}

PetscErrorCode OdeSolver_WaveEq_Imex::integrate(IntegratorContext_WaveEq_Imex *obj)
{
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Starting OdeSolver_WaveEq_Imex::integrate in odeSolver_waveImex.cpp.\n");
#endif
  PetscErrorCode ierr = 0;
  double startTime = MPI_Wtime();
  int   stopIntegration = 0;

  if (_finalT==_initT) { return ierr; }
  else if (_deltaT==0) { _deltaT = (_finalT-_initT)/_maxNumSteps; }

  // write initial condition
  ierr = obj->timeMonitor(_currT,_stepCount,stopIntegration); CHKERRQ(ierr); // write first step

  while (_stepCount<_maxNumSteps && _currT<_finalT) {
    _currT = _currT + _deltaT;
    if (_currT>_finalT) { _currT = _finalT; }
    _stepCount++;
    ierr = obj->d_dt(_currT,_deltaT,_varNext,_var,_varPrev,_varIm, _varImPrev); CHKERRQ(ierr);
    ierr = obj->timeMonitor(_currT,_stepCount,stopIntegration); CHKERRQ(ierr);

    // accept time step and update explicitly integrated variables
    for (map<string,Vec>::iterator it = _var.begin(); it != _var.end(); it++ ) {
      VecCopy(_var[it->first],_varPrev[it->first]);
      VecCopy(_varNext[it->first],_var[it->first]);
      VecSet(_varNext[it->first],0.0);
    }

    // accept updated state for implicit variables
    for (map<string,Vec>::iterator it = _varImPrev.begin(); it!=_varImPrev.end(); it++ ) {
      VecCopy(_varIm[it->first],_varImPrev[it->first]);
    }
  }

  _runTime += MPI_Wtime() - startTime;
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Ending FEuler::integrate in odeSolver_waveImex.cpp.\n");
#endif
  return ierr;
}


