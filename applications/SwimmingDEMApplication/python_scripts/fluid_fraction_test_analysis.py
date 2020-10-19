from KratosMultiphysics import Model, Parameters, Logger

import os
import sys

file_path = os.path.abspath(__file__)
dir_path = os.path.dirname(file_path)
sys.path.insert(0, dir_path)

from swimming_DEM_analysis import SwimmingDEMAnalysis

import swimming_DEM_procedures as SDP

class FluidFractionTestAnalysis(SwimmingDEMAnalysis):
    def __init__(self, model, iteration, varying_parameters = Parameters("{}")):
        """
        The default constructor of the class

        Keyword arguments:
        self -- It signifies an instance of a class.
        iteration -- The number of the fluid_model_part that is running
        model -- the container of the fluid model part.
        varying_parameters -- The whole project_parameters.
        """
        from KratosMultiphysics.SwimmingDEMApplication import hdf5_script
        self.projector_post_process = hdf5_script.ErrorProjectionPostProcessTool(iteration)
        super(FluidFractionTestAnalysis, self).__init__(model, varying_parameters)
        self.project_parameters = varying_parameters

    def Initialize(self):
        super(FluidFractionTestAnalysis, self).Initialize()
        self._GetSolver().ConstructL2ErrorProjector()

    def GetDebugInfo(self):
        return SDP.Counter(is_dead = 1)

    def _CreateSolver(self):
        import fluid_fraction_test_solver as sdem_solver
        return sdem_solver.FluidFractionTestSolver(self.model,
                                                self.project_parameters,
                                                self.GetFieldUtility(),
                                                self._GetFluidAnalysis()._GetSolver(),
                                                self._GetDEMAnalysis()._GetSolver(),
                                                self.vars_man)

    def FinalizeSolutionStep(self):
        # printing if required
        if self._GetSolver().CannotIgnoreFluidNow():
            self._GetFluidAnalysis().FinalizeSolutionStep()

        self._GetDEMAnalysis().FinalizeSolutionStep()

        # coupling checks (debugging)
        if self.debug_info_counter.Tick():
            self.dem_volume_tool.UpdateDataAndPrint(
                self.project_parameters["fluid_domain_volume"].GetDouble())

        super(FluidFractionTestAnalysis, self).FinalizeSolutionStep()
        self.velocity_error_projected, self.pressure_error_projected, self.error_model_part = self._GetSolver().ProjectL2Error()
        self.projector_post_process.WriteData(self.error_model_part, self.velocity_error_projected, self.pressure_error_projected)

    def TransferBodyForceFromDisperseToFluid(self):
        pass

if __name__ == "__main__":
    # Setting parameters

    with open('ProjectParameters.json','r') as parameter_file:
        parameters = Parameters(parameter_file.read())

    # Create Model
    model = Model()

    # To avoid too many prints
    Logger.GetDefaultOutput().SetSeverity(Logger.Severity.WARNING)

    test = FluidFractionTestAnalysis(model, parameters)
    test.Run()