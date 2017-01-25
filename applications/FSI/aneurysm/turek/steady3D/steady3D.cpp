#include "MultiLevelProblem.hpp"
#include "NumericVector.hpp"
#include "Fluid.hpp"
#include "Solid.hpp"
#include "Parameter.hpp"
#include "FemusInit.hpp"
#include "SparseMatrix.hpp"
#include "FElemTypeEnum.hpp"
#include "Files.hpp"
#include "MonolithicFSINonLinearImplicitSystem.hpp"
#include "../../include/FSISteadyStateAssembly.hpp"
#include "VTKWriter.hpp"

double scale = 1000.;

using namespace std;
using namespace femus;

bool SetBoundaryConditionOmino(const std::vector < double >& x, const char name[], 
			       double &value, const int facename, const double time);

bool SetBoundaryConditionAorta(const std::vector < double >& x, const char name[], 
			       double &value, const int facename, const double time);

bool SetBoundaryCondition(const std::vector < double >& x, const char name[],
                          double &value, const int facename, const double time);

bool SetBoundaryConditionTurek(const std::vector < double >& x, const char name[],
                               double &value, const int facename, const double time);

bool SetBoundaryConditionPorous(const std::vector < double >& x, const char name[], 
				double &value, const int facename, const double time);

bool SetBoundaryConditionThrombus(const std::vector < double >& x, const char name[], 
				  double &value, const int facename, const double time);

bool SetBoundaryConditionOminoPorous(const std::vector < double >& x, const char name[], 
				double &value, const int facename, const double time);
//------------------------------------------------------------------------------------------------------------------

