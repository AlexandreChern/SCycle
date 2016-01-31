#include "powerLaw.hpp"

#define FILENAME "powerLaw.cpp"


PowerLaw::PowerLaw(Domain& D)
: SymmLinearElastic(D), _file(D._file),_delim(D._delim),_inputDir(D._inputDir),
  _viscDistribution("unspecified"),_AFile("unspecified"),_BFile("unspecified"),_nFile("unspecified"),
  _A(NULL),_n(NULL),_B(NULL),_effVisc(NULL),
  _stressxzP(NULL),_sigmadev(NULL),
  _gxyP(NULL),_dgxyP(NULL),
  _gxzP(NULL),_dgxzP(NULL),
  _gTxyP(NULL),_gTxzP(NULL),
  _T(NULL),
  _stressxyPV(NULL),_stressxzPV(NULL),_sigmadevV(NULL),
  _gTxyPV(NULL),_gTxzPV(NULL),
  _gxyPV(NULL),_dgxyPV(NULL),
  _gxzPV(NULL),_dgxzPV(NULL),
  _TV(NULL),_effViscV(NULL)
{
  #if VERBOSE > 1
    std::string funcName = "PowerLaw::PowerLaw";
    PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s\n",funcName.c_str(),FILENAME);
  #endif


  // set viscosity
  loadSettings(_file);
  checkInput();
  if (_viscDistribution.compare("loadFromFile")==0) { loadFieldsFromFiles(); }
  setFields();

  VecDuplicate(_uP,&_stressxzP); VecSet(_stressxzP,0.0);
  VecDuplicate(_uP,&_sigmadev); VecSet(_sigmadev,0.0);
  VecDuplicate(_uP,&_effVisc); VecSet(_sigmadev,0.0);


  VecDuplicate(_uP,&_gxyP);
  PetscObjectSetName((PetscObject) _gxyP, "_gxyP");
  VecSet(_gxyP,0.0);
  VecDuplicate(_uP,&_dgxyP);
  PetscObjectSetName((PetscObject) _dgxyP, "_dgxyP");
  VecSet(_dgxyP,0.0);

  VecDuplicate(_uP,&_gxzP);
  PetscObjectSetName((PetscObject) _gxzP, "_gxzP");
  VecSet(_gxzP,0.0);
  VecDuplicate(_uP,&_dgxzP);
  PetscObjectSetName((PetscObject) _dgxzP, "_dgxzP");
  VecSet(_dgxzP,0.0);


  VecDuplicate(_uP,&_gTxyP); VecSet(_gTxyP,0.0);
  VecDuplicate(_uP,&_gTxzP); VecSet(_gTxzP,0.0);



  // add viscous strain to integrated variables, stored in _var
  _var.push_back(_gxyP);
  _var.push_back(_gxzP);

  if (_isMMS) {
    setMMSInitialConditions();
  }


  #if VERBOSE > 1
    PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s\n",funcName.c_str(),FILENAME);
  #endif
}

PowerLaw::~PowerLaw()
{
  #if VERBOSE > 1
    std::string funcName = "PowerLaw::PowerLaw";
    PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s\n",funcName.c_str(),FILENAME);
  #endif


  VecDestroy(&_A);
  VecDestroy(&_n);
  VecDestroy(&_B);

  VecDestroy(&_stressxzP);
  VecDestroy(&_sigmadev);

  VecDestroy(&_gTxyP);
  VecDestroy(&_gTxzP);
  VecDestroy(&_gxyP);
  VecDestroy(&_gxzP);
  VecDestroy(&_dgxyP);
  VecDestroy(&_dgxzP);
  VecDestroy(&_T);

  PetscViewerDestroy(&_stressxyPV);
  PetscViewerDestroy(&_stressxzPV);
  PetscViewerDestroy(&_sigmadevV);
  PetscViewerDestroy(&_gTxyPV);
  PetscViewerDestroy(&_gTxzPV);
  PetscViewerDestroy(&_gxyPV);
  PetscViewerDestroy(&_gxzPV);
  PetscViewerDestroy(&_dgxyPV);
  PetscViewerDestroy(&_dgxzPV);
  PetscViewerDestroy(&_TV);


  #if VERBOSE > 1
    PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s\n",funcName.c_str(),FILENAME);
  #endif
}


PetscErrorCode PowerLaw::integrate()
{
  PetscErrorCode ierr = 0;
  #if VERBOSE > 1
    std::string funcName = "PowerLaw::PowerLaw";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
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
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s\n",funcName.c_str(),FILENAME);
  CHKERRQ(ierr);
#endif
  return ierr;
}


