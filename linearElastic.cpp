#include "linearElastic.hpp"


using namespace std;


LinearElastic::LinearElastic(Domain&D)
: _order(D._order),_Ny(D._Ny),_Nz(D._Nz),
  _Ly(D._Ly),_Lz(D._Lz),_dy(D._dy),_dz(D._dz),
  _problemType(D._problemType),
  _isMMS(!D._shearDistribution.compare("mms")),
  _outputDir(D._outputDir),
  _v0(D._v0),_vL(D._vL),
  _muArrPlus(D._muArrPlus),_muP(D._muP),
  _bcRPShift(NULL),_surfDispPlus(NULL),
  _rhsP(NULL),_uP(NULL),_stressxyP(NULL),
  _linSolver(D._linSolver),_kspP(NULL),_pcP(NULL),
  _kspTol(D._kspTol),_sbpP(D,*D._muArrPlus,D._muP),
  _timeIntegrator(D._timeIntegrator),
  _stride1D(D._stride1D),_stride2D(D._stride2D),_maxStepCount(D._maxStepCount),
  _initTime(D._initTime),_currTime(_initTime),_maxTime(D._maxTime),
  _minDeltaT(D._minDeltaT),_maxDeltaT(D._maxDeltaT),
  _stepCount(0),_atol(D._atol),_initDeltaT(D._initDeltaT),
  _timeV1D(NULL),_timeV2D(NULL),_surfDispPlusViewer(NULL),
  _integrateTime(0),_writeTime(0),_linSolveTime(0),_factorTime(0),_linSolveCount(0),
  _uPV(NULL),
  _bcTType(D._bcTType),_bcRType(D._bcRType),_bcBType(D._bcBType),_bcLType(D._bcLType),
  _bcTP(NULL),_bcRP(NULL),_bcBP(NULL),_bcLP(NULL)
{
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"\nStarting LinearElastic::LinearElastic in lithosphere.cpp.\n");
#endif

  // boundary conditions
  VecCreate(PETSC_COMM_WORLD,&_bcLP);
  VecSetSizes(_bcLP,PETSC_DECIDE,_Nz);
  VecSetFromOptions(_bcLP);     PetscObjectSetName((PetscObject) _bcLP, "_bcLP");
  VecSet(_bcLP,0.0);

  VecDuplicate(_bcLP,&_bcRPShift); PetscObjectSetName((PetscObject) _bcRPShift, "bcRplusShift");
  VecDuplicate(_bcLP,&_bcRP); PetscObjectSetName((PetscObject) _bcRP, "bcRplus");
  VecSet(_bcRP,_vL*_initTime/2.0);

  VecCreate(PETSC_COMM_WORLD,&_bcTP);
  VecSetSizes(_bcTP,PETSC_DECIDE,_Ny);
  VecSetFromOptions(_bcTP);     PetscObjectSetName((PetscObject) _bcTP, "_bcTP");
  VecSet(_bcTP,0.0);

  VecDuplicate(_bcTP,&_bcBP); PetscObjectSetName((PetscObject) _bcBP, "_bcBP");
  VecSet(_bcBP,0.0);

  KSPCreate(PETSC_COMM_WORLD,&_kspP);
  setupKSP(_sbpP,_kspP,_pcP);

  VecCreate(PETSC_COMM_WORLD,&_rhsP);
  VecSetSizes(_rhsP,PETSC_DECIDE,_Ny*_Nz);
  VecSetFromOptions(_rhsP);

  VecDuplicate(_rhsP,&_uP);



  VecDuplicate(_bcTP,&_surfDispPlus); PetscObjectSetName((PetscObject) _surfDispPlus, "_surfDispPlus");

  if (_timeIntegrator.compare("FEuler")==0) {
    _quadrature = new FEuler(_maxStepCount,_maxTime,_initDeltaT,D._timeControlType);
  }
  else if (_timeIntegrator.compare("RK32")==0) {
    _quadrature = new RK32(_maxStepCount,_maxTime,_initDeltaT,D._timeControlType);
  }
  else {
    PetscPrintf(PETSC_COMM_WORLD,"ERROR: timeIntegrator type type not understood\n");
    assert(0); // automatically fail
  }

#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Ending LinearElastic::LinearElastic in lithosphere.cpp.\n\n");
#endif
}


LinearElastic::~LinearElastic()
{
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Starting LinearElastic::~LinearElastic in lithosphere.cpp.\n");
#endif

  // boundary conditions
  VecDestroy(&_bcLP);
  VecDestroy(&_bcRP);
  VecDestroy(&_bcTP);
  VecDestroy(&_bcBP);

  // body fields
  VecDestroy(&_rhsP);
  VecDestroy(&_uP);
  VecDestroy(&_stressxyP);
  VecDestroy(&_surfDispPlus);

  KSPDestroy(&_kspP);

  PetscViewerDestroy(&_timeV1D);
  PetscViewerDestroy(&_timeV2D);
  PetscViewerDestroy(&_surfDispPlusViewer);
  PetscViewerDestroy(&_uPV);

  VecDestroy(&_bcRPShift);


#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Ending LinearElastic::~LinearElastic in lithosphere.cpp.\n");
#endif
}


/*
 * Set up the Krylov Subspace and Preconditioner (KSP) environment. A
 * table of options available through PETSc and linked external packages
 * is available at
 * http://www.mcs.anl.gov/petsc/documentation/linearsolvertable.html.
 *
 * The methods implemented here are:
 *     Algorithm             Package           input file syntax
 * algebraic multigrid       HYPRE                AMG
 * direct LU                 MUMPS                MUMPSLU
 * direct Cholesky           MUMPS                MUMPSCHOLESKY
 *
 * A list of options for each algorithm that can be set can be optained
 * by running the code with the argument main <input file> -help and
 * searching through the output for "Preconditioner (PC) options" and
 * "Krylov Method (KSP) options".
 *
 * To view convergence information, including number of iterations, use
 * the command line argument: -ksp_converged_reason.
 *
 * For information regarding HYPRE's solver options, especially the
 * preconditioner options, use the User manual online. Also, use -ksp_view.
 */
