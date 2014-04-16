#include <petscts.h>
#include <string>
#include <assert.h>
#include "userContext.h"
#include "rateAndState.h"

/*
 * Computes rate and state friction as a function of input velocity
 * and relevant rate and state parameters.
 * ind = index corresponding to velocity input (used in a, psi, and s_NORM).
 * out = a.*asinh( (V ./ (2.*v0)) .* exp((psi)./p.a) )
 */
//~PetscErrorCode rateAndStateFrictionScalar(const PetscInt ind, PetscScalar vel,PetscScalar *out, void * ctx)
PetscErrorCode stressMstrength(const PetscInt ind,const PetscScalar vel,PetscScalar *out, void * ctx)
{
  PetscErrorCode ierr;
  UserContext    *D = (UserContext*) ctx;
  PetscScalar    psi,a,sigma_n,eta,tau;
  PetscInt       Istart,Iend;

#if VERBOSE > 1
  //~ ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting stressMstrength in rateAndState.c\n");CHKERRQ(ierr);
#endif

  ierr = VecGetOwnershipRange(D->psi,&Istart,&Iend);
  if ( (ind>=Istart) & (ind<Iend) ) {
    ierr = VecGetValues(D->tempPsi,1,&ind,&psi);CHKERRQ(ierr);
    ierr = VecGetValues(D->a,1,&ind,&a);CHKERRQ(ierr);
    ierr = VecGetValues(D->s_NORM,1,&ind,&sigma_n);CHKERRQ(ierr);
    ierr = VecGetValues(D->eta,1,&ind,&eta);CHKERRQ(ierr);
    ierr = VecGetValues(D->tau,1,&ind,&tau);CHKERRQ(ierr);
  }
  else {
    SETERRQ(PETSC_COMM_WORLD,1,"Attempting to access nonlocal array values in stressMstrength\n");
  }

   *out = (PetscScalar) a*sigma_n*asinh( (double) (vel/2/D->v0)*exp(psi/a) ) + eta*vel - tau;
  if (isnan(*out)) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,"psi=%g,a=%g,sigma_n=%g,eta=%g,tau=%g,vel=%g\n",psi,a,sigma_n,eta,tau,vel);
    CHKERRQ(ierr);
    //~SETERRQ(PETSC_COMM_SELF,1,"stressMstrength evaluated to nan");
    }
  else if (isinf(*out)) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,"psi=%g,a=%g,sigma_n=%g,eta=%g,tau=%g,vel=%g\n",psi,a,sigma_n,eta,tau,vel);
    CHKERRQ(ierr);
    //~SETERRQ(PETSC_COMM_SELF,1,"stressMstrength evaluated to inf");
    }

  assert(!isnan(*out));
  assert(!isinf(*out));

#if VERBOSE > 1
  //~ ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending stressMstrength in rateAndState.c\n");CHKERRQ(ierr);
#endif
  return ierr;
}

/*
 * Computes dPsi, the change in the state evolution parameter
 */
PetscErrorCode agingLaw(const PetscInt ind,const PetscScalar psi,PetscScalar *dPsi, void *ctx)
{
  PetscErrorCode ierr = 0;
  PetscInt       Istart,Iend;
  PetscScalar    b,vel;
  UserContext    *D = (UserContext *) ctx;

  double startTime = MPI_Wtime();

  ierr = VecGetOwnershipRange(D->psi,&Istart,&Iend);
  if ( (ind>=Istart) & (ind<Iend) ) {
    //~ierr = VecGetValues(D->psi,1,&ind,&psi);CHKERRQ(ierr);
    ierr = VecGetValues(D->b,1,&ind,&b);CHKERRQ(ierr);
    ierr = VecGetValues(D->V,1,&ind,&vel);CHKERRQ(ierr);
  }
  else {
    SETERRQ(PETSC_COMM_WORLD,1,"Attempting to access nonlocal array values in agingLaw\n");
  }

  if (b==0) { *dPsi = 0; }
  else {
    *dPsi = (PetscScalar) (b*D->v0/D->D_c)*( exp((double) ( (D->f0-psi)/b) ) - (vel/D->v0) );
  }

  //~if (ind==0) {
    //~ierr = PetscPrintf(PETSC_COMM_WORLD,"psi=%g,b=%g,f0=%g,D_c=%g,v0=%g,vel=%g\n",psi,b,D->f0,D->D_c,D->v0,vel);
    //~CHKERRQ(ierr);
  //~}

  if (isnan(*dPsi)) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,"psi=%g,b=%g,f0=%g,D_c=%g,v0=%g,vel=%g\n",psi,b,D->f0,D->D_c,D->v0,vel);
    CHKERRQ(ierr);
    //~SETERRQ(PETSC_COMM_SELF,1,"agingLaw evaluated to nan");
    }
  else if (isinf(*dPsi)) {
    ierr = PetscPrintf(PETSC_COMM_WORLD,"psi=%g,b=%g,f0=%g,D_c=%g,v0=%g,vel=%g\n",psi,b,D->f0,D->D_c,D->v0,vel);
    CHKERRQ(ierr);
    //~SETERRQ(PETSC_COMM_SELF,1,"agingLaw evaluated to inf");
    }

  assert(!isnan(*dPsi));
  assert(!isinf(*dPsi));

  double endTime = MPI_Wtime();
  D->agingLawTime = D->agingLawTime + (endTime-startTime);

  return ierr;
}