PetscErrorCode PowerLaw::d_dt(const PetscScalar time,const_it_vec varBegin,const_it_vec varEnd,
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


PetscErrorCode PowerLaw::d_dt_eqCycle(const PetscScalar time,const_it_vec varBegin,const_it_vec varEnd,
                 it_vec dvarBegin,it_vec dvarEnd)
{
  PetscErrorCode ierr = 0;
  #if VERBOSE > 1
    std::string funcName = "PowerLaw::PowerLaw";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s: time=%.15e\n",funcName.c_str(),FILENAME,time);
    CHKERRQ(ierr);
  #endif

  // update boundaries
  ierr = VecCopy(*(varBegin+1),_bcLP);CHKERRQ(ierr);
  ierr = VecScale(_bcLP,0.5);CHKERRQ(ierr);
  ierr = VecSet(_bcRP,_vL*time/2.0);CHKERRQ(ierr);
  ierr = VecAXPY(_bcRP,1.0,_bcRPShift);CHKERRQ(ierr);

  // add source terms to rhs: d/dy( 2*mu*strainV_xy) + d/dz( 2*mu*strainV_xz)
  Vec viscSource;
  ierr = VecDuplicate(_gxyP,&viscSource);CHKERRQ(ierr);
  ierr = setViscStrainSourceTerms(viscSource,varBegin,varEnd);CHKERRQ(ierr);

  // set up rhs vector
  ierr = _sbpP.setRhs(_rhsP,_bcLP,_bcRP,_bcTP,_bcBP);CHKERRQ(ierr); // update rhs from BCs
  ierr = VecAXPY(_rhsP,1.0,viscSource);CHKERRQ(ierr);
  VecDestroy(&viscSource);

  // solve for displacement
  double startTime = MPI_Wtime();
  ierr = KSPSolve(_kspP,_rhsP,_uP);CHKERRQ(ierr);
  _linSolveTime += MPI_Wtime() - startTime;
  _linSolveCount++;
  ierr = setSurfDisp();

  // set shear traction on fault
  ierr = setStresses(time,varBegin,varEnd);CHKERRQ(ierr);
  ierr = _fault.setTauQS(_stressxyP,NULL);CHKERRQ(ierr);

  // set rates
  ierr = _fault.d_dt(varBegin,varEnd, dvarBegin, dvarEnd); // sets rates for slip and state
  ierr = setViscStrainRates(time,varBegin,varEnd,dvarBegin,dvarEnd);CHKERRQ(ierr); // sets viscous strain rates

  //~VecSet(*dvarBegin,0.0);
  //~VecSet(*(dvarBegin+1),0.0);
  //~VecSet(*(dvarBegin+2),0.0);
  //~VecSet(*(dvarBegin+3),0.0);


  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s: time=%.15e\n",funcName.c_str(),FILENAME,time);
      CHKERRQ(ierr);
  #endif
  return ierr;
}

PetscErrorCode PowerLaw::d_dt_mms(const PetscScalar time,const_it_vec varBegin,const_it_vec varEnd,
                 it_vec dvarBegin,it_vec dvarEnd)
{
  PetscErrorCode ierr = 0;
  #if VERBOSE > 1
    string funcName = "PowerLaw::d_dt_mms";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s: time=%.15e\n",funcName.c_str(),FILENAME,time);
    CHKERRQ(ierr);
  #endif

  // create rhs: set boundary conditions, set rhs, add source terms
  ierr = setMMSBoundaryConditions(time);CHKERRQ(ierr); // modifies _bcLP,_bcRP,_bcTP, and _bcBP
  ierr = _sbpP.setRhs(_rhsP,_bcLP,_bcRP,_bcTP,_bcBP);CHKERRQ(ierr);
  Vec viscSource,HxviscSource,uSource,HxuSource;
  ierr = VecDuplicate(_uP,&viscSource);CHKERRQ(ierr);
  ierr = VecDuplicate(_uP,&HxviscSource);CHKERRQ(ierr);
  ierr = VecDuplicate(_uP,&uSource);CHKERRQ(ierr);
  ierr = VecDuplicate(_uP,&HxuSource);CHKERRQ(ierr);

  mapToVec(viscSource,MMS_gSource,_Nz,_dy,_dz,time);
  ierr = _sbpP.H(viscSource,HxviscSource);
  VecDestroy(&viscSource);
  mapToVec(uSource,MMS_uSource,_Nz,_dy,_dz,time);
  ierr = _sbpP.H(uSource,HxuSource);
  VecDestroy(&uSource);

  ierr = VecAXPY(_rhsP,1.0,HxviscSource);CHKERRQ(ierr); // rhs = rhs + viscSource
  ierr = VecAXPY(_rhsP,1.0,HxuSource);CHKERRQ(ierr); // rhs = rhs + H*usource
  VecDestroy(&HxviscSource);
  VecDestroy(&HxuSource);


  double startTime = MPI_Wtime();
  ierr = KSPSolve(_kspP,_rhsP,_uP);CHKERRQ(ierr);
  _linSolveTime += MPI_Wtime() - startTime;
  _linSolveCount++;
  ierr = setSurfDisp();

  // update fields on fault
  ierr = _sbpP.muxDy(_uP,_stressxyP); CHKERRQ(ierr);
  //~ierr = _fault.setTauQS(_stressxyP,NULL);CHKERRQ(ierr);

  // update rates
  VecSet(*dvarBegin,0.0); // d/dt psi
  VecSet(*(dvarBegin+1),0.0); // slip vel
  //~VecSet(*(dvarBegin+2),0.0); // slip vel
  //~VecSet(*(dvarBegin+3),0.0); // slip vel

  mapToVec(*(dvarBegin+2),MMS_gxy_t,_Nz,_dy,_dz,time);
  mapToVec(*(dvarBegin+3),MMS_gxz_t,_Nz,_dy,_dz,time);

  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s: time=%.15e\n",funcName.c_str(),FILENAME,time);
    CHKERRQ(ierr);
  #endif
  return ierr;
}


PetscErrorCode PowerLaw::setViscStrainSourceTerms(Vec& out,const_it_vec varBegin,const_it_vec varEnd)
{
  PetscErrorCode ierr = 0;
  #if VERBOSE > 1
    string funcName = "PowerLaw::setViscStrainSourceTerms";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s: time=%.15e\n",funcName.c_str(),FILENAME,time);
    CHKERRQ(ierr);
  #endif

  Vec source;
  VecDuplicate(_gxyP,&source);

  // add source terms to rhs: d/dy( 2*mu*epsV_xy) + d/dz( 2*mu*epsV_xz)
  // + Hz^-1 E0z mu epsV_xz + Hz^-1 ENz mu epsV_xz
  Vec sourcexy_y;
  VecDuplicate(_gxyP,&sourcexy_y);
  ierr = _sbpP.Dyxmu(*(varBegin+2),sourcexy_y);CHKERRQ(ierr);
  ierr = VecScale(sourcexy_y,2.0);CHKERRQ(ierr);
  ierr = VecCopy(sourcexy_y,source);CHKERRQ(ierr); // sourcexy_y -> source
  VecDestroy(&sourcexy_y);


  if (_Nz > 1)
  {
    Vec sourcexz_z;
    VecDuplicate(_gxzP,&sourcexz_z);

    ierr = _sbpP.Dzxmu(*(varBegin+3),sourcexz_z);CHKERRQ(ierr);
    ierr = VecScale(sourcexz_z,2.0);CHKERRQ(ierr);

    ierr = VecAXPY(source,1.0,sourcexz_z);CHKERRQ(ierr); // source += Hxsourcexz_z
    VecDestroy(&sourcexz_z);

    Vec temp1,bcT,bcB;
    VecDuplicate(_gxzP,&temp1);
    VecDuplicate(_gxzP,&bcT);
    VecDuplicate(_gxzP,&bcB);



    _sbpP.HzinvxE0z(*(varBegin+3),temp1);
    ierr = MatMult(_muP,temp1,bcT); CHKERRQ(ierr);

    _sbpP.HzinvxENz(*(varBegin+3),temp1);
    ierr = MatMult(_muP,temp1,bcB); CHKERRQ(ierr);

    ierr = VecAXPY(source,2.0,bcT);CHKERRQ(ierr);
    ierr = VecAXPY(source,-2.0,bcB);CHKERRQ(ierr);

    VecDestroy(&temp1);
    VecDestroy(&bcT);
    VecDestroy(&bcB);
  }


  ierr = _sbpP.H(source,out); CHKERRQ(ierr);
  VecDestroy(&source);

  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s: time=%.15e\n",funcName.c_str(),FILENAME,time);
      CHKERRQ(ierr);
  #endif
  return ierr = 0;
}



PetscErrorCode PowerLaw::setViscousStrainRateSAT(Vec &u, Vec &gL, Vec &gR, Vec &out)
{
    PetscErrorCode ierr = 0;
  #if VERBOSE > 1
    string funcName = "PowerLaw::viscousStrainRateSAT";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s: time=%.15e\n",funcName.c_str(),FILENAME,time);
    CHKERRQ(ierr);
  #endif

  VecSet(out,0.0);

  Vec GL, GR,temp1;
  VecDuplicate(u,&GL);
  VecDuplicate(u,&GR);
  VecDuplicate(u,&temp1);

  ierr = _sbpP.HyinvxE0y(u,temp1);CHKERRQ(ierr);
  ierr = _sbpP.Hyinvxe0y(gL,GL);CHKERRQ(ierr);
  VecAXPY(out,-_sbpP._alphaDy,temp1);
  VecAXPY(out,_sbpP._alphaDy,GL);

  ierr = _sbpP.HyinvxENy(u,temp1);CHKERRQ(ierr);
  ierr = _sbpP.HyinvxeNy(gR,GR);CHKERRQ(ierr);
  VecAXPY(out,_sbpP._alphaDy,temp1);
  VecAXPY(out,-_sbpP._alphaDy,GR);

  VecDestroy(&GL);
  VecDestroy(&GR);
  VecDestroy(&temp1);

  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s: time=%.15e\n",funcName.c_str(),FILENAME,time);
      CHKERRQ(ierr);
  #endif
  return ierr = 0;
}



PetscErrorCode PowerLaw::setViscStrainRates(const PetscScalar time,const_it_vec varBegin,const_it_vec varEnd,
                 it_vec dvarBegin,it_vec dvarEnd)
{
    PetscErrorCode ierr = 0;
  #if VERBOSE > 1
    string funcName = "PowerLaw::setViscStrainRates";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s: time=%.15e\n",funcName.c_str(),FILENAME,time);
    CHKERRQ(ierr);
  #endif

  // add SAT terms to strain rate for epsxy
  Vec SAT;
  VecDuplicate(_gTxyP,&SAT);
  ierr = setViscousStrainRateSAT(_uP,_bcLP,_bcRP,SAT);CHKERRQ(ierr);

  PetscScalar deps,invVisc,epsVisc,sat,sigmaxy,sigmaxz,sigmadev,A,B,n,T,effVisc=0;
  PetscInt Ii,Istart,Iend;
  VecGetOwnershipRange(*(dvarBegin+2),&Istart,&Iend);
  for (Ii=Istart;Ii<Iend;Ii++) {
    VecGetValues(_stressxyP,1,&Ii,&sigmaxy);
    VecGetValues(*(varBegin+2),1,&Ii,&epsVisc);
    VecGetValues(SAT,1,&Ii,&sat);

    VecGetValues(_sigmadev,1,&Ii,&sigmadev);
    VecGetValues(_A,1,&Ii,&A);
    VecGetValues(_B,1,&Ii,&B);
    VecGetValues(_n,1,&Ii,&n);
    VecGetValues(_T,1,&Ii,&T);
    invVisc = A*pow(sigmadev,n-1.0)*exp(-B/T) * 1e-3; // *1e-3 to get resulting eff visc in GPa s
    effVisc = 1.0/invVisc;
    VecSetValues(_effVisc,1,&Ii,&effVisc,INSERT_VALUES);

    //~PetscPrintf(PETSC_COMM_WORLD,"  Ii = %i| A = %e, B = %e, n = %e, T = %e, visc = %e\n",Ii,A,B,n,T,invVisc);

    // d/dt epsVxy = mu/visc * ( 0.5*d/dy u - epsxy) - SAT
    deps = sigmaxy*invVisc - _muArrPlus[Ii]*invVisc * sat;
    VecSetValues(*(dvarBegin+2),1,&Ii,&deps,INSERT_VALUES);

    if (_Nz > 1) {
      //~VecGetValues(_gTxzP,1,&Ii,&epsTot);
      VecGetValues(_stressxzP,1,&Ii,&sigmaxz);
      VecGetValues(*(varBegin+3),1,&Ii,&epsVisc);

      // d/dt epsVxz = mu/visc * ( 0.5*d/dz u - epsxz)
      deps = sigmaxz*invVisc;
      VecSetValues(*(dvarBegin+3),1,&Ii,&deps,INSERT_VALUES);
    }
  }
  VecAssemblyBegin(*(dvarBegin+2));
  VecAssemblyEnd(*(dvarBegin+2));
  VecAssemblyBegin(_effVisc);
  VecAssemblyEnd(_effVisc);

  VecDestroy(&SAT);


  if (_Nz > 1) {
    VecAssemblyBegin(*(dvarBegin+3));
    VecAssemblyEnd(*(dvarBegin+3));
  }

  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s: time=%.15e\n",funcName.c_str(),FILENAME,time);
      CHKERRQ(ierr);
  #endif
  return ierr = 0;
}

// computes sigmaxy, sigmaxz, and sigmadev = sqrt(sigmaxy^2 + sigmaxz^2)
PetscErrorCode PowerLaw::setStresses(const PetscScalar time,const_it_vec varBegin,const_it_vec varEnd)
{
    PetscErrorCode ierr = 0;
  #if VERBOSE > 1
  string funcName = "PowerLaw::setStresses";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s: time=%.15e\n",funcName.c_str(),FILENAME,time);
    CHKERRQ(ierr);
  #endif

  // compute strains and rates
  _sbpP.Dy(_uP,_gTxyP);
  VecScale(_gTxyP,0.5);

  _sbpP.Dz(_uP,_gTxzP);
  VecScale(_gTxzP,0.5);

  PetscScalar epsTot,epsVisc,sigmaxy,sigmaxz,sigmadev=0;
  PetscInt Ii,Istart,Iend;
  VecGetOwnershipRange(_gTxyP,&Istart,&Iend);
  for (Ii=Istart;Ii<Iend;Ii++) {
    VecGetValues(_gTxyP,1,&Ii,&epsTot);
    VecGetValues(*(varBegin+2),1,&Ii,&epsVisc);

    // solve for stressxyP = 2*mu*epsExy (elastic strain)
    //                     = 2*mu*(0.5*d/dy(uhat) - epsVxy)
    sigmaxy = 2.0*_muArrPlus[Ii] * (epsTot - epsVisc);
    VecSetValues(_stressxyP,1,&Ii,&sigmaxy,INSERT_VALUES);

    sigmadev = sigmaxy*sigmaxy;

    if (_Nz > 1) {
      VecGetValues(_gTxzP,1,&Ii,&epsTot);
      VecGetValues(*(varBegin+3),1,&Ii,&epsVisc);

      // solve for stressxzP = 2*mu*epsExy (elastic strain)
      //                     = 2*mu*(0.5*d/dz(uhat) - epsVxz)
      sigmaxz = 2.0*_muArrPlus[Ii] * (epsTot - epsVisc);
      VecSetValues(_stressxzP,1,&Ii,&sigmaxz,INSERT_VALUES);

      sigmadev += sigmaxz*sigmaxz;
    }
    sigmadev = sqrt(sigmadev);
    VecSetValues(_sigmadev,1,&Ii,&sigmadev,INSERT_VALUES);
  }
  VecAssemblyBegin(_stressxyP);  VecAssemblyBegin(_sigmadev);
  VecAssemblyEnd(_stressxyP);  VecAssemblyEnd(_sigmadev);

  if (_Nz > 1) {
    VecAssemblyBegin(_stressxzP);
    VecAssemblyEnd(_stressxzP);
  }

  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s: time=%.15e\n",funcName.c_str(),FILENAME,time);
      CHKERRQ(ierr);
  #endif
  return ierr = 0;
}


PetscErrorCode PowerLaw::setMMSInitialConditions()
{
  PetscErrorCode ierr = 0;
  #if VERBOSE > 1
  string funcName = "PowerLaw::setMMSInitialConditions()";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s\n",funcName.c_str(),FILENAME);CHKERRQ(ierr);
  #endif

  PetscScalar time = _initTime;

  mapToVec(_gxyP,MMS_gxy,_Nz,_dy,_dz,time);
  mapToVec(_gxyP,MMS_gxz,_Nz,_dy,_dz,time);


  // create rhs: set boundary conditions, set rhs, add source terms
  ierr = setMMSBoundaryConditions(time);CHKERRQ(ierr); // modifies _bcLP,_bcRP,_bcTP, and _bcBP
  ierr = _sbpP.setRhs(_rhsP,_bcLP,_bcRP,_bcTP,_bcBP);CHKERRQ(ierr);
  Vec viscSource,HxviscSource,uSource,HxuSource;
  ierr = VecDuplicate(_uP,&viscSource);CHKERRQ(ierr);
  ierr = VecDuplicate(_uP,&HxviscSource);CHKERRQ(ierr);
  ierr = VecDuplicate(_uP,&uSource);CHKERRQ(ierr);
  ierr = VecDuplicate(_uP,&HxuSource);CHKERRQ(ierr);

  mapToVec(viscSource,MMS_gSource,_Nz,_dy,_dz,time);
  ierr = _sbpP.H(viscSource,HxviscSource);
  VecDestroy(&viscSource);
  mapToVec(uSource,MMS_uSource,_Nz,_dy,_dz,time);
  ierr = _sbpP.H(uSource,HxuSource);
  VecDestroy(&uSource);

  ierr = VecAXPY(_rhsP,1.0,HxviscSource);CHKERRQ(ierr); // rhs = rhs + viscSource
  ierr = VecAXPY(_rhsP,1.0,HxuSource);CHKERRQ(ierr); // rhs = rhs + H*usource
  VecDestroy(&HxviscSource);
  VecDestroy(&HxuSource);


  // solve for displacement
  double startTime = MPI_Wtime();
  ierr = KSPSolve(_kspP,_rhsP,_uP);CHKERRQ(ierr);
  _linSolveTime += MPI_Wtime() - startTime;
  _linSolveCount++;
  ierr = setSurfDisp();

  // set shear traction on fault
  VecSet(_stressxyP,0.0);
  ierr = _fault.setTauQS(_stressxyP,NULL);CHKERRQ(ierr);

  // set rates for locked fault
  //~VecSet(*dvarBegin,0.0);
  //~VecSet(*(dvarBegin+1),0.0);
  //~ierr = _fault.d_dt(varBegin,varEnd, dvarBegin, dvarEnd); // sets rates for slip and state
  //~ierr = setViscStrainRates(time,varBegin,varEnd,dvarBegin,dvarEnd);CHKERRQ(ierr); // sets viscous strain rates


  VecDestroy(&viscSource);


  #if VERBOSE > 1
  PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s.\n",funcName.c_str(),FILENAME);
  #endif
  return ierr;
}


PetscErrorCode PowerLaw::measureMMSError()
{
  PetscErrorCode ierr = 0;

  // measure error between analytical and numerical solution
  Vec uA,gxyA,gxzA;
  VecDuplicate(_uP,&uA);
  VecDuplicate(_uP,&gxyA);
  VecDuplicate(_uP,&gxzA);
  mapToVec(uA,MMS_uA,_Nz,_dy,_dz,_currTime);
  mapToVec(gxyA,MMS_gxy,_Nz,_dy,_dz,_currTime);
  mapToVec(gxzA,MMS_gxz,_Nz,_dy,_dz,_currTime);

  double err2u = computeNormDiff_2(_uP,uA);
  double err2epsxy = computeNormDiff_2(_gxyP,gxyA);
  double err2epsxz = computeNormDiff_2(_gxzP,gxzA);

  PetscPrintf(PETSC_COMM_WORLD,"%3i %.4e %.4e % .15e %.4e % .15e\n",
              _Ny,_dy,err2u,log2(err2u),err2epsxy,log2(err2epsxy));

  return ierr;
}


PetscErrorCode PowerLaw::timeMonitor(const PetscReal time,const PetscInt stepCount,
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

// Outputs data at each time step.
PetscErrorCode PowerLaw::debug(const PetscReal time,const PetscInt stepCount,
                     const_it_vec varBegin,const_it_vec varEnd,
                     const_it_vec dvarBegin,const_it_vec dvarEnd,const char *stage)
{
  PetscErrorCode ierr = 0;

#if ODEPRINT > 0
  PetscInt       Istart,Iend;
  PetscScalar    bcRval,uVal,psiVal,velVal,dQVal,tauQS;
  PetscScalar    epsVxy,depsVxy;

  //~PetscScalar k = _muArrPlus[0]/2/_Ly;

  ierr= VecGetOwnershipRange(*varBegin,&Istart,&Iend);CHKERRQ(ierr);
  ierr = VecGetValues(*varBegin,1,&Istart,&psiVal);CHKERRQ(ierr);

  ierr = VecGetValues(*(varBegin+1),1,&Istart,&uVal);CHKERRQ(ierr);

  ierr= VecGetOwnershipRange(*dvarBegin,&Istart,&Iend);CHKERRQ(ierr);
  ierr = VecGetValues(*dvarBegin,1,&Istart,&dQVal);CHKERRQ(ierr);
  ierr = VecGetValues(*(dvarBegin+1),1,&Istart,&velVal);CHKERRQ(ierr);

  ierr = VecGetValues(*(varBegin+2),1,&Istart,&epsVxy);CHKERRQ(ierr);
  ierr = VecGetValues(*(dvarBegin+2),1,&Istart,&depsVxy);CHKERRQ(ierr);

  ierr= VecGetOwnershipRange(_bcRP,&Istart,&Iend);CHKERRQ(ierr);
  ierr = VecGetValues(_bcRP,1,&Istart,&bcRval);CHKERRQ(ierr);

  ierr = VecGetValues(_fault._tauQSP,1,&Istart,&tauQS);CHKERRQ(ierr);

  if (stepCount == 0) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,"%-4s %-6s  | %-15s %-15s %-15s | %-15s %-15s %-16s | %-15s\n",
                       "Step","Stage", "bcR","D","eVxy", "tauQS","V","deVxy","time");
    CHKERRQ(ierr);
  }
  ierr = PetscPrintf(PETSC_COMM_WORLD,"%4i %-6s ",stepCount,stage);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," | %.9e %.9e %.9e ",bcRval,uVal,epsVxy);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," | %.9e %.9e %.9e ",tauQS,velVal,depsVxy);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD," | %.9e\n",time);CHKERRQ(ierr);


  //~VecView(_fault._tauQSP,PETSC_VIEWER_STDOUT_WORLD);
