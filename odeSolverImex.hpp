#ifndef ODESOLVERIMEX_HPP_INCLUDED
#define ODESOLVERIMEX_HPP_INCLUDED

#include <petscts.h>
#include <string>
#include <cmath>
#include <vector>
#include <map>
#include <algorithm>
#include <assert.h>
#include "integratorContextImex.hpp"
#include "genFuncs.hpp"

/*
 * Provides a set of algorithms to solve a system of ODEs of
 * the form y' = f(t,y) using IMEX time stepping,
 * where y is represented as an array of one or more Vecs
 * (PETSc data type).
 *
 * Containers for integration:
 *   var          map<string,Vec> of explicitly integrated variables
 *   varImMult    map<string,Vec> of implicitly integrated variables, computed at intermediate stages
 *   varIm1       map<string,Vec> of implicitly integrated variables, computed once per time step
 *
 * SOLVER TYPE      ALGORITHM
 *  IMEX        explicit part Runge-Kutta (2,3), implicit controlled by user
 *
 *
 * At minimum, the user must specify:
 *     QUANTITY               FUNCTION
 *  max number of steps      constructor
 *  solver type              constructor
 *  initial conditions       setInitialConds     Note: this array will be modified during integration
 *  initial step size        constructor
 *  step size alg            constructor (this is only used by the adaptive time-stepping algorithm)
 *  f(t,y)                   object passed to integrate must have member function d_dt(PetscScalar, PetscScalar*,PetscScalar*)
 *  final time               constructor and setTimeRange
 *  timeMonitor              object passed to integrate must have member function timeMonitor
 *
 *
 * Optional fields that can also be specified:
 *     QUANTITY               FUNCTION
 *  tolerance                setTolerance
 *  maximum step size        setStepSize
 *  minimum step size        setTimeStepBounds
 *  initial step size        setTimeStepBounds
 *
 * Once the odeSolver context is set, call integrate() to perform
 * the integration.
 *
 * y(t=final time) is stored in the initial conditions array.  Summary output
 * information is provided by viewSolver.  Users can obtain information at
 * each time step within a user-defined monitor function.
 *
 */

class OdeSolverImex
{
  protected:

    PetscReal               _initT,_finalT,_currT,_deltaT;
    PetscInt                _maxNumSteps,_stepCount;
    std::map<string,Vec>    _varEx,_dvar; // explicit integration variable and rate
    std::map<string,Vec>    _varIm; // implicit integration variable, once per time step
    std::vector<string>     _errInds; // which inds of _var to use for error control
    int                     _lenVar;
    double                  _runTime;
    string                  _controlType;

  public:

    PetscReal   _minDeltaT,_maxDeltaT;
    PetscReal   _atol; // absolute and relative tolerances
    PetscReal   _kappa,_ord; // safety factor in step size determinance
    PetscReal   _absErr[3]; // safety factor in step size determinance
    PetscInt    _numRejectedSteps,_numMinSteps,_numMaxSteps;

    OdeSolverImex(PetscInt maxNumSteps,PetscReal finalT,PetscReal deltaT,string controlType);
    virtual ~OdeSolverImex() {};

    virtual PetscErrorCode setTimeRange(const PetscReal initT,const PetscReal finalT) = 0;
    virtual PetscErrorCode setStepSize(const PetscReal deltaT) = 0;

    virtual PetscErrorCode setTolerance(const PetscReal atol) = 0;
    virtual PetscErrorCode setTimeStepBounds(const PetscReal minDeltaT, const PetscReal maxDeltaT) = 0;
    virtual PetscErrorCode setInitialConds(std::map<string,Vec>& varEx, std::map<string,Vec>& varIm) = 0;
    virtual PetscErrorCode setErrInds(std::vector<string>& errInds) = 0;
    virtual PetscErrorCode view() = 0;
    virtual PetscErrorCode integrate(IntegratorContextImex *obj) = 0;


    virtual PetscReal computeStepSize(const PetscReal totErr) = 0;
    virtual PetscReal computeError() = 0;
};

// Explicit RK32 scheme from Hairer et al., with added Backward Euler implicit scheme once per time step
class RK32_WBE : public OdeSolverImex
{
  public:

    // intermediate values for time stepping for the explicit variable
    std::map<string,Vec> _varHalfdT,_dvarHalfdT,_vardT,_dvardT,_var2nd,_dvar2nd,_var3rd;
    std::map<string,Vec> _vardTIm;


    RK32_WBE(PetscInt maxNumSteps,PetscReal finalT,PetscReal deltaT,string controlType);
    ~RK32_WBE();

    PetscErrorCode setTimeRange(const PetscReal initT,const PetscReal finalT);
    PetscErrorCode setStepSize(const PetscReal deltaT);

    PetscErrorCode setTolerance(const PetscReal atol);
    PetscErrorCode setTimeStepBounds(const PetscReal minDeltaT, const PetscReal maxDeltaT);
    PetscErrorCode setInitialConds(std::map<string,Vec>& varEx, std::map<string,Vec>& varIm);
    PetscErrorCode setErrInds(std::vector<string>& errInds);
    PetscErrorCode view();
    PetscErrorCode integrate(IntegratorContextImex *obj);


    PetscReal computeStepSize(const PetscReal totErr);
    PetscReal computeError();
};


// Runge-Kutta 4(3) scheme for explicit time integration, with added
// Backward Euler implicit scheme once per time step.
// Based on "ARK4(3)6L[2]SA-ERK" algorithm from Kennedy and Carpenter (2003):
// "Additive Runge-Kutta schemes for convection-diffusion-reaction equations"
// Note: Has matching IMEX equivalent
class RK43_WBE : public OdeSolverImex
{
  public:

    // intermediate values for time stepping for the explicit variable
    std::map<string,Vec> _k1,_k2,_k3,_k4,_k5,_k6,_y4,_y3;
    std::map<string,Vec> _f1,_f2,_f3,_f4,_f5,_f6;

    // intermediate value for implict variable
    std::map<string,Vec> _vardTIm;

    std::map<string,Vec> _var, _dvar; // accepted stages



    RK43_WBE(PetscInt maxNumSteps,PetscReal finalT,PetscReal deltaT,string controlType);
    ~RK43_WBE();

    PetscErrorCode setTimeRange(const PetscReal initT,const PetscReal finalT);
    PetscErrorCode setStepSize(const PetscReal deltaT);

    PetscErrorCode setTolerance(const PetscReal atol);
    PetscErrorCode setTimeStepBounds(const PetscReal minDeltaT, const PetscReal maxDeltaT);
    PetscErrorCode setInitialConds(std::map<string,Vec>& varEx, std::map<string,Vec>& varIm);
    PetscErrorCode setErrInds(std::vector<string>& errInds);
    PetscErrorCode view();
    PetscErrorCode integrate(IntegratorContextImex *obj);


    PetscReal computeStepSize(const PetscReal totErr);
    PetscReal computeError();
};

#endif