PetscErrorCode LinearElastic::setupKSP(SbpOps& sbp,KSP& ksp,PC& pc)
{
  PetscErrorCode ierr = 0;

#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting LinearElastic::setupKSP in lithosphere.cpp\n");CHKERRQ(ierr);
#endif

  //~ierr = KSPSetType(_ksp,KSPGMRES);CHKERRQ(ierr);
  //~ierr = KSPSetOperators(_ksp,_A,_A,SAME_PRECONDITIONER);CHKERRQ(ierr);
  //~ierr = KSPGetPC(_ksp,&_pc);CHKERRQ(ierr);


  // use PETSc's direct LU - only available on 1 processor!!!
  //~ierr = PCSetType(D.pc,PCLU);CHKERRQ(ierr);

  if (_linSolver.compare("AMG")==0) { // algebraic multigrid from HYPRE
    // uses HYPRE's solver AMG (not HYPRE's preconditioners)
    ierr = KSPSetType(ksp,KSPRICHARDSON);CHKERRQ(ierr);
#if VERSION < 6
    ierr = KSPSetOperators(ksp,sbp._A,sbp._A,SAME_PRECONDITIONER);CHKERRQ(ierr);
#elif VERSION == 6
    ierr = KSPSetOperators(ksp,sbp._A,sbp._A);CHKERRQ(ierr);
    ierr = KPSSetReusePreconditioner(ksp,PETSC_TRUE);CHKERRQ(ierr);
#endif
    ierr = KSPGetPC(ksp,&pc);CHKERRQ(ierr);
    ierr = PCSetType(pc,PCHYPRE);CHKERRQ(ierr);
    ierr = PCHYPRESetType(pc,"boomeramg");CHKERRQ(ierr);
    ierr = KSPSetTolerances(ksp,_kspTol,PETSC_DEFAULT,PETSC_DEFAULT,PETSC_DEFAULT);CHKERRQ(ierr);
    ierr = PCFactorSetLevels(pc,4);CHKERRQ(ierr);
    ierr = KSPSetInitialGuessNonzero(ksp,PETSC_TRUE);CHKERRQ(ierr);
  }
  else if (_linSolver.compare("PCG")==0) { // preconditioned conjugate gradient
    // use built in preconditioned conjugate gradient
    ierr = KSPSetType(ksp,KSPCG);CHKERRQ(ierr);
    ierr = KSPGetPC(ksp,&pc);CHKERRQ(ierr);

    // preconditioned with PCGAMG
    PCSetType(pc,PCGAMG); // default?


    // preconditioned with HYPRE's AMG
    //~PCSetType(pc,PCHYPRE);
    //~ierr = PCHYPRESetType(pc,"boomeramg");CHKERRQ(ierr);
    //~ierr = PCFactorSetLevels(pc,3);CHKERRQ(ierr);

    // preconditioned with HYPRE
    //~PCSetType(pc,PCHYPRE);
    //~ierr = PCHYPRESetType(pc,"euclid");CHKERRQ(ierr);
    //~ierr = PetscOptionsSetValue("-pc_hypre_euclid_levels","1");CHKERRQ(ierr); // this appears to be fastest
#if VERSION < 6
    ierr = KSPSetOperators(ksp,sbp._A,sbp._A,SAME_PRECONDITIONER);CHKERRQ(ierr);
#elif VERSION == 6
    ierr = KSPSetOperators(ksp,sbp._A,sbp._A);CHKERRQ(ierr);
    ierr = KPSSetReusePreconditioner(ksp,PETSC_TRUE);CHKERRQ(ierr);
#endif
    ierr = KSPSetTolerances(ksp,_kspTol,PETSC_DEFAULT,PETSC_DEFAULT,PETSC_DEFAULT);CHKERRQ(ierr);
    ierr = KSPSetInitialGuessNonzero(ksp,PETSC_TRUE);CHKERRQ(ierr);
  }
  else if (_linSolver.compare("MUMPSLU")==0) { // direct LU from MUMPS
    // use direct LU from MUMPS
    ierr = KSPSetType(ksp,KSPPREONLY);CHKERRQ(ierr);
#if VERSION < 6
    ierr = KSPSetOperators(ksp,sbp._A,sbp._A,SAME_PRECONDITIONER);CHKERRQ(ierr);
#elif VERSION == 6
    ierr = KSPSetOperators(ksp,sbp._A,sbp._A);CHKERRQ(ierr);
    ierr = KSPSetReusePreconditioner(ksp,PETSC_TRUE);CHKERRQ(ierr);
#endif
    ierr = KSPGetPC(ksp,&pc);CHKERRQ(ierr);
    PCSetType(pc,PCLU);
    PCFactorSetMatSolverPackage(pc,MATSOLVERMUMPS);
    PCFactorSetUpMatSolverPackage(pc);
  }

  else if (_linSolver.compare("MUMPSCHOLESKY")==0) { // direct Cholesky (RR^T) from MUMPS
    // use direct LL^T (Cholesky factorization) from MUMPS
    ierr = KSPSetType(ksp,KSPPREONLY);CHKERRQ(ierr);
#if VERSION < 6
    ierr = KSPSetOperators(ksp,sbp._A,sbp._A,SAME_PRECONDITIONER);CHKERRQ(ierr);
#elif VERSION == 6
    ierr = KSPSetOperators(ksp,sbp._A,sbp._A);CHKERRQ(ierr);
    ierr = KPSSetReusePreconditioner(ksp,PETSC_TRUE);CHKERRQ(ierr);
#endif
    ierr = KSPGetPC(ksp,&pc);CHKERRQ(ierr);
    PCSetType(pc,PCCHOLESKY);
    PCFactorSetMatSolverPackage(pc,MATSOLVERMUMPS);
    PCFactorSetUpMatSolverPackage(pc);
  }
  else {
    ierr = PetscPrintf(PETSC_COMM_WORLD,"ERROR: linSolver type not understood\n");
    assert(0>1);
  }

  // finish setting up KSP context using options defined above
  ierr = KSPSetFromOptions(ksp);CHKERRQ(ierr);

  // perform computation of preconditioners now, rather than on first use
  ierr = KSPSetUp(ksp);CHKERRQ(ierr);



#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending LinearElastic::setupKSP in lithosphere.cpp\n");CHKERRQ(ierr);
#endif
  return ierr;
}



PetscErrorCode LinearElastic::timeMonitor(const PetscReal time,const PetscInt stepCount,
                             const_it_vec varBegin,const_it_vec varEnd,
                             const_it_vec dvarBegin,const_it_vec dvarEnd)
{
  PetscErrorCode ierr = 0;
  _stepCount++;
  _currTime = time;
  if ( stepCount % _stride1D == 0) {
    //~ierr = PetscViewerHDF5IncrementTimestep(D->viewer);CHKERRQ(ierr);
    ierr = writeStep1D();CHKERRQ(ierr);
  }

  if ( stepCount % _stride2D == 0) {
    //~ierr = PetscViewerHDF5IncrementTimestep(D->viewer);CHKERRQ(ierr);
    ierr = writeStep2D();CHKERRQ(ierr);
  }

#if VERBOSE > 0
  ierr = PetscPrintf(PETSC_COMM_WORLD,"%i %.15e\n",stepCount,_currTime);CHKERRQ(ierr);
#endif
  return ierr;
}

PetscErrorCode LinearElastic::view()
{
  PetscErrorCode ierr = 0;
  ierr = _quadrature->view();
  ierr = PetscPrintf(PETSC_COMM_WORLD,"\n-------------------------------\n\n");CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Runtime Summary:\n");CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   time spent in integration (s): %g\n",_integrateTime);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   time spent writing output (s): %g\n",_writeTime);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   time spent setting up linear solve context (e.g. factoring) (s): %g\n",_factorTime);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   number of times linear system was solved: %i\n",_linSolveCount);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   time spent solving linear system (s): %g\n",_linSolveTime);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"\n");CHKERRQ(ierr);

  ierr = KSPView(_kspP,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  return ierr;
}



PetscErrorCode SymmLinearElastic::measureMMSError()
{
  PetscErrorCode ierr = 0;

  // measure error between analytical and numerical solution
  Vec uA;
  VecDuplicate(*(_var.begin()+1),&uA);
  mapToVec(uA,MMS_uA,_Nz,_dy,_dz,_currTime);

  double err2 = computeNormDiff_2(*(_var.begin()+1),uA);

  PetscPrintf(PETSC_COMM_WORLD,"%3i %.4e %.4e % .15e \n",
              _Ny,_dy,err2,log2(err2));

  return ierr;
}





//======================================================================
//================= Symmetric LinearElastic Functions ==================