#endif
  return ierr;
}


PetscErrorCode PowerLaw::writeContext(const string outputDir)
{
  PetscErrorCode ierr = 0;
  #if VERBOSE > 1
    string funcName = "PowerLaw::writeContext";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
  #endif

  PetscViewer    vw;

  std::string str = outputDir + "powerLawA";
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,str.c_str(),FILE_MODE_WRITE,&vw);CHKERRQ(ierr);
  ierr = VecView(_A,vw);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&vw);CHKERRQ(ierr);

  str = outputDir + "powerLawB";
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,str.c_str(),FILE_MODE_WRITE,&vw);CHKERRQ(ierr);
  ierr = VecView(_B,vw);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&vw);CHKERRQ(ierr);

  str = outputDir + "n";
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,str.c_str(),FILE_MODE_WRITE,&vw);CHKERRQ(ierr);
  ierr = VecView(_n,vw);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&vw);CHKERRQ(ierr);

  // output critical distance
  str =  outputDir + "T";
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,str.c_str(),FILE_MODE_WRITE,&vw);CHKERRQ(ierr);
  ierr = VecView(_T,vw);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&vw);CHKERRQ(ierr);


#if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
  #endif
  return ierr;
}


PetscErrorCode PowerLaw::writeStep1D()
{
  PetscErrorCode ierr = 0;
  #if VERBOSE > 1
    string funcName = "PowerLaw::writeStep1D";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s at step %i\n",funcName.c_str(),FILENAME,_stepCount);
    CHKERRQ(ierr);
  #endif

  double startTime = MPI_Wtime();

  if (_stepCount==0) {
    // write contextual fields
    ierr = _sbpP.writeOps(_outputDir);CHKERRQ(ierr);
    ierr = writeContext(_outputDir);CHKERRQ(ierr);
    ierr = _fault.writeContext(_outputDir);CHKERRQ(ierr);

    ierr = PetscViewerASCIIOpen(PETSC_COMM_WORLD,(_outputDir+"time.txt").c_str(),&_timeV1D);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(_timeV1D, "%.15e\n",_currTime);CHKERRQ(ierr);

    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"surfDispPlus").c_str(),
                                 FILE_MODE_WRITE,&_surfDispPlusViewer);CHKERRQ(ierr);
    ierr = VecView(_surfDispPlus,_surfDispPlusViewer);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_surfDispPlusViewer);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"surfDispPlus").c_str(),
                                   FILE_MODE_APPEND,&_surfDispPlusViewer);CHKERRQ(ierr);

    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"bcR").c_str(),
              FILE_MODE_WRITE,&_bcRPlusV);CHKERRQ(ierr);
    ierr = VecView(_bcRP,_bcRPlusV);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_bcRPlusV);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"bcR").c_str(),
                                   FILE_MODE_APPEND,&_bcRPlusV);CHKERRQ(ierr);

    ierr = _fault.writeStep(_outputDir,_stepCount);CHKERRQ(ierr);
  }
  else {
    ierr = PetscViewerASCIIPrintf(_timeV1D, "%.15e\n",_currTime);CHKERRQ(ierr);
    ierr = _fault.writeStep(_outputDir,_stepCount);CHKERRQ(ierr);

    ierr = VecView(_surfDispPlus,_surfDispPlusViewer);CHKERRQ(ierr);
    ierr = VecView(_bcRP,_bcRPlusV);CHKERRQ(ierr);
  }

  _writeTime += MPI_Wtime() - startTime;
  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s at step %i\n",funcName.c_str(),FILENAME,_stepCount);
    CHKERRQ(ierr);
  #endif
  return ierr;
}