int main(int argc, char **args) {

  // ******* Init Petsc-MPI communicator *******
  FemusInit mpinit(argc, args, MPI_COMM_WORLD);

    unsigned simulation = 0;

  if(argc >= 2) {
    if(!strcmp("0", args[1])) {    /** FSI Turek3D no stent */
      simulation = 0;
    }
    else if(!strcmp("1", args[1])) {     /** FSI Omino no stent */
      simulation = 1;
    }
    else if(!strcmp("2", args[1])) {   /** FSI Thoracic Aortic Aneurysm */
      simulation = 2;
    }
    else if(!strcmp("3", args[1])) {   /** FSI Abdominal Aortic Aneurysm */
      simulation = 3;
    }
  }

  bool dimension2D = false;

  //Files files;
  //files.CheckIODirectories();
  //files.RedirectCout();

  // ******* Extract the problem dimension and simulation identifier based on the inline input *******


  // ******* Extract the mesh.neu file name based on the simulation identifier *******
   std::string infile;
  if(simulation == 0) {
    infile = "./input/Turek_3D_D.neu";
  }
  else if(simulation == 1) {
    infile = "./input/aneurysm_omino.neu";
  }
  else if(simulation == 2) {
    infile = "./input/aneurisma_aorta.neu";
  }
  else if(simulation == 3) {
    infile = "./input/AAA.neu";
  }
  //std::string infile = "./input/aneurysm_omino.neu";
  //std::string infile = "./input/aneurisma_aorta.neu";
  // std::string infile = "./input/turek_porous_scaled.neu";
  //std::string infile = "./input/turek_porous_omino.neu";
  //std::string infile = "./input/Turek_3D_D.neu";
  //std::string infile = "./input/AAA_thrombus.neu";
  //std::string infile = "./input/AAA.neu";
  // ******* Set physics parameters *******
  double Lref, Uref, rhof, muf, rhos, ni, E;

  Lref = 1.;
  Uref = 1.;

  rhof = 1035.;
  muf = 3.38 * 1.0e-6 * rhof;
  rhos = 1120;
  ni = 0.5;
  E = 6000;
  
  // Maximum aneurysm_omino deformation (velocity = 0.1)
//   rhof = 1035.;
//   muf = 3.38 * 1.0e-6 * rhof;
//   rhos = 1120;
//   ni = 0.5;
//   E = 6000;
  
  // Maximum Turek_3D_D deformation (velocity = 0.2)
//   rhof = 1035.;
//   muf = 3.38 * 1.0e-6 * rhof;
//   rhos = 1120;
//   ni = 0.5;
//   E = 6000;

  //E = 60000 * 1.e1;
  
  Parameter par(Lref, Uref);

  // Generate Solid Object
  Solid solid;
  solid = Solid(par, E, ni, rhos, "Mooney-Rivlin");

  cout << "Solid properties: " << endl;
  cout << solid << endl;

  // Generate Fluid Object
  Fluid fluid(par, muf, rhof, "Newtonian");
  cout << "Fluid properties: " << endl;
  cout << fluid << endl;

  // ******* Init multilevel mesh from mesh.neu file *******
  unsigned short numberOfUniformRefinedMeshes, numberOfAMRLevels;

  numberOfUniformRefinedMeshes = 2;
  numberOfAMRLevels = 0;

  std::cout << 0 << std::endl;

  MultiLevelMesh ml_msh(numberOfUniformRefinedMeshes + numberOfAMRLevels, numberOfUniformRefinedMeshes,
                        infile.c_str(), "fifth", Lref, NULL);

  //ml_msh.EraseCoarseLevels(numberOfUniformRefinedMeshes - 1);

  ml_msh.PrintInfo();

  // ******* Init multilevel solution ******
  MultiLevelSolution ml_sol(&ml_msh);

  // ******* Add solution variables to multilevel solution and pair them *******
  ml_sol.AddSolution("DX", LAGRANGE, SECOND, 1);
  ml_sol.AddSolution("DY", LAGRANGE, SECOND, 1);
  if(!dimension2D) ml_sol.AddSolution("DZ", LAGRANGE, SECOND, 1);

  ml_sol.AddSolution("U", LAGRANGE, SECOND, 1);
  ml_sol.AddSolution("V", LAGRANGE, SECOND, 1);
  if(!dimension2D) ml_sol.AddSolution("W", LAGRANGE, SECOND, 1);

  // Pair each velocity variable with the corresponding displacement variable
  ml_sol.PairSolution("U", "DX"); // Add this line
  ml_sol.PairSolution("V", "DY"); // Add this line
  if(!dimension2D) ml_sol.PairSolution("W", "DZ"); // Add this line

  // Since the Pressure is a Lagrange multiplier it is used as an implicit variable
  ml_sol.AddSolution("P", DISCONTINOUS_POLYNOMIAL, FIRST, 1);
  ml_sol.AssociatePropertyToSolution("P", "Pressure", false); // Add this line

  // ******* Initialize solution *******
  ml_sol.Initialize("All");
    if(simulation == 0) {
    ml_sol.AttachSetBoundaryConditionFunction(SetBoundaryConditionTurek);
  }
  else if(simulation == 1) {
    ml_sol.AttachSetBoundaryConditionFunction(SetBoundaryConditionOmino);
  }
  else if(simulation == 2) {
    ml_sol.AttachSetBoundaryConditionFunction(SetBoundaryConditionAorta);
  }
  else if(simulation == 3) {
    ml_sol.AttachSetBoundaryConditionFunction(SetBoundaryConditionThrombus);
  }
  //ml_sol.AttachSetBoundaryConditionFunction(SetBoundaryConditionAorta);
  //ml_sol.AttachSetBoundaryConditionFunction(SetBoundaryConditionOmino);
  //ml_sol.AttachSetBoundaryConditionFunction(SetBoundaryConditionTurek);
  //ml_sol.AttachSetBoundaryConditionFunction(SetBoundaryConditionPorous);
  //ml_sol.AttachSetBoundaryConditionFunction(SetBoundaryConditionOminoPorous);
  //ml_sol.AttachSetBoundaryConditionFunction(SetBoundaryConditionThrombus);
  
  // ******* Set boundary conditions *******
  ml_sol.GenerateBdc("DX", "Steady");
  ml_sol.GenerateBdc("DY", "Steady");
  if(!dimension2D) ml_sol.GenerateBdc("DZ", "Steady");
  ml_sol.GenerateBdc("U", "Steady");
  ml_sol.GenerateBdc("V", "Steady");
  if(!dimension2D) ml_sol.GenerateBdc("W", "Steady");
  ml_sol.GenerateBdc("P", "Steady");


  // ******* Define the FSI Multilevel Problem *******

  MultiLevelProblem ml_prob(&ml_sol);
  // Add fluid object
  ml_prob.parameters.set<Fluid>("Fluid") = fluid;
  // Add Solid Object
  ml_prob.parameters.set<Solid>("Solid") = solid;

  // ******* Add FSI system to the MultiLevel problem *******
  MonolithicFSINonLinearImplicitSystem & system = ml_prob.add_system<MonolithicFSINonLinearImplicitSystem> ("Fluid-Structure-Interaction");
  system.AddSolutionToSystemPDE("DX");
  system.AddSolutionToSystemPDE("DY");
  if(!dimension2D) system.AddSolutionToSystemPDE("DZ");
  system.AddSolutionToSystemPDE("U");
  system.AddSolutionToSystemPDE("V");
  if(!dimension2D) system.AddSolutionToSystemPDE("W");
  system.AddSolutionToSystemPDE("P");

  // ******* System Fluid-Structure-Interaction Assembly *******
  system.SetAssembleFunction(FSISteadyStateAssembly);

  // ******* set MG-Solver *******
  system.SetMgType(F_CYCLE);

  system.SetNonLinearConvergenceTolerance(1.e-9);
  system.SetResidualUpdateConvergenceTolerance(1.e-15);
  system.SetMaxNumberOfNonLinearIterations(4);
  system.SetMaxNumberOfResidualUpdatesForNonlinearIteration(1);

  system.SetNumberPreSmoothingStep(0);
  system.SetNumberPostSmoothingStep(1);

  // ******* Set Preconditioner *******

  system.SetMgSmoother(ASM_SMOOTHER);

  system.init();

  // ******* Set Smoother *******
  system.SetSolverFineGrids(RICHARDSON);
  //system.SetSolverFineGrids(GMRES);

  system.SetPreconditionerFineGrids(ILU_PRECOND);

  system.SetTolerances(1.e-12, 1.e-20, 1.e+50, 20, 10);

  // ******* Add variables to be solved *******
  system.ClearVariablesToBeSolved();
  system.AddVariableToBeSolved("All");

  // ******* Set the last (1) variables in system (i.e. P) to be a schur variable *******
  system.SetNumberOfSchurVariables(1);

  // ******* Set block size for the ASM smoothers *******
  system.SetElementBlockNumber(2);

  // ******* For Gmres Preconditioner only *******
  //system.SetDirichletBCsHandling(ELIMINATION);


  // ******* Print solution *******
  ml_sol.SetWriter(VTK);


  std::vector<std::string> mov_vars;
  mov_vars.push_back("DX");
  mov_vars.push_back("DY");
  mov_vars.push_back("DZ");
  ml_sol.GetWriter()->SetMovingMesh(mov_vars);

  std::vector<std::string> print_vars;
  print_vars.push_back("All");

  ml_sol.GetWriter()->SetDebugOutput(true);
  ml_sol.GetWriter()->Write(DEFAULT_OUTPUTDIR, "biquadratic", print_vars, 0);


  // ******* Solve *******
  std::cout << std::endl;
  std::cout << " *********** Fluid-Structure-Interaction ************  " << std::endl;
  system.MGsolve();

  ml_sol.GetWriter()->Write(DEFAULT_OUTPUTDIR, "biquadratic", print_vars, 1);

  // ******* Clear all systems *******
  ml_prob.clear();
  return 0;
}