SymmLinearElastic::SymmLinearElastic(Domain&D)
: LinearElastic(D),_fault(D)
{
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"\n\nStarting SymmLinearElastic::SymmLinearElastic in lithosphere.cpp.\n");
#endif

  VecDuplicate(_rhsP,&_stressxyP);

  Vec varPsi; VecDuplicate(_fault._psi,&varPsi); VecCopy(_fault._psi,varPsi);
  _var.push_back(varPsi);
  Vec varSlip; VecDuplicate(_fault._slip,&varSlip); VecCopy(_fault._slip,varSlip);
  _var.push_back(varSlip);

  if (_isMMS) {
    setMMSInitialConditions();
  }
  else {
    setShifts(); // set _bcRPShift
    VecAXPY(_bcRP,1.0,_bcRPShift);

    _sbpP.setRhs(_rhsP,_bcLP,_bcRP,_bcTP,_bcBP);
    double startTime = MPI_Wtime();
    KSPSolve(_kspP,_rhsP,_uP);
    _factorTime += MPI_Wtime() - startTime;

    _sbpP.muxDy(_uP,_stressxyP);
    _fault.setTauQS(_stressxyP,NULL);
    _fault.setFaultDisp(_bcLP,NULL);

    _fault.computeVel();
  }

  setSurfDisp();


#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Ending SymmLinearElastic::SymmLinearElastic in lithosphere.cpp.\n\n\n");
#endif
}

SymmLinearElastic::~SymmLinearElastic()
{

  VecDestroy(&_bcRPShift);
};


// destructor is covered by base class


PetscErrorCode SymmLinearElastic::setShifts()
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting SymmLinearElastic::setShifts in lithosphere.cpp\n");CHKERRQ(ierr);
#endif

  PetscInt Ii,Istart,Iend;
  PetscScalar v,bcRshift;
  ierr = VecGetOwnershipRange(_bcRPShift,&Istart,&Iend);CHKERRQ(ierr);
  for (Ii=Istart;Ii<Iend;Ii++) {
    v = _fault.getTauInf(Ii);
    //~bcRshift = 0.8*  v*_Ly/_muArrPlus[_Ny*_Nz-_Nz+Ii]; // use last values of muArr
    //~bcRshift = v*_Ly/_muArrPlus[Ii]; // use first values of muArr
    bcRshift = 0. * v;
    ierr = VecSetValue(_bcRPShift,Ii,bcRshift,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(_bcRPShift);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(_bcRPShift);CHKERRQ(ierr);

#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending SymmLinearElastic::setShifts in lithosphere.cpp\n");CHKERRQ(ierr);
#endif
  return ierr;
}


PetscErrorCode SymmLinearElastic::setSurfDisp()
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting SymmLinearElastic::setSurfDisp in lithosphere.cpp\n");CHKERRQ(ierr);
#endif

  PetscInt    Ii,Istart,Iend;
  PetscScalar u,y,z;
  ierr = VecGetOwnershipRange(_uP,&Istart,&Iend);
  for (Ii=Istart;Ii<Iend;Ii++) {
    z = Ii-_Nz*(Ii/_Nz);
    y = Ii/_Nz;
    if (z == 0) {
      ierr = VecGetValues(_uP,1,&Ii,&u);CHKERRQ(ierr);
      ierr = VecSetValue(_surfDispPlus,y,u,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = VecAssemblyBegin(_surfDispPlus);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(_surfDispPlus);CHKERRQ(ierr);

#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending SymmLinearElastic::setSurfDisp in lithosphere.cpp\n");CHKERRQ(ierr);
#endif
  return ierr;
}



PetscErrorCode SymmLinearElastic::writeStep1D()
{
  PetscErrorCode ierr = 0;
  string funcName = "SymmLinearElastic::writeStep1D";
  string fileName = "linearElastic.cpp";
  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s at step %i\n",funcName.c_str(),fileName.c_str(),_stepCount);
    CHKERRQ(ierr);
  #endif
  double startTime = MPI_Wtime();


  if (_stepCount==0) {
    ierr = _sbpP.writeOps(_outputDir);CHKERRQ(ierr);
    ierr = _fault.writeContext(_outputDir);CHKERRQ(ierr);
    ierr = PetscViewerASCIIOpen(PETSC_COMM_WORLD,(_outputDir+"time.txt").c_str(),&_timeV1D);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(_timeV1D, "%.15e\n",_currTime);CHKERRQ(ierr);

    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"surfDispPlus").c_str(),FILE_MODE_WRITE,
                                 &_surfDispPlusViewer);CHKERRQ(ierr);
    ierr = VecView(_surfDispPlus,_surfDispPlusViewer);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_surfDispPlusViewer);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"surfDispPlus").c_str(),
                                   FILE_MODE_APPEND,&_surfDispPlusViewer);CHKERRQ(ierr);

    // boundary conditions
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"bcR").c_str(),FILE_MODE_WRITE,
                                 &_bcRPlusV);CHKERRQ(ierr);
    ierr = VecView(_bcRP,_bcRPlusV);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_bcRPlusV);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"bcR").c_str(),
                                   FILE_MODE_APPEND,&_bcRPlusV);CHKERRQ(ierr);

  ierr = _fault.writeStep(_outputDir,_stepCount);CHKERRQ(ierr);
  }
  else {
    ierr = PetscViewerASCIIPrintf(_timeV1D, "%.15e\n",_currTime);CHKERRQ(ierr);
    ierr = VecView(_surfDispPlus,_surfDispPlusViewer);CHKERRQ(ierr);

    ierr = VecView(_bcRP,_bcRPlusV);CHKERRQ(ierr);

    ierr = _fault.writeStep(_outputDir,_stepCount);CHKERRQ(ierr);
  }

  _writeTime += MPI_Wtime() - startTime;
  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s at step %i\n",funcName.c_str(),fileName.c_str(),_stepCount);
    CHKERRQ(ierr);
  #endif
  return ierr;
}



PetscErrorCode SymmLinearElastic::writeStep2D()
{
  PetscErrorCode ierr = 0;
  string funcName = "SymmLinearElastic::writeStep2D";
  string fileName = "linearElastic.cpp";
  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s at step %i\n",funcName.c_str(),fileName.c_str(),_stepCount);
    CHKERRQ(ierr);
  #endif
  double startTime = MPI_Wtime();


  if (_stepCount==0) {
    ierr = PetscViewerASCIIOpen(PETSC_COMM_WORLD,(_outputDir+"time2D.txt").c_str(),&_timeV2D);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(_timeV2D, "%.15e\n",_currTime);CHKERRQ(ierr);

    // output body fields
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"uBodyP").c_str(),
              FILE_MODE_WRITE,&_uPV);CHKERRQ(ierr);
    ierr = VecView(_uP,_uPV);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_uPV);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"uBodyP").c_str(),
                                   FILE_MODE_APPEND,&_uPV);CHKERRQ(ierr);

    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"stressxyP").c_str(),
              FILE_MODE_WRITE,&_stressxyPV);CHKERRQ(ierr);
    ierr = VecView(_stressxyP,_stressxyPV);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_stressxyPV);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"stressxyP").c_str(),
                                   FILE_MODE_APPEND,&_stressxyPV);CHKERRQ(ierr);

    //~if (_isMMS) {
      //~ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"uAnal").c_str(),
                //~FILE_MODE_WRITE,&_uAnalV);CHKERRQ(ierr);
      //~ierr = VecView(_uAnal,_uAnalV);CHKERRQ(ierr);
      //~ierr = PetscViewerDestroy(&_uAnalV);CHKERRQ(ierr);
      //~ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"uAnal").c_str(),
                                     //~FILE_MODE_APPEND,&_uAnalV);CHKERRQ(ierr);
      //~}
  }
  else {
    ierr = PetscViewerASCIIPrintf(_timeV2D, "%.15e\n",_currTime);CHKERRQ(ierr);

    ierr = VecView(_uP,_uPV);CHKERRQ(ierr);
    ierr = VecView(_stressxyP,_stressxyPV);CHKERRQ(ierr);

  }


  _writeTime += MPI_Wtime() - startTime;
  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s at step %i\n",funcName.c_str(),fileName.c_str(),_stepCount);
    CHKERRQ(ierr);
  #endif
  return ierr;
}


