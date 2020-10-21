// KRATOS ___ ___  _  ___   __   ___ ___ ___ ___ 
//       / __/ _ \| \| \ \ / /__|   \_ _| __| __|
//      | (_| (_) | .` |\ V /___| |) | || _|| _| 
//       \___\___/|_|\_| \_/    |___/___|_| |_|  APPLICATION
//
//  License: BSD License
//					 Kratos default license: kratos/license.txt
//
//  Main authors:  Riccardo Rossi
//

#if !defined(KRATOS_EULERIAN_CONVECTION_DIFFUSION_EPOXY_ELEMENT_INCLUDED )
#define  KRATOS_EULERIAN_CONVECTION_DIFFUSION_EPOXY_ELEMENT_INCLUDED


// System includes


// External includes


// Project includes
#include "includes/define.h"
#include "includes/element.h"
#include "includes/ublas_interface.h"
#include "includes/variables.h"
#include "includes/cfd_variables.h"
#include "includes/serializer.h"
#include "utilities/math_utils.h"
#include "utilities/geometry_utilities.h"



namespace Kratos
{

///formulation described in https://docs.google.com/document/d/13a_zGLj6xORDuLgoOG5LwHI6BwShvfO166opZ815zLY/edit?usp=sharing
template< unsigned int TDim, unsigned int TNumNodes>
class KRATOS_API(CONVECTION_DIFFUSION_APPLICATION) EulerianConvectionDiffusionEpoxyElement
    : public Element
{
public:
    /// Counted pointer of
    KRATOS_CLASS_INTRUSIVE_POINTER_DEFINITION(EulerianConvectionDiffusionEpoxyElement);

    typedef std::size_t SizeType;
    typedef std::size_t IndexType;
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

    /// Default constructor.

    EulerianConvectionDiffusionEpoxyElement() : Element()
    {
    }

    EulerianConvectionDiffusionEpoxyElement(IndexType NewId, GeometryType::Pointer pGeometry)
    : Element(NewId, pGeometry)
    {}

    EulerianConvectionDiffusionEpoxyElement(IndexType NewId, GeometryType::Pointer pGeometry, PropertiesType::Pointer pProperties)
    : Element(NewId, pGeometry, pProperties)
    {}

    /// Destructor.
    virtual ~EulerianConvectionDiffusionEpoxyElement() {};

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

    Element::Pointer Create(
        IndexType NewId, 
        NodesArrayType const& ThisNodes, 
        PropertiesType::Pointer pProperties
        ) const override
    {
        return Kratos::make_intrusive<EulerianConvectionDiffusionEpoxyElement>(NewId, GetGeometry().Create(ThisNodes), pProperties);
    }
    
    Element::Pointer Create(
        IndexType NewId,
        GeometryType::Pointer pGeom,
        PropertiesType::Pointer pProperties
        ) const override
    {
        return Kratos::make_intrusive<EulerianConvectionDiffusionEpoxyElement>(NewId, pGeom, pProperties);
    }

    void EquationIdVector(EquationIdVectorType& rResult, ProcessInfo& rCurrentProcessInfo) override;

    void GetDofList(DofsVectorType& ElementalDofList, ProcessInfo& rCurrentProcessInfo) override;

    void CalculateLocalSystem(MatrixType& rLeftHandSideMatrix, VectorType& rRightHandSideVector, ProcessInfo& rCurrentProcessInfo) override;

	void CalculateRightHandSide(VectorType& rRightHandSideVector, ProcessInfo& rCurrentProcessInfo) override;

    void FinalizeSolutionStep(const ProcessInfo& rCurrentProcessInfo) override;

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

    void Initialize(const ProcessInfo& rCurrentProcessInfo) override;

    void GetValueOnIntegrationPoints(const Variable<double>& rVariable,
        std::vector<double>& rOutput,
        const ProcessInfo& rCurrentProcessInfo) override;

    void CalculateOnIntegrationPoints(const Variable<double>& rVariable,
        std::vector<double>& rOutput,
        const ProcessInfo& rCurrentProcessInfo) override;
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


    std::string Info() const override
    {
        return "EulerianConvectionDiffusionEpoxyElement #" + this->Id();
    }

    /// Print information about this object.

    void PrintInfo(std::ostream& rOStream) const override
    {
        rOStream << Info() << Id();
    }

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

protected:

    struct ElementVariables
    {
        double theta;
        double dyn_st_beta;
        double dt_inv;
        double lumping_factor;
        double conductivity;
        double specific_heat;
        double density;
        double beta;
        double div_v;

        array_1d<double,TNumNodes> phi;
        array_1d<double,TNumNodes> phi_old;
        array_1d<double,TNumNodes> volumetric_source;
        array_1d< array_1d<double,3 >, TNumNodes> v;
        array_1d< array_1d<double,3 >, TNumNodes> vold;
    };

    void InitializeEulerianElement(ElementVariables& rVariables, const ProcessInfo& rCurrentProcessInfo);

    void CalculateGeometry(BoundedMatrix<double,TNumNodes,TDim>& rDN_DX, double& rVolume);

    double ComputeH(BoundedMatrix<double,TNumNodes,TDim>& rDN_DX);

    void GetNodalValues(ElementVariables& rVariables, ProcessInfo& rCurrentProcessInfo);

    double CalculateTau(const ElementVariables& rVariables, double norm_vel, double h);

    // Member Variables


//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

private:

    Vector m_degree_of_cure_vector;

    Vector m_glass_transition_temperature;

    Vector m_heat_of_reaction;

    Vector m_pre_strain_vector;

    double m_specific_heat_capacity = 0.0;

    double ComputeDegreeOfCure(double rDegreeOfCure, double Temperature);

    double ComputeGlassTransitionTemperature(double DegreeOfCure);
    
    double ComputePreStrainFactor(double DegreeOfCure, double temperature_previous, double temperature_current);

    double ComputeHeatOfReaction(
        double degree_of_cure_previous,
        double degree_of_cure_current,
        double delta_time);

    double ComputeSpecificHeatCapacity(
        double Temperature,
        double GlassTransitionTemperature,
        double DegreeOfCure);

    // Serialization

    friend class Serializer;

    void save(Serializer& rSerializer) const override
    {
        KRATOS_SERIALIZE_SAVE_BASE_CLASS(rSerializer, Element);
    }

    void load(Serializer& rSerializer) override
    {
        KRATOS_SERIALIZE_LOAD_BASE_CLASS(rSerializer, Element);
    }


}; // Class EulerianConvectionDiffusionEpoxyElement

} // namespace Kratos.

#endif // KRATOS_EULERIAN_CONVECTION_DIFFUSION_EPOXY_ELEMENT_INCLUDED  defined
