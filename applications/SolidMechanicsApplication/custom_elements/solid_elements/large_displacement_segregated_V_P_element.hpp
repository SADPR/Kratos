//
//   Project Name:        KratosSolidMechanicsApplication $
//   Created by:          $Author:            JMCarbonell $
//   Last modified by:    $Co-Author:                     $
//   Date:                $Date:               April 2018 $
//   Revision:            $Revision:                  0.0 $
//
//

#if !defined(KRATOS_LARGE_DISPLACEMENT_SEGREGATED_V_P_ELEMENT_H_INCLUDED)
#define  KRATOS_LARGE_DISPLACEMENT_SEGREGATED_V_P_ELEMENT_H_INCLUDED

// System includes

// External includes

// Project includes
#include "custom_elements/solid_elements/large_displacement_V_element.hpp"


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

/// Large Displacement Lagrangian V Element for 3D and 2D geometries

/**
 * Implements a Large Displacement Lagrangian definition for structural analysis.
 * This works for linear Triangles and Tetrahedra (base class)
 */

class KRATOS_API(SOLID_MECHANICS_APPLICATION) LargeDisplacementSegregatedVPElement
    : public LargeDisplacementVElement
{
public:

    ///@name Type Definitions
    ///@{
    ///Reference type definition for constitutive laws
    typedef ConstitutiveLaw ConstitutiveLawType;
    ///Pointer type for constitutive laws
    typedef ConstitutiveLawType::Pointer ConstitutiveLawPointerType;
    ///StressMeasure from constitutive laws
    typedef ConstitutiveLawType::StressMeasure StressMeasureType;
    ///Type definition for integration methods
    typedef GeometryData::IntegrationMethod IntegrationMethod;

    /// Counted pointer of LargeDisplacementSegregatedVPElement
    KRATOS_CLASS_POINTER_DEFINITION( LargeDisplacementSegregatedVPElement );
    ///@}

    enum StepType {VELOCITY_STEP = 1, PRESSURE_STEP = 2}
    
    ///@name Life Cycle
    ///@{

    /// Empty constructor needed for serialization
    LargeDisplacementSegregatedVPElement();

    /// Default constructors
    LargeDisplacementSegregatedVPElement(IndexType NewId, GeometryType::Pointer pGeometry);

    LargeDisplacementSegregatedVPElement(IndexType NewId, GeometryType::Pointer pGeometry, PropertiesType::Pointer pProperties);

    ///Copy constructor
    LargeDisplacementSegregatedVPElement(LargeDisplacementSegregatedVPElement const& rOther);


    /// Destructor.
    virtual ~LargeDisplacementSegregatedVPElement();

    ///@}
    ///@name Operators
    ///@{

    /// Assignment operator.
    LargeDisplacementSegregatedVPElement& operator=(LargeDisplacementSegregatedVPElement const& rOther);

    ///@}
    ///@name Operations
    ///@{
    /**
     * Returns the currently selected integration method
     * @return current integration method selected
     */
    /**
     * creates a new total lagrangian updated element pointer
     * @param NewId: the ID of the new element
     * @param ThisNodes: the nodes of the new element
     * @param pProperties: the properties assigned to the new element
     * @return a Pointer to the new element
     */
    Element::Pointer Create(IndexType NewId, NodesArrayType const& ThisNodes, PropertiesType::Pointer pProperties) const override;

    /**
     * clones the selected element variables, creating a new one
     * @param NewId: the ID of the new element
     * @param ThisNodes: the nodes of the new element
     * @param pProperties: the properties assigned to the new element
     * @return a Pointer to the new element
     */
    Element::Pointer Clone(IndexType NewId, NodesArrayType const& ThisNodes) const override;

    //************* GETTING METHODS

    /**
    * Sets on rElementalDofList the degrees of freedom of the considered element geometry
    */
    void GetDofList(DofsVectorType& rElementalDofList, ProcessInfo& rCurrentProcessInfo) override;

    /**
     * Sets on rResult the ID's of the element degrees of freedom
     */
    void EquationIdVector(EquationIdVectorType& rResult, ProcessInfo& rCurrentProcessInfo) override;

    /**
     * Sets on rValues the nodal displacements
     */
    void GetValuesVector(Vector& rValues, int Step = 0) override;

    /**
     * Sets on rValues the nodal velocities
     */
    void GetFirstDerivativesVector(Vector& rValues, int Step = 0) override;

    /**
     * Sets on rValues the nodal accelerations
     */
    void GetSecondDerivativesVector(Vector& rValues, int Step = 0) override;

 
    //************************************************************************************
    //************************************************************************************
    /**
     * This function provides the place to perform checks on the completeness of the input.
     * It is designed to be called only once (or anyway, not often) typically at the beginning
     * of the calculations, so to verify that nothing is missing from the input
     * or that no common error is found.
     * @param rCurrentProcessInfo
     */
    int Check(const ProcessInfo& rCurrentProcessInfo) override;

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
    ///@}
    ///@name Protected Operators
    ///@{
    ///@}
    ///@name Protected Operations
    ///@{


    /**
     * Calculation and addition of the matrices of the LHS
     */

    void CalculateAndAddLHS(LocalSystemComponents& rLocalSystem,
                            ElementVariables& rVariables,
                            double& rIntegrationWeight) override;
    
    /**
     * Get element size from the dofs
     */    
    unsigned int GetDofsSize() override;

    /**
     * Set Variables of the Element to the Parameters of the Constitutive Law
     */
    void SetElementVariables(ElementVariables& rVariables,
                             ConstitutiveLaw::Parameters& rValues,
                             const int & rPointNumber) override;
    
    /**
     * Calculation of the velocity gradient
     */
    void CalculateVelocityGradient(Matrix& rH,
                                   const Matrix& rDN_DX,
                                   unsigned int step = 0);

    /**
     * Calculation of the velocity gradient
     */
    void CalculateVelocityGradientVector(Vector& rH,
                                         const Matrix& rDN_DX,
                                         unsigned int step = 0);

    
    /**
     * Calculation of the symmetric velocity gradient Vector
     */
    void CalculateSymmetricVelocityGradient(const Matrix& rH,
                                            Vector& rStrainVector);

    /**
     * Calculation of the skew symmetric velocity gradient Vector
     */
    void CalculateSkewSymmetricVelocityGradient(const Matrix& rH,
                                                Vector& rStrainVector);

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
    ///@}
    ///@name Serialization
    ///@{
    friend class Serializer;

    // A private default constructor necessary for serialization

    virtual void save(Serializer& rSerializer) const override;

    virtual void load(Serializer& rSerializer) override;


    ///@name Private Inquiry
    ///@{
    ///@}
    ///@name Un accessible methods
    ///@{
    ///@}

}; // Class LargeDisplacementSegregatedVPElement

///@}
///@name Type Definitions
///@{
///@}
///@name Input and output
///@{
///@}

} // namespace Kratos.
#endif // KRATOS_LARGE_DISPLACEMENT_SEGREGATED_V_P_ELEMENT_H_INCLUDED  defined 