PetscErrorCode SymmLinearElastic::integrate()
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting LinearElastic::integrate in lithosphere.cpp\n");CHKERRQ(ierr);
#endif
  double startTime = MPI_Wtime();

  _stepCount++;

  // call odeSolver routine integrate here
  _quadrature->setTolerance(_atol);CHKERRQ(ierr);
  _quadrature->setTimeStepBounds(_minDeltaT,_maxDeltaT);CHKERRQ(ierr);
  ierr = _quadrature->setTimeRange(_initTime,_maxTime);
  ierr = _quadrature->setInitialConds(_var);CHKERRQ(ierr);

  ierr = _quadrature->integrate(this);CHKERRQ(ierr);
  _integrateTime += MPI_Wtime() - startTime;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending LinearElastic::integrate in lithosphere.cpp\n");CHKERRQ(ierr);
#endif
  return ierr;
}


PetscErrorCode SymmLinearElastic::d_dt(const PetscScalar time,const_it_vec varBegin,const_it_vec varEnd,
                 it_vec dvarBegin,it_vec dvarEnd)
{
  PetscErrorCode ierr = 0;
  if (_isMMS) {
    ierr = d_dt_mms(time,varBegin,varEnd,dvarBegin,dvarEnd);CHKERRQ(ierr);
  }
  else {
    ierr = d_dt_eqCycle(time,varBegin,varEnd,dvarBegin,dvarEnd);CHKERRQ(ierr);
  }
  return ierr;
}

// I'm not sure this makes sense as a function given my current MMS test
PetscErrorCode SymmLinearElastic::d_dt_mms(const PetscScalar time,const_it_vec varBegin,const_it_vec varEnd,
                 it_vec dvarBegin,it_vec dvarEnd)
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting SymmLinearElastic::d_dt in lithosphere.cpp: time=%.15e\n",time);CHKERRQ(ierr);
#endif

  Vec source,Hxsource;
  VecDuplicate(_uP,&source);
  VecDuplicate(_uP,&Hxsource);
  mapToVec(source,MMS_uSource,_Nz,_dy,_dz,time);
  ierr = _sbpP.H(source,Hxsource);
  VecDestroy(&source);


  // set rhs, including body source term
  setMMSBoundaryConditions(time); // modifies _bcLP,_bcRP,_bcTP, and _bcBP
  ierr = _sbpP.setRhs(_rhsP,_bcLP,_bcRP,_bcTP,_bcBP);CHKERRQ(ierr);
  ierr = VecAXPY(_rhsP,1.0,Hxsource);CHKERRQ(ierr); // rhs = rhs + H*source
  VecDestroy(&Hxsource);


  // solve for displacement
  double startTime = MPI_Wtime();
  ierr = KSPSolve(_kspP,_rhsP,_uP);CHKERRQ(ierr);
  _linSolveTime += MPI_Wtime() - startTime;
  _linSolveCount++;
  ierr = setSurfDisp();

  // solve for shear stress
  _sbpP.muxDy(_uP,_stressxyP);


  // update rates
  VecSet(*dvarBegin,0.0);
  //~VecSet(*(dvarBegin+1),0.0);
  //~ierr = mapToVec(*(dvarBegin+1),MMS_uA_t,_Nz,_dy,_dz,time); CHKERRQ(ierr);


#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending SymmLinearElastic::d_dt in lithosphere.cpp: time=%.15e\n",time);CHKERRQ(ierr);
#endif
  return ierr;
}


PetscErrorCode SymmLinearElastic::d_dt_eqCycle(const PetscScalar time,const_it_vec varBegin,const_it_vec varEnd,
                 it_vec dvarBegin,it_vec dvarEnd)
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting SymmLinearElastic::d_dt in lithosphere.cpp: time=%.15e\n",time);CHKERRQ(ierr);
#endif


  // update boundaries
  ierr = VecCopy(*(varBegin+1),_bcLP);CHKERRQ(ierr);
  ierr = VecScale(_bcLP,0.5);CHKERRQ(ierr); // var holds slip, bcL is displacement at y=0+
  ierr = VecSet(_bcRP,_vL*time/2.0);CHKERRQ(ierr);
  ierr = VecAXPY(_bcRP,1.0,_bcRPShift);CHKERRQ(ierr);

  // solve for displacement
  ierr = _sbpP.setRhs(_rhsP,_bcLP,_bcRP,_bcTP,_bcBP);CHKERRQ(ierr);
  double startTime = MPI_Wtime();
  ierr = KSPSolve(_kspP,_rhsP,_uP);CHKERRQ(ierr);
  _linSolveTime += MPI_Wtime() - startTime;
  _linSolveCount++;
  ierr = setSurfDisp();

  // solve for shear stress
  ierr = _sbpP.muxDy(_uP,_stressxyP); CHKERRQ(ierr);

  // update fields on fault
  ierr = _fault.setTauQS(_stressxyP,NULL);CHKERRQ(ierr);
  ierr = _fault.d_dt(varBegin,varEnd, dvarBegin, dvarEnd);

  //~VecSet(*dvarBegin,0.0);
  //~VecSet(*(dvarBegin+1),0.0);

#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending SymmLinearElastic::d_dt in lithosphere.cpp: time=%.15e\n",time);CHKERRQ(ierr);
#endif
  return ierr;
}