PetscErrorCode PowerLaw::writeStep2D()
{
  PetscErrorCode ierr = 0;
  #if VERBOSE > 1
    string funcName = "PowerLaw::writeStep2D";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s at step %i\n",funcName.c_str(),FILENAME,_stepCount);
    CHKERRQ(ierr);
  #endif

  double startTime = MPI_Wtime();

  if (_stepCount==0) {
    // write contextual fields
    ierr = PetscViewerASCIIOpen(PETSC_COMM_WORLD,(_outputDir+"time2D.txt").c_str(),&_timeV2D);CHKERRQ(ierr);
    ierr = PetscViewerASCIIPrintf(_timeV2D, "%.15e\n",_currTime);CHKERRQ(ierr);

    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"uBodyP").c_str(),
              FILE_MODE_WRITE,&_uPV);CHKERRQ(ierr);
    ierr = VecView(_uP,_uPV);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_uPV);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"uBodyP").c_str(),
                                   FILE_MODE_APPEND,&_uPV);CHKERRQ(ierr);

    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"totStrainxyP").c_str(),
              FILE_MODE_WRITE,&_gTxyPV);CHKERRQ(ierr);
    ierr = VecView(_gTxyP,_gTxyPV);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_gTxyPV);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"totStrainxyP").c_str(),
                                   FILE_MODE_APPEND,&_gTxyPV);CHKERRQ(ierr);

    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"stressxyP").c_str(),
              FILE_MODE_WRITE,&_stressxyPV);CHKERRQ(ierr);
    ierr = VecView(_stressxyP,_stressxyPV);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_stressxyPV);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"stressxyP").c_str(),
                                   FILE_MODE_APPEND,&_stressxyPV);CHKERRQ(ierr);

    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"epsVxyP").c_str(),
              FILE_MODE_WRITE,&_gxyPV);CHKERRQ(ierr);
    ierr = VecView(_gxyP,_gxyPV);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_gxyPV);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"epsVxyP").c_str(),
                                   FILE_MODE_APPEND,&_gxyPV);CHKERRQ(ierr);

    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"effVisc").c_str(),
              FILE_MODE_WRITE,&_effViscV);CHKERRQ(ierr);
    ierr = VecView(_effVisc,_effViscV);CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&_effViscV);CHKERRQ(ierr);
    ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"effVisc").c_str(),
                                   FILE_MODE_APPEND,&_effViscV);CHKERRQ(ierr);
    //~if (_isMMS) {
      //~ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"uAnal").c_str(),
                //~FILE_MODE_WRITE,&_uAnalV);CHKERRQ(ierr);
      //~ierr = VecView(_uAnal,_uAnalV);CHKERRQ(ierr);
      //~ierr = PetscViewerDestroy(&_uAnalV);CHKERRQ(ierr);
      //~ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"uAnal").c_str(),
                                     //~FILE_MODE_APPEND,&_uAnalV);CHKERRQ(ierr);
    //~}
    if (_Nz>1)
    {
      ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"totStrainxzP").c_str(),
              FILE_MODE_WRITE,&_gTxzPV);CHKERRQ(ierr);
      ierr = VecView(_gTxzP,_gTxzPV);CHKERRQ(ierr);
      ierr = PetscViewerDestroy(&_gTxzPV);CHKERRQ(ierr);
      ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"totStrainxzP").c_str(),
                                     FILE_MODE_APPEND,&_gTxzPV);CHKERRQ(ierr);

      ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"stressxzP").c_str(),
               FILE_MODE_WRITE,&_stressxzPV);CHKERRQ(ierr);
      ierr = VecView(_stressxzP,_stressxzPV);CHKERRQ(ierr);
      ierr = PetscViewerDestroy(&_stressxzPV);CHKERRQ(ierr);
      ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"stressxzP").c_str(),
                                     FILE_MODE_APPEND,&_stressxzPV);CHKERRQ(ierr);

      ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"epsVxzP").c_str(),
               FILE_MODE_WRITE,&_gxzPV);CHKERRQ(ierr);
      ierr = VecView(_gxzP,_gxzPV);CHKERRQ(ierr);
      ierr = PetscViewerDestroy(&_gxzPV);CHKERRQ(ierr);
      ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,(_outputDir+"epsVxzP").c_str(),
                                   FILE_MODE_APPEND,&_gxzPV);CHKERRQ(ierr);
    }
  }
  else {
    ierr = PetscViewerASCIIPrintf(_timeV2D, "%.15e\n",_currTime);CHKERRQ(ierr);

    ierr = VecView(_uP,_uPV);CHKERRQ(ierr);
    ierr = VecView(_gTxyP,_gTxyPV);CHKERRQ(ierr);
    ierr = VecView(_stressxyP,_stressxyPV);CHKERRQ(ierr);
    ierr = VecView(_gxyP,_gxyPV);CHKERRQ(ierr);
    ierr = VecView(_effVisc,_effViscV);CHKERRQ(ierr);
    //~if (_isMMS) {ierr = VecView(_uAnal,_uAnalV);CHKERRQ(ierr);}
    if (_Nz>1)
    {
      ierr = VecView(_gTxzP,_gTxzPV);CHKERRQ(ierr);
      ierr = VecView(_stressxzP,_stressxzPV);CHKERRQ(ierr);
      ierr = VecView(_gxzP,_gxzPV);CHKERRQ(ierr);
    }
  }

  _writeTime += MPI_Wtime() - startTime;
  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s at step %i\n",funcName.c_str(),FILENAME,_stepCount);
    CHKERRQ(ierr);
  #endif
  return ierr;
}

