import KratosMultiphysics
# import KratosMultiphysics.SolidMechanicsApplication
import KratosMultiphysics.StructuralMechanicsApplication
import KratosMultiphysics.FemToDemApplication
import KratosMultiphysics.FemToDemApplication.MainCouplingFemDem as MainCouplingFemDem

model = KratosMultiphysics.Model()
MainCouplingFemDem.MainCoupledFemDem_Solution(model).Run()