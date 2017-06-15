#ifndef HEATEQUATION_H_INCLUDED
#define HEATEQUATION_H_INCLUDED

#include <petscksp.h>
#include <string>
#include <cmath>
#include <assert.h>
#include <vector>
#include "genFuncs.hpp"
#include "domain.hpp"
#include "sbpOps.hpp"
#include "sbpOps_c.hpp"
#include "sbpOps_fc.hpp"
#include "sbpOps_fc_coordTrans.hpp"
#include "integratorContextEx.hpp"
#include "integratorContextImex.hpp"
#include "odeSolver.hpp"
#include "odeSolverImex.hpp"



/* Base class for the heat equation
 */
class HeatEquation: public IntegratorContextEx
{
  private:
    // disable default copy constructor and assignment operator
    HeatEquation(const HeatEquation &that);
    HeatEquation& operator=(const HeatEquation &rhs);

  protected:

    // domain dimensions etc
    const PetscInt       _order,_Ny,_Nz;
    const PetscScalar    _Ly,_Lz,_dy,_dz;
    const Vec            *_y,*_z; // to handle variable grid spacing
    const PetscScalar    _kspTol;

    const char       *_file; // input file location
    std::string       _outputDir; // output file location
    std::string       _delim; // format is: var delim value (without the white space)
    std::string       _inputDir; // directory to load viscosity from
    std::string       _heatFieldsDistribution;
    std::string       _kFile,_rhoFile,_hFile,_cFile; // names of each file within loadFromFile


    std::vector<double> _rhoVals,_rhoDepths,_kVals,_kDepths,_hVals,_hDepths,_cVals,_cDepths,_TVals,_TDepths;
    Vec     _k,_kL,_rho,_c,_h;  // thermal conductivity, left bc portion of k, density, heat capacity, heat generation
    PetscViewer  _TV; // temperature viewer
    PetscViewer  _bcRVw,_bcTVw,_bcLVw,_bcBVw; // output BCs
    PetscViewer          _timeV; // time output viewer for debugging

    //~ SbpOps_fc* _sbpT;
    SbpOps* _sbpT;
    Vec _bcT,_bcR,_bcB,_bcL; // boundary conditions

    // linear system data
    std::string          _linSolver;
    KSP                  _ksp;
    PC                   _pc;
    Mat                  _I,_rhoC,_A,_pcMat; // intermediates for Backward Euler
    std::string          _sbpType;
    int                  _computePC; // # of steps since PC was last computed

    // runtime data
    double               _linSolveTime,_linSolveTime1,_factorTime;
    PetscInt             _linSolveCount,_pcRecomputeCount;

    // load settings from input file
    PetscErrorCode loadSettings(const char *file);
    PetscErrorCode setFields();
    PetscErrorCode setVecFromVectors(Vec& vec, vector<double>& vals,vector<double>& depths);
    PetscErrorCode loadFieldsFromFiles();
    PetscErrorCode setKL();

    PetscErrorCode checkInput();     // check input from file

    PetscErrorCode computeSteadyStateTemp();
    PetscErrorCode setBCsforBE();
    PetscErrorCode setupKSP(SbpOps* sbp,const PetscScalar dt);


  public:

    Vec _T;

    HeatEquation(Domain&D);
    ~HeatEquation();

    PetscErrorCode getTemp(Vec& T); // return temperature
    PetscErrorCode setTemp(Vec& T); // set temperature

    // compute rate
    PetscErrorCode d_dt(const PetscScalar time,const Vec slipVel,const Vec& tau, const Vec& sigmaxy,
      const Vec& sigmaxz, const Vec& dgxy, const Vec& dgxz,const Vec& T, Vec& dTdt);

    // implicitly solve for temperature using backward Euler
    PetscErrorCode be(const PetscScalar time,const Vec slipVel,const Vec& tau, const Vec& sigmaxy,
      const Vec& sigmaxz, const Vec& dgxy, const Vec& dgxz,Vec& T,const Vec& To,const PetscScalar dt);

    // to test with MMS
    //~ PetscErrorCode setMMSInitialConditions();
    PetscErrorCode setMMSBoundaryConditions(const double time,
        std::string bcRType,std::string bcTType,std::string bcLType,std::string bcBType);
    PetscErrorCode d_dt_mms(const PetscScalar time,const Vec& T, Vec& dTdt);
    PetscErrorCode d_dt(const PetscScalar time,const_it_vec varBegin,it_vec dvarBegin);
    PetscErrorCode timeMonitor(const PetscReal time,const PetscInt stepCount,
                     const_it_vec varBegin,const_it_vec dvarBegin);
    PetscErrorCode debug(const PetscReal time,const PetscInt stepCount,
                         const_it_vec varBegin,const_it_vec dvarBegin,const char *stage);
    PetscErrorCode integrate(); // will call OdeSolver method by same name

    // IO commands
    PetscErrorCode view();
    PetscErrorCode writeContext();
    PetscErrorCode writeStep2D(const PetscInt stepCount);
};





#endif
