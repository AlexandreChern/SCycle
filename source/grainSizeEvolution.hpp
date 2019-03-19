#ifndef GRAINSIZEEVOLUTION_H_INCLUDED
#define GRAINSIZEEVOLUTION_H_INCLUDED

#include <petscksp.h>
#include <string>
#include <cmath>
#include <vector>
#include "genFuncs.hpp"
#include "domain.hpp"
#include "heatEquation.hpp"

using namespace std;

/*
 * Class to explore grain size evolution. Currently uses the grain evolution law
 * from Austin and Evans (2007).
 */

class GrainSizeEvolution
{
private:
  // disable default copy constructor and assignment operator
  GrainSizeEvolution(const GrainSizeEvolution &that);
  GrainSizeEvolution& operator=(const GrainSizeEvolution &rhs);

public:

  Domain         *_D;
  const char     *_file;
  string          _delim; // format is: var delim value (without the white space)
  string          _outputDir;  // output data

  const PetscInt  _order,_Ny,_Nz;
  PetscScalar     _Ly,_Lz,_dy,_dz;
  Vec            *_y,*_z;

  // material properties
  Vec             _A,_QR,_p; // static grain growth parameters
  Vec             _f; // fraction of mechanical work done by dislocation creep that reduces grain size
  vector<double>  _AVals,_ADepths,_QRVals,_QRDepths,_pVals,_pDepths,_fVals,_fDepths;
  vector<double>  _gVals,_gDepths; // for initialization

  Vec          _g; // grain size
  Vec          _dg; // rate of grain size evolution


  // viewers
  // viewers:
  // 1st string = key naming relevant field, e.g. "slip"
  // 2nd PetscViewer = PetscViewer object for file IO
  // 3rd string = full file path name for output
  //~ map <string,PetscViewer>  _viewers;
  map <string,pair<PetscViewer,string> >  _viewers;

  GrainSizeEvolution(Domain& D);
  ~GrainSizeEvolution();

  // initialize and set data
  PetscErrorCode loadSettings(const char *file); // load settings from input file
  PetscErrorCode checkInput(); // check input from file
  PetscErrorCode allocateFields(); // allocate space for member fields
  PetscErrorCode setMaterialParameters();
  PetscErrorCode loadFieldsFromFiles(); // load non-effective-viscosity parameters

  // for steady-state computation
  PetscErrorCode initiateVarSS(map<string,Vec>& varSS);
  PetscErrorCode computeSteadyStateGrainSize(const Vec& sdev, const Vec& dgdev_disl, const Vec& Temp);


  // methods for explicit time stepping
  PetscErrorCode initiateIntegrand(const PetscScalar time,map<string,Vec>& varEx);
  PetscErrorCode updateFields(const PetscScalar time,const map<string,Vec>& varEx);
  PetscErrorCode d_dt(Vec& grainSize_t,const Vec& grainSize,const Vec& sdev, const Vec& dgdev_disl, const Vec& Temp);

  // file I/O
  PetscErrorCode writeDomain(const string outputDir);
  PetscErrorCode writeContext(const string outputDir);
  PetscErrorCode writeStep(const PetscInt stepCount, const PetscScalar time,const string outputDir);

};

#endif