//---------------------------------------------------------------------------------------------------------------------





bool SetBoundaryCondition(const std::vector < double >& x, const char name[], double &value, const int facename, const double time) {
  bool test = 1; //dirichlet
  value = 0.;

  if(!strcmp(name, "U") || !strcmp(name, "V") || !strcmp(name, "W")) {

    if(1 == facename) {
      test = 0;
      value = 10.;
    }
    else if(2 == facename) {
      test = 0;
      value = 10.;

    }
    else if(3 == facename) {
      test = 0;
      value = 20.;
    }
  }

  else if(!strcmp(name, "P")) {
    test = 0;
    value = 0.;
  }

  else if(!strcmp(name, "DX") || !strcmp(name, "DY") || !strcmp(name, "DZ")) {
    if(7 == facename) {
      test = 0;
      value = 1.;
    }
  }

  return test;
}


bool SetBoundaryConditionTurek(const std::vector < double >& x, const char name[], double &value, const int facename, const double time) {
  bool test = 1; //dirichlet
  value = 0.;

  if(!strcmp(name, "U")) {

    if(1 == facename) {
      double r2 = ((x[1] * 1000.) - 7.) * ((x[1] * 1000.) - 7.) + (x[2] * 1000.) * (x[2] * 1000.);
      value = -0.2 * (1. - r2); //inflow
      //value=25;
    }
    else if(2 == facename) {
      test = 0;
      value = 0.;
    }
  }
  else if(!strcmp(name, "V") || !strcmp(name, "W")) {
    if(2 == facename) {
      test = 0;
      value = 0.;
    }
  }
  else if(!strcmp(name, "P")) {
    test = 0;
    value = 0.;
  }
  else if(!strcmp(name, "DX")) {
    if( 5 == facename || 6 == facename) {
      test = 0;
      value = 0;
    }
  }
  else if(!strcmp(name, "DY")) {
    if( 5 == facename || 6 == facename) {
      test = 0;
      value = 0;
    }
  }
  else if(!strcmp(name, "DZ")) {
    if( 5 == facename || 6 == facename) {
      test = 0;
      value = 0;
    }
  }

  return test;
}


