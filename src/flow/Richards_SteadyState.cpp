/*
This is the flow component of the Amanzi code. 

Copyright 2010-2012 held jointly by LANS/LANL, LBNL, and PNNL. 
Amanzi is released under the three-clause BSD License. 
The terms of use and "as is" disclaimer for this license are 
provided in the top-level COPYRIGHT file.

Author:  Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#include <algorithm>
#include <vector>

#include "Richards_PK.hpp"

namespace Amanzi {
namespace AmanziFlow {

/* ******************************************************************
*  Wrapper for advance to steady-state routines.                                                    
****************************************************************** */
int Richards_PK::AdvanceToSteadyState()
{
  T_physics = ti_specs_sss_.T0;
  dT = ti_specs_sss_.dT0;

  int ierr = 0;
  if (ti_method_sss == FLOW_TIME_INTEGRATION_PICARD) {
    ierr = AdvanceToSteadyState_Picard(ti_specs_sss_);
  } else if (ti_method_sss == FLOW_TIME_INTEGRATION_BACKWARD_EULER) {
    ierr = AdvanceToSteadyState_BackwardEuler(ti_specs_sss_);
  } else if (ti_method_sss == FLOW_TIME_INTEGRATION_BDF1) {
    ierr = AdvanceToSteadyState_BDF1(ti_specs_sss_);
  } else if (ti_method_sss == FLOW_TIME_INTEGRATION_BDF2) {
    ierr = AdvanceToSteadyState_BDF2(ti_specs_sss_);
  }

  Epetra_Vector& ws = FS->ref_water_saturation();
  Epetra_Vector& ws_prev = FS->ref_prev_water_saturation();
  DeriveSaturationFromPressure(*solution_cells, ws);
  ws_prev = ws;

  return ierr;
}


/* ******************************************************************* 
* Performs one time step of size dT using first-order time integrator.
******************************************************************* */
int Richards_PK::AdvanceToSteadyState_BDF1(TI_Specs& ti_specs)
{
  bool last_step = false;

  int max_itrs = ti_specs.max_itrs;
  double T0 = ti_specs.T0;
  double T1 = ti_specs.T1;
  double dT0 = ti_specs.dT0;

  int itrs = 0;
  while (itrs < max_itrs && T_physics < T1) {
    if (itrs == 0) {  // initialization of BDF1
      Epetra_Vector udot(*super_map_);
      ComputeUDot(T0, *solution, udot);
      bdf1_dae->set_initial_state(T0, *solution, udot);

      int ierr;
      update_precon(T0, *solution, dT0, ierr);
    }

    double dTnext;
    bdf1_dae->bdf1_step(dT, *solution, dTnext);
    bdf1_dae->commit_solution(dT, *solution);
    bdf1_dae->write_bdf1_stepping_statistics();

    T_physics = bdf1_dae->most_recent_time();
    dT = dTnext;
    itrs++;

    double Tdiff = T1 - T_physics;
    if (dTnext > Tdiff) {
      dT = Tdiff * 0.99999991;  // To avoid hitting the wrong BC
      last_step = true;
    }
    if (last_step && dT < 1e-3) break;
  }

  ti_specs.num_itrs = itrs;
  return 0;
}


/* ******************************************************************* 
* Performs one time step of size dT using second-order time integrator.
******************************************************************* */
int Richards_PK::AdvanceToSteadyState_BDF2(TI_Specs& ti_specs)
{
  bool last_step = false;

  int max_itrs = ti_specs.max_itrs;
  double T0 = ti_specs.T0;
  double T1 = ti_specs.T1;
  double dT0 = ti_specs.dT0;

  int itrs = 0;
  while (itrs < max_itrs && T_physics < T1) {
    if (itrs == 0) {  // initialization of BDF2
      Epetra_Vector udot(*super_map_);
      ComputeUDot(T0, *solution, udot);
      bdf2_dae->set_initial_state(T0, *solution, udot);

      int ierr;
      update_precon(T0, *solution, dT0, ierr);
    }

    double dTnext;
    bdf2_dae->bdf2_step(dT, 0.0, *solution, dTnext);
    bdf2_dae->commit_solution(dT, *solution);
    bdf2_dae->write_bdf2_stepping_statistics();

    T_physics = bdf2_dae->most_recent_time();
    dT = dTnext;
    itrs++;

    double Tdiff = T1 - T_physics;
    if (dTnext > Tdiff) {
      dT = Tdiff * 0.99999991;  // To avoid hitting the wrong BC
      last_step = true;
    }
    if (last_step && dT < 1e-3) break;
  }

  ti_specs.num_itrs = itrs;
  return 0;
}


