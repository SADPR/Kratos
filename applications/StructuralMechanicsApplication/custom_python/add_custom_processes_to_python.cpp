// KRATOS  ___|  |                   |                   |
//       \___ \  __|  __| |   |  __| __| |   |  __| _` | |
//             | |   |    |   | (    |   |   | |   (   | |
//       _____/ \__|_|   \__,_|\___|\__|\__,_|_|  \__,_|_| MECHANICS
//
//  License:		 BSD License
//					 license: structural_mechanics_application/license.txt
//
//  Main authors:    Vicente Mataix Ferrandiz
//

// System includes

// External includes


// Project includes
#include "includes/model_part.h"
#include "custom_python/add_custom_processes_to_python.h"
#include "structural_mechanics_application_variables.h"

//Processes
#include "custom_processes/prism_neighbours_process.h"
#include "custom_processes/apply_multi_point_constraints_process.h"
#include "custom_processes/postprocess_eigenvalues_process.h"
#include "custom_processes/total_structural_mass_process.h"
#include "custom_processes/shell_to_solid_shell_process.h"
#include "custom_processes/skin_detection_process.h"

namespace Kratos
{
namespace Python
{

void  AddCustomProcessesToPython(pybind11::module& m)
{
    using namespace pybind11;

    /// Processes
    class_<ApplyMultipointConstraintsProcess, Process>(m,"ApplyMultipointConstraintsProcess")
        .def(init<ModelPart&>())
        .def(init< ModelPart&, Parameters& >())
	.def("AddMasterSlaveRelation", &ApplyMultipointConstraintsProcess::AddMasterSlaveRelationWithNodesAndVariableComponents)
        .def("AddMasterSlaveRelation", &ApplyMultipointConstraintsProcess::AddMasterSlaveRelationWithNodeIdsAndVariableComponents)
	.def("AddMasterSlaveRelation", &ApplyMultipointConstraintsProcess::AddMasterSlaveRelationWithNodesAndVariable)
        .def("AddMasterSlaveRelation", &ApplyMultipointConstraintsProcess::AddMasterSlaveRelationWithNodeIdsAndVariable)
        .def("SetActive", &ApplyMultipointConstraintsProcess::SetActive)      
        .def("PrintData", &ApplyMultipointConstraintsProcess::PrintData);

    class_<PostprocessEigenvaluesProcess, Process>(m,"PostprocessEigenvaluesProcess")
        .def(init<ModelPart&, Parameters>());
    
    class_<TotalStructuralMassProcess, Process >(m,"TotalStructuralMassProcess")
        .def(init<ModelPart&>())
        ;
    

    class_<PrismNeighboursProcess, Process>(m, "PrismNeighboursProcess")
        .def(init<ModelPart&>())
        .def(init<ModelPart&, const bool >())
        ;

    class_<ShellToSolidShellProcess<3>, Process>(m, "TriangleShellToSolidShellProcess")
        .def(init<ModelPart&>())
        .def(init< ModelPart&, Parameters >())
        ;

    class_<ShellToSolidShellProcess<4>, Process>(m, "QuadrilateralShellToSolidShellProcess")
        .def(init<ModelPart&>())
        .def(init< ModelPart&, Parameters >())
        ;

    class_<SkinDetectionProcess<2>, Process>(m, "SkinDetectionProcess2D")
        .def(init<ModelPart&>())
        .def(init< ModelPart&, Parameters >())
        ;

    class_<SkinDetectionProcess<3>, Process>(m, "SkinDetectionProcess3D")
        .def(init<ModelPart&>())
        .def(init< ModelPart&, Parameters >())
        ;
}

}  // namespace Python.  

} // Namespace Kratos

