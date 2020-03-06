//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ \.
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics FemDem Application
//
//  License:		 BSD License
//					 Kratos default license: kratos/license.txt
//
//  Main authors:    Alejandro Cornejo
//                   Ruben Zorrilla
//

#if !defined(KRATOS_AITKEN_RELAXATION_UTILITY)
#define  KRATOS_AITKEN_RELAXATION_UTILITY

// System includes

// External includes
#include "utilities/math_utils.h"
#include "spaces/ublas_space.h"
#include "fem_to_dem_application_variables.h"

namespace Kratos
{
///@name Kratos Globals
///@{
///@}

///@name Type Definitions
///@{
///@}

///@name  Enum's
///@{
///@}

///@name  Functions
///@{
///@}

///@name Kratos Classes
///@{

/** @brief Aitken relaxation technique for FSI PFEM-FEM-DEM coupling
 */
class AitkenRelaxationUtility
{
public:

    ///@name Type Definitions
    ///@{
    KRATOS_CLASS_POINTER_DEFINITION(AitkenRelaxationUtility);

    typedef UblasSpace<double, Matrix, Vector> TSpace;
    typedef typename TSpace::VectorType VectorType;
    typedef typename TSpace::VectorPointerType VectorPointerType;

    ///@}
    ///@name Life Cycle
    ///@{

   /**
     * Constructor.
     * AitkenRelaxationUtility
     */
    AitkenRelaxationUtility(const double OmegaOld = 0.825)
    {
        mOmegaOld = OmegaOld;
    }

    /**
     * Copy Constructor.
     */
    AitkenRelaxationUtility(const AitkenRelaxationUtility& rOther)
    {
        mOmegaOld = rOther.mOmegaOld;
    }

    /**
     * Destructor.
     */
    virtual ~AitkenRelaxationUtility() {}


    ///@name Operators
    ///@{
    ///@}

    ///@name Operations
    ///@{

    /**
     * Initialize the internal iteration counter
     */
    void InitializeSolutionStep()
    {
        KRATOS_TRY
        mConvergenceAcceleratorIteration = 1;
        KRATOS_CATCH( "" )
    }

    /**
     * Performs the solution update
     * The correction is done as u_i+1 = u_i + w_i+1*r_i+1 where w_i+1 is de relaxation parameter computed using the Aitken's formula.
     * @param rResidualVector: Residual vector from the residual evaluation
     * @param rIterationGuess: Current iteration guess
     */
    void UpdateSolution(
        const Vector& rResidualVector,
        Vector& rIterationGuess
        )
    {
        KRATOS_TRY
        VectorPointerType pAux(new VectorType(rResidualVector));
        std::swap(mpResidualVectorNew, pAux);

        if (mConvergenceAcceleratorIteration == 1) {
            TSpace::UnaliasedAdd(rIterationGuess, mOmegaOld, *mpResidualVectorNew);
        } else {
            VectorType Aux1minus0(*mpResidualVectorNew);                  // Auxiliar copy of mpResidualVectorNew
            TSpace::UnaliasedAdd(Aux1minus0, -1.0, *mpResidualVectorOld); // mpResidualVectorNew - mpResidualVectorOld

            const double denominator = TSpace::Dot(Aux1minus0, Aux1minus0);
            const double numerator   = TSpace::Dot(*mpResidualVectorOld, Aux1minus0);

            mOmegaNew = -mOmegaOld * (numerator / denominator);

            TSpace::UnaliasedAdd(rIterationGuess, mOmegaNew, *mpResidualVectorNew);
            mOmegaOld = mOmegaNew;
        }
        KRATOS_CATCH("")
    }

    /**
     * Updates the Aitken iteration values for the next non-linear iteration
     */
    void FinalizeNonLinearIteration()
    {
        KRATOS_TRY
        std::swap(mpResidualVectorOld, mpResidualVectorNew);
        mConvergenceAcceleratorIteration += 1;
        KRATOS_CATCH("")
    }

    /**
     * Reset the convergence accelerator iterations counter
     */
    void FinalizeSolutionStep()
    {
        KRATOS_TRY
        mConvergenceAcceleratorIteration = 1;
        mVectorSize = 0;
        KRATOS_CATCH("")
    }

    /**
     * Computes the norm of a vector -> Used for computing the norm of the
     */
    double ComputeNorm(const Vector& rVector)
    {
        return MathUtils<double>::Norm(rVector);
    }