PetscErrorCode SymmLinearElastic::setMMSBoundaryConditions(const double time)
{
  PetscErrorCode ierr = 0;
  string funcName = "SymmLinearElastic::setMMSBoundaryConditions";
  string fileName = "lithosphere.cpp";
  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s\n",funcName.c_str(),fileName.c_str());CHKERRQ(ierr);
  #endif

  // set up boundary conditions: L and R
  PetscScalar y,z,v;
  PetscInt Ii,Istart,Iend;
  ierr = VecGetOwnershipRange(_bcLP,&Istart,&Iend);CHKERRQ(ierr);
  for(Ii=Istart;Ii<Iend;Ii++) {
    z = _dz * Ii;

    y = 0;
    if (!_bcLType.compare("displacement")) { v = MMS_uA(y,z,time); } // uAnal(y=0,z)
    else if (!_bcLType.compare("traction")) { v = MMS_mu(y,z) * MMS_uA_y(y,z,time); } // sigma_xy = mu * d/dy u
    ierr = VecSetValues(_bcLP,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);

    y = _Ly;
    if (!_bcRType.compare("displacement")) { v = MMS_uA(y,z,time); } // uAnal(y=Ly,z)
    else if (!_bcRType.compare("traction")) { v = MMS_mu(y,z) * MMS_uA_y(y,z,time); } // sigma_xy = mu * d/dy u
    ierr = VecSetValues(_bcRP,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(_bcLP);CHKERRQ(ierr);
  ierr = VecAssemblyBegin(_bcRP);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(_bcLP);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(_bcRP);CHKERRQ(ierr);

  // set up boundary conditions: T and B
  ierr = VecGetOwnershipRange(_bcLP,&Istart,&Iend);CHKERRQ(ierr);
  for(Ii=Istart;Ii<Iend;Ii++) {
    y = _dy * Ii;

    z = 0;
    if (!_bcTType.compare("displacement")) { v = MMS_uA(y,z,time); } // uAnal(y,z=0)
    else if (!_bcTType.compare("traction")) { v = MMS_mu(y,z) * MMS_uA_z(y,z,time); } // sigma_xz = mu * d/dz u
    ierr = VecSetValues(_bcTP,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);

    z = _Lz;
    if (!_bcBType.compare("displacement")) { v = MMS_uA(y,z,time); } // uAnal(y,z=Lz)
    else if (!_bcBType.compare("traction")) { v = MMS_mu(y,z) * MMS_uA_z(y,z,time); } // sigma_xz = mu * d/dz u
    ierr = VecSetValues(_bcBP,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(_bcTP);CHKERRQ(ierr);
  ierr = VecAssemblyBegin(_bcBP);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(_bcTP);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(_bcBP);CHKERRQ(ierr);

  #if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s.\n",funcName.c_str(),fileName.c_str());
  #endif
  return ierr;
}

PetscErrorCode SymmLinearElastic::setMMSInitialConditions()
{
  PetscErrorCode ierr = 0;
  string funcName = "SymmLinearElastic::setMMSInitialConditions";
  string fileName = "lithosphere.cpp";
  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s\n",funcName.c_str(),fileName.c_str());CHKERRQ(ierr);
  #endif

  PetscScalar time = _initTime;

  Vec source,Hxsource;
  VecDuplicate(_uP,&source);
  VecDuplicate(_uP,&Hxsource);

  ierr = mapToVec(source,MMS_uSource,_Nz,_dy,_dz,time); CHKERRQ(ierr);
  ierr = _sbpP.H(source,Hxsource); CHKERRQ(ierr);
  VecDestroy(&source);


  // set rhs, including body source term
  ierr = setMMSBoundaryConditions(time); CHKERRQ(ierr);
  ierr = _sbpP.setRhs(_rhsP,_bcLP,_bcRP,_bcTP,_bcBP);CHKERRQ(ierr);
  ierr = VecAXPY(_rhsP,1.0,Hxsource);CHKERRQ(ierr); // rhs = rhs + H*source
  VecDestroy(&Hxsource);


  // solve for displacement
  double startTime = MPI_Wtime();
  ierr = KSPSolve(_kspP,_rhsP,_uP);CHKERRQ(ierr);
  _linSolveTime += MPI_Wtime() - startTime;
  _linSolveCount++;
  ierr = setSurfDisp();

  // solve for shear stress
  _sbpP.muxDy(_uP,_stressxyP);


  #if VERBOSE > 1
    PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s.\n",funcName.c_str(),fileName.c_str());
  #endif
  return ierr;
}



// Outputs data at each time step.
PetscErrorCode SymmLinearElastic::debug(const PetscReal time,const PetscInt stepCount,
                     const_it_vec varBegin,const_it_vec varEnd,
                     const_it_vec dvarBegin,const_it_vec dvarEnd,const char *stage)
{
  PetscErrorCode ierr = 0;

#if ODEPRINT > 0
  PetscInt       Istart,Iend;
  PetscScalar    bcRval,uVal,psiVal,velVal,dQVal,tauQS;

  //~PetscScalar k = _muArrPlus[0]/2/_Ly;

  ierr= VecGetOwnershipRange(*varBegin,&Istart,&Iend);CHKERRQ(ierr);
  ierr = VecGetValues(*varBegin,1,&Istart,&psiVal);CHKERRQ(ierr);

  ierr = VecGetValues(*(varBegin+1),1,&Istart,&uVal);CHKERRQ(ierr);

  ierr= VecGetOwnershipRange(*dvarBegin,&Istart,&Iend);CHKERRQ(ierr);
  ierr = VecGetValues(*dvarBegin,1,&Istart,&dQVal);CHKERRQ(ierr);
  ierr = VecGetValues(*(dvarBegin+1),1,&Istart,&velVal);CHKERRQ(ierr);

  ierr= VecGetOwnershipRange(_bcRP,&Istart,&Iend);CHKERRQ(ierr);
  ierr = VecGetValues(_bcRP,1,&Istart,&bcRval);CHKERRQ(ierr);

  ierr = VecGetValues(_fault._tauQSP,1,&Istart,&tauQS);CHKERRQ(ierr);

  if (stepCount == 0) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,"%-4s %-6s | %-15s %-15s %-15s | %-15s %-15s %-16s | %-15s\n",
                       "Step","Stage","bcR","D","Q","tauQS","V","dQ","time");
    CHKERRQ(ierr);
  }
  ierr = PetscPrintf(PETSC_COMM_WORLD,"%4i %-6s ",stepCount,stage);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," | %.9e %.9e %.9e ",bcRval,uVal,psiVal);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," | %.9e %.9e %.9e ",tauQS,velVal,dQVal);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," | %.9e\n",time);CHKERRQ(ierr);



  //~VecView(_fault._tauQSP,PETSC_VIEWER_STDOUT_WORLD);
#endif
  return ierr;
}