bool SetBoundaryConditionPorous(const std::vector < double >& x, const char name[], double &value, const int facename, const double time) {
  bool test = 1; //dirichlet
  value = 0.;

  if(!strcmp(name, "U")  || !strcmp(name, "W")) {
    if(2 == facename) {
      test = 0;
      value = 0.;
    }
  }
  else if( !strcmp(name, "V") ) {
    if(1 == facename) {
      double r2 = (x[0] * 1000.) * (x[0] * 1000.) + (x[2] * 1000.) * (x[2] * 1000.);
      value = 0.25 * (1. - r2); //inflow
    }
    else if (2 == facename) {
      test = 0;
      value = 0.;
    }
  }
  else if(!strcmp(name, "P")) {
    test = 0;
    value = 0.;
  }
  else if(!strcmp(name, "DX")) {
    if( 1 == facename || 2 == facename || 5 == facename ) {
      test = 0;
      value = 0;
    }
  }
  else if(!strcmp(name, "DY")) {
    if( 5 == facename ) {
      test = 0;
      value = 0;
    }
  }
  else if(!strcmp(name, "DZ")) {
    if(1 == facename || 2 == facename || 5 == facename ) {
      test = 0;
      value = 0;
    }
  }

  return test;
}

bool SetBoundaryConditionOminoPorous(const std::vector < double >& x, const char name[], double &value, const int facename, const double time) {
  bool test = 1; //dirichlet
  value = 0.;

  if(!strcmp(name, "U")  || !strcmp(name, "V")) {
    if(1 == facename || 2 == facename) {
      test = 0;
      value = 0.;
    }
  }
  else if( !strcmp(name, "W") ) {
    if(6 == facename) {
      double r2 = (x[0] /.000375) * (x[0] /.000375) + (x[1] /.000375) * (x[1] /.000375);
      value = 1.0 * (1. - r2); //inflow
    }
    else if (1 == facename || 2 == facename) {
      test = 0;
      value = 0.;
    }
  }
  else if(!strcmp(name, "P")) {
    test = 0;
    value = 0.;
  }
  else if(!strcmp(name, "DX")) {
    if( 1 == facename || 2 == facename || 6 == facename || 5 == facename ) {
      test = 0;
      value = 0;
    }
  }
  else if(!strcmp(name, "DY")) {
    if( 5 == facename || 6 == facename ) {
      test = 0;
      value = 0;
    }
  }
  else if(!strcmp(name, "DZ")) {
    if(1 == facename || 2 == facename || 5 == facename ) {
      test = 0;
      value = 0;
    }
  }

  return test;
}

