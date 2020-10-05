from __future__ import print_function, absolute_import, division #makes KratosMultiphysics backward compatible with python 2.6 and 2.7

import KratosMultiphysics
from KratosMultiphysics import Logger
import KratosMultiphysics.FemToDemApplication as KratosFemDem
import KratosMultiphysics.FemToDemApplication.MainCouplingPfemFemDemAitken as MainCouplingPfemFemDemAitken
import KratosMultiphysics.KratosUnittest as KratosUnittest
import os
import shutil

def Wait():
    input("Press Something")

def KratosPrintInfo(message):
    KratosMultiphysics.Logger.Print(message, label="")
    KratosMultiphysics.Logger.Flush()

#============================================================================================================================
class MainCouplingPfemFemDemAitkenForTestingSolution(MainCouplingPfemFemDemAitken.MainCouplingPfemFemDemAitken_Solution):
#============================================================================================================================

#============================================================================================================================
    def FinalizeSolutionStep(self):

        self.PFEM_Solution.FinalizeSolutionStep()
        self.PFEM_Solution.OutputSolutionStep()
        self.FEMDEM_Solution.FinalizeSolutionStep()
        KM.PfemFluidDynamicsApplication.PostProcessUtilities().RebuildPostProcessModelPart(self.PFEM_Solution.post_process_model_part, self.PFEM_Solution.main_model_part)
        # self.PrintResults()

        self.CheckControlValuesForTesting()

#============================================================================================================================
    def CheckControlValuesForTesting(self):  # KratosPrintInfo(str(dy))
        
        for node in self.FEMDEM_Solution.FEM_Solution.main_model_part.GetSubModelPart("testing_nodes").Nodes:
            KratosPrintInfo("hey")


#============================================================================================================================
    def Finalize(self):
        super(MainCouplingFemDemForTestingSolution, self).Finalize()

        # shutil.rmtree(self.FEM_Solution.problem_name + "_Graphs")
        # shutil.rmtree(self.FEM_Solution.problem_name + "_MPI_results")
        # shutil.rmtree(self.FEM_Solution.problem_name + "_Post_Files")
        # shutil.rmtree(self.FEM_Solution.problem_name + "_Results_and_Data")
        # # shutil.rmtree("__pycache__")
        # os.remove("PlotFile.txt")
        # os.remove(self.FEM_Solution.problem_name + "_0.post.bin")
        # os.remove(self.FEM_Solution.problem_name + ".post.lst")
        # os.remove("tests.post.lst")


class TestAnalytics(KratosUnittest.TestCase):
    
    def setUp(self):
        pass

    @classmethod
    def 2d_fsi(self):
        model = KratosMultiphysics.Model()
        MainCouplingPfemFemDemAitkenForTestingSolution(model, "fsi_tests/wall_2d/", ).Run()



if __name__ == "__main__":
    KratosMultiphysics.Logger.GetDefaultOutput().SetSeverity(Logger.Severity.WARNING)
    KratosUnittest.main()