PetscErrorCode setRateAndState(UserContext &D)
{
  PetscErrorCode ierr;
  PetscInt       Ii,Istart,Iend;
  PetscScalar    v;

#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Starting rateAndStateConstParams in rateAndState.c\n");CHKERRQ(ierr);
#endif

  // Set normal stress along fault
  PetscScalar s_NORMVal = 50.0;
  ierr = VecDuplicate(D.a,&D.s_NORM);CHKERRQ(ierr);
  ierr = VecSet(D.s_NORM,s_NORMVal);CHKERRQ(ierr);
  ierr = VecAssemblyBegin(D.s_NORM);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(D.s_NORM);CHKERRQ(ierr);

  // Set a
  PetscScalar aVal = 0.015;
  ierr = VecSet(D.a,aVal);CHKERRQ(ierr);
  ierr = VecAssemblyBegin(D.a);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(D.a);CHKERRQ(ierr);

  // Set b
  PetscScalar L1 = D.H;  //Defines depth at which (a-b) begins to increase.
  PetscScalar L2 = 1.5*D.H;  //This is depth at which increase stops and fault is purely velocity strengthening.

  PetscInt    N1 = L1/D.dz;
  PetscInt    N2 = L2/D.dz;

  ierr = VecGetOwnershipRange(D.b,&Istart,&Iend);CHKERRQ(ierr);
  for (Ii=Istart;Ii<Iend;Ii++) {
    if (Ii < N1+1) {
      v=0.02;
      ierr = VecSetValues(D.b,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);
    }
    else if (Ii>N1 && Ii<=N2) {
      v = 0.02/(L1-L2);v = v*Ii*D.dz - v*L2;
      ierr = VecSetValues(D.b,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);
    }
    else {
      v = 0.0;
      ierr = VecSetValues(D.b,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = VecAssemblyBegin(D.b);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(D.b);CHKERRQ(ierr);

  //~ierr = VecSet(D.b,0.02);CHKERRQ(ierr); // for spring-slider!!!!!!!!!!!!!!!!

  /* p.tau_inf = p.s_NORM(1)*p.a(1)*asinh( p.vp/(2*p.v0)*exp(p.f0/p.a(1)) ) */
  v = 0.5*D.vp*exp(D.f0/aVal)/D.v0;
  D.tau_inf = s_NORMVal * aVal * asinh((double) v);

  // set state
  ierr = VecSet(D.psi,D.f0);CHKERRQ(ierr);
  ierr = VecSet(D.tempPsi,D.f0);CHKERRQ(ierr);

  // set shear modulus and radiation damping coefficient
  Vec muVec;
  ierr = VecCreate(PETSC_COMM_WORLD,&muVec);CHKERRQ(ierr);
  ierr = VecSetSizes(muVec,PETSC_DECIDE,D.Ny*D.Nz);CHKERRQ(ierr);
  ierr = VecSetFromOptions(muVec);CHKERRQ(ierr);
  ierr = VecGetOwnershipRange(muVec,&Istart,&Iend);CHKERRQ(ierr);
  PetscScalar y,z,r,rbar=0.25*D.W*D.W,rw=1+0.5*D.W/D.D;
  for (Ii=Istart;Ii<Iend;Ii++) {
    z = D.dz*(Ii-D.Nz*(Ii/D.Nz));
    y = D.dz*(Ii/D.Nz);
    r=y*y+(0.25*D.W*D.W/D.D/D.D)*z*z;
    v = 0.5*(D.muOut-D.muIn)*(tanh((double)(r-rbar)/rw)+1) + D.muIn;
    D.muArr[Ii]=v;
    ierr = VecSetValues(muVec,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);

    if (Ii<D.Nz) {
      if (z<D.D) {v = 0.5*sqrt(D.rhoIn*v);}
      else {v = 0.5*sqrt(D.rhoOut*v);}
      ierr = VecSetValues(D.eta,1,&Ii,&v,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = VecAssemblyBegin(muVec);CHKERRQ(ierr);  ierr = VecAssemblyBegin(D.eta);CHKERRQ(ierr);
  ierr = VecAssemblyEnd(muVec);CHKERRQ(ierr);    ierr = VecAssemblyEnd(D.eta);CHKERRQ(ierr);
  //~ierr = VecView(muVec,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  //~ierr = VecSet(muVec,D.G);CHKERRQ(ierr); // !!!!!!!!!!

  ierr = MatSetSizes(D.mu,PETSC_DECIDE,PETSC_DECIDE,D.Ny*D.Nz,D.Ny*D.Nz);CHKERRQ(ierr);
  ierr = MatSetFromOptions(D.mu);CHKERRQ(ierr);
  ierr = MatMPIAIJSetPreallocation(D.mu,1,NULL,1,NULL);CHKERRQ(ierr);
  ierr = MatSeqAIJSetPreallocation(D.mu,1,NULL);CHKERRQ(ierr);
  ierr = MatSetUp(D.mu);CHKERRQ(ierr);
  ierr = MatDiagonalSet(D.mu,muVec,INSERT_VALUES);CHKERRQ(ierr);

    //~//  radiation damping
  //~v = D.G/(2*D.cs);
  //~ierr = VecSet(D.eta,v);CHKERRQ(ierr);
  //~ierr = VecAssemblyBegin(D.eta);CHKERRQ(ierr);
  //~ierr = VecAssemblyEnd(D.eta);CHKERRQ(ierr);

#if VERBOSE > 1
  ierr = PetscPrintf(PETSC_COMM_WORLD,"Ending rateAndStateConstParams in rateAndState.c\n");CHKERRQ(ierr);
#endif
  return 0;
}

PetscErrorCode writeRateAndState(UserContext &D)
{
  PetscErrorCode ierr;
  PetscViewer    viewer;
  const char * outFileLoc;

  std::string str = D.outFileRoot + "a"; outFileLoc = str.c_str();
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,outFileLoc,FILE_MODE_WRITE,&viewer);CHKERRQ(ierr);
  ierr = VecView(D.a,viewer);CHKERRQ(ierr);

  str = D.outFileRoot + "b"; outFileLoc = str.c_str();
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,outFileLoc,FILE_MODE_WRITE,&viewer);CHKERRQ(ierr);
  ierr = VecView(D.b,viewer);CHKERRQ(ierr);

  str = D.outFileRoot + "eta"; outFileLoc = str.c_str();
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,outFileLoc,FILE_MODE_WRITE,&viewer);CHKERRQ(ierr);
  ierr = VecView(D.eta,viewer);CHKERRQ(ierr);

  str = D.outFileRoot + "s_NORM"; outFileLoc = str.c_str();
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,outFileLoc,FILE_MODE_WRITE,&viewer);CHKERRQ(ierr);
  ierr = VecView(D.s_NORM,viewer);CHKERRQ(ierr);

  str = D.outFileRoot + "mu"; outFileLoc = str.c_str();
  ierr = PetscViewerBinaryOpen(PETSC_COMM_WORLD,outFileLoc,FILE_MODE_WRITE,&viewer);CHKERRQ(ierr);
  ierr = MatView(D.mu,viewer);CHKERRQ(ierr);

  return ierr;
}