bool SetBoundaryConditionOmino(const std::vector < double >& x, const char name[], double &value, const int facename, const double time) {
  bool test = 1; //dirichlet
  value = 0.;
  
  if(!strcmp(name, "V")){
    if(3 == facename) {
      double r2 = ((x[2] * 1000.) + 0.403) * ((x[2] * 1000.) + 0.403) + ((x[0] * 1000.) + 0.589) * ((x[0] * 1000.) + 0.589);
      value = 0.1 * (1. - r2); //inflow
      //value = 0.1;
    }
    else if(1 == facename || 2 == facename ) {
      test = 0;
      value = 0;
    }
  }
  if(!strcmp(name, "U") || !strcmp(name, "W")){
    if(1 == facename || 2 == facename) {
      test = 0;
      value = 0;
    } 
  }
  else if(!strcmp(name, "P")) {
    if(1 == facename || 2 == facename) {
    test = 0;
    value = 0;
    }
  }
  else if(!strcmp(name, "DX") || !strcmp(name, "DY") || !strcmp(name, "DZ")) {
    if(7 == facename) {
      test = 0;
      value = 0.;
    }
  }

  return test;
}

bool SetBoundaryConditionAorta(const std::vector < double >& x, const char name[], double &value, const int facename, const double time) {
  bool test = 1; //dirichlet
  value = 0.;

  if(!strcmp(name, "V")) {
    if(1 == facename) {
      test = 0;
      value = 60;
    }
    if(2 == facename || 3 == facename || 4 == facename) {
      test = 0;
      value = 20;
    }
    else if(5 == facename) {
//       test = 0;
//       value = 0.5 *1.e-01; //Pressure value/rhof
      double r2 = ((x[0] + 0.075563)/0.0104) * ((x[0] + 0.075563)/0.0104) + (x[2] /0.0104) * (x[2] /0.0104);
      value = 0.03 * (1. - r2); //inflow
    }
  }
  
  if(!strcmp(name, "U") || !strcmp(name, "W")){
    if(1 == facename || 2 == facename || 3 == facename || 4 == facename){// || 5 == facename) {
      test = 0;
      value = 0;
    }
  }

  else if(!strcmp(name, "P")) {
    test = 0;
    value = 0.;
  }

//   else if(!strcmp(name, "DX") || !strcmp(name, "DY") || !strcmp(name, "DZ")) {
//     if(1 == facename || 6 == facename || 11 == facename || 2 == facename || 3 == facename || 4 == facename
//       || 7 == facename || 8 == facename || 9 == facename ) {
//       test = 0;
//       value = 0;
//     }

  else if(!strcmp(name, "DX") || !strcmp(name, "DY") || !strcmp(name, "DZ")) {
    if(11 == facename) {
      test = 0;
      value = 0;
    }
  }

  return test;
}

bool SetBoundaryConditionThrombus(const std::vector < double >& x, const char name[], double &value, const int facename, const double time) {
  bool test = 1; //dirichlet
  value = 0.;

  if(!strcmp(name, "V")) {

    if(1 == facename) {
      double r2 = (x[0] * 100.) * (x[0] * 100.) + (x[2] * 100.) * (x[2] * 100.);
      value = -0.01/.9 * (.9 - r2); //inflow
    }
    else if(2 == facename) {
      test = 0;
      value = 10.;
    }
  }
  else if(!strcmp(name, "U") || !strcmp(name, "W")) {
    if(2 == facename) {
      test = 0;
      value = 0.;
    }
  }
  else if(!strcmp(name, "P")) {
    test = 0;
    value = 0.;
  }
  else if(!strcmp(name, "DX")) {
    if( 5 == facename ) {
      test = 0;
      value = 0;
    }
  }
  else if(!strcmp(name, "DY")) {
    if( 5 == facename) {
      test = 0;
      value = 0;
    }
  }
  else if(!strcmp(name, "DZ")) {
    if( 5 == facename) {
      test = 0;
      value = 0;
    }
  }

  return test;
}