//================= Full LinearElastic (+ and - sides) Functions =========
FullLinearElastic::FullLinearElastic(Domain&D)
: LinearElastic(D),
  _muArrMinus(D._muArrMinus),_muM(D._muM),
  _bcLMShift(NULL),_surfDispMinus(NULL),
  _rhsM(NULL),_uM(NULL),_sigma_xyMinus(NULL),
  _surfDispMinusViewer(NULL),
  _kspM(NULL),_pcMinus(NULL),
  _sbpMinus(D,*D._muArrMinus,D._muM),
  _bcTMinus(NULL),_bcRMinus(NULL),_bcBMinus(NULL),_bcLMinus(NULL),
  _fault(D)
{
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Starting FullLinearElastic::FullLinearElastic in lithosphere.cpp.\n");
#endif

  //~_fault = new FullFault(D);

  // initialize y<0 boundary conditions
  VecDuplicate(_bcLP,&_bcLMShift);
  PetscObjectSetName((PetscObject) _bcLMShift, "_bcLMShift");
  setShifts(); // set position of boundary from steady sliding
  VecAXPY(_bcRP,1.0,_bcRPShift);


  // fault initial displacement on minus side
  VecDuplicate(_bcLP,&_bcRMinus); PetscObjectSetName((PetscObject) _bcRMinus, "_bcRMinus");
  VecSet(_bcRMinus,0.0);

  // remote displacement on - side
  VecDuplicate(_bcLP,&_bcLMinus); PetscObjectSetName((PetscObject) _bcLMinus, "bcLMinus");
  VecSet(_bcLMinus,-_vL*_initTime/2.0);
  VecAXPY(_bcLMinus,1.0,_bcLMShift);

  VecDuplicate(_bcTP,&_bcTMinus); PetscObjectSetName((PetscObject) _bcTMinus, "bcTMinus");
  VecSet(_bcTMinus,0.0);
  VecDuplicate(_bcBP,&_bcBMinus); PetscObjectSetName((PetscObject) _bcBMinus, "bcBMinus");
  VecSet(_bcBMinus,0.0);

  // initialize and allocate memory for body fields
  double startTime;

  VecDuplicate(_rhsP,&_uP);
  VecDuplicate(_rhsP,&_stressxyP);

  VecCreate(PETSC_COMM_WORLD,&_rhsM);
  VecSetSizes(_rhsM,PETSC_DECIDE,_Ny*_Nz);
  VecSetFromOptions(_rhsM);
  VecDuplicate(_rhsM,&_uM);
  VecDuplicate(_rhsM,&_sigma_xyMinus);


  // initialize KSP for y<0
  KSPCreate(PETSC_COMM_WORLD,&_kspM);
  startTime = MPI_Wtime();
  setupKSP(_sbpMinus,_kspM,_pcMinus);
  _factorTime += MPI_Wtime() - startTime;


  // solve for displacement and shear stress in y<0
  _sbpMinus.setRhs(_rhsM,_bcLMinus,_bcRMinus,_bcTMinus,_bcBMinus);
  KSPSolve(_kspM,_rhsM,_uM);
  //~MatMult(_sbpMinus._muxDy_Iz,_uM,_sigma_xyMinus);
  _sbpMinus.muxDy(_uM,_sigma_xyMinus);

  // solve for displacement and shear stress in y>0
  _sbpP.setRhs(_rhsP,_bcLP,_bcRP,_bcTP,_bcBP);
  startTime = MPI_Wtime();
  KSPSolve(_kspP,_rhsP,_uP);
  _factorTime += MPI_Wtime() - startTime;
  //~MatMult(_sbpP._muxDy_Iz,_uP,_stressxyP);
  _sbpP.muxDy(_uP,_stressxyP);


  //~setU(); // set _uP, _uM analytically
  //~setSigmaxy(); // set shear stresses analytically


  //~PetscPrintf(PETSC_COMM_WORLD,"_bcLMinus = \n");
  //~printVec(_bcLMinus);
  //~PetscPrintf(PETSC_COMM_WORLD,"_bcRMinus = \n");
  //~printVec(_bcRMinus);
  //~PetscPrintf(PETSC_COMM_WORLD,"_bcLP = \n");
  //~printVec(_bcLP);
  //~PetscPrintf(PETSC_COMM_WORLD,"_bcRP = \n");
  //~printVec(_bcRP);
  //~PetscPrintf(PETSC_COMM_WORLD,"_uP = \n");
  //~printVec(_uP);
  //~PetscPrintf(PETSC_COMM_WORLD,"_uM = \n");
  //~printVec(_uM);
  //~PetscPrintf(PETSC_COMM_WORLD,"_sigma_xyMinus = \n");
  //~printVec(_sigma_xyMinus);
  //~PetscPrintf(PETSC_COMM_WORLD,"_stressxyP = \n");
  //~printVec(_stressxyP);
  //~PetscPrintf(PETSC_COMM_WORLD,"_sigma_xy diff = \n");
  //~printVecsDiff(_stressxyP,_sigma_xyMinus);
  //~assert(0>1);


  // set up fault
  _fault.setTauQS(_stressxyP,_sigma_xyMinus);
  _fault.setFaultDisp(_bcLP,_bcRMinus);
  _fault.computeVel();

  _var.push_back(_fault._psi);
  _var.push_back(_fault._uP);
  _var.push_back(_fault._uM);


  VecDuplicate(_bcTMinus,&_surfDispMinus); PetscObjectSetName((PetscObject) _surfDispMinus, "_surfDispMinus");
  setSurfDisp(); // extract surface displacement from displacement fields

#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Ending FullLinearElastic::FullLinearElastic in lithosphere.cpp.\n");
#endif
}

FullLinearElastic::~FullLinearElastic()
{
#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Starting FullLinearElastic::~FullLinearElastic in lithosphere.cpp.\n");
#endif

  // boundary conditions: minus side
  VecDestroy(&_bcLMinus);
  VecDestroy(&_bcLMShift);
  VecDestroy(&_bcRMinus);
  VecDestroy(&_bcTMinus);
  VecDestroy(&_bcBMinus);

  // body fields
  VecDestroy(&_rhsM);
  VecDestroy(&_uM);
  VecDestroy(&_sigma_xyMinus);

  VecDestroy(&_surfDispMinus);

  KSPDestroy(&_kspM);


  PetscViewerDestroy(&_surfDispMinusViewer);

#if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Ending FullLinearElastic::~FullLinearElastic in lithosphere.cpp.\n");
#endif
}


//===================== private member functions =======================



/* Set displacement at sides equal to steady-sliding values:
 *   u ~ tau_fric*L/mu
 */