PetscErrorCode PowerLaw::view()
{
  PetscErrorCode ierr = 0;
  ierr = _quadrature->view();
  ierr = PetscPrintf(PETSC_COMM_WORLD,"\n-------------------------------\n\n");CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Runtime Summary:\n");CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   time spent in integration (s): %g\n",_integrateTime);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   time spent writing output (s): %g\n",_writeTime);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   number of times linear system was solved: %i\n",_linSolveCount);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"   time spent solving linear system (s): %g\n",_linSolveTime);CHKERRQ(ierr);
  ierr = PetscPrintf(PETSC_COMM_WORLD,"\n");CHKERRQ(ierr);
  return ierr;
}



// loads settings from the input text file
PetscErrorCode PowerLaw::loadSettings(const char *file)
{
  PetscErrorCode ierr = 0;
#if VERBOSE > 1
    std::string funcName = "PowerLaw::loadSettings()";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
  #endif
  PetscMPIInt rank,size;
  MPI_Comm_size(PETSC_COMM_WORLD,&size);
  MPI_Comm_rank(PETSC_COMM_WORLD,&rank);


  ifstream infile( file );
  string line,var;
  size_t pos = 0;
  while (getline(infile, line))
  {
    istringstream iss(line);
    pos = line.find(_delim); // find position of the delimiter
    var = line.substr(0,pos);

    // viscosity for asthenosphere
    if (var.compare("viscDistribution")==0) {
      _viscDistribution = line.substr(pos+_delim.length(),line.npos).c_str();
    }

    // names of each field's source file
    else if (var.compare("AFile")==0) {
      _AFile = line.substr(pos+_delim.length(),line.npos).c_str();
    }
    else if (var.compare("BFile")==0) {
      _BFile = line.substr(pos+_delim.length(),line.npos).c_str();
    }
    else if (var.compare("nFile")==0) {
      _nFile = line.substr(pos+_delim.length(),line.npos).c_str();
    }
    else if (var.compare("TFile")==0) {
      _TFile = line.substr(pos+_delim.length(),line.npos).c_str();
    }

    // if values are set by a vector
    else if (var.compare("AVals")==0) {
      string str = line.substr(pos+_delim.length(),line.npos);
      loadVectorFromInputFile(str,_AVals);
    }
    else if (var.compare("ADepths")==0) {
      string str = line.substr(pos+_delim.length(),line.npos);
      loadVectorFromInputFile(str,_ADepths);
    }
    else if (var.compare("BVals")==0) {
      string str = line.substr(pos+_delim.length(),line.npos);
      loadVectorFromInputFile(str,_BVals);
    }
    else if (var.compare("BDepths")==0) {
      string str = line.substr(pos+_delim.length(),line.npos);
      loadVectorFromInputFile(str,_BDepths);
    }
    else if (var.compare("nVals")==0) {
      string str = line.substr(pos+_delim.length(),line.npos);
      loadVectorFromInputFile(str,_nVals);
    }
    else if (var.compare("nDepths")==0) {
      string str = line.substr(pos+_delim.length(),line.npos);
      loadVectorFromInputFile(str,_nDepths);
    }
    else if (var.compare("TVals")==0) {
      string str = line.substr(pos+_delim.length(),line.npos);
      loadVectorFromInputFile(str,_TVals);
    }
    else if (var.compare("TDepths")==0) {
      string str = line.substr(pos+_delim.length(),line.npos);
      loadVectorFromInputFile(str,_TDepths);
    }

  }

  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
  #endif
  return ierr;
}

 //parse input file and load values into data members