/* ******************************************************************
* Calculates steady-state solution assuming that absolute and
* relative permeabilities do not depend explicitly on time.
* This is the experimental method.                                                 
****************************************************************** */
int Richards_PK::AdvanceToSteadyState_Picard(TI_Specs& ti_specs)
{
  Epetra_Vector  solution_old(*solution);
  Epetra_Vector& solution_new = *solution;
  Epetra_Vector  residual(*solution);

  Epetra_Vector* solution_old_cells = FS->CreateCellView(solution_old);
  Epetra_Vector* solution_new_cells = FS->CreateCellView(solution_new);

  if (!is_matrix_symmetric) solver->SetAztecOption(AZ_solver, AZ_gmres);
  solver->SetAztecOption(AZ_output, AZ_none);
  solver->SetAztecOption(AZ_conv, AZ_rhs);

  // update steady state boundary conditions
  double time = T_physics;
  bc_pressure->Compute(time);
  bc_flux->Compute(time);
  bc_head->Compute(time);

  int max_itrs = ti_specs.max_itrs;
  double residual_tol = ti_specs.residual_tol;

  int itrs = 0;
  double L2norm, L2error = 1.0;

  while (L2error > residual_tol && itrs < max_itrs) {
    // update dynamic boundary conditions
    bc_seepage->Compute(time);
    ProcessBoundaryConditions(
        bc_pressure, bc_head, bc_flux, bc_seepage,
        *solution_faces, atm_pressure,
        bc_markers, bc_values);

    if (!is_matrix_symmetric) {
      CalculateRelativePermeabilityFace(*solution_cells);
      Krel_cells->PutScalar(1.0);
    } else {
      CalculateRelativePermeabilityCell(*solution_cells);
      Krel_faces->PutScalar(1.0);
    }

    // create algebraic problem
    matrix_->CreateMFDstiffnessMatrices(*Krel_cells, *Krel_faces);
    matrix_->CreateMFDrhsVectors();
    AddGravityFluxes_MFD(K, *Krel_cells, *Krel_faces, matrix_);
    matrix_->ApplyBoundaryConditions(bc_markers, bc_values);
    matrix_->AssembleGlobalMatrices();
    rhs = matrix_->rhs();  // export RHS from the matrix class

    // create preconditioner
    preconditioner_->CreateMFDstiffnessMatrices(*Krel_cells, *Krel_faces);
    preconditioner_->CreateMFDrhsVectors();
    AddGravityFluxes_MFD(K, *Krel_cells, *Krel_faces, preconditioner_);
    preconditioner_->ApplyBoundaryConditions(bc_markers, bc_values);
    preconditioner_->AssembleGlobalMatrices();
    preconditioner_->ComputeSchurComplement(bc_markers, bc_values);
    preconditioner_->UpdatePreconditioner();

    // check convergence of non-linear residual
    L2error = matrix_->ComputeResidual(solution_new, residual);
    residual.Norm2(&L2error);
    rhs->Norm2(&L2norm);
    L2error /= L2norm;

    // call AztecOO solver
    Epetra_Vector b(*rhs);
    solver->SetRHS(&b);  // AztecOO modifies the right-hand-side.
    solver->SetLHS(&*solution);  // initial solution guess

    solver->Iterate(max_itrs, convergence_tol);
    int num_itrs = solver->NumIters();
    double linear_residual = solver->TrueResidual() / L2norm;

    // update relaxation
    double relaxation;
    relaxation = CalculateRelaxationFactor(*solution_old_cells, *solution_new_cells);

    if (MyPID == 0 && verbosity >= FLOW_VERBOSITY_HIGH) {
      std::printf("Picard:%4d  ||r||=%9.4e relax=%8.3e  solver(%9.4e,%4d)\n",
          itrs, L2error, relaxation, linear_residual, num_itrs);
    }

    int ndof = ncells_owned + nfaces_owned;
    for (int c = 0; c < ndof; c++) {
      solution_new[c] = (1.0 - relaxation) * solution_old[c] + relaxation * solution_new[c];
      solution_old[c] = solution_new[c];
    }

    T_physics += dT;
    itrs++;
  }

  ti_specs.num_itrs = itrs;
  return 0;
}


/* ******************************************************************
* Calculate relazation factor.                                                       
****************************************************************** */
double Richards_PK::CalculateRelaxationFactor(const Epetra_Vector& uold, const Epetra_Vector& unew)
{ 
  Epetra_Vector dSdP(mesh_->cell_map(false));
  DerivedSdP(uold, dSdP);

  double relaxation = 1.0;
  for (int c = 0; c < ncells_owned; c++) {
    double diff = dSdP[c] * fabs(unew[c] - uold[c]);
    if (diff > 3e-2) relaxation = std::min(relaxation, 3e-2 / diff);
  }

  for (int c = 0; c < ncells_owned; c++) {
    double diff = fabs(unew[c] - uold[c]);
    double umax = std::max(fabs(unew[c]), fabs(uold[c]));
    if (diff > 1e-2 * umax) relaxation = std::min(relaxation, 1e-2 * umax / diff);
  }

#ifdef HAVE_MPI
  double relaxation_tmp = relaxation;
  mesh_->get_comm()->MinAll(&relaxation_tmp, &relaxation, 1);  // find the global minimum
#endif

  return relaxation;
}

}  // namespace AmanziFlow
}  // namespace Amanzi