PetscErrorCode FullLinearElastic::setShifts()
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting FullLinearElastic::setShifts in lithosphere.cpp\n");CHKERRQ(ierr);
#endif


  PetscInt Ii,Istart,Iend;
  PetscScalar v,bcRshift;
  ierr = VecGetOwnershipRange(_bcRPShift,&Istart,&Iend);CHKERRQ(ierr);
  for (Ii=Istart;Ii<Iend;Ii++) {
    v = _fault.getTauInf(Ii);
    //~v = 0;
    //~v = 0.8*v;
    bcRshift = v*_Ly/_muArrPlus[_Ny*_Nz-_Nz+Ii]; // use last values of muArr

    ierr = VecSetValue(_bcRPShift,Ii,bcRshift,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(_bcRPShift);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(_bcRPShift);CHKERRQ(ierr);


  ierr = VecGetOwnershipRange(_bcLMShift,&Istart,&Iend);CHKERRQ(ierr);
  for (Ii=Istart;Ii<Iend;Ii++) {
    v = _fault.getTauInf(Ii);
     //~v = 0;
     v = 0.8*v;
    bcRshift = -v*_Ly/_muArrMinus[_Ny*_Nz-_Nz+Ii]; // use last values of muArr
    ierr = VecSetValue(_bcLMShift,Ii,bcRshift,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(_bcLMShift);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(_bcLMShift);CHKERRQ(ierr);

#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending FullLinearElastic::setShifts in lithosphere.cpp\n");CHKERRQ(ierr);
#endif
  return ierr;
}


PetscErrorCode FullLinearElastic::setSurfDisp()
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting FullLinearElastic::setSurfDisp in lithosphere.cpp\n");CHKERRQ(ierr);
#endif

  PetscInt    Ii,Istart,Iend;
  PetscScalar u,y,z;
  ierr = VecGetOwnershipRange(_uP,&Istart,&Iend);
  for (Ii=Istart;Ii<Iend;Ii++) {
    z = Ii-_Nz*(Ii/_Nz);
    y = Ii/_Nz;
    if (z == 0) {
      ierr = VecGetValues(_uP,1,&Ii,&u);CHKERRQ(ierr);
      ierr = VecSetValue(_surfDispPlus,y,u,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = VecAssemblyBegin(_surfDispPlus);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(_surfDispPlus);CHKERRQ(ierr);


  ierr = VecGetOwnershipRange(_uM,&Istart,&Iend);
  for (Ii=Istart;Ii<Iend;Ii++) {
    z = Ii-_Nz*(Ii/_Nz);
    y = Ii/_Nz;
    if (z == 0) {
      ierr = VecGetValues(_uM,1,&Ii,&u);CHKERRQ(ierr);
      ierr = VecSetValue(_surfDispMinus,y,u,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = VecAssemblyBegin(_surfDispMinus);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(_surfDispMinus);CHKERRQ(ierr);

#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending FullLinearElastic::setSurfDisp in lithosphere.cpp\n");CHKERRQ(ierr);
#endif
  return ierr;
}


PetscErrorCode FullLinearElastic::writeStep1D()
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting FullLinearElastic::writeStep1D in lithosphere.cpp at step %i\n",_stepCount);CHKERRQ(ierr);
#endif
  double startTime = MPI_Wtime();

  if (_stepCount==0) {
    ierr = _sbpP.writeOps(_outputDir+"plus_");CHKERRQ(ierr);
    if (_problemType.compare("full")==0) { ierr = _sbpMinus.writeOps(_outputDir+"minus_");CHKERRQ(ierr); }
    ierr = _fault.writeContext(_outputDir);CHKERRQ(ierr);
    ierr = PetscViewerASCIIOpen(PETSC_COMM_WORLD,(_outputDir+"time.txt").c_str(),&_timeV1D);CHKERRQ(ierr);

    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"surfDispPlus").c_str(),FILE_MODE_WRITE,
                                 &_surfDispPlusViewer);CHKERRQ(ierr);
    ierr = VecView(_surfDispPlus,_surfDispPlusViewer);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_surfDispPlusViewer);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"surfDispPlus").c_str(),
                                   FILE_MODE_APPEND,&_surfDispPlusViewer);CHKERRQ(ierr);

    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"surfDispMinus").c_str(),FILE_MODE_WRITE,
                                 &_surfDispMinusViewer);CHKERRQ(ierr);
    ierr = VecView(_surfDispMinus,_surfDispMinusViewer);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_surfDispMinusViewer);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"surfDispMinus").c_str(),
                                   FILE_MODE_APPEND,&_surfDispMinusViewer);CHKERRQ(ierr);
  }
  else {
    ierr = VecView(_surfDispPlus,_surfDispPlusViewer);CHKERRQ(ierr);
    ierr = VecView(_surfDispMinus,_surfDispMinusViewer);CHKERRQ(ierr);
  }
  ierr = _fault.writeStep(_outputDir,_stepCount);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPrintf(_timeV1D, "%.15e\n",_currTime);CHKERRQ(ierr);

  _writeTime += MPI_Wtime() - startTime;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending FullLinearElastic::writeStep in lithosphere.cpp at step %i\n",_stepCount);CHKERRQ(ierr);
#endif
  return ierr;
}


PetscErrorCode FullLinearElastic::writeStep2D()
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting FullLinearElastic::writeStep2D in lithosphere.cpp at step %i\n",_stepCount);CHKERRQ(ierr);
#endif
  double startTime = MPI_Wtime();


  _writeTime += MPI_Wtime() - startTime;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending FullLinearElastic::writeStep in lithosphere.cpp at step %i\n",_stepCount);CHKERRQ(ierr);
#endif
  return ierr;
}

PetscErrorCode FullLinearElastic::integrate()
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting LinearElastic::integrate in lithosphere.cpp\n");CHKERRQ(ierr);
#endif
  double startTime = MPI_Wtime();

  // call odeSolver routine integrate here
  _quadrature->setTolerance(_atol);CHKERRQ(ierr);
  _quadrature->setTimeStepBounds(_minDeltaT,_maxDeltaT);CHKERRQ(ierr);
  ierr = _quadrature->setTimeRange(_initTime,_maxTime);
  ierr = _quadrature->setInitialConds(_var);CHKERRQ(ierr);

  ierr = _quadrature->integrate(this);CHKERRQ(ierr);
  _integrateTime += MPI_Wtime() - startTime;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending LinearElastic::integrate in lithosphere.cpp\n");CHKERRQ(ierr);
#endif
  return ierr;
}

PetscErrorCode FullLinearElastic::setU()
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting FullLinearElastic::setU in lithosphere.cpp: time=%.15e\n",time);CHKERRQ(ierr);
#endif


  PetscInt       Ii,Istart,Iend;
  PetscScalar    bcLPlus,bcRPlus,bcLMinus,bcRMinus,v;

  ierr = VecGetOwnershipRange(_bcRP,&Istart,&Iend);CHKERRQ(ierr);
  ierr = VecGetValues(_bcLP,1,&Istart,&bcLPlus);CHKERRQ(ierr);
  ierr = VecGetValues(_bcRP,1,&Istart,&bcRPlus);CHKERRQ(ierr);
  ierr = VecGetValues(_bcLMinus,1,&Istart,&bcLMinus);CHKERRQ(ierr);
  ierr = VecGetValues(_bcRMinus,1,&Istart,&bcRMinus);CHKERRQ(ierr);


  for (Ii=Istart;Ii<Iend;Ii++) {
    v = bcLPlus + Ii*(bcRPlus-bcLPlus)/_Ny;//bcL:(bcR-bcL)/(dom.Ny-1):bcR
    ierr = VecSetValues(_uP,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);

    v = bcLMinus + Ii*(bcRMinus - bcLMinus)/_Ny;
    ierr = VecSetValues(_uM,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(_uP);CHKERRQ(ierr);
  ierr = VecAssemblyBegin(_uM);CHKERRQ(ierr);

  ierr = VecAssemblyEnd(_uP);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(_uM);CHKERRQ(ierr);


  //~PetscPrintf(PETSC_COMM_WORLD,"_uP = \n");
  //~printVec(_uP);
  //~PetscPrintf(PETSC_COMM_WORLD,"_uM = \n");
  //~printVec(_uM);
  //~assert(0>1);


#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending FullLinearElastic::setU in lithosphere.cpp: time=%.15e\n",time);CHKERRQ(ierr);
#endif
return ierr;
}


PetscErrorCode FullLinearElastic::setSigmaxy()
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting FullLinearElastic::setSigmaxy in lithosphere.cpp: time=%.15e\n",time);CHKERRQ(ierr);
#endif


  PetscInt       Istart,Iend;
  PetscScalar    bcLPlus,bcRPlus,bcLMinus,bcRMinus;

  ierr = VecGetOwnershipRange(_bcRP,&Istart,&Iend);CHKERRQ(ierr);
  ierr = VecGetValues(_bcLP,1,&Istart,&bcLPlus);CHKERRQ(ierr);
  ierr = VecGetValues(_bcRP,1,&Istart,&bcRPlus);CHKERRQ(ierr);
  ierr = VecGetValues(_bcLMinus,1,&Istart,&bcLMinus);CHKERRQ(ierr);
  ierr = VecGetValues(_bcRMinus,1,&Istart,&bcRMinus);CHKERRQ(ierr);

  ierr = VecSet(_sigma_xyMinus,_muArrMinus[0]*(bcRMinus - bcLMinus)/_Ly);CHKERRQ(ierr);
  ierr = VecSet(_stressxyP,_muArrPlus[0]*(bcRPlus - bcLPlus)/_Ly);CHKERRQ(ierr);

  assert(_muArrPlus[0]*(bcRPlus - bcLPlus)/_Ly < 40);


#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending FullLinearElastic::setSigmaxy in lithosphere.cpp: time=%.15e\n",time);CHKERRQ(ierr);
#endif
return ierr;
}

