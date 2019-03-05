//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ \.
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics FemDem Application
//
//  License:             BSD License
//                               Kratos default license:
//kratos/license.txt
//
//  Main authors:    Alejandro Cornejo Velazquez
//

#include "custom_processes/expand_wet_nodes_process.h"

namespace Kratos {

ExpandWetNodesProcess::ExpandWetNodesProcess(
    ModelPart& r_model_part)
    : mrModelPart(r_model_part)
{
}

/***********************************************************************************/
/***********************************************************************************/

void ExpandWetNodesProcess::Execute() 
{
    int extrapolated_elements = 1;
    int pressure_id;
    while (extrapolated_elements > 0) {
        extrapolated_elements = 0;
        for (auto it_elem = mrModelPart.Elements().ptr_begin(); it_elem != mrModelPart.Elements().ptr_end(); ++it_elem) {
            
            bool element_done = (*it_elem)->GetValue(PRESSURE_EXPANDED);
            bool condition_is_active = true;
            if ((*it_elem)->IsDefined(ACTIVE)) {
                condition_is_active = (*it_elem)->Is(ACTIVE);
            }
            if (this->ElementHasWetNodes(it_elem, pressure_id) && 
                                  condition_is_active == false && 
                                         element_done == false) {

                this->ExpandWetNodes(it_elem, pressure_id);
                extrapolated_elements++;
                (*it_elem)->SetValue(PRESSURE_EXPANDED, true);
            }
        }
    }
}

/***********************************************************************************/
/***********************************************************************************/

bool ExpandWetNodesProcess::ElementHasWetNodes(
    ModelPart::ElementsContainerType::ptr_iterator itElem,
    int& rPressureId
    )
{
    auto& r_geometry = (*itElem)->GetGeometry();
    for (IndexType i = 0; i < r_geometry.PointsNumber(); ++i) {
        const int pressure_id = r_geometry[i].GetValue(PRESSURE_ID);
        if (pressure_id != 0) {
            rPressureId = pressure_id;
            return true;
        }
    }
	return false;
}

/***********************************************************************************/
/***********************************************************************************/

void ExpandWetNodesProcess::ExpandWetNodes(
    ModelPart::ElementsContainerType::ptr_iterator itElem,
    const int PressureId
    )
{
    auto& r_geometry = (*itElem)->GetGeometry();
    for (IndexType i = 0; i < r_geometry.PointsNumber(); ++i) {
        r_geometry[i].SetValue(PRESSURE_ID, PressureId);
    }

    // Indicator to recosntructu the Pressure afterwards
    auto& r_process_info = mrModelPart.GetProcessInfo();
    r_process_info[RECONSTRUCT_PRESSURE_LOAD] = 1;
}

/***********************************************************************************/
/***********************************************************************************/

}