PetscErrorCode PowerLaw::loadFieldsFromFiles()
{
  PetscErrorCode ierr = 0;
  #if VERBOSE > 1
    std::string funcName = "PowerLaw::loadFieldsFromFiles()";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
  #endif

  // load A
  ierr = VecCreate(PETSC_COMM_WORLD,&_A);CHKERRQ(ierr);
  ierr = VecSetSizes(_A,PETSC_DECIDE,_Ny*_Nz);CHKERRQ(ierr);
  ierr = VecSetFromOptions(_A);
  PetscObjectSetName((PetscObject) _A, "_A");
  ierr = loadVecFromInputFile(_A,_inputDir,_AFile);CHKERRQ(ierr);


  // load n
  ierr = VecCreate(PETSC_COMM_WORLD,&_n);CHKERRQ(ierr);
  ierr = VecSetSizes(_n,PETSC_DECIDE,_Ny*_Nz);CHKERRQ(ierr);
  ierr = VecSetFromOptions(_n);
  PetscObjectSetName((PetscObject) _n, "_n");
  ierr = loadVecFromInputFile(_n,_inputDir,_nFile);CHKERRQ(ierr);

    // load B
  ierr = VecCreate(PETSC_COMM_WORLD,&_B);CHKERRQ(ierr);
  ierr = VecSetSizes(_B,PETSC_DECIDE,_Ny*_Nz);CHKERRQ(ierr);
  ierr = VecSetFromOptions(_B);
  PetscObjectSetName((PetscObject) _B, "_B");
  ierr = loadVecFromInputFile(_B,_inputDir,_BFile);CHKERRQ(ierr);

    // load T (initial condition)
  ierr = VecCreate(PETSC_COMM_WORLD,&_T);CHKERRQ(ierr);
  ierr = VecSetSizes(_T,PETSC_DECIDE,_Ny*_Nz);CHKERRQ(ierr);
  ierr = VecSetFromOptions(_T);
  PetscObjectSetName((PetscObject) _T, "_T");
  ierr = loadVecFromInputFile(_T,_inputDir,_TFile);CHKERRQ(ierr);



  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
  #endif
  return ierr;
}