    void InitializeInterfaceSubModelPart(ModelPart &rSolidModelPart)
    {
        mVectorSize = 0;
        if (rSolidModelPart.HasSubModelPart("fsi_interface_model_part")) {
            auto &r_interface_sub_model = rSolidModelPart.GetSubModelPart("fsi_interface_model_part");
            r_interface_sub_model.Nodes().clear();
        } else {
            auto &r_interface_sub_model = rSolidModelPart.CreateSubModelPart("fsi_interface_model_part");
        }

        auto &r_interface_sub_model  = rSolidModelPart.GetSubModelPart("fsi_interface_model_part");
        auto &r_solid_skin_sub_model = rSolidModelPart.GetSubModelPart("SkinDEMModelPart");

        const auto it_node_begin = r_solid_skin_sub_model.NodesBegin();
        // #pragma omp parallel for
        for (int i = 0; i < static_cast<int>(r_solid_skin_sub_model.Nodes().size()); i++) {
            auto it_node = it_node_begin + i;
            if (it_node->FastGetSolutionStepValue(PRESSURE) != 0.0) {
                r_interface_sub_model.AddNode(*(it_node.base()));
                mVectorSize++;
            }
        }
        mVectorSize *= 3;
    }

    void ResetNodalValues(ModelPart &rSolidModelPart)
    {
        auto &r_interface_sub_model = rSolidModelPart.GetSubModelPart("fsi_interface_model_part");
        const Vector& r_zero_vector = ZeroVector(3);

        const auto it_node_begin = r_interface_sub_model.NodesBegin();
        #pragma omp parallel for
        for (int i = 0; i < static_cast<int>(r_interface_sub_model.Nodes().size()); i++) {
            auto it_node = it_node_begin + i;
            auto &r_var_1 = it_node->FastGetSolutionStepValue(RELAXED_VELOCITY);
            auto &r_var_2 = it_node->FastGetSolutionStepValue(OLD_RELAXED_VELOCITY);
            auto &r_var_3 = it_node->FastGetSolutionStepValue(FSI_INTERFACE_RESIDUAL);
            r_var_1 = r_zero_vector;
            r_var_2 = r_zero_vector;
            r_var_3 = r_zero_vector;
        }
    }

    void SavePreviousRelaxedValues(ModelPart &rSolidModelPart)
    {
        auto &r_interface_sub_model = rSolidModelPart.GetSubModelPart("fsi_interface_model_part");

        const auto it_node_begin = r_interface_sub_model.NodesBegin();

        #pragma omp parallel for
        for (int i = 0; i < static_cast<int>(r_interface_sub_model.Nodes().size()); i++) {
            auto it_node = it_node_begin + i;
            const auto &r_relaxed_velocity = it_node->FastGetSolutionStepValue(RELAXED_VELOCITY);
            auto &r_old_relaxed_velocity   = it_node->GetSolutionStepValue(OLD_RELAXED_VELOCITY);
            noalias(r_old_relaxed_velocity) = r_relaxed_velocity;
        }
    }



    ///@}

    ///@name Access
    ///@{
    ///@}

    ///@name Inquiry
    ///@{
    ///@}

    ///@name Input and output
    ///@{
    ///@}

    ///@name Friends
    ///@{
    ///@}

protected:

    ///@name Protected static Member Variables
    ///@{
    ///@}

    ///@name Protected member Variables
    ///@{

    unsigned int mConvergenceAcceleratorIteration;

    double mOmegaOld;
    double mOmegaNew;

    VectorPointerType mpResidualVectorOld;
    VectorPointerType mpResidualVectorNew;

    unsigned int mVectorSize;

    ///@}

    ///@name Protected Operators
    ///@{
    ///@}

    ///@name Protected Operations
    ///@{
    ///@}

    ///@name Protected  Access
    ///@{
    ///@}

    ///@name Protected Inquiry
    ///@{
    ///@}

    ///@name Protected LifeCycle
    ///@{
    ///@}

private:

    ///@name Static Member Variables
    ///@{
    ///@}

    ///@name Member Variables
    ///@{
    ///@}

    ///@name Private Operators
    ///@{
    ///@}

    ///@name Private Operations
    ///@{
    ///@}

    ///@name Private  Access
    ///@{
    ///@}

    ///@name Serialization
    ///@{
    ///@}

    ///@name Private Inquiry
    ///@{
    ///@}

    ///@name Un accessible methods
    ///@{
    ///@}

}; /* Class AitkenConvergenceAccelerator */

///@}

///@name Type Definitions
///@{
///@}

///@name Input and output
///@{
///@}

} // namespace Kratos

#endif /* KRATOS_AITKEN_RELAXATION_UTILITY defined */