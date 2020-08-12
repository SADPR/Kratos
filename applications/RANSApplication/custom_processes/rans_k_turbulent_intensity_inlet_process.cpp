//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ `
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics
//
//  License:         BSD License
//                   Kratos default license: kratos/license.txt
//
//  Main authors:    Suneth Warnakulasuriya
//

// System includes
#include <cmath>

// External includes

// Project includes
#include "includes/define.h"
#include "includes/variables.h"
#include "utilities/parallel_utilities.h"

// Application includes
#include "rans_application_variables.h"

// Include base h
#include "rans_k_turbulent_intensity_inlet_process.h"

namespace Kratos
{
RansKTurbulentIntensityInletProcess::RansKTurbulentIntensityInletProcess(
    Model& rModel,
    Parameters rParameters)
: mrModel(rModel), mrParameters(rParameters)
{
    KRATOS_TRY

    Parameters default_parameters = Parameters(R"(
        {
            "model_part_name"     : "PLEASE_SPECIFY_MODEL_PART_NAME",
            "turbulent_intensity" : 0.05,
            "echo_level"          : 0,
            "is_fixed"            : true,
            "min_value"           : 1e-14
        })");

    mrParameters.ValidateAndAssignDefaults(default_parameters);

    mTurbulentIntensity = mrParameters["turbulent_intensity"].GetDouble();
    mIsConstrained = mrParameters["is_fixed"].GetBool();
    mEchoLevel = mrParameters["echo_level"].GetInt();
    mModelPartName = mrParameters["model_part_name"].GetString();
    mMinValue = mrParameters["min_value"].GetDouble();

    KRATOS_ERROR_IF(mTurbulentIntensity < 0.0)
        << "Turbulent intensity needs to be positive in the modelpart "
        << mModelPartName << "\n.";
    KRATOS_ERROR_IF(mMinValue < 0.0)
        << "Minimum turbulent kinetic energy needs to be positive in the "
           "modelpart "
        << mModelPartName << "\n.";

    KRATOS_CATCH("");
}

void RansKTurbulentIntensityInletProcess::ExecuteInitialize()
{
    if (mIsConstrained) {
        auto& r_nodes = mrModel.GetModelPart(mModelPartName).Nodes();

        BlockPartition<ModelPart::NodesContainerType>(r_nodes).for_each(
            [&](ModelPart::NodeType& rNode) {
                rNode.Fix(TURBULENT_KINETIC_ENERGY);
            });

        KRATOS_INFO_IF(this->Info(), mEchoLevel > 0)
            << "Fixed TURBULENT_KINETIC_ENERGY dofs in " << mModelPartName << ".\n";
    }
}

void RansKTurbulentIntensityInletProcess::ExecuteInitializeSolutionStep()
{
    Execute();
}

void RansKTurbulentIntensityInletProcess::Execute()
{
    KRATOS_TRY

    auto& r_nodes = mrModel.GetModelPart(mModelPartName).Nodes();

    BlockPartition<ModelPart::NodesContainerType>(r_nodes).for_each([&](ModelPart::NodeType& rNode) {
        const array_1d<double, 3>& r_velocity = rNode.FastGetSolutionStepValue(VELOCITY);
        const double velocity_magnitude = norm_2(r_velocity);

        rNode.FastGetSolutionStepValue(TURBULENT_KINETIC_ENERGY) = std::max(
            1.5 * std::pow(mTurbulentIntensity * velocity_magnitude, 2), mMinValue);
    });

    KRATOS_INFO_IF(this->Info(), mEchoLevel > 0)
        << "Applied k values to " << mModelPartName << ".\n";

    KRATOS_CATCH("");
}

int RansKTurbulentIntensityInletProcess::Check()
{
    KRATOS_TRY

    const auto& r_model_part = mrModel.GetModelPart(mModelPartName);

    KRATOS_ERROR_IF(!r_model_part.HasNodalSolutionStepVariable(TURBULENT_KINETIC_ENERGY))
        << "TURBULENT_KINETIC_ENERGY is not found in nodal solution step variables list of "
        << mModelPartName << ".";

    KRATOS_ERROR_IF(!r_model_part.HasNodalSolutionStepVariable(VELOCITY))
        << "VELOCITY is not found in nodal solution step variables list of "
        << mModelPartName << ".";

    return 0;

    KRATOS_CATCH("");
}

std::string RansKTurbulentIntensityInletProcess::Info() const
{
    return std::string("RansKTurbulentIntensityInletProcess");
}

void RansKTurbulentIntensityInletProcess::PrintInfo(std::ostream& rOStream) const
{
    rOStream << this->Info();
}

void RansKTurbulentIntensityInletProcess::PrintData(std::ostream& rOStream) const
{
}

} // namespace Kratos.