// Check that required fields have been set by the input file
PetscErrorCode PowerLaw::checkInput()
{
  PetscErrorCode ierr = 0;
  #if VERBOSE > 1
    std::string funcName = "PowerLaw::checkInput";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
  #endif

  assert(_viscDistribution.compare("mms")==0 ||
      _viscDistribution.compare("layered")==0 ||
      _viscDistribution.compare("loadFromFile")==0 );

  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
  #endif
  return ierr;
}

// set off-fault material properties
PetscErrorCode PowerLaw::setFields()
{
  PetscErrorCode ierr = 0;
  #if VERBOSE > 1
    std::string funcName = "PowerLaw::setFields";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
  #endif

  ierr = VecDuplicate(_uP,&_A);CHKERRQ(ierr);
  ierr = VecDuplicate(_uP,&_B);CHKERRQ(ierr);
  ierr = VecDuplicate(_uP,&_n);CHKERRQ(ierr);
  ierr = VecDuplicate(_uP,&_T);CHKERRQ(ierr);


  // set each field using it's vals and depths std::vectors
  if (_Nz == 1) {
    VecSet(_A,_AVals[0]);
    VecSet(_B,_BVals[0]);
    VecSet(_n,_nVals[0]);
    VecSet(_T,_TVals[0]);
  }
  else {
    ierr = setVecFromVectors(_A,_AVals,_ADepths);CHKERRQ(ierr);
    ierr = setVecFromVectors(_B,_BVals,_BDepths);CHKERRQ(ierr);
    ierr = setVecFromVectors(_n,_nVals,_nDepths);CHKERRQ(ierr);
    ierr = setVecFromVectors(_T,_TVals,_TDepths);CHKERRQ(ierr);
  }


  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
  #endif
return ierr;
}