PetscErrorCode FullLinearElastic::d_dt(const PetscScalar time,const_it_vec varBegin,const_it_vec varEnd,
                 it_vec dvarBegin,it_vec dvarEnd)
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting FullLinearElastic::d_dt in lithosphere.cpp: time=%.15e\n",time);CHKERRQ(ierr);
#endif

  // update boundaries: + side
  ierr = VecCopy(*(varBegin+1),_bcLP);CHKERRQ(ierr);
  ierr = VecSet(_bcRP,_vL*time/2.0);CHKERRQ(ierr);
  ierr = VecAXPY(_bcRP,1.0,_bcRPShift);CHKERRQ(ierr);

  // update boundaries: - side
  ierr = VecCopy(*(varBegin+2),_bcRMinus);CHKERRQ(ierr);
  ierr = VecSet(_bcLMinus,-_vL*time/2.0);CHKERRQ(ierr);
  ierr = VecAXPY(_bcLMinus,1.0,_bcLMShift);CHKERRQ(ierr);

   // solve for displacement: + side
  ierr = _sbpP.setRhs(_rhsP,_bcLP,_bcRP,_bcTP,_bcBP);CHKERRQ(ierr);
  double startTime = MPI_Wtime();
  ierr = KSPSolve(_kspP,_rhsP,_uP);CHKERRQ(ierr);
  _linSolveTime += MPI_Wtime() - startTime;
  _linSolveCount++;


  // solve for displacement: - side
  ierr = _sbpMinus.setRhs(_rhsM,_bcLMinus,_bcRMinus,_bcTMinus,_bcBMinus);CHKERRQ(ierr);
  startTime = MPI_Wtime();
  ierr = KSPSolve(_kspM,_rhsM,_uM);CHKERRQ(ierr);
  _linSolveTime += MPI_Wtime() - startTime;
  _linSolveCount++;

  // set _uP, _uM analytically
  //~setU();


  // solve for shear stress
  //~ierr = MatMult(_sbpMinus._muxDy_Iz,_uM,_sigma_xyMinus);CHKERRQ(ierr);
  //~ierr = MatMult(_sbpP._muxDy_Iz,_uP,_stressxyP);CHKERRQ(ierr);
  _sbpMinus.muxDy(_uM,_sigma_xyMinus);
  _sbpP.muxDy(_uP,_stressxyP);



  // set shear stresses analytically
  //~setSigmaxy();

  //~PetscPrintf(PETSC_COMM_WORLD,"_sigma_xyMinus = \n");
  //~printVec(_sigma_xyMinus);
  //~PetscPrintf(PETSC_COMM_WORLD,"_stressxyP = \n");
  //~printVec(_stressxyP);
  //~assert(0>1);

  ierr = _fault.setTauQS(_stressxyP,_sigma_xyMinus);CHKERRQ(ierr);
  ierr = _fault.d_dt(varBegin,varEnd, dvarBegin, dvarEnd);

  ierr = setSurfDisp();

#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending FullLinearElastic::d_dt in lithosphere.cpp: time=%.15e\n",time);CHKERRQ(ierr);
#endif
  return ierr;
}



// Outputs data at each time step.
PetscErrorCode FullLinearElastic::debug(const PetscReal time,const PetscInt stepCount,
                     const_it_vec varBegin,const_it_vec varEnd,
                     const_it_vec dvarBegin,const_it_vec dvarEnd,const char *stage)
{
  PetscErrorCode ierr = 0;
#if ODEPRINT > 0
  PetscInt       Istart,Iend;
  PetscScalar    bcRPlus,bcLMinus,uMinus,uPlus,psi,velMinus,velPlus,dPsi,
                 tauQSPlus,tauQSMinus;

  //~PetscScalar k = _muArrPlus[0]/2/_Ly;

  ierr= VecGetOwnershipRange(*varBegin,&Istart,&Iend);CHKERRQ(ierr);
  ierr = VecGetValues(*varBegin,1,&Istart,&psi);CHKERRQ(ierr);

  ierr = VecGetValues(*(varBegin+1),1,&Istart,&uPlus);CHKERRQ(ierr);
  ierr = VecGetValues(*(varBegin+2),1,&Istart,&uMinus);CHKERRQ(ierr);

  ierr= VecGetOwnershipRange(*dvarBegin,&Istart,&Iend);CHKERRQ(ierr);
  ierr = VecGetValues(*dvarBegin,1,&Istart,&dPsi);CHKERRQ(ierr);
  ierr = VecGetValues(*(dvarBegin+1),1,&Istart,&velPlus);CHKERRQ(ierr);
  ierr = VecGetValues(*(dvarBegin+2),1,&Istart,&velMinus);CHKERRQ(ierr);

  ierr= VecGetOwnershipRange(_bcRP,&Istart,&Iend);CHKERRQ(ierr);
  ierr = VecGetValues(_bcRP,1,&Istart,&bcRPlus);CHKERRQ(ierr);
  ierr = VecGetValues(_bcLMinus,1,&Istart,&bcLMinus);CHKERRQ(ierr);

  ierr = VecGetValues(_fault._tauQSP,1,&Istart,&tauQSPlus);CHKERRQ(ierr);
  ierr = VecGetValues(_fault._tauQSMinus,1,&Istart,&tauQSMinus);CHKERRQ(ierr);

  if (stepCount == 0) {
    //~ierr = PetscPrintf(PETSC_COMM_WORLD,"%-4s|| %-4s %-6s | %-15s %-15s %-15s | %-15s %-15s %-16s | %-15s\n",
                       //~"Side","Step","Stage","gR","D","Q","VL","V","dQ","time");
    //~CHKERRQ(ierr);
  }
  ierr = PetscPrintf(PETSC_COMM_WORLD,"%i %-6s | %.9e %.9e | %.9e %.9e | %.9e\n",stepCount,stage,
              uPlus-uMinus,psi,velPlus-velMinus,dPsi,time);CHKERRQ(ierr);

#if ODEPRINT > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"    y>0 |  %.9e  %.9e %.9e  %.9e \n",
              bcRPlus,uPlus,tauQSPlus,velPlus);CHKERRQ(ierr);

  ierr = PetscPrintf(PETSC_COMM_WORLD,"    y<0 | %.9e %.9e %.9e %.9e \n",
              bcLMinus,uMinus,tauQSMinus,velMinus);CHKERRQ(ierr);
#endif
#endif
  return ierr;
}


PetscErrorCode FullLinearElastic::measureMMSError()
{
  PetscErrorCode ierr = 0;

  // measure error between uAnal and _uP (the numerical solution)
  //~Vec diff;
  //~ierr = VecDuplicate(_uP,&diff);CHKERRQ(ierr);
  //~ierr = VecWAXPY(diff,-1.0,_uP,_uAnal);CHKERRQ(ierr);
  //~PetscScalar err;
  //~ierr = VecNorm(diff,NORM_2,&err);CHKERRQ(ierr);
  //~err = err/sqrt(_Ny*_Nz);

  //~double err = computeNormDiff_Mat(_sbpP._H,_uP,_uAnal);
  double err = 1e8;

  PetscPrintf(PETSC_COMM_WORLD,"Ny = %i, dy = %e MMS err = %e, log2(err) = %e\n",_Ny,_dy,err,log2(err));

  return ierr;
}

