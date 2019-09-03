from __future__ import print_function, absolute_import, division  # makes KratosMultiphysics backward compatible with python 2.6 and 2.7

# Importing the Kratos Library
import KratosMultiphysics

# Import applications
import KratosMultiphysics.StructuralMechanicsApplication as StructuralMechanicsApplication

import KratosMultiphysics.kratos_utilities as kratos_utilities
if kratos_utilities.CheckIfApplicationsAvailable("MeshingApplication"):
    import KratosMultiphysics.MeshingApplication as MeshingApplication
    missing_meshing_dependencies = True
else:
    missing_meshing_dependencies = False

# Import base class file
from KratosMultiphysics.StructuralMechanicsApplication import structural_mechanics_implicit_dynamic_solver

# Import auxiliar methods
from KratosMultiphysics.StructuralMechanicsApplication import auxiliar_methods_adaptative_solvers
from KratosMultiphysics.StructuralMechanicsApplication import adaptative_remeshing_structural_mechanics_utilities

def CreateSolver(model, custom_settings):
    return AdaptativeRemeshingImplicitMechanicalSolver(model, custom_settings)

class AdaptativeRemeshingImplicitMechanicalSolver(structural_mechanics_implicit_dynamic_solver.ImplicitMechanicalSolver):
    """The structural mechanics implicit dynamic solver. (Fot adaptative remeshing)
    See structural_mechanics_implicit_dynamic_solver.py for more information.
    """
    def __init__(self, model, custom_settings):
        # Set defaults and validate custom settings.
        self.adaptative_remeshing_utilities = adaptative_remeshing_structural_mechanics_utilities.AdaptativeRemeshingMechanicalUtilities()
        adaptative_remesh_parameters = self.adaptative_remeshing_utilities.GetDefaultParameters()

        # Validate the remaining settings in the base class.
        self.validate_and_transfer_matching_settings(custom_settings, adaptative_remesh_parameters)
        self.adaptative_remesh_parameters = adaptative_remesh_parameters
        self.adaptative_remeshing_utilities.SetDefaultParameters(self.adaptative_remesh_parameters)

        # Construct the base solver.
        super(AdaptativeRemeshingImplicitMechanicalSolver, self).__init__(model, custom_settings)
        KratosMultiphysics.Logger.PrintInfo("::[AdaptativeRemeshingImplicitMechanicalSolver]:: ", "Construction finished")

    #### Private functions ####

    def AddVariables(self):
        super(AdaptativeRemeshingImplicitMechanicalSolver, self).AddVariables()
        if not missing_meshing_dependencies:
            self.main_model_part.AddNodalSolutionStepVariable(KratosMultiphysics.NODAL_H)
        KratosMultiphysics.Logger.PrintInfo("::[AdaptativeRemeshingImplicitMechanicalSolver]:: ", "Variables ADDED")

    def get_remeshing_process(self):
        if not hasattr(self, '_remeshing_process'):
            self._remeshing_process = self._create_remeshing_process()
        return self._remeshing_process

    def _create_remeshing_process(self):
        return auxiliar_methods_adaptative_solvers.CreateRemeshingProcess(self.main_model_part, self.adaptative_remesh_parameters)

    def get_metric_process(self):
        if not hasattr(self, '_metric_process'):
            self._metric_process = self._create_metric_process()
        return self._metric_process

    def _create_metric_process(self):
        return auxiliar_methods_adaptative_solvers.CreateMetricProcess(self.main_model_part, self.adaptative_remesh_parameters)

    def _create_convergence_criterion(self):
        error_criteria = self.settings["convergence_criterion"].GetString()
        conv_settings = self._get_convergence_criterion_settings()
        return self.adaptative_remeshing_utilities.GetConvergenceCriteria(error_criteria, conv_settings)