// Fills vec with the linear interpolation between the pairs of points (vals,depths)
// this probably won't work if the vector is 2D instead of 1D
PetscErrorCode PowerLaw::setVecFromVectors(Vec& vec, vector<double>& vals,vector<double>& depths)
{
  PetscErrorCode ierr = 0;
  PetscInt       Ii,Istart,Iend;
  PetscScalar    v,z,z0,z1,v0,v1;
  #if VERBOSE > 1
    std::string funcName = "PowerLaw::setVecFromVectors";
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
  #endif

  // Find the appropriate starting pair of points to interpolate between: (z0,v0) and (z1,v1)
  z1 = depths.back();
  depths.pop_back();
  z0 = depths.back();
  v1 = vals.back();
  vals.pop_back();
  v0 = vals.back();
  ierr = VecGetOwnershipRange(vec,&Istart,&Iend);CHKERRQ(ierr);
  z = _dz*(Iend-1);
  while (z<z0) {
    z1 = depths.back();
    depths.pop_back();
    z0 = depths.back();
    v1 = vals.back();
    vals.pop_back();
    v0 = vals.back();
    //~PetscPrintf(PETSC_COMM_WORLD,"2: z = %g: z0 = %g   z1 = %g   v0 = %g  v1 = %g\n",z,z0,z1,v0,v1);
  }


  for (Ii=Iend-1; Ii>=Istart; Ii--) {
    z = _dz*Ii;
    if (z==z1) { v = v1; }
    else if (z==z0) { v = v0; }
    else if (z>z0 && z<z1) { v = (v1 - v0)/(z1-z0) * (z-z0) + v0; }

    // if z is no longer bracketed by (z0,z1), move on to the next pair of points
    if (z<=z0) {
      z1 = depths.back();
      depths.pop_back();
      z0 = depths.back();
      v1 = vals.back();
      vals.pop_back();
      v0 = vals.back();
    }
    ierr = VecSetValues(vec,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = VecAssemblyBegin(vec);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(vec);CHKERRQ(ierr);

  VecView(vec,PETSC_VIEWER_STDOUT_WORLD);

  #if VERBOSE > 1
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending %s in %s\n",funcName.c_str(),FILENAME);
    CHKERRQ(ierr);
  #endif
  return ierr;
}
