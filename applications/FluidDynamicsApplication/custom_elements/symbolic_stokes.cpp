//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ `
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics
//
//  License:         BSD License
//                   Kratos default license: kratos/license.txt
//
//  Main author:     Daniel Diez
//  Co-authors:      Ruben Zorrilla
//

#include "symbolic_stokes.h"
#include "custom_utilities/symbolic_stokes_data.h"

namespace Kratos
{

///////////////////////////////////////////////////////////////////////////////////////////////////
// Life cycle

template <class TElementData>
SymbolicStokes<TElementData>::SymbolicStokes(IndexType NewId)
    : FluidElement<TElementData>(NewId) {}

template <class TElementData>
SymbolicStokes<TElementData>::SymbolicStokes(
    IndexType NewId, const NodesArrayType &ThisNodes)
    : FluidElement<TElementData>(NewId, ThisNodes) {}

template <class TElementData>
SymbolicStokes<TElementData>::SymbolicStokes(
    IndexType NewId, GeometryType::Pointer pGeometry)
    : FluidElement<TElementData>(NewId, pGeometry) {}

template <class TElementData>
SymbolicStokes<TElementData>::SymbolicStokes(
    IndexType NewId, GeometryType::Pointer pGeometry, Properties::Pointer pProperties)
    : FluidElement<TElementData>(NewId, pGeometry, pProperties) {}

template <class TElementData>
SymbolicStokes<TElementData>::~SymbolicStokes() {}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Public Operations

template <class TElementData>
Element::Pointer SymbolicStokes<TElementData>::Create(
    IndexType NewId,
    NodesArrayType const &ThisNodes,
    Properties::Pointer pProperties) const
{
    return Kratos::make_intrusive<SymbolicStokes>(NewId, this->GetGeometry().Create(ThisNodes), pProperties);
}

template <class TElementData>
Element::Pointer SymbolicStokes<TElementData>::Create(
    IndexType NewId,
    GeometryType::Pointer pGeom,
    Properties::Pointer pProperties) const
{
    return Kratos::make_intrusive<SymbolicStokes>(NewId, pGeom, pProperties);
}

template <class TElementData>
void SymbolicStokes<TElementData>::CalculateLocalSystem(
    MatrixType &rLeftHandSideMatrix,
    VectorType &rRightHandSideVector,
    ProcessInfo &rCurrentProcessInfo)
{
    // Resize and intialize output
    if (rLeftHandSideMatrix.size1() != LocalSize)
        rLeftHandSideMatrix.resize(LocalSize, LocalSize, false);

    if (rRightHandSideVector.size() != LocalSize)
        rRightHandSideVector.resize(LocalSize, false);

    noalias(rLeftHandSideMatrix) = ZeroMatrix(LocalSize, LocalSize);
    noalias(rRightHandSideVector) = ZeroVector(LocalSize);

    if (TElementData::ElementManagesTimeIntegration){
        TElementData data;
        data.Initialize(*this, rCurrentProcessInfo);

        //Get Shape function data
        Vector gauss_weights;
        Matrix shape_functions;
        ShapeFunctionDerivativesArrayType shape_derivatives;
        this->CalculateGeometryData(gauss_weights, shape_functions, shape_derivatives);
        const unsigned int number_of_gauss_points = gauss_weights.size();
        // Iterate over integration points to evaluate local contribution
        for (unsigned int g = 0; g < number_of_gauss_points; g++) {
            UpdateIntegrationPointData(data, g, gauss_weights[g], row(shape_functions, g), shape_derivatives[g]);
            this->AddTimeIntegratedSystem(data, rLeftHandSideMatrix, rRightHandSideVector);
        }

    } else{
        KRATOS_ERROR << "SymbolicStokes is supposed to manage time integration." << std::endl;
    }
}

template <class TElementData>
void SymbolicStokes<TElementData>::CalculateRightHandSide(
    VectorType &rRightHandSideVector,
    ProcessInfo &rCurrentProcessInfo)
{
    MatrixType tmp;
    CalculateLocalSystem(tmp, rRightHandSideVector, rCurrentProcessInfo);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
// Public Inquiry

template <class TElementData>
int SymbolicStokes<TElementData>::Check(const ProcessInfo &rCurrentProcessInfo)
{
    KRATOS_TRY;
    int out = FluidElement<TElementData>::Check(rCurrentProcessInfo);
    KRATOS_ERROR_IF_NOT(out == 0)
        << "Error in base class Check for Element " << this->Info() << std::endl
        << "Error code is " << out << std::endl;

    KRATOS_CHECK_VARIABLE_KEY( DIVERGENCE );

    return 0;

    KRATOS_CATCH("");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Public I/O

template <class TElementData>
std::string SymbolicStokes<TElementData>::Info() const
{
    std::stringstream buffer;
    buffer << "SymbolicStokes" << Dim << "D" << NumNodes << "N #" << this->Id();
    return buffer.str();
}

template <class TElementData>
void SymbolicStokes<TElementData>::PrintInfo(
    std::ostream &rOStream) const
{
    rOStream << this->Info() << std::endl;

    if (this->GetConstitutiveLaw() != nullptr){
        rOStream << "with constitutive law " << std::endl;
        this->GetConstitutiveLaw()->PrintInfo(rOStream);
    }
}

template <class TElementData>
void SymbolicStokes<TElementData>::Calculate(
    const Variable<Vector >& rVariable,
    Vector& rOutput,
    const ProcessInfo& rCurrentProcessInfo )
{
    noalias( rOutput ) = ZeroVector( StrainSize );

    if (rVariable == FLUID_STRESS) {

        // creating a new data container that goes out of scope after the function is left
        TElementData dataLocal;

        // transferring the velocity (among other variables)
        dataLocal.Initialize(*this, rCurrentProcessInfo);

        Vector gauss_weights;
        Matrix shape_functions;
        ShapeFunctionDerivativesArrayType shape_derivatives;

        // computing DN_DX values for the strain rate
        this->CalculateGeometryData(gauss_weights, shape_functions, shape_derivatives);
        const unsigned int number_of_gauss_points = gauss_weights.size();

        double sumOfGaussWeights = 0.0;

        for (unsigned int g = 0; g < number_of_gauss_points; g++){

            UpdateIntegrationPointData(dataLocal, g, gauss_weights[g], row(shape_functions, g), shape_derivatives[g]);

            const Vector gauss_point_contribution = dataLocal.ShearStress;

            noalias( rOutput ) += gauss_point_contribution * gauss_weights[g];
            sumOfGaussWeights += gauss_weights[g];
        }

        for (unsigned int i = 0; i < StrainSize; i++){
            rOutput[i] = ( 1.0 / sumOfGaussWeights ) * rOutput[i];
        }

    } else {

        Element::Calculate(rVariable, rOutput, rCurrentProcessInfo);

    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Protected operations

template <class TElementData>
void SymbolicStokes<TElementData>::AddTimeIntegratedSystem(
    TElementData &rData,
    MatrixType &rLHS,
    VectorType &rRHS)
{
    this->ComputeGaussPointLHSContribution(rData, rLHS);
    this->ComputeGaussPointRHSContribution(rData, rRHS);
}

template <class TElementData>
void SymbolicStokes<TElementData>::AddTimeIntegratedLHS(
    TElementData &rData,
    MatrixType &rLHS)
{
    this->ComputeGaussPointLHSContribution(rData, rLHS);
}

template <class TElementData>
void SymbolicStokes<TElementData>::AddTimeIntegratedRHS(
    TElementData &rData,
    VectorType &rRHS)
{
    this->ComputeGaussPointRHSContribution(rData, rRHS);
}

template <class TElementData>
void SymbolicStokes<TElementData>::UpdateIntegrationPointData(
    TElementData& rData,
    unsigned int IntegrationPointIndex,
    double Weight,
    const typename TElementData::MatrixRowType& rN,
    const typename TElementData::ShapeDerivativesType& rDN_DX) const
{
    rData.UpdateGeometryValues(IntegrationPointIndex, Weight, rN, rDN_DX);
    this->CalculateMaterialResponse(rData);
}

template <>
void SymbolicStokes<SymbolicStokesData<2,3>>::ComputeGaussPointLHSContribution(
    SymbolicStokesData<2,3> &rData,
    MatrixType &rLHS)
{
    const double rho = rData.Density;
    const double mu = rData.EffectiveViscosity;

    const double h = rData.ElementSize;

    const double dt = rData.DeltaTime;
    const double bdf0 = rData.bdf0;

    const double dyn_tau = rData.DynamicTau;

    // Get constitutive matrix
    const Matrix &C = rData.C;

    // Get shape function values
    const auto &N = rData.N;
    const auto &DN = rData.DN_DX;

    // Stabilization parameters
    constexpr double stab_c1 = 4.0;
    constexpr double stab_c2 = 2.0;

    auto &lhs = rData.lhs;

    double dt_inv = 0.0;
    if (dt > 1e-09)
    {
        dt_inv = 1.0/dt;
    }
    if (std::abs(bdf0) < 1e-9)
    {
        dt_inv = 0.0;
    }

    const double clhs0 =             pow(DN(0,0), 2);
const double clhs1 =             bdf0*rho;
const double clhs2 =             pow(N[0], 2)*clhs1;
const double clhs3 =             C(0,0)*DN(0,0) + C(0,2)*DN(0,1);
const double clhs4 =             C(0,2)*DN(0,0);
const double clhs5 =             C(2,2)*DN(0,1) + clhs4;
const double clhs6 =             DN(0,0)*mu;
const double clhs7 =             DN(0,1)*clhs6;
const double clhs8 =             C(0,1)*DN(0,1) + clhs4;
const double clhs9 =             C(1,2)*DN(0,1);
const double clhs10 =             C(2,2)*DN(0,0) + clhs9;
const double clhs11 =             DN(0,0)*N[0];
const double clhs12 =             C(0,0)*DN(1,0) + C(0,2)*DN(1,1);
const double clhs13 =             C(0,2)*DN(1,0);
const double clhs14 =             C(2,2)*DN(1,1) + clhs13;
const double clhs15 =             DN(0,0)*DN(1,0);
const double clhs16 =             N[0]*clhs1;
const double clhs17 =             N[1]*clhs16;
const double clhs18 =             clhs15*mu + clhs17;
const double clhs19 =             DN(1,1)*clhs6;
const double clhs20 =             C(0,1)*DN(1,1) + clhs13;
const double clhs21 =             C(1,2)*DN(1,1);
const double clhs22 =             C(2,2)*DN(1,0) + clhs21;
const double clhs23 =             DN(0,0)*N[1];
const double clhs24 =             C(0,0)*DN(2,0) + C(0,2)*DN(2,1);
const double clhs25 =             C(0,2)*DN(2,0);
const double clhs26 =             C(2,2)*DN(2,1) + clhs25;
const double clhs27 =             DN(0,0)*DN(2,0);
const double clhs28 =             N[2]*clhs16;
const double clhs29 =             clhs27*mu + clhs28;
const double clhs30 =             DN(2,1)*clhs6;
const double clhs31 =             C(0,1)*DN(2,1) + clhs25;
const double clhs32 =             C(1,2)*DN(2,1);
const double clhs33 =             C(2,2)*DN(2,0) + clhs32;
const double clhs34 =             DN(0,0)*N[2];
const double clhs35 =             C(0,1)*DN(0,0) + clhs9;
const double clhs36 =             pow(DN(0,1), 2);
const double clhs37 =             C(1,1)*DN(0,1) + C(1,2)*DN(0,0);
const double clhs38 =             DN(0,1)*N[0];
const double clhs39 =             DN(0,1)*mu;
const double clhs40 =             DN(1,0)*clhs39;
const double clhs41 =             C(0,1)*DN(1,0) + clhs21;
const double clhs42 =             C(1,1)*DN(1,1) + C(1,2)*DN(1,0);
const double clhs43 =             DN(0,1)*DN(1,1);
const double clhs44 =             clhs17 + clhs43*mu;
const double clhs45 =             DN(0,1)*N[1];
const double clhs46 =             DN(2,0)*clhs39;
const double clhs47 =             C(0,1)*DN(2,0) + clhs32;
const double clhs48 =             C(1,1)*DN(2,1) + C(1,2)*DN(2,0);
const double clhs49 =             DN(0,1)*DN(2,1);
const double clhs50 =             clhs28 + clhs49*mu;
const double clhs51 =             DN(0,1)*N[2];
const double clhs52 =             1.0/(dt_inv*dyn_tau*rho + mu*stab_c1/pow(h, 2));
const double clhs53 =             clhs1*clhs52;
const double clhs54 =             clhs53 + 1;
const double clhs55 =             DN(1,0)*N[0];
const double clhs56 =             DN(1,1)*N[0];
const double clhs57 =             clhs52*(clhs15 + clhs43);
const double clhs58 =             DN(2,0)*N[0];
const double clhs59 =             DN(2,1)*N[0];
const double clhs60 =             clhs52*(clhs27 + clhs49);
const double clhs61 =             pow(DN(1,0), 2);
const double clhs62 =             pow(N[1], 2)*clhs1;
const double clhs63 =             DN(1,0)*mu;
const double clhs64 =             DN(1,1)*clhs63;
const double clhs65 =             DN(1,0)*N[1];
const double clhs66 =             DN(1,0)*DN(2,0);
const double clhs67 =             N[1]*N[2]*clhs1;
const double clhs68 =             clhs66*mu + clhs67;
const double clhs69 =             DN(2,1)*clhs63;
const double clhs70 =             DN(1,0)*N[2];
const double clhs71 =             pow(DN(1,1), 2);
const double clhs72 =             DN(1,1)*N[1];
const double clhs73 =             DN(2,0)*mu;
const double clhs74 =             DN(1,1)*clhs73;
const double clhs75 =             DN(1,1)*DN(2,1);
const double clhs76 =             clhs67 + clhs75*mu;
const double clhs77 =             DN(1,1)*N[2];
const double clhs78 =             DN(2,0)*N[1];
const double clhs79 =             DN(2,1)*N[1];
const double clhs80 =             clhs52*(clhs66 + clhs75);
const double clhs81 =             pow(DN(2,0), 2);
const double clhs82 =             pow(N[2], 2)*clhs1;
const double clhs83 =             DN(2,1)*clhs73;
const double clhs84 =             DN(2,0)*N[2];
const double clhs85 =             pow(DN(2,1), 2);
const double clhs86 =             DN(2,1)*N[2];
            lhs(0,0)=DN(0,0)*clhs3 + DN(0,1)*clhs5 + clhs0*mu + clhs2;
            lhs(0,1)=DN(0,0)*clhs8 + DN(0,1)*clhs10 + clhs7;
            lhs(0,2)=-clhs11;
            lhs(0,3)=DN(0,0)*clhs12 + DN(0,1)*clhs14 + clhs18;
            lhs(0,4)=DN(0,0)*clhs20 + DN(0,1)*clhs22 + clhs19;
            lhs(0,5)=-clhs23;
            lhs(0,6)=DN(0,0)*clhs24 + DN(0,1)*clhs26 + clhs29;
            lhs(0,7)=DN(0,0)*clhs31 + DN(0,1)*clhs33 + clhs30;
            lhs(0,8)=-clhs34;
            lhs(1,0)=DN(0,0)*clhs5 + DN(0,1)*clhs35 + clhs7;
            lhs(1,1)=DN(0,0)*clhs10 + DN(0,1)*clhs37 + clhs2 + clhs36*mu;
            lhs(1,2)=-clhs38;
            lhs(1,3)=DN(0,0)*clhs14 + DN(0,1)*clhs41 + clhs40;
            lhs(1,4)=DN(0,0)*clhs22 + DN(0,1)*clhs42 + clhs44;
            lhs(1,5)=-clhs45;
            lhs(1,6)=DN(0,0)*clhs26 + DN(0,1)*clhs47 + clhs46;
            lhs(1,7)=DN(0,0)*clhs33 + DN(0,1)*clhs48 + clhs50;
            lhs(1,8)=-clhs51;
            lhs(2,0)=clhs11*clhs54;
            lhs(2,1)=clhs38*clhs54;
            lhs(2,2)=clhs52*(clhs0 + clhs36);
            lhs(2,3)=clhs23*clhs53 + clhs55;
            lhs(2,4)=clhs45*clhs53 + clhs56;
            lhs(2,5)=clhs57;
            lhs(2,6)=clhs34*clhs53 + clhs58;
            lhs(2,7)=clhs51*clhs53 + clhs59;
            lhs(2,8)=clhs60;
            lhs(3,0)=DN(1,0)*clhs3 + DN(1,1)*clhs5 + clhs18;
            lhs(3,1)=DN(1,0)*clhs8 + DN(1,1)*clhs10 + clhs40;
            lhs(3,2)=-clhs55;
            lhs(3,3)=DN(1,0)*clhs12 + DN(1,1)*clhs14 + clhs61*mu + clhs62;
            lhs(3,4)=DN(1,0)*clhs20 + DN(1,1)*clhs22 + clhs64;
            lhs(3,5)=-clhs65;
            lhs(3,6)=DN(1,0)*clhs24 + DN(1,1)*clhs26 + clhs68;
            lhs(3,7)=DN(1,0)*clhs31 + DN(1,1)*clhs33 + clhs69;
            lhs(3,8)=-clhs70;
            lhs(4,0)=DN(1,0)*clhs5 + DN(1,1)*clhs35 + clhs19;
            lhs(4,1)=DN(1,0)*clhs10 + DN(1,1)*clhs37 + clhs44;
            lhs(4,2)=-clhs56;
            lhs(4,3)=DN(1,0)*clhs14 + DN(1,1)*clhs41 + clhs64;
            lhs(4,4)=DN(1,0)*clhs22 + DN(1,1)*clhs42 + clhs62 + clhs71*mu;
            lhs(4,5)=-clhs72;
            lhs(4,6)=DN(1,0)*clhs26 + DN(1,1)*clhs47 + clhs74;
            lhs(4,7)=DN(1,0)*clhs33 + DN(1,1)*clhs48 + clhs76;
            lhs(4,8)=-clhs77;
            lhs(5,0)=clhs23 + clhs53*clhs55;
            lhs(5,1)=clhs45 + clhs53*clhs56;
            lhs(5,2)=clhs57;
            lhs(5,3)=clhs54*clhs65;
            lhs(5,4)=clhs54*clhs72;
            lhs(5,5)=clhs52*(clhs61 + clhs71);
            lhs(5,6)=clhs53*clhs70 + clhs78;
            lhs(5,7)=clhs53*clhs77 + clhs79;
            lhs(5,8)=clhs80;
            lhs(6,0)=DN(2,0)*clhs3 + DN(2,1)*clhs5 + clhs29;
            lhs(6,1)=DN(2,0)*clhs8 + DN(2,1)*clhs10 + clhs46;
            lhs(6,2)=-clhs58;
            lhs(6,3)=DN(2,0)*clhs12 + DN(2,1)*clhs14 + clhs68;
            lhs(6,4)=DN(2,0)*clhs20 + DN(2,1)*clhs22 + clhs74;
            lhs(6,5)=-clhs78;
            lhs(6,6)=DN(2,0)*clhs24 + DN(2,1)*clhs26 + clhs81*mu + clhs82;
            lhs(6,7)=DN(2,0)*clhs31 + DN(2,1)*clhs33 + clhs83;
            lhs(6,8)=-clhs84;
            lhs(7,0)=DN(2,0)*clhs5 + DN(2,1)*clhs35 + clhs30;
            lhs(7,1)=DN(2,0)*clhs10 + DN(2,1)*clhs37 + clhs50;
            lhs(7,2)=-clhs59;
            lhs(7,3)=DN(2,0)*clhs14 + DN(2,1)*clhs41 + clhs69;
            lhs(7,4)=DN(2,0)*clhs22 + DN(2,1)*clhs42 + clhs76;
            lhs(7,5)=-clhs79;
            lhs(7,6)=DN(2,0)*clhs26 + DN(2,1)*clhs47 + clhs83;
            lhs(7,7)=DN(2,0)*clhs33 + DN(2,1)*clhs48 + clhs82 + clhs85*mu;
            lhs(7,8)=-clhs86;
            lhs(8,0)=clhs34 + clhs53*clhs58;
            lhs(8,1)=clhs51 + clhs53*clhs59;
            lhs(8,2)=clhs60;
            lhs(8,3)=clhs53*clhs78 + clhs70;
            lhs(8,4)=clhs53*clhs79 + clhs77;
            lhs(8,5)=clhs80;
            lhs(8,6)=clhs54*clhs84;
            lhs(8,7)=clhs54*clhs86;
            lhs(8,8)=clhs52*(clhs81 + clhs85);


    // Add intermediate results to local system
    noalias(rLHS) += lhs * rData.Weight;
}

template <>
void SymbolicStokes<SymbolicStokesData<2,4>>::ComputeGaussPointLHSContribution(
    SymbolicStokesData<2,4> &rData,
    MatrixType &rLHS)
{
    const double rho = rData.Density;
    const double mu = rData.EffectiveViscosity;

    const double h = rData.ElementSize;

    const double dt = rData.DeltaTime;
    const double bdf0 = rData.bdf0;

    const double dyn_tau = rData.DynamicTau;

    // Get constitutive matrix
    const Matrix &C = rData.C;

    // Get shape function values
    const auto &N = rData.N;
    const auto &DN = rData.DN_DX;

    // Stabilization parameters
    constexpr double stab_c1 = 4.0;
    constexpr double stab_c2 = 2.0;

    auto &lhs = rData.lhs;
    double dt_inv = 0.0;
    if (dt > 1e-09)
    {
        dt_inv = 1.0/dt;
    }
    if (std::abs(bdf0) < 1e-9)
    {
        dt_inv = 0.0;
    }

    const double clhs0 =             pow(DN(0,0), 2);
const double clhs1 =             bdf0*rho;
const double clhs2 =             pow(N[0], 2)*clhs1;
const double clhs3 =             C(0,0)*DN(0,0) + C(0,2)*DN(0,1);
const double clhs4 =             C(0,2)*DN(0,0);
const double clhs5 =             C(2,2)*DN(0,1) + clhs4;
const double clhs6 =             DN(0,0)*mu;
const double clhs7 =             DN(0,1)*clhs6;
const double clhs8 =             C(0,1)*DN(0,1) + clhs4;
const double clhs9 =             C(1,2)*DN(0,1);
const double clhs10 =             C(2,2)*DN(0,0) + clhs9;
const double clhs11 =             DN(0,0)*N[0];
const double clhs12 =             C(0,0)*DN(1,0) + C(0,2)*DN(1,1);
const double clhs13 =             C(0,2)*DN(1,0);
const double clhs14 =             C(2,2)*DN(1,1) + clhs13;
const double clhs15 =             DN(0,0)*DN(1,0);
const double clhs16 =             N[0]*clhs1;
const double clhs17 =             N[1]*clhs16;
const double clhs18 =             clhs15*mu + clhs17;
const double clhs19 =             DN(1,1)*clhs6;
const double clhs20 =             C(0,1)*DN(1,1) + clhs13;
const double clhs21 =             C(1,2)*DN(1,1);
const double clhs22 =             C(2,2)*DN(1,0) + clhs21;
const double clhs23 =             DN(0,0)*N[1];
const double clhs24 =             C(0,0)*DN(2,0) + C(0,2)*DN(2,1);
const double clhs25 =             C(0,2)*DN(2,0);
const double clhs26 =             C(2,2)*DN(2,1) + clhs25;
const double clhs27 =             DN(0,0)*DN(2,0);
const double clhs28 =             N[2]*clhs16;
const double clhs29 =             clhs27*mu + clhs28;
const double clhs30 =             DN(2,1)*clhs6;
const double clhs31 =             C(0,1)*DN(2,1) + clhs25;
const double clhs32 =             C(1,2)*DN(2,1);
const double clhs33 =             C(2,2)*DN(2,0) + clhs32;
const double clhs34 =             DN(0,0)*N[2];
const double clhs35 =             C(0,0)*DN(3,0) + C(0,2)*DN(3,1);
const double clhs36 =             C(0,2)*DN(3,0);
const double clhs37 =             C(2,2)*DN(3,1) + clhs36;
const double clhs38 =             DN(0,0)*DN(3,0);
const double clhs39 =             N[3]*clhs16;
const double clhs40 =             clhs38*mu + clhs39;
const double clhs41 =             DN(3,1)*clhs6;
const double clhs42 =             C(0,1)*DN(3,1) + clhs36;
const double clhs43 =             C(1,2)*DN(3,1);
const double clhs44 =             C(2,2)*DN(3,0) + clhs43;
const double clhs45 =             DN(0,0)*N[3];
const double clhs46 =             C(0,1)*DN(0,0) + clhs9;
const double clhs47 =             pow(DN(0,1), 2);
const double clhs48 =             C(1,1)*DN(0,1) + C(1,2)*DN(0,0);
const double clhs49 =             DN(0,1)*N[0];
const double clhs50 =             DN(0,1)*mu;
const double clhs51 =             DN(1,0)*clhs50;
const double clhs52 =             C(0,1)*DN(1,0) + clhs21;
const double clhs53 =             C(1,1)*DN(1,1) + C(1,2)*DN(1,0);
const double clhs54 =             DN(0,1)*DN(1,1);
const double clhs55 =             clhs17 + clhs54*mu;
const double clhs56 =             DN(0,1)*N[1];
const double clhs57 =             DN(2,0)*clhs50;
const double clhs58 =             C(0,1)*DN(2,0) + clhs32;
const double clhs59 =             C(1,1)*DN(2,1) + C(1,2)*DN(2,0);
const double clhs60 =             DN(0,1)*DN(2,1);
const double clhs61 =             clhs28 + clhs60*mu;
const double clhs62 =             DN(0,1)*N[2];
const double clhs63 =             DN(3,0)*clhs50;
const double clhs64 =             C(0,1)*DN(3,0) + clhs43;
const double clhs65 =             C(1,1)*DN(3,1) + C(1,2)*DN(3,0);
const double clhs66 =             DN(0,1)*DN(3,1);
const double clhs67 =             clhs39 + clhs66*mu;
const double clhs68 =             DN(0,1)*N[3];
const double clhs69 =             1.0/(dt_inv*dyn_tau*rho + mu*stab_c1/pow(h, 2));
const double clhs70 =             clhs1*clhs69;
const double clhs71 =             clhs70 + 1;
const double clhs72 =             DN(1,0)*N[0];
const double clhs73 =             DN(1,1)*N[0];
const double clhs74 =             clhs69*(clhs15 + clhs54);
const double clhs75 =             DN(2,0)*N[0];
const double clhs76 =             DN(2,1)*N[0];
const double clhs77 =             clhs69*(clhs27 + clhs60);
const double clhs78 =             DN(3,0)*N[0];
const double clhs79 =             DN(3,1)*N[0];
const double clhs80 =             clhs69*(clhs38 + clhs66);
const double clhs81 =             pow(DN(1,0), 2);
const double clhs82 =             pow(N[1], 2)*clhs1;
const double clhs83 =             DN(1,0)*mu;
const double clhs84 =             DN(1,1)*clhs83;
const double clhs85 =             DN(1,0)*N[1];
const double clhs86 =             DN(1,0)*DN(2,0);
const double clhs87 =             N[1]*clhs1;
const double clhs88 =             N[2]*clhs87;
const double clhs89 =             clhs86*mu + clhs88;
const double clhs90 =             DN(2,1)*clhs83;
const double clhs91 =             DN(1,0)*N[2];
const double clhs92 =             DN(1,0)*DN(3,0);
const double clhs93 =             N[3]*clhs87;
const double clhs94 =             clhs92*mu + clhs93;
const double clhs95 =             DN(3,1)*clhs83;
const double clhs96 =             DN(1,0)*N[3];
const double clhs97 =             pow(DN(1,1), 2);
const double clhs98 =             DN(1,1)*N[1];
const double clhs99 =             DN(1,1)*mu;
const double clhs100 =             DN(2,0)*clhs99;
const double clhs101 =             DN(1,1)*DN(2,1);
const double clhs102 =             clhs101*mu + clhs88;
const double clhs103 =             DN(1,1)*N[2];
const double clhs104 =             DN(3,0)*clhs99;
const double clhs105 =             DN(1,1)*DN(3,1);
const double clhs106 =             clhs105*mu + clhs93;
const double clhs107 =             DN(1,1)*N[3];
const double clhs108 =             DN(2,0)*N[1];
const double clhs109 =             DN(2,1)*N[1];
const double clhs110 =             clhs69*(clhs101 + clhs86);
const double clhs111 =             DN(3,0)*N[1];
const double clhs112 =             DN(3,1)*N[1];
const double clhs113 =             clhs69*(clhs105 + clhs92);
const double clhs114 =             pow(DN(2,0), 2);
const double clhs115 =             pow(N[2], 2)*clhs1;
const double clhs116 =             DN(2,0)*mu;
const double clhs117 =             DN(2,1)*clhs116;
const double clhs118 =             DN(2,0)*N[2];
const double clhs119 =             DN(2,0)*DN(3,0);
const double clhs120 =             N[2]*N[3]*clhs1;
const double clhs121 =             clhs119*mu + clhs120;
const double clhs122 =             DN(3,1)*clhs116;
const double clhs123 =             DN(2,0)*N[3];
const double clhs124 =             pow(DN(2,1), 2);
const double clhs125 =             DN(2,1)*N[2];
const double clhs126 =             DN(3,0)*mu;
const double clhs127 =             DN(2,1)*clhs126;
const double clhs128 =             DN(2,1)*DN(3,1);
const double clhs129 =             clhs120 + clhs128*mu;
const double clhs130 =             DN(2,1)*N[3];
const double clhs131 =             DN(3,0)*N[2];
const double clhs132 =             DN(3,1)*N[2];
const double clhs133 =             clhs69*(clhs119 + clhs128);
const double clhs134 =             pow(DN(3,0), 2);
const double clhs135 =             pow(N[3], 2)*clhs1;
const double clhs136 =             DN(3,1)*clhs126;
const double clhs137 =             DN(3,0)*N[3];
const double clhs138 =             pow(DN(3,1), 2);
const double clhs139 =             DN(3,1)*N[3];
            lhs(0,0)=DN(0,0)*clhs3 + DN(0,1)*clhs5 + clhs0*mu + clhs2;
            lhs(0,1)=DN(0,0)*clhs8 + DN(0,1)*clhs10 + clhs7;
            lhs(0,2)=-clhs11;
            lhs(0,3)=DN(0,0)*clhs12 + DN(0,1)*clhs14 + clhs18;
            lhs(0,4)=DN(0,0)*clhs20 + DN(0,1)*clhs22 + clhs19;
            lhs(0,5)=-clhs23;
            lhs(0,6)=DN(0,0)*clhs24 + DN(0,1)*clhs26 + clhs29;
            lhs(0,7)=DN(0,0)*clhs31 + DN(0,1)*clhs33 + clhs30;
            lhs(0,8)=-clhs34;
            lhs(0,9)=DN(0,0)*clhs35 + DN(0,1)*clhs37 + clhs40;
            lhs(0,10)=DN(0,0)*clhs42 + DN(0,1)*clhs44 + clhs41;
            lhs(0,11)=-clhs45;
            lhs(1,0)=DN(0,0)*clhs5 + DN(0,1)*clhs46 + clhs7;
            lhs(1,1)=DN(0,0)*clhs10 + DN(0,1)*clhs48 + clhs2 + clhs47*mu;
            lhs(1,2)=-clhs49;
            lhs(1,3)=DN(0,0)*clhs14 + DN(0,1)*clhs52 + clhs51;
            lhs(1,4)=DN(0,0)*clhs22 + DN(0,1)*clhs53 + clhs55;
            lhs(1,5)=-clhs56;
            lhs(1,6)=DN(0,0)*clhs26 + DN(0,1)*clhs58 + clhs57;
            lhs(1,7)=DN(0,0)*clhs33 + DN(0,1)*clhs59 + clhs61;
            lhs(1,8)=-clhs62;
            lhs(1,9)=DN(0,0)*clhs37 + DN(0,1)*clhs64 + clhs63;
            lhs(1,10)=DN(0,0)*clhs44 + DN(0,1)*clhs65 + clhs67;
            lhs(1,11)=-clhs68;
            lhs(2,0)=clhs11*clhs71;
            lhs(2,1)=clhs49*clhs71;
            lhs(2,2)=clhs69*(clhs0 + clhs47);
            lhs(2,3)=clhs23*clhs70 + clhs72;
            lhs(2,4)=clhs56*clhs70 + clhs73;
            lhs(2,5)=clhs74;
            lhs(2,6)=clhs34*clhs70 + clhs75;
            lhs(2,7)=clhs62*clhs70 + clhs76;
            lhs(2,8)=clhs77;
            lhs(2,9)=clhs45*clhs70 + clhs78;
            lhs(2,10)=clhs68*clhs70 + clhs79;
            lhs(2,11)=clhs80;
            lhs(3,0)=DN(1,0)*clhs3 + DN(1,1)*clhs5 + clhs18;
            lhs(3,1)=DN(1,0)*clhs8 + DN(1,1)*clhs10 + clhs51;
            lhs(3,2)=-clhs72;
            lhs(3,3)=DN(1,0)*clhs12 + DN(1,1)*clhs14 + clhs81*mu + clhs82;
            lhs(3,4)=DN(1,0)*clhs20 + DN(1,1)*clhs22 + clhs84;
            lhs(3,5)=-clhs85;
            lhs(3,6)=DN(1,0)*clhs24 + DN(1,1)*clhs26 + clhs89;
            lhs(3,7)=DN(1,0)*clhs31 + DN(1,1)*clhs33 + clhs90;
            lhs(3,8)=-clhs91;
            lhs(3,9)=DN(1,0)*clhs35 + DN(1,1)*clhs37 + clhs94;
            lhs(3,10)=DN(1,0)*clhs42 + DN(1,1)*clhs44 + clhs95;
            lhs(3,11)=-clhs96;
            lhs(4,0)=DN(1,0)*clhs5 + DN(1,1)*clhs46 + clhs19;
            lhs(4,1)=DN(1,0)*clhs10 + DN(1,1)*clhs48 + clhs55;
            lhs(4,2)=-clhs73;
            lhs(4,3)=DN(1,0)*clhs14 + DN(1,1)*clhs52 + clhs84;
            lhs(4,4)=DN(1,0)*clhs22 + DN(1,1)*clhs53 + clhs82 + clhs97*mu;
            lhs(4,5)=-clhs98;
            lhs(4,6)=DN(1,0)*clhs26 + DN(1,1)*clhs58 + clhs100;
            lhs(4,7)=DN(1,0)*clhs33 + DN(1,1)*clhs59 + clhs102;
            lhs(4,8)=-clhs103;
            lhs(4,9)=DN(1,0)*clhs37 + DN(1,1)*clhs64 + clhs104;
            lhs(4,10)=DN(1,0)*clhs44 + DN(1,1)*clhs65 + clhs106;
            lhs(4,11)=-clhs107;
            lhs(5,0)=clhs23 + clhs70*clhs72;
            lhs(5,1)=clhs56 + clhs70*clhs73;
            lhs(5,2)=clhs74;
            lhs(5,3)=clhs71*clhs85;
            lhs(5,4)=clhs71*clhs98;
            lhs(5,5)=clhs69*(clhs81 + clhs97);
            lhs(5,6)=clhs108 + clhs70*clhs91;
            lhs(5,7)=clhs103*clhs70 + clhs109;
            lhs(5,8)=clhs110;
            lhs(5,9)=clhs111 + clhs70*clhs96;
            lhs(5,10)=clhs107*clhs70 + clhs112;
            lhs(5,11)=clhs113;
            lhs(6,0)=DN(2,0)*clhs3 + DN(2,1)*clhs5 + clhs29;
            lhs(6,1)=DN(2,0)*clhs8 + DN(2,1)*clhs10 + clhs57;
            lhs(6,2)=-clhs75;
            lhs(6,3)=DN(2,0)*clhs12 + DN(2,1)*clhs14 + clhs89;
            lhs(6,4)=DN(2,0)*clhs20 + DN(2,1)*clhs22 + clhs100;
            lhs(6,5)=-clhs108;
            lhs(6,6)=DN(2,0)*clhs24 + DN(2,1)*clhs26 + clhs114*mu + clhs115;
            lhs(6,7)=DN(2,0)*clhs31 + DN(2,1)*clhs33 + clhs117;
            lhs(6,8)=-clhs118;
            lhs(6,9)=DN(2,0)*clhs35 + DN(2,1)*clhs37 + clhs121;
            lhs(6,10)=DN(2,0)*clhs42 + DN(2,1)*clhs44 + clhs122;
            lhs(6,11)=-clhs123;
            lhs(7,0)=DN(2,0)*clhs5 + DN(2,1)*clhs46 + clhs30;
            lhs(7,1)=DN(2,0)*clhs10 + DN(2,1)*clhs48 + clhs61;
            lhs(7,2)=-clhs76;
            lhs(7,3)=DN(2,0)*clhs14 + DN(2,1)*clhs52 + clhs90;
            lhs(7,4)=DN(2,0)*clhs22 + DN(2,1)*clhs53 + clhs102;
            lhs(7,5)=-clhs109;
            lhs(7,6)=DN(2,0)*clhs26 + DN(2,1)*clhs58 + clhs117;
            lhs(7,7)=DN(2,0)*clhs33 + DN(2,1)*clhs59 + clhs115 + clhs124*mu;
            lhs(7,8)=-clhs125;
            lhs(7,9)=DN(2,0)*clhs37 + DN(2,1)*clhs64 + clhs127;
            lhs(7,10)=DN(2,0)*clhs44 + DN(2,1)*clhs65 + clhs129;
            lhs(7,11)=-clhs130;
            lhs(8,0)=clhs34 + clhs70*clhs75;
            lhs(8,1)=clhs62 + clhs70*clhs76;
            lhs(8,2)=clhs77;
            lhs(8,3)=clhs108*clhs70 + clhs91;
            lhs(8,4)=clhs103 + clhs109*clhs70;
            lhs(8,5)=clhs110;
            lhs(8,6)=clhs118*clhs71;
            lhs(8,7)=clhs125*clhs71;
            lhs(8,8)=clhs69*(clhs114 + clhs124);
            lhs(8,9)=clhs123*clhs70 + clhs131;
            lhs(8,10)=clhs130*clhs70 + clhs132;
            lhs(8,11)=clhs133;
            lhs(9,0)=DN(3,0)*clhs3 + DN(3,1)*clhs5 + clhs40;
            lhs(9,1)=DN(3,0)*clhs8 + DN(3,1)*clhs10 + clhs63;
            lhs(9,2)=-clhs78;
            lhs(9,3)=DN(3,0)*clhs12 + DN(3,1)*clhs14 + clhs94;
            lhs(9,4)=DN(3,0)*clhs20 + DN(3,1)*clhs22 + clhs104;
            lhs(9,5)=-clhs111;
            lhs(9,6)=DN(3,0)*clhs24 + DN(3,1)*clhs26 + clhs121;
            lhs(9,7)=DN(3,0)*clhs31 + DN(3,1)*clhs33 + clhs127;
            lhs(9,8)=-clhs131;
            lhs(9,9)=DN(3,0)*clhs35 + DN(3,1)*clhs37 + clhs134*mu + clhs135;
            lhs(9,10)=DN(3,0)*clhs42 + DN(3,1)*clhs44 + clhs136;
            lhs(9,11)=-clhs137;
            lhs(10,0)=DN(3,0)*clhs5 + DN(3,1)*clhs46 + clhs41;
            lhs(10,1)=DN(3,0)*clhs10 + DN(3,1)*clhs48 + clhs67;
            lhs(10,2)=-clhs79;
            lhs(10,3)=DN(3,0)*clhs14 + DN(3,1)*clhs52 + clhs95;
            lhs(10,4)=DN(3,0)*clhs22 + DN(3,1)*clhs53 + clhs106;
            lhs(10,5)=-clhs112;
            lhs(10,6)=DN(3,0)*clhs26 + DN(3,1)*clhs58 + clhs122;
            lhs(10,7)=DN(3,0)*clhs33 + DN(3,1)*clhs59 + clhs129;
            lhs(10,8)=-clhs132;
            lhs(10,9)=DN(3,0)*clhs37 + DN(3,1)*clhs64 + clhs136;
            lhs(10,10)=DN(3,0)*clhs44 + DN(3,1)*clhs65 + clhs135 + clhs138*mu;
            lhs(10,11)=-clhs139;
            lhs(11,0)=clhs45 + clhs70*clhs78;
            lhs(11,1)=clhs68 + clhs70*clhs79;
            lhs(11,2)=clhs80;
            lhs(11,3)=clhs111*clhs70 + clhs96;
            lhs(11,4)=clhs107 + clhs112*clhs70;
            lhs(11,5)=clhs113;
            lhs(11,6)=clhs123 + clhs131*clhs70;
            lhs(11,7)=clhs130 + clhs132*clhs70;
            lhs(11,8)=clhs133;
            lhs(11,9)=clhs137*clhs71;
            lhs(11,10)=clhs139*clhs71;
            lhs(11,11)=clhs69*(clhs134 + clhs138);


    // Add intermediate results to local system
    noalias(rLHS) += lhs * rData.Weight;
}

template <>
void SymbolicStokes<SymbolicStokesData<3,4>>::ComputeGaussPointLHSContribution(
    SymbolicStokesData<3,4> &rData,
    MatrixType &rLHS)
{

    const double rho = rData.Density;
    const double mu = rData.EffectiveViscosity;

    const double h = rData.ElementSize;

    const double dt = rData.DeltaTime;
    const double bdf0 = rData.bdf0;

    const double dyn_tau = rData.DynamicTau;

    // Get constitutive matrix
    const Matrix &C = rData.C;

    // Get shape function values
    const auto &N = rData.N;
    const auto &DN = rData.DN_DX;

    // Stabilization parameters
    constexpr double stab_c1 = 4.0;
    constexpr double stab_c2 = 2.0;

    auto &lhs = rData.lhs;
    double dt_inv = 0.0;
    if (dt > 1e-09)
    {
        dt_inv = 1.0/dt;
    }
    if (std::abs(bdf0) < 1e-9)
    {
        dt_inv = 0.0;
    }

    const double clhs0 =             pow(DN(0,0), 2);
const double clhs1 =             bdf0*rho;
const double clhs2 =             pow(N[0], 2)*clhs1;
const double clhs3 =             C(0,0)*DN(0,0) + C(0,3)*DN(0,1) + C(0,5)*DN(0,2);
const double clhs4 =             C(0,3)*DN(0,0);
const double clhs5 =             C(3,3)*DN(0,1) + C(3,5)*DN(0,2) + clhs4;
const double clhs6 =             C(0,5)*DN(0,0);
const double clhs7 =             C(3,5)*DN(0,1) + C(5,5)*DN(0,2) + clhs6;
const double clhs8 =             DN(0,0)*mu;
const double clhs9 =             DN(0,1)*clhs8;
const double clhs10 =             C(0,1)*DN(0,1) + C(0,4)*DN(0,2) + clhs4;
const double clhs11 =             C(1,3)*DN(0,1);
const double clhs12 =             C(3,3)*DN(0,0) + C(3,4)*DN(0,2) + clhs11;
const double clhs13 =             C(3,5)*DN(0,0);
const double clhs14 =             C(4,5)*DN(0,2);
const double clhs15 =             C(1,5)*DN(0,1) + clhs13 + clhs14;
const double clhs16 =             DN(0,2)*clhs8;
const double clhs17 =             C(0,2)*DN(0,2) + C(0,4)*DN(0,1) + clhs6;
const double clhs18 =             C(3,4)*DN(0,1);
const double clhs19 =             C(2,3)*DN(0,2) + clhs13 + clhs18;
const double clhs20 =             C(2,5)*DN(0,2);
const double clhs21 =             C(4,5)*DN(0,1) + C(5,5)*DN(0,0) + clhs20;
const double clhs22 =             DN(0,0)*N[0];
const double clhs23 =             C(0,0)*DN(1,0) + C(0,3)*DN(1,1) + C(0,5)*DN(1,2);
const double clhs24 =             C(0,3)*DN(1,0);
const double clhs25 =             C(3,3)*DN(1,1) + C(3,5)*DN(1,2) + clhs24;
const double clhs26 =             C(0,5)*DN(1,0);
const double clhs27 =             C(3,5)*DN(1,1) + C(5,5)*DN(1,2) + clhs26;
const double clhs28 =             DN(0,0)*DN(1,0);
const double clhs29 =             N[0]*clhs1;
const double clhs30 =             N[1]*clhs29;
const double clhs31 =             clhs28*mu + clhs30;
const double clhs32 =             DN(1,1)*clhs8;
const double clhs33 =             C(0,1)*DN(1,1) + C(0,4)*DN(1,2) + clhs24;
const double clhs34 =             C(1,3)*DN(1,1);
const double clhs35 =             C(3,3)*DN(1,0) + C(3,4)*DN(1,2) + clhs34;
const double clhs36 =             C(3,5)*DN(1,0);
const double clhs37 =             C(4,5)*DN(1,2);
const double clhs38 =             C(1,5)*DN(1,1) + clhs36 + clhs37;
const double clhs39 =             DN(1,2)*clhs8;
const double clhs40 =             C(0,2)*DN(1,2) + C(0,4)*DN(1,1) + clhs26;
const double clhs41 =             C(3,4)*DN(1,1);
const double clhs42 =             C(2,3)*DN(1,2) + clhs36 + clhs41;
const double clhs43 =             C(2,5)*DN(1,2);
const double clhs44 =             C(4,5)*DN(1,1) + C(5,5)*DN(1,0) + clhs43;
const double clhs45 =             DN(0,0)*N[1];
const double clhs46 =             C(0,0)*DN(2,0) + C(0,3)*DN(2,1) + C(0,5)*DN(2,2);
const double clhs47 =             C(0,3)*DN(2,0);
const double clhs48 =             C(3,3)*DN(2,1) + C(3,5)*DN(2,2) + clhs47;
const double clhs49 =             C(0,5)*DN(2,0);
const double clhs50 =             C(3,5)*DN(2,1) + C(5,5)*DN(2,2) + clhs49;
const double clhs51 =             DN(0,0)*DN(2,0);
const double clhs52 =             N[2]*clhs29;
const double clhs53 =             clhs51*mu + clhs52;
const double clhs54 =             DN(2,1)*clhs8;
const double clhs55 =             C(0,1)*DN(2,1) + C(0,4)*DN(2,2) + clhs47;
const double clhs56 =             C(1,3)*DN(2,1);
const double clhs57 =             C(3,3)*DN(2,0) + C(3,4)*DN(2,2) + clhs56;
const double clhs58 =             C(3,5)*DN(2,0);
const double clhs59 =             C(4,5)*DN(2,2);
const double clhs60 =             C(1,5)*DN(2,1) + clhs58 + clhs59;
const double clhs61 =             DN(2,2)*clhs8;
const double clhs62 =             C(0,2)*DN(2,2) + C(0,4)*DN(2,1) + clhs49;
const double clhs63 =             C(3,4)*DN(2,1);
const double clhs64 =             C(2,3)*DN(2,2) + clhs58 + clhs63;
const double clhs65 =             C(2,5)*DN(2,2);
const double clhs66 =             C(4,5)*DN(2,1) + C(5,5)*DN(2,0) + clhs65;
const double clhs67 =             DN(0,0)*N[2];
const double clhs68 =             C(0,0)*DN(3,0) + C(0,3)*DN(3,1) + C(0,5)*DN(3,2);
const double clhs69 =             C(0,3)*DN(3,0);
const double clhs70 =             C(3,3)*DN(3,1) + C(3,5)*DN(3,2) + clhs69;
const double clhs71 =             C(0,5)*DN(3,0);
const double clhs72 =             C(3,5)*DN(3,1) + C(5,5)*DN(3,2) + clhs71;
const double clhs73 =             DN(0,0)*DN(3,0);
const double clhs74 =             N[3]*clhs29;
const double clhs75 =             clhs73*mu + clhs74;
const double clhs76 =             DN(3,1)*clhs8;
const double clhs77 =             C(0,1)*DN(3,1) + C(0,4)*DN(3,2) + clhs69;
const double clhs78 =             C(1,3)*DN(3,1);
const double clhs79 =             C(3,3)*DN(3,0) + C(3,4)*DN(3,2) + clhs78;
const double clhs80 =             C(3,5)*DN(3,0);
const double clhs81 =             C(4,5)*DN(3,2);
const double clhs82 =             C(1,5)*DN(3,1) + clhs80 + clhs81;
const double clhs83 =             DN(3,2)*clhs8;
const double clhs84 =             C(0,2)*DN(3,2) + C(0,4)*DN(3,1) + clhs71;
const double clhs85 =             C(3,4)*DN(3,1);
const double clhs86 =             C(2,3)*DN(3,2) + clhs80 + clhs85;
const double clhs87 =             C(2,5)*DN(3,2);
const double clhs88 =             C(4,5)*DN(3,1) + C(5,5)*DN(3,0) + clhs87;
const double clhs89 =             DN(0,0)*N[3];
const double clhs90 =             C(0,1)*DN(0,0) + C(1,5)*DN(0,2) + clhs11;
const double clhs91 =             C(0,4)*DN(0,0) + clhs14 + clhs18;
const double clhs92 =             pow(DN(0,1), 2);
const double clhs93 =             C(1,1)*DN(0,1) + C(1,3)*DN(0,0) + C(1,4)*DN(0,2);
const double clhs94 =             C(1,4)*DN(0,1);
const double clhs95 =             C(3,4)*DN(0,0) + C(4,4)*DN(0,2) + clhs94;
const double clhs96 =             DN(0,1)*mu;
const double clhs97 =             DN(0,2)*clhs96;
const double clhs98 =             C(1,2)*DN(0,2) + C(1,5)*DN(0,0) + clhs94;
const double clhs99 =             C(2,4)*DN(0,2);
const double clhs100 =             C(4,4)*DN(0,1) + C(4,5)*DN(0,0) + clhs99;
const double clhs101 =             DN(0,1)*N[0];
const double clhs102 =             DN(1,0)*clhs96;
const double clhs103 =             C(0,1)*DN(1,0) + C(1,5)*DN(1,2) + clhs34;
const double clhs104 =             C(0,4)*DN(1,0) + clhs37 + clhs41;
const double clhs105 =             C(1,1)*DN(1,1) + C(1,3)*DN(1,0) + C(1,4)*DN(1,2);
const double clhs106 =             C(1,4)*DN(1,1);
const double clhs107 =             C(3,4)*DN(1,0) + C(4,4)*DN(1,2) + clhs106;
const double clhs108 =             DN(0,1)*DN(1,1);
const double clhs109 =             clhs108*mu + clhs30;
const double clhs110 =             DN(1,2)*clhs96;
const double clhs111 =             C(1,2)*DN(1,2) + C(1,5)*DN(1,0) + clhs106;
const double clhs112 =             C(2,4)*DN(1,2);
const double clhs113 =             C(4,4)*DN(1,1) + C(4,5)*DN(1,0) + clhs112;
const double clhs114 =             DN(0,1)*N[1];
const double clhs115 =             DN(2,0)*clhs96;
const double clhs116 =             C(0,1)*DN(2,0) + C(1,5)*DN(2,2) + clhs56;
const double clhs117 =             C(0,4)*DN(2,0) + clhs59 + clhs63;
const double clhs118 =             C(1,1)*DN(2,1) + C(1,3)*DN(2,0) + C(1,4)*DN(2,2);
const double clhs119 =             C(1,4)*DN(2,1);
const double clhs120 =             C(3,4)*DN(2,0) + C(4,4)*DN(2,2) + clhs119;
const double clhs121 =             DN(0,1)*DN(2,1);
const double clhs122 =             clhs121*mu + clhs52;
const double clhs123 =             DN(2,2)*clhs96;
const double clhs124 =             C(1,2)*DN(2,2) + C(1,5)*DN(2,0) + clhs119;
const double clhs125 =             C(2,4)*DN(2,2);
const double clhs126 =             C(4,4)*DN(2,1) + C(4,5)*DN(2,0) + clhs125;
const double clhs127 =             DN(0,1)*N[2];
const double clhs128 =             DN(3,0)*clhs96;
const double clhs129 =             C(0,1)*DN(3,0) + C(1,5)*DN(3,2) + clhs78;
const double clhs130 =             C(0,4)*DN(3,0) + clhs81 + clhs85;
const double clhs131 =             C(1,1)*DN(3,1) + C(1,3)*DN(3,0) + C(1,4)*DN(3,2);
const double clhs132 =             C(1,4)*DN(3,1);
const double clhs133 =             C(3,4)*DN(3,0) + C(4,4)*DN(3,2) + clhs132;
const double clhs134 =             DN(0,1)*DN(3,1);
const double clhs135 =             clhs134*mu + clhs74;
const double clhs136 =             DN(3,2)*clhs96;
const double clhs137 =             C(1,2)*DN(3,2) + C(1,5)*DN(3,0) + clhs132;
const double clhs138 =             C(2,4)*DN(3,2);
const double clhs139 =             C(4,4)*DN(3,1) + C(4,5)*DN(3,0) + clhs138;
const double clhs140 =             DN(0,1)*N[3];
const double clhs141 =             C(0,2)*DN(0,0) + C(2,3)*DN(0,1) + clhs20;
const double clhs142 =             C(1,2)*DN(0,1) + C(2,3)*DN(0,0) + clhs99;
const double clhs143 =             pow(DN(0,2), 2);
const double clhs144 =             C(2,2)*DN(0,2) + C(2,4)*DN(0,1) + C(2,5)*DN(0,0);
const double clhs145 =             DN(0,2)*N[0];
const double clhs146 =             DN(0,2)*mu;
const double clhs147 =             DN(1,0)*clhs146;
const double clhs148 =             C(0,2)*DN(1,0) + C(2,3)*DN(1,1) + clhs43;
const double clhs149 =             DN(1,1)*clhs146;
const double clhs150 =             C(1,2)*DN(1,1) + C(2,3)*DN(1,0) + clhs112;
const double clhs151 =             C(2,2)*DN(1,2) + C(2,4)*DN(1,1) + C(2,5)*DN(1,0);
const double clhs152 =             DN(0,2)*DN(1,2);
const double clhs153 =             clhs152*mu + clhs30;
const double clhs154 =             DN(0,2)*N[1];
const double clhs155 =             DN(2,0)*clhs146;
const double clhs156 =             C(0,2)*DN(2,0) + C(2,3)*DN(2,1) + clhs65;
const double clhs157 =             DN(2,1)*clhs146;
const double clhs158 =             C(1,2)*DN(2,1) + C(2,3)*DN(2,0) + clhs125;
const double clhs159 =             C(2,2)*DN(2,2) + C(2,4)*DN(2,1) + C(2,5)*DN(2,0);
const double clhs160 =             DN(0,2)*DN(2,2);
const double clhs161 =             clhs160*mu + clhs52;
const double clhs162 =             DN(0,2)*N[2];
const double clhs163 =             DN(3,0)*clhs146;
const double clhs164 =             C(0,2)*DN(3,0) + C(2,3)*DN(3,1) + clhs87;
const double clhs165 =             DN(3,1)*clhs146;
const double clhs166 =             C(1,2)*DN(3,1) + C(2,3)*DN(3,0) + clhs138;
const double clhs167 =             C(2,2)*DN(3,2) + C(2,4)*DN(3,1) + C(2,5)*DN(3,0);
const double clhs168 =             DN(0,2)*DN(3,2);
const double clhs169 =             clhs168*mu + clhs74;
const double clhs170 =             DN(0,2)*N[3];
const double clhs171 =             1.0/(dt_inv*dyn_tau*rho + mu*stab_c1/pow(h, 2));
const double clhs172 =             clhs1*clhs171;
const double clhs173 =             clhs172 + 1;
const double clhs174 =             DN(1,0)*N[0];
const double clhs175 =             DN(1,1)*N[0];
const double clhs176 =             DN(1,2)*N[0];
const double clhs177 =             clhs171*(clhs108 + clhs152 + clhs28);
const double clhs178 =             DN(2,0)*N[0];
const double clhs179 =             DN(2,1)*N[0];
const double clhs180 =             DN(2,2)*N[0];
const double clhs181 =             clhs171*(clhs121 + clhs160 + clhs51);
const double clhs182 =             DN(3,0)*N[0];
const double clhs183 =             DN(3,1)*N[0];
const double clhs184 =             DN(3,2)*N[0];
const double clhs185 =             clhs171*(clhs134 + clhs168 + clhs73);
const double clhs186 =             pow(DN(1,0), 2);
const double clhs187 =             pow(N[1], 2)*clhs1;
const double clhs188 =             DN(1,0)*mu;
const double clhs189 =             DN(1,1)*clhs188;
const double clhs190 =             DN(1,2)*clhs188;
const double clhs191 =             DN(1,0)*N[1];
const double clhs192 =             DN(1,0)*DN(2,0);
const double clhs193 =             N[1]*clhs1;
const double clhs194 =             N[2]*clhs193;
const double clhs195 =             clhs192*mu + clhs194;
const double clhs196 =             DN(2,1)*clhs188;
const double clhs197 =             DN(2,2)*clhs188;
const double clhs198 =             DN(1,0)*N[2];
const double clhs199 =             DN(1,0)*DN(3,0);
const double clhs200 =             N[3]*clhs193;
const double clhs201 =             clhs199*mu + clhs200;
const double clhs202 =             DN(3,1)*clhs188;
const double clhs203 =             DN(3,2)*clhs188;
const double clhs204 =             DN(1,0)*N[3];
const double clhs205 =             pow(DN(1,1), 2);
const double clhs206 =             DN(1,1)*mu;
const double clhs207 =             DN(1,2)*clhs206;
const double clhs208 =             DN(1,1)*N[1];
const double clhs209 =             DN(2,0)*clhs206;
const double clhs210 =             DN(1,1)*DN(2,1);
const double clhs211 =             clhs194 + clhs210*mu;
const double clhs212 =             DN(2,2)*clhs206;
const double clhs213 =             DN(1,1)*N[2];
const double clhs214 =             DN(3,0)*clhs206;
const double clhs215 =             DN(1,1)*DN(3,1);
const double clhs216 =             clhs200 + clhs215*mu;
const double clhs217 =             DN(3,2)*clhs206;
const double clhs218 =             DN(1,1)*N[3];
const double clhs219 =             pow(DN(1,2), 2);
const double clhs220 =             DN(1,2)*N[1];
const double clhs221 =             DN(1,2)*mu;
const double clhs222 =             DN(2,0)*clhs221;
const double clhs223 =             DN(2,1)*clhs221;
const double clhs224 =             DN(1,2)*DN(2,2);
const double clhs225 =             clhs194 + clhs224*mu;
const double clhs226 =             DN(1,2)*N[2];
const double clhs227 =             DN(3,0)*clhs221;
const double clhs228 =             DN(3,1)*clhs221;
const double clhs229 =             DN(1,2)*DN(3,2);
const double clhs230 =             clhs200 + clhs229*mu;
const double clhs231 =             DN(1,2)*N[3];
const double clhs232 =             DN(2,0)*N[1];
const double clhs233 =             DN(2,1)*N[1];
const double clhs234 =             DN(2,2)*N[1];
const double clhs235 =             clhs171*(clhs192 + clhs210 + clhs224);
const double clhs236 =             DN(3,0)*N[1];
const double clhs237 =             DN(3,1)*N[1];
const double clhs238 =             DN(3,2)*N[1];
const double clhs239 =             clhs171*(clhs199 + clhs215 + clhs229);
const double clhs240 =             pow(DN(2,0), 2);
const double clhs241 =             pow(N[2], 2)*clhs1;
const double clhs242 =             DN(2,0)*mu;
const double clhs243 =             DN(2,1)*clhs242;
const double clhs244 =             DN(2,2)*clhs242;
const double clhs245 =             DN(2,0)*N[2];
const double clhs246 =             DN(2,0)*DN(3,0);
const double clhs247 =             N[2]*N[3]*clhs1;
const double clhs248 =             clhs246*mu + clhs247;
const double clhs249 =             DN(3,1)*clhs242;
const double clhs250 =             DN(3,2)*clhs242;
const double clhs251 =             DN(2,0)*N[3];
const double clhs252 =             pow(DN(2,1), 2);
const double clhs253 =             DN(2,1)*mu;
const double clhs254 =             DN(2,2)*clhs253;
const double clhs255 =             DN(2,1)*N[2];
const double clhs256 =             DN(3,0)*clhs253;
const double clhs257 =             DN(2,1)*DN(3,1);
const double clhs258 =             clhs247 + clhs257*mu;
const double clhs259 =             DN(3,2)*clhs253;
const double clhs260 =             DN(2,1)*N[3];
const double clhs261 =             pow(DN(2,2), 2);
const double clhs262 =             DN(2,2)*N[2];
const double clhs263 =             DN(2,2)*mu;
const double clhs264 =             DN(3,0)*clhs263;
const double clhs265 =             DN(3,1)*clhs263;
const double clhs266 =             DN(2,2)*DN(3,2);
const double clhs267 =             clhs247 + clhs266*mu;
const double clhs268 =             DN(2,2)*N[3];
const double clhs269 =             DN(3,0)*N[2];
const double clhs270 =             DN(3,1)*N[2];
const double clhs271 =             DN(3,2)*N[2];
const double clhs272 =             clhs171*(clhs246 + clhs257 + clhs266);
const double clhs273 =             pow(DN(3,0), 2);
const double clhs274 =             pow(N[3], 2)*clhs1;
const double clhs275 =             DN(3,0)*mu;
const double clhs276 =             DN(3,1)*clhs275;
const double clhs277 =             DN(3,2)*clhs275;
const double clhs278 =             DN(3,0)*N[3];
const double clhs279 =             pow(DN(3,1), 2);
const double clhs280 =             DN(3,1)*DN(3,2)*mu;
const double clhs281 =             DN(3,1)*N[3];
const double clhs282 =             pow(DN(3,2), 2);
const double clhs283 =             DN(3,2)*N[3];
            lhs(0,0)=DN(0,0)*clhs3 + DN(0,1)*clhs5 + DN(0,2)*clhs7 + clhs0*mu + clhs2;
            lhs(0,1)=DN(0,0)*clhs10 + DN(0,1)*clhs12 + DN(0,2)*clhs15 + clhs9;
            lhs(0,2)=DN(0,0)*clhs17 + DN(0,1)*clhs19 + DN(0,2)*clhs21 + clhs16;
            lhs(0,3)=-clhs22;
            lhs(0,4)=DN(0,0)*clhs23 + DN(0,1)*clhs25 + DN(0,2)*clhs27 + clhs31;
            lhs(0,5)=DN(0,0)*clhs33 + DN(0,1)*clhs35 + DN(0,2)*clhs38 + clhs32;
            lhs(0,6)=DN(0,0)*clhs40 + DN(0,1)*clhs42 + DN(0,2)*clhs44 + clhs39;
            lhs(0,7)=-clhs45;
            lhs(0,8)=DN(0,0)*clhs46 + DN(0,1)*clhs48 + DN(0,2)*clhs50 + clhs53;
            lhs(0,9)=DN(0,0)*clhs55 + DN(0,1)*clhs57 + DN(0,2)*clhs60 + clhs54;
            lhs(0,10)=DN(0,0)*clhs62 + DN(0,1)*clhs64 + DN(0,2)*clhs66 + clhs61;
            lhs(0,11)=-clhs67;
            lhs(0,12)=DN(0,0)*clhs68 + DN(0,1)*clhs70 + DN(0,2)*clhs72 + clhs75;
            lhs(0,13)=DN(0,0)*clhs77 + DN(0,1)*clhs79 + DN(0,2)*clhs82 + clhs76;
            lhs(0,14)=DN(0,0)*clhs84 + DN(0,1)*clhs86 + DN(0,2)*clhs88 + clhs83;
            lhs(0,15)=-clhs89;
            lhs(1,0)=DN(0,0)*clhs5 + DN(0,1)*clhs90 + DN(0,2)*clhs91 + clhs9;
            lhs(1,1)=DN(0,0)*clhs12 + DN(0,1)*clhs93 + DN(0,2)*clhs95 + clhs2 + clhs92*mu;
            lhs(1,2)=DN(0,0)*clhs19 + DN(0,1)*clhs98 + DN(0,2)*clhs100 + clhs97;
            lhs(1,3)=-clhs101;
            lhs(1,4)=DN(0,0)*clhs25 + DN(0,1)*clhs103 + DN(0,2)*clhs104 + clhs102;
            lhs(1,5)=DN(0,0)*clhs35 + DN(0,1)*clhs105 + DN(0,2)*clhs107 + clhs109;
            lhs(1,6)=DN(0,0)*clhs42 + DN(0,1)*clhs111 + DN(0,2)*clhs113 + clhs110;
            lhs(1,7)=-clhs114;
            lhs(1,8)=DN(0,0)*clhs48 + DN(0,1)*clhs116 + DN(0,2)*clhs117 + clhs115;
            lhs(1,9)=DN(0,0)*clhs57 + DN(0,1)*clhs118 + DN(0,2)*clhs120 + clhs122;
            lhs(1,10)=DN(0,0)*clhs64 + DN(0,1)*clhs124 + DN(0,2)*clhs126 + clhs123;
            lhs(1,11)=-clhs127;
            lhs(1,12)=DN(0,0)*clhs70 + DN(0,1)*clhs129 + DN(0,2)*clhs130 + clhs128;
            lhs(1,13)=DN(0,0)*clhs79 + DN(0,1)*clhs131 + DN(0,2)*clhs133 + clhs135;
            lhs(1,14)=DN(0,0)*clhs86 + DN(0,1)*clhs137 + DN(0,2)*clhs139 + clhs136;
            lhs(1,15)=-clhs140;
            lhs(2,0)=DN(0,0)*clhs7 + DN(0,1)*clhs91 + DN(0,2)*clhs141 + clhs16;
            lhs(2,1)=DN(0,0)*clhs15 + DN(0,1)*clhs95 + DN(0,2)*clhs142 + clhs97;
            lhs(2,2)=DN(0,0)*clhs21 + DN(0,1)*clhs100 + DN(0,2)*clhs144 + clhs143*mu + clhs2;
            lhs(2,3)=-clhs145;
            lhs(2,4)=DN(0,0)*clhs27 + DN(0,1)*clhs104 + DN(0,2)*clhs148 + clhs147;
            lhs(2,5)=DN(0,0)*clhs38 + DN(0,1)*clhs107 + DN(0,2)*clhs150 + clhs149;
            lhs(2,6)=DN(0,0)*clhs44 + DN(0,1)*clhs113 + DN(0,2)*clhs151 + clhs153;
            lhs(2,7)=-clhs154;
            lhs(2,8)=DN(0,0)*clhs50 + DN(0,1)*clhs117 + DN(0,2)*clhs156 + clhs155;
            lhs(2,9)=DN(0,0)*clhs60 + DN(0,1)*clhs120 + DN(0,2)*clhs158 + clhs157;
            lhs(2,10)=DN(0,0)*clhs66 + DN(0,1)*clhs126 + DN(0,2)*clhs159 + clhs161;
            lhs(2,11)=-clhs162;
            lhs(2,12)=DN(0,0)*clhs72 + DN(0,1)*clhs130 + DN(0,2)*clhs164 + clhs163;
            lhs(2,13)=DN(0,0)*clhs82 + DN(0,1)*clhs133 + DN(0,2)*clhs166 + clhs165;
            lhs(2,14)=DN(0,0)*clhs88 + DN(0,1)*clhs139 + DN(0,2)*clhs167 + clhs169;
            lhs(2,15)=-clhs170;
            lhs(3,0)=clhs173*clhs22;
            lhs(3,1)=clhs101*clhs173;
            lhs(3,2)=clhs145*clhs173;
            lhs(3,3)=clhs171*(clhs0 + clhs143 + clhs92);
            lhs(3,4)=clhs172*clhs45 + clhs174;
            lhs(3,5)=clhs114*clhs172 + clhs175;
            lhs(3,6)=clhs154*clhs172 + clhs176;
            lhs(3,7)=clhs177;
            lhs(3,8)=clhs172*clhs67 + clhs178;
            lhs(3,9)=clhs127*clhs172 + clhs179;
            lhs(3,10)=clhs162*clhs172 + clhs180;
            lhs(3,11)=clhs181;
            lhs(3,12)=clhs172*clhs89 + clhs182;
            lhs(3,13)=clhs140*clhs172 + clhs183;
            lhs(3,14)=clhs170*clhs172 + clhs184;
            lhs(3,15)=clhs185;
            lhs(4,0)=DN(1,0)*clhs3 + DN(1,1)*clhs5 + DN(1,2)*clhs7 + clhs31;
            lhs(4,1)=DN(1,0)*clhs10 + DN(1,1)*clhs12 + DN(1,2)*clhs15 + clhs102;
            lhs(4,2)=DN(1,0)*clhs17 + DN(1,1)*clhs19 + DN(1,2)*clhs21 + clhs147;
            lhs(4,3)=-clhs174;
            lhs(4,4)=DN(1,0)*clhs23 + DN(1,1)*clhs25 + DN(1,2)*clhs27 + clhs186*mu + clhs187;
            lhs(4,5)=DN(1,0)*clhs33 + DN(1,1)*clhs35 + DN(1,2)*clhs38 + clhs189;
            lhs(4,6)=DN(1,0)*clhs40 + DN(1,1)*clhs42 + DN(1,2)*clhs44 + clhs190;
            lhs(4,7)=-clhs191;
            lhs(4,8)=DN(1,0)*clhs46 + DN(1,1)*clhs48 + DN(1,2)*clhs50 + clhs195;
            lhs(4,9)=DN(1,0)*clhs55 + DN(1,1)*clhs57 + DN(1,2)*clhs60 + clhs196;
            lhs(4,10)=DN(1,0)*clhs62 + DN(1,1)*clhs64 + DN(1,2)*clhs66 + clhs197;
            lhs(4,11)=-clhs198;
            lhs(4,12)=DN(1,0)*clhs68 + DN(1,1)*clhs70 + DN(1,2)*clhs72 + clhs201;
            lhs(4,13)=DN(1,0)*clhs77 + DN(1,1)*clhs79 + DN(1,2)*clhs82 + clhs202;
            lhs(4,14)=DN(1,0)*clhs84 + DN(1,1)*clhs86 + DN(1,2)*clhs88 + clhs203;
            lhs(4,15)=-clhs204;
            lhs(5,0)=DN(1,0)*clhs5 + DN(1,1)*clhs90 + DN(1,2)*clhs91 + clhs32;
            lhs(5,1)=DN(1,0)*clhs12 + DN(1,1)*clhs93 + DN(1,2)*clhs95 + clhs109;
            lhs(5,2)=DN(1,0)*clhs19 + DN(1,1)*clhs98 + DN(1,2)*clhs100 + clhs149;
            lhs(5,3)=-clhs175;
            lhs(5,4)=DN(1,0)*clhs25 + DN(1,1)*clhs103 + DN(1,2)*clhs104 + clhs189;
            lhs(5,5)=DN(1,0)*clhs35 + DN(1,1)*clhs105 + DN(1,2)*clhs107 + clhs187 + clhs205*mu;
            lhs(5,6)=DN(1,0)*clhs42 + DN(1,1)*clhs111 + DN(1,2)*clhs113 + clhs207;
            lhs(5,7)=-clhs208;
            lhs(5,8)=DN(1,0)*clhs48 + DN(1,1)*clhs116 + DN(1,2)*clhs117 + clhs209;
            lhs(5,9)=DN(1,0)*clhs57 + DN(1,1)*clhs118 + DN(1,2)*clhs120 + clhs211;
            lhs(5,10)=DN(1,0)*clhs64 + DN(1,1)*clhs124 + DN(1,2)*clhs126 + clhs212;
            lhs(5,11)=-clhs213;
            lhs(5,12)=DN(1,0)*clhs70 + DN(1,1)*clhs129 + DN(1,2)*clhs130 + clhs214;
            lhs(5,13)=DN(1,0)*clhs79 + DN(1,1)*clhs131 + DN(1,2)*clhs133 + clhs216;
            lhs(5,14)=DN(1,0)*clhs86 + DN(1,1)*clhs137 + DN(1,2)*clhs139 + clhs217;
            lhs(5,15)=-clhs218;
            lhs(6,0)=DN(1,0)*clhs7 + DN(1,1)*clhs91 + DN(1,2)*clhs141 + clhs39;
            lhs(6,1)=DN(1,0)*clhs15 + DN(1,1)*clhs95 + DN(1,2)*clhs142 + clhs110;
            lhs(6,2)=DN(1,0)*clhs21 + DN(1,1)*clhs100 + DN(1,2)*clhs144 + clhs153;
            lhs(6,3)=-clhs176;
            lhs(6,4)=DN(1,0)*clhs27 + DN(1,1)*clhs104 + DN(1,2)*clhs148 + clhs190;
            lhs(6,5)=DN(1,0)*clhs38 + DN(1,1)*clhs107 + DN(1,2)*clhs150 + clhs207;
            lhs(6,6)=DN(1,0)*clhs44 + DN(1,1)*clhs113 + DN(1,2)*clhs151 + clhs187 + clhs219*mu;
            lhs(6,7)=-clhs220;
            lhs(6,8)=DN(1,0)*clhs50 + DN(1,1)*clhs117 + DN(1,2)*clhs156 + clhs222;
            lhs(6,9)=DN(1,0)*clhs60 + DN(1,1)*clhs120 + DN(1,2)*clhs158 + clhs223;
            lhs(6,10)=DN(1,0)*clhs66 + DN(1,1)*clhs126 + DN(1,2)*clhs159 + clhs225;
            lhs(6,11)=-clhs226;
            lhs(6,12)=DN(1,0)*clhs72 + DN(1,1)*clhs130 + DN(1,2)*clhs164 + clhs227;
            lhs(6,13)=DN(1,0)*clhs82 + DN(1,1)*clhs133 + DN(1,2)*clhs166 + clhs228;
            lhs(6,14)=DN(1,0)*clhs88 + DN(1,1)*clhs139 + DN(1,2)*clhs167 + clhs230;
            lhs(6,15)=-clhs231;
            lhs(7,0)=clhs172*clhs174 + clhs45;
            lhs(7,1)=clhs114 + clhs172*clhs175;
            lhs(7,2)=clhs154 + clhs172*clhs176;
            lhs(7,3)=clhs177;
            lhs(7,4)=clhs173*clhs191;
            lhs(7,5)=clhs173*clhs208;
            lhs(7,6)=clhs173*clhs220;
            lhs(7,7)=clhs171*(clhs186 + clhs205 + clhs219);
            lhs(7,8)=clhs172*clhs198 + clhs232;
            lhs(7,9)=clhs172*clhs213 + clhs233;
            lhs(7,10)=clhs172*clhs226 + clhs234;
            lhs(7,11)=clhs235;
            lhs(7,12)=clhs172*clhs204 + clhs236;
            lhs(7,13)=clhs172*clhs218 + clhs237;
            lhs(7,14)=clhs172*clhs231 + clhs238;
            lhs(7,15)=clhs239;
            lhs(8,0)=DN(2,0)*clhs3 + DN(2,1)*clhs5 + DN(2,2)*clhs7 + clhs53;
            lhs(8,1)=DN(2,0)*clhs10 + DN(2,1)*clhs12 + DN(2,2)*clhs15 + clhs115;
            lhs(8,2)=DN(2,0)*clhs17 + DN(2,1)*clhs19 + DN(2,2)*clhs21 + clhs155;
            lhs(8,3)=-clhs178;
            lhs(8,4)=DN(2,0)*clhs23 + DN(2,1)*clhs25 + DN(2,2)*clhs27 + clhs195;
            lhs(8,5)=DN(2,0)*clhs33 + DN(2,1)*clhs35 + DN(2,2)*clhs38 + clhs209;
            lhs(8,6)=DN(2,0)*clhs40 + DN(2,1)*clhs42 + DN(2,2)*clhs44 + clhs222;
            lhs(8,7)=-clhs232;
            lhs(8,8)=DN(2,0)*clhs46 + DN(2,1)*clhs48 + DN(2,2)*clhs50 + clhs240*mu + clhs241;
            lhs(8,9)=DN(2,0)*clhs55 + DN(2,1)*clhs57 + DN(2,2)*clhs60 + clhs243;
            lhs(8,10)=DN(2,0)*clhs62 + DN(2,1)*clhs64 + DN(2,2)*clhs66 + clhs244;
            lhs(8,11)=-clhs245;
            lhs(8,12)=DN(2,0)*clhs68 + DN(2,1)*clhs70 + DN(2,2)*clhs72 + clhs248;
            lhs(8,13)=DN(2,0)*clhs77 + DN(2,1)*clhs79 + DN(2,2)*clhs82 + clhs249;
            lhs(8,14)=DN(2,0)*clhs84 + DN(2,1)*clhs86 + DN(2,2)*clhs88 + clhs250;
            lhs(8,15)=-clhs251;
            lhs(9,0)=DN(2,0)*clhs5 + DN(2,1)*clhs90 + DN(2,2)*clhs91 + clhs54;
            lhs(9,1)=DN(2,0)*clhs12 + DN(2,1)*clhs93 + DN(2,2)*clhs95 + clhs122;
            lhs(9,2)=DN(2,0)*clhs19 + DN(2,1)*clhs98 + DN(2,2)*clhs100 + clhs157;
            lhs(9,3)=-clhs179;
            lhs(9,4)=DN(2,0)*clhs25 + DN(2,1)*clhs103 + DN(2,2)*clhs104 + clhs196;
            lhs(9,5)=DN(2,0)*clhs35 + DN(2,1)*clhs105 + DN(2,2)*clhs107 + clhs211;
            lhs(9,6)=DN(2,0)*clhs42 + DN(2,1)*clhs111 + DN(2,2)*clhs113 + clhs223;
            lhs(9,7)=-clhs233;
            lhs(9,8)=DN(2,0)*clhs48 + DN(2,1)*clhs116 + DN(2,2)*clhs117 + clhs243;
            lhs(9,9)=DN(2,0)*clhs57 + DN(2,1)*clhs118 + DN(2,2)*clhs120 + clhs241 + clhs252*mu;
            lhs(9,10)=DN(2,0)*clhs64 + DN(2,1)*clhs124 + DN(2,2)*clhs126 + clhs254;
            lhs(9,11)=-clhs255;
            lhs(9,12)=DN(2,0)*clhs70 + DN(2,1)*clhs129 + DN(2,2)*clhs130 + clhs256;
            lhs(9,13)=DN(2,0)*clhs79 + DN(2,1)*clhs131 + DN(2,2)*clhs133 + clhs258;
            lhs(9,14)=DN(2,0)*clhs86 + DN(2,1)*clhs137 + DN(2,2)*clhs139 + clhs259;
            lhs(9,15)=-clhs260;
            lhs(10,0)=DN(2,0)*clhs7 + DN(2,1)*clhs91 + DN(2,2)*clhs141 + clhs61;
            lhs(10,1)=DN(2,0)*clhs15 + DN(2,1)*clhs95 + DN(2,2)*clhs142 + clhs123;
            lhs(10,2)=DN(2,0)*clhs21 + DN(2,1)*clhs100 + DN(2,2)*clhs144 + clhs161;
            lhs(10,3)=-clhs180;
            lhs(10,4)=DN(2,0)*clhs27 + DN(2,1)*clhs104 + DN(2,2)*clhs148 + clhs197;
            lhs(10,5)=DN(2,0)*clhs38 + DN(2,1)*clhs107 + DN(2,2)*clhs150 + clhs212;
            lhs(10,6)=DN(2,0)*clhs44 + DN(2,1)*clhs113 + DN(2,2)*clhs151 + clhs225;
            lhs(10,7)=-clhs234;
            lhs(10,8)=DN(2,0)*clhs50 + DN(2,1)*clhs117 + DN(2,2)*clhs156 + clhs244;
            lhs(10,9)=DN(2,0)*clhs60 + DN(2,1)*clhs120 + DN(2,2)*clhs158 + clhs254;
            lhs(10,10)=DN(2,0)*clhs66 + DN(2,1)*clhs126 + DN(2,2)*clhs159 + clhs241 + clhs261*mu;
            lhs(10,11)=-clhs262;
            lhs(10,12)=DN(2,0)*clhs72 + DN(2,1)*clhs130 + DN(2,2)*clhs164 + clhs264;
            lhs(10,13)=DN(2,0)*clhs82 + DN(2,1)*clhs133 + DN(2,2)*clhs166 + clhs265;
            lhs(10,14)=DN(2,0)*clhs88 + DN(2,1)*clhs139 + DN(2,2)*clhs167 + clhs267;
            lhs(10,15)=-clhs268;
            lhs(11,0)=clhs172*clhs178 + clhs67;
            lhs(11,1)=clhs127 + clhs172*clhs179;
            lhs(11,2)=clhs162 + clhs172*clhs180;
            lhs(11,3)=clhs181;
            lhs(11,4)=clhs172*clhs232 + clhs198;
            lhs(11,5)=clhs172*clhs233 + clhs213;
            lhs(11,6)=clhs172*clhs234 + clhs226;
            lhs(11,7)=clhs235;
            lhs(11,8)=clhs173*clhs245;
            lhs(11,9)=clhs173*clhs255;
            lhs(11,10)=clhs173*clhs262;
            lhs(11,11)=clhs171*(clhs240 + clhs252 + clhs261);
            lhs(11,12)=clhs172*clhs251 + clhs269;
            lhs(11,13)=clhs172*clhs260 + clhs270;
            lhs(11,14)=clhs172*clhs268 + clhs271;
            lhs(11,15)=clhs272;
            lhs(12,0)=DN(3,0)*clhs3 + DN(3,1)*clhs5 + DN(3,2)*clhs7 + clhs75;
            lhs(12,1)=DN(3,0)*clhs10 + DN(3,1)*clhs12 + DN(3,2)*clhs15 + clhs128;
            lhs(12,2)=DN(3,0)*clhs17 + DN(3,1)*clhs19 + DN(3,2)*clhs21 + clhs163;
            lhs(12,3)=-clhs182;
            lhs(12,4)=DN(3,0)*clhs23 + DN(3,1)*clhs25 + DN(3,2)*clhs27 + clhs201;
            lhs(12,5)=DN(3,0)*clhs33 + DN(3,1)*clhs35 + DN(3,2)*clhs38 + clhs214;
            lhs(12,6)=DN(3,0)*clhs40 + DN(3,1)*clhs42 + DN(3,2)*clhs44 + clhs227;
            lhs(12,7)=-clhs236;
            lhs(12,8)=DN(3,0)*clhs46 + DN(3,1)*clhs48 + DN(3,2)*clhs50 + clhs248;
            lhs(12,9)=DN(3,0)*clhs55 + DN(3,1)*clhs57 + DN(3,2)*clhs60 + clhs256;
            lhs(12,10)=DN(3,0)*clhs62 + DN(3,1)*clhs64 + DN(3,2)*clhs66 + clhs264;
            lhs(12,11)=-clhs269;
            lhs(12,12)=DN(3,0)*clhs68 + DN(3,1)*clhs70 + DN(3,2)*clhs72 + clhs273*mu + clhs274;
            lhs(12,13)=DN(3,0)*clhs77 + DN(3,1)*clhs79 + DN(3,2)*clhs82 + clhs276;
            lhs(12,14)=DN(3,0)*clhs84 + DN(3,1)*clhs86 + DN(3,2)*clhs88 + clhs277;
            lhs(12,15)=-clhs278;
            lhs(13,0)=DN(3,0)*clhs5 + DN(3,1)*clhs90 + DN(3,2)*clhs91 + clhs76;
            lhs(13,1)=DN(3,0)*clhs12 + DN(3,1)*clhs93 + DN(3,2)*clhs95 + clhs135;
            lhs(13,2)=DN(3,0)*clhs19 + DN(3,1)*clhs98 + DN(3,2)*clhs100 + clhs165;
            lhs(13,3)=-clhs183;
            lhs(13,4)=DN(3,0)*clhs25 + DN(3,1)*clhs103 + DN(3,2)*clhs104 + clhs202;
            lhs(13,5)=DN(3,0)*clhs35 + DN(3,1)*clhs105 + DN(3,2)*clhs107 + clhs216;
            lhs(13,6)=DN(3,0)*clhs42 + DN(3,1)*clhs111 + DN(3,2)*clhs113 + clhs228;
            lhs(13,7)=-clhs237;
            lhs(13,8)=DN(3,0)*clhs48 + DN(3,1)*clhs116 + DN(3,2)*clhs117 + clhs249;
            lhs(13,9)=DN(3,0)*clhs57 + DN(3,1)*clhs118 + DN(3,2)*clhs120 + clhs258;
            lhs(13,10)=DN(3,0)*clhs64 + DN(3,1)*clhs124 + DN(3,2)*clhs126 + clhs265;
            lhs(13,11)=-clhs270;
            lhs(13,12)=DN(3,0)*clhs70 + DN(3,1)*clhs129 + DN(3,2)*clhs130 + clhs276;
            lhs(13,13)=DN(3,0)*clhs79 + DN(3,1)*clhs131 + DN(3,2)*clhs133 + clhs274 + clhs279*mu;
            lhs(13,14)=DN(3,0)*clhs86 + DN(3,1)*clhs137 + DN(3,2)*clhs139 + clhs280;
            lhs(13,15)=-clhs281;
            lhs(14,0)=DN(3,0)*clhs7 + DN(3,1)*clhs91 + DN(3,2)*clhs141 + clhs83;
            lhs(14,1)=DN(3,0)*clhs15 + DN(3,1)*clhs95 + DN(3,2)*clhs142 + clhs136;
            lhs(14,2)=DN(3,0)*clhs21 + DN(3,1)*clhs100 + DN(3,2)*clhs144 + clhs169;
            lhs(14,3)=-clhs184;
            lhs(14,4)=DN(3,0)*clhs27 + DN(3,1)*clhs104 + DN(3,2)*clhs148 + clhs203;
            lhs(14,5)=DN(3,0)*clhs38 + DN(3,1)*clhs107 + DN(3,2)*clhs150 + clhs217;
            lhs(14,6)=DN(3,0)*clhs44 + DN(3,1)*clhs113 + DN(3,2)*clhs151 + clhs230;
            lhs(14,7)=-clhs238;
            lhs(14,8)=DN(3,0)*clhs50 + DN(3,1)*clhs117 + DN(3,2)*clhs156 + clhs250;
            lhs(14,9)=DN(3,0)*clhs60 + DN(3,1)*clhs120 + DN(3,2)*clhs158 + clhs259;
            lhs(14,10)=DN(3,0)*clhs66 + DN(3,1)*clhs126 + DN(3,2)*clhs159 + clhs267;
            lhs(14,11)=-clhs271;
            lhs(14,12)=DN(3,0)*clhs72 + DN(3,1)*clhs130 + DN(3,2)*clhs164 + clhs277;
            lhs(14,13)=DN(3,0)*clhs82 + DN(3,1)*clhs133 + DN(3,2)*clhs166 + clhs280;
            lhs(14,14)=DN(3,0)*clhs88 + DN(3,1)*clhs139 + DN(3,2)*clhs167 + clhs274 + clhs282*mu;
            lhs(14,15)=-clhs283;
            lhs(15,0)=clhs172*clhs182 + clhs89;
            lhs(15,1)=clhs140 + clhs172*clhs183;
            lhs(15,2)=clhs170 + clhs172*clhs184;
            lhs(15,3)=clhs185;
            lhs(15,4)=clhs172*clhs236 + clhs204;
            lhs(15,5)=clhs172*clhs237 + clhs218;
            lhs(15,6)=clhs172*clhs238 + clhs231;
            lhs(15,7)=clhs239;
            lhs(15,8)=clhs172*clhs269 + clhs251;
            lhs(15,9)=clhs172*clhs270 + clhs260;
            lhs(15,10)=clhs172*clhs271 + clhs268;
            lhs(15,11)=clhs272;
            lhs(15,12)=clhs173*clhs278;
            lhs(15,13)=clhs173*clhs281;
            lhs(15,14)=clhs173*clhs283;
            lhs(15,15)=clhs171*(clhs273 + clhs279 + clhs282);


    // Add intermediate results to local system
    noalias(rLHS) += lhs * rData.Weight;
}

template <>
void SymbolicStokes<SymbolicStokesData<3,6>>::ComputeGaussPointLHSContribution(
    SymbolicStokesData<3,6> &rData,
    MatrixType &rLHS)
{

    const double rho = rData.Density;
    const double mu = rData.EffectiveViscosity;

    const double h = rData.ElementSize;

    const double dt = rData.DeltaTime;
    const double bdf0 = rData.bdf0;

    const double dyn_tau = rData.DynamicTau;

    // Get constitutive matrix
    const Matrix &C = rData.C;

    // Get shape function values
    const auto &N = rData.N;
    const auto &DN = rData.DN_DX;

    // Stabilization parameters
    constexpr double stab_c1 = 4.0;
    constexpr double stab_c2 = 2.0;

    auto &lhs = rData.lhs;
    double dt_inv = 0.0;
    if (dt > 1e-09)
    {
        dt_inv = 1.0/dt;
    }
    if (std::abs(bdf0) < 1e-9)
    {
        dt_inv = 0.0;
    }

    const double clhs0 =             pow(DN(0,0), 2);
const double clhs1 =             bdf0*rho;
const double clhs2 =             pow(N[0], 2)*clhs1;
const double clhs3 =             C(0,0)*DN(0,0) + C(0,3)*DN(0,1) + C(0,5)*DN(0,2);
const double clhs4 =             C(0,3)*DN(0,0);
const double clhs5 =             C(3,3)*DN(0,1) + C(3,5)*DN(0,2) + clhs4;
const double clhs6 =             C(0,5)*DN(0,0);
const double clhs7 =             C(3,5)*DN(0,1) + C(5,5)*DN(0,2) + clhs6;
const double clhs8 =             DN(0,0)*mu;
const double clhs9 =             DN(0,1)*clhs8;
const double clhs10 =             C(0,1)*DN(0,1) + C(0,4)*DN(0,2) + clhs4;
const double clhs11 =             C(1,3)*DN(0,1);
const double clhs12 =             C(3,3)*DN(0,0) + C(3,4)*DN(0,2) + clhs11;
const double clhs13 =             C(3,5)*DN(0,0);
const double clhs14 =             C(4,5)*DN(0,2);
const double clhs15 =             C(1,5)*DN(0,1) + clhs13 + clhs14;
const double clhs16 =             DN(0,2)*clhs8;
const double clhs17 =             C(0,2)*DN(0,2) + C(0,4)*DN(0,1) + clhs6;
const double clhs18 =             C(3,4)*DN(0,1);
const double clhs19 =             C(2,3)*DN(0,2) + clhs13 + clhs18;
const double clhs20 =             C(2,5)*DN(0,2);
const double clhs21 =             C(4,5)*DN(0,1) + C(5,5)*DN(0,0) + clhs20;
const double clhs22 =             DN(0,0)*N[0];
const double clhs23 =             C(0,0)*DN(1,0) + C(0,3)*DN(1,1) + C(0,5)*DN(1,2);
const double clhs24 =             C(0,3)*DN(1,0);
const double clhs25 =             C(3,3)*DN(1,1) + C(3,5)*DN(1,2) + clhs24;
const double clhs26 =             C(0,5)*DN(1,0);
const double clhs27 =             C(3,5)*DN(1,1) + C(5,5)*DN(1,2) + clhs26;
const double clhs28 =             DN(0,0)*DN(1,0);
const double clhs29 =             N[0]*clhs1;
const double clhs30 =             N[1]*clhs29;
const double clhs31 =             clhs28*mu + clhs30;
const double clhs32 =             DN(1,1)*clhs8;
const double clhs33 =             C(0,1)*DN(1,1) + C(0,4)*DN(1,2) + clhs24;
const double clhs34 =             C(1,3)*DN(1,1);
const double clhs35 =             C(3,3)*DN(1,0) + C(3,4)*DN(1,2) + clhs34;
const double clhs36 =             C(3,5)*DN(1,0);
const double clhs37 =             C(4,5)*DN(1,2);
const double clhs38 =             C(1,5)*DN(1,1) + clhs36 + clhs37;
const double clhs39 =             DN(1,2)*clhs8;
const double clhs40 =             C(0,2)*DN(1,2) + C(0,4)*DN(1,1) + clhs26;
const double clhs41 =             C(3,4)*DN(1,1);
const double clhs42 =             C(2,3)*DN(1,2) + clhs36 + clhs41;
const double clhs43 =             C(2,5)*DN(1,2);
const double clhs44 =             C(4,5)*DN(1,1) + C(5,5)*DN(1,0) + clhs43;
const double clhs45 =             DN(0,0)*N[1];
const double clhs46 =             C(0,0)*DN(2,0) + C(0,3)*DN(2,1) + C(0,5)*DN(2,2);
const double clhs47 =             C(0,3)*DN(2,0);
const double clhs48 =             C(3,3)*DN(2,1) + C(3,5)*DN(2,2) + clhs47;
const double clhs49 =             C(0,5)*DN(2,0);
const double clhs50 =             C(3,5)*DN(2,1) + C(5,5)*DN(2,2) + clhs49;
const double clhs51 =             DN(0,0)*DN(2,0);
const double clhs52 =             N[2]*clhs29;
const double clhs53 =             clhs51*mu + clhs52;
const double clhs54 =             DN(2,1)*clhs8;
const double clhs55 =             C(0,1)*DN(2,1) + C(0,4)*DN(2,2) + clhs47;
const double clhs56 =             C(1,3)*DN(2,1);
const double clhs57 =             C(3,3)*DN(2,0) + C(3,4)*DN(2,2) + clhs56;
const double clhs58 =             C(3,5)*DN(2,0);
const double clhs59 =             C(4,5)*DN(2,2);
const double clhs60 =             C(1,5)*DN(2,1) + clhs58 + clhs59;
const double clhs61 =             DN(2,2)*clhs8;
const double clhs62 =             C(0,2)*DN(2,2) + C(0,4)*DN(2,1) + clhs49;
const double clhs63 =             C(3,4)*DN(2,1);
const double clhs64 =             C(2,3)*DN(2,2) + clhs58 + clhs63;
const double clhs65 =             C(2,5)*DN(2,2);
const double clhs66 =             C(4,5)*DN(2,1) + C(5,5)*DN(2,0) + clhs65;
const double clhs67 =             DN(0,0)*N[2];
const double clhs68 =             C(0,0)*DN(3,0) + C(0,3)*DN(3,1) + C(0,5)*DN(3,2);
const double clhs69 =             C(0,3)*DN(3,0);
const double clhs70 =             C(3,3)*DN(3,1) + C(3,5)*DN(3,2) + clhs69;
const double clhs71 =             C(0,5)*DN(3,0);
const double clhs72 =             C(3,5)*DN(3,1) + C(5,5)*DN(3,2) + clhs71;
const double clhs73 =             DN(0,0)*DN(3,0);
const double clhs74 =             N[3]*clhs29;
const double clhs75 =             clhs73*mu + clhs74;
const double clhs76 =             DN(3,1)*clhs8;
const double clhs77 =             C(0,1)*DN(3,1) + C(0,4)*DN(3,2) + clhs69;
const double clhs78 =             C(1,3)*DN(3,1);
const double clhs79 =             C(3,3)*DN(3,0) + C(3,4)*DN(3,2) + clhs78;
const double clhs80 =             C(3,5)*DN(3,0);
const double clhs81 =             C(4,5)*DN(3,2);
const double clhs82 =             C(1,5)*DN(3,1) + clhs80 + clhs81;
const double clhs83 =             DN(3,2)*clhs8;
const double clhs84 =             C(0,2)*DN(3,2) + C(0,4)*DN(3,1) + clhs71;
const double clhs85 =             C(3,4)*DN(3,1);
const double clhs86 =             C(2,3)*DN(3,2) + clhs80 + clhs85;
const double clhs87 =             C(2,5)*DN(3,2);
const double clhs88 =             C(4,5)*DN(3,1) + C(5,5)*DN(3,0) + clhs87;
const double clhs89 =             DN(0,0)*N[3];
const double clhs90 =             C(0,0)*DN(4,0) + C(0,3)*DN(4,1) + C(0,5)*DN(4,2);
const double clhs91 =             C(0,3)*DN(4,0);
const double clhs92 =             C(3,3)*DN(4,1) + C(3,5)*DN(4,2) + clhs91;
const double clhs93 =             C(0,5)*DN(4,0);
const double clhs94 =             C(3,5)*DN(4,1) + C(5,5)*DN(4,2) + clhs93;
const double clhs95 =             DN(0,0)*DN(4,0);
const double clhs96 =             N[4]*clhs29;
const double clhs97 =             clhs95*mu + clhs96;
const double clhs98 =             DN(4,1)*clhs8;
const double clhs99 =             C(0,1)*DN(4,1) + C(0,4)*DN(4,2) + clhs91;
const double clhs100 =             C(1,3)*DN(4,1);
const double clhs101 =             C(3,3)*DN(4,0) + C(3,4)*DN(4,2) + clhs100;
const double clhs102 =             C(3,5)*DN(4,0);
const double clhs103 =             C(4,5)*DN(4,2);
const double clhs104 =             C(1,5)*DN(4,1) + clhs102 + clhs103;
const double clhs105 =             DN(4,2)*clhs8;
const double clhs106 =             C(0,2)*DN(4,2) + C(0,4)*DN(4,1) + clhs93;
const double clhs107 =             C(3,4)*DN(4,1);
const double clhs108 =             C(2,3)*DN(4,2) + clhs102 + clhs107;
const double clhs109 =             C(2,5)*DN(4,2);
const double clhs110 =             C(4,5)*DN(4,1) + C(5,5)*DN(4,0) + clhs109;
const double clhs111 =             DN(0,0)*N[4];
const double clhs112 =             C(0,0)*DN(5,0) + C(0,3)*DN(5,1) + C(0,5)*DN(5,2);
const double clhs113 =             C(0,3)*DN(5,0);
const double clhs114 =             C(3,3)*DN(5,1) + C(3,5)*DN(5,2) + clhs113;
const double clhs115 =             C(0,5)*DN(5,0);
const double clhs116 =             C(3,5)*DN(5,1) + C(5,5)*DN(5,2) + clhs115;
const double clhs117 =             DN(0,0)*DN(5,0);
const double clhs118 =             N[5]*clhs29;
const double clhs119 =             clhs117*mu + clhs118;
const double clhs120 =             DN(5,1)*clhs8;
const double clhs121 =             C(0,1)*DN(5,1) + C(0,4)*DN(5,2) + clhs113;
const double clhs122 =             C(1,3)*DN(5,1);
const double clhs123 =             C(3,3)*DN(5,0) + C(3,4)*DN(5,2) + clhs122;
const double clhs124 =             C(3,5)*DN(5,0);
const double clhs125 =             C(4,5)*DN(5,2);
const double clhs126 =             C(1,5)*DN(5,1) + clhs124 + clhs125;
const double clhs127 =             DN(5,2)*clhs8;
const double clhs128 =             C(0,2)*DN(5,2) + C(0,4)*DN(5,1) + clhs115;
const double clhs129 =             C(3,4)*DN(5,1);
const double clhs130 =             C(2,3)*DN(5,2) + clhs124 + clhs129;
const double clhs131 =             C(2,5)*DN(5,2);
const double clhs132 =             C(4,5)*DN(5,1) + C(5,5)*DN(5,0) + clhs131;
const double clhs133 =             DN(0,0)*N[5];
const double clhs134 =             C(0,1)*DN(0,0) + C(1,5)*DN(0,2) + clhs11;
const double clhs135 =             C(0,4)*DN(0,0) + clhs14 + clhs18;
const double clhs136 =             pow(DN(0,1), 2);
const double clhs137 =             C(1,1)*DN(0,1) + C(1,3)*DN(0,0) + C(1,4)*DN(0,2);
const double clhs138 =             C(1,4)*DN(0,1);
const double clhs139 =             C(3,4)*DN(0,0) + C(4,4)*DN(0,2) + clhs138;
const double clhs140 =             DN(0,1)*mu;
const double clhs141 =             DN(0,2)*clhs140;
const double clhs142 =             C(1,2)*DN(0,2) + C(1,5)*DN(0,0) + clhs138;
const double clhs143 =             C(2,4)*DN(0,2);
const double clhs144 =             C(4,4)*DN(0,1) + C(4,5)*DN(0,0) + clhs143;
const double clhs145 =             DN(0,1)*N[0];
const double clhs146 =             DN(1,0)*clhs140;
const double clhs147 =             C(0,1)*DN(1,0) + C(1,5)*DN(1,2) + clhs34;
const double clhs148 =             C(0,4)*DN(1,0) + clhs37 + clhs41;
const double clhs149 =             C(1,1)*DN(1,1) + C(1,3)*DN(1,0) + C(1,4)*DN(1,2);
const double clhs150 =             C(1,4)*DN(1,1);
const double clhs151 =             C(3,4)*DN(1,0) + C(4,4)*DN(1,2) + clhs150;
const double clhs152 =             DN(0,1)*DN(1,1);
const double clhs153 =             clhs152*mu + clhs30;
const double clhs154 =             DN(1,2)*clhs140;
const double clhs155 =             C(1,2)*DN(1,2) + C(1,5)*DN(1,0) + clhs150;
const double clhs156 =             C(2,4)*DN(1,2);
const double clhs157 =             C(4,4)*DN(1,1) + C(4,5)*DN(1,0) + clhs156;
const double clhs158 =             DN(0,1)*N[1];
const double clhs159 =             DN(2,0)*clhs140;
const double clhs160 =             C(0,1)*DN(2,0) + C(1,5)*DN(2,2) + clhs56;
const double clhs161 =             C(0,4)*DN(2,0) + clhs59 + clhs63;
const double clhs162 =             C(1,1)*DN(2,1) + C(1,3)*DN(2,0) + C(1,4)*DN(2,2);
const double clhs163 =             C(1,4)*DN(2,1);
const double clhs164 =             C(3,4)*DN(2,0) + C(4,4)*DN(2,2) + clhs163;
const double clhs165 =             DN(0,1)*DN(2,1);
const double clhs166 =             clhs165*mu + clhs52;
const double clhs167 =             DN(2,2)*clhs140;
const double clhs168 =             C(1,2)*DN(2,2) + C(1,5)*DN(2,0) + clhs163;
const double clhs169 =             C(2,4)*DN(2,2);
const double clhs170 =             C(4,4)*DN(2,1) + C(4,5)*DN(2,0) + clhs169;
const double clhs171 =             DN(0,1)*N[2];
const double clhs172 =             DN(3,0)*clhs140;
const double clhs173 =             C(0,1)*DN(3,0) + C(1,5)*DN(3,2) + clhs78;
const double clhs174 =             C(0,4)*DN(3,0) + clhs81 + clhs85;
const double clhs175 =             C(1,1)*DN(3,1) + C(1,3)*DN(3,0) + C(1,4)*DN(3,2);
const double clhs176 =             C(1,4)*DN(3,1);
const double clhs177 =             C(3,4)*DN(3,0) + C(4,4)*DN(3,2) + clhs176;
const double clhs178 =             DN(0,1)*DN(3,1);
const double clhs179 =             clhs178*mu + clhs74;
const double clhs180 =             DN(3,2)*clhs140;
const double clhs181 =             C(1,2)*DN(3,2) + C(1,5)*DN(3,0) + clhs176;
const double clhs182 =             C(2,4)*DN(3,2);
const double clhs183 =             C(4,4)*DN(3,1) + C(4,5)*DN(3,0) + clhs182;
const double clhs184 =             DN(0,1)*N[3];
const double clhs185 =             DN(4,0)*clhs140;
const double clhs186 =             C(0,1)*DN(4,0) + C(1,5)*DN(4,2) + clhs100;
const double clhs187 =             C(0,4)*DN(4,0) + clhs103 + clhs107;
const double clhs188 =             C(1,1)*DN(4,1) + C(1,3)*DN(4,0) + C(1,4)*DN(4,2);
const double clhs189 =             C(1,4)*DN(4,1);
const double clhs190 =             C(3,4)*DN(4,0) + C(4,4)*DN(4,2) + clhs189;
const double clhs191 =             DN(0,1)*DN(4,1);
const double clhs192 =             clhs191*mu + clhs96;
const double clhs193 =             DN(4,2)*clhs140;
const double clhs194 =             C(1,2)*DN(4,2) + C(1,5)*DN(4,0) + clhs189;
const double clhs195 =             C(2,4)*DN(4,2);
const double clhs196 =             C(4,4)*DN(4,1) + C(4,5)*DN(4,0) + clhs195;
const double clhs197 =             DN(0,1)*N[4];
const double clhs198 =             DN(5,0)*clhs140;
const double clhs199 =             C(0,1)*DN(5,0) + C(1,5)*DN(5,2) + clhs122;
const double clhs200 =             C(0,4)*DN(5,0) + clhs125 + clhs129;
const double clhs201 =             C(1,1)*DN(5,1) + C(1,3)*DN(5,0) + C(1,4)*DN(5,2);
const double clhs202 =             C(1,4)*DN(5,1);
const double clhs203 =             C(3,4)*DN(5,0) + C(4,4)*DN(5,2) + clhs202;
const double clhs204 =             DN(0,1)*DN(5,1);
const double clhs205 =             clhs118 + clhs204*mu;
const double clhs206 =             DN(5,2)*clhs140;
const double clhs207 =             C(1,2)*DN(5,2) + C(1,5)*DN(5,0) + clhs202;
const double clhs208 =             C(2,4)*DN(5,2);
const double clhs209 =             C(4,4)*DN(5,1) + C(4,5)*DN(5,0) + clhs208;
const double clhs210 =             DN(0,1)*N[5];
const double clhs211 =             C(0,2)*DN(0,0) + C(2,3)*DN(0,1) + clhs20;
const double clhs212 =             C(1,2)*DN(0,1) + C(2,3)*DN(0,0) + clhs143;
const double clhs213 =             pow(DN(0,2), 2);
const double clhs214 =             C(2,2)*DN(0,2) + C(2,4)*DN(0,1) + C(2,5)*DN(0,0);
const double clhs215 =             DN(0,2)*N[0];
const double clhs216 =             DN(0,2)*mu;
const double clhs217 =             DN(1,0)*clhs216;
const double clhs218 =             C(0,2)*DN(1,0) + C(2,3)*DN(1,1) + clhs43;
const double clhs219 =             DN(1,1)*clhs216;
const double clhs220 =             C(1,2)*DN(1,1) + C(2,3)*DN(1,0) + clhs156;
const double clhs221 =             C(2,2)*DN(1,2) + C(2,4)*DN(1,1) + C(2,5)*DN(1,0);
const double clhs222 =             DN(0,2)*DN(1,2);
const double clhs223 =             clhs222*mu + clhs30;
const double clhs224 =             DN(0,2)*N[1];
const double clhs225 =             DN(2,0)*clhs216;
const double clhs226 =             C(0,2)*DN(2,0) + C(2,3)*DN(2,1) + clhs65;
const double clhs227 =             DN(2,1)*clhs216;
const double clhs228 =             C(1,2)*DN(2,1) + C(2,3)*DN(2,0) + clhs169;
const double clhs229 =             C(2,2)*DN(2,2) + C(2,4)*DN(2,1) + C(2,5)*DN(2,0);
const double clhs230 =             DN(0,2)*DN(2,2);
const double clhs231 =             clhs230*mu + clhs52;
const double clhs232 =             DN(0,2)*N[2];
const double clhs233 =             DN(3,0)*clhs216;
const double clhs234 =             C(0,2)*DN(3,0) + C(2,3)*DN(3,1) + clhs87;
const double clhs235 =             DN(3,1)*clhs216;
const double clhs236 =             C(1,2)*DN(3,1) + C(2,3)*DN(3,0) + clhs182;
const double clhs237 =             C(2,2)*DN(3,2) + C(2,4)*DN(3,1) + C(2,5)*DN(3,0);
const double clhs238 =             DN(0,2)*DN(3,2);
const double clhs239 =             clhs238*mu + clhs74;
const double clhs240 =             DN(0,2)*N[3];
const double clhs241 =             DN(4,0)*clhs216;
const double clhs242 =             C(0,2)*DN(4,0) + C(2,3)*DN(4,1) + clhs109;
const double clhs243 =             DN(4,1)*clhs216;
const double clhs244 =             C(1,2)*DN(4,1) + C(2,3)*DN(4,0) + clhs195;
const double clhs245 =             C(2,2)*DN(4,2) + C(2,4)*DN(4,1) + C(2,5)*DN(4,0);
const double clhs246 =             DN(0,2)*DN(4,2);
const double clhs247 =             clhs246*mu + clhs96;
const double clhs248 =             DN(0,2)*N[4];
const double clhs249 =             DN(5,0)*clhs216;
const double clhs250 =             C(0,2)*DN(5,0) + C(2,3)*DN(5,1) + clhs131;
const double clhs251 =             DN(5,1)*clhs216;
const double clhs252 =             C(1,2)*DN(5,1) + C(2,3)*DN(5,0) + clhs208;
const double clhs253 =             C(2,2)*DN(5,2) + C(2,4)*DN(5,1) + C(2,5)*DN(5,0);
const double clhs254 =             DN(0,2)*DN(5,2);
const double clhs255 =             clhs118 + clhs254*mu;
const double clhs256 =             DN(0,2)*N[5];
const double clhs257 =             1.0/(dt_inv*dyn_tau*rho + mu*stab_c1/pow(h, 2));
const double clhs258 =             clhs1*clhs257;
const double clhs259 =             clhs258 + 1;
const double clhs260 =             DN(1,0)*N[0];
const double clhs261 =             DN(1,1)*N[0];
const double clhs262 =             DN(1,2)*N[0];
const double clhs263 =             clhs257*(clhs152 + clhs222 + clhs28);
const double clhs264 =             DN(2,0)*N[0];
const double clhs265 =             DN(2,1)*N[0];
const double clhs266 =             DN(2,2)*N[0];
const double clhs267 =             clhs257*(clhs165 + clhs230 + clhs51);
const double clhs268 =             DN(3,0)*N[0];
const double clhs269 =             DN(3,1)*N[0];
const double clhs270 =             DN(3,2)*N[0];
const double clhs271 =             clhs257*(clhs178 + clhs238 + clhs73);
const double clhs272 =             DN(4,0)*N[0];
const double clhs273 =             DN(4,1)*N[0];
const double clhs274 =             DN(4,2)*N[0];
const double clhs275 =             clhs257*(clhs191 + clhs246 + clhs95);
const double clhs276 =             DN(5,0)*N[0];
const double clhs277 =             DN(5,1)*N[0];
const double clhs278 =             DN(5,2)*N[0];
const double clhs279 =             clhs257*(clhs117 + clhs204 + clhs254);
const double clhs280 =             pow(DN(1,0), 2);
const double clhs281 =             pow(N[1], 2)*clhs1;
const double clhs282 =             DN(1,0)*mu;
const double clhs283 =             DN(1,1)*clhs282;
const double clhs284 =             DN(1,2)*clhs282;
const double clhs285 =             DN(1,0)*N[1];
const double clhs286 =             DN(1,0)*DN(2,0);
const double clhs287 =             N[1]*clhs1;
const double clhs288 =             N[2]*clhs287;
const double clhs289 =             clhs286*mu + clhs288;
const double clhs290 =             DN(2,1)*clhs282;
const double clhs291 =             DN(2,2)*clhs282;
const double clhs292 =             DN(1,0)*N[2];
const double clhs293 =             DN(1,0)*DN(3,0);
const double clhs294 =             N[3]*clhs287;
const double clhs295 =             clhs293*mu + clhs294;
const double clhs296 =             DN(3,1)*clhs282;
const double clhs297 =             DN(3,2)*clhs282;
const double clhs298 =             DN(1,0)*N[3];
const double clhs299 =             DN(1,0)*DN(4,0);
const double clhs300 =             N[4]*clhs287;
const double clhs301 =             clhs299*mu + clhs300;
const double clhs302 =             DN(4,1)*clhs282;
const double clhs303 =             DN(4,2)*clhs282;
const double clhs304 =             DN(1,0)*N[4];
const double clhs305 =             DN(1,0)*DN(5,0);
const double clhs306 =             N[5]*clhs287;
const double clhs307 =             clhs305*mu + clhs306;
const double clhs308 =             DN(5,1)*clhs282;
const double clhs309 =             DN(5,2)*clhs282;
const double clhs310 =             DN(1,0)*N[5];
const double clhs311 =             pow(DN(1,1), 2);
const double clhs312 =             DN(1,1)*mu;
const double clhs313 =             DN(1,2)*clhs312;
const double clhs314 =             DN(1,1)*N[1];
const double clhs315 =             DN(2,0)*clhs312;
const double clhs316 =             DN(1,1)*DN(2,1);
const double clhs317 =             clhs288 + clhs316*mu;
const double clhs318 =             DN(2,2)*clhs312;
const double clhs319 =             DN(1,1)*N[2];
const double clhs320 =             DN(3,0)*clhs312;
const double clhs321 =             DN(1,1)*DN(3,1);
const double clhs322 =             clhs294 + clhs321*mu;
const double clhs323 =             DN(3,2)*clhs312;
const double clhs324 =             DN(1,1)*N[3];
const double clhs325 =             DN(4,0)*clhs312;
const double clhs326 =             DN(1,1)*DN(4,1);
const double clhs327 =             clhs300 + clhs326*mu;
const double clhs328 =             DN(4,2)*clhs312;
const double clhs329 =             DN(1,1)*N[4];
const double clhs330 =             DN(5,0)*clhs312;
const double clhs331 =             DN(1,1)*DN(5,1);
const double clhs332 =             clhs306 + clhs331*mu;
const double clhs333 =             DN(5,2)*clhs312;
const double clhs334 =             DN(1,1)*N[5];
const double clhs335 =             pow(DN(1,2), 2);
const double clhs336 =             DN(1,2)*N[1];
const double clhs337 =             DN(1,2)*mu;
const double clhs338 =             DN(2,0)*clhs337;
const double clhs339 =             DN(2,1)*clhs337;
const double clhs340 =             DN(1,2)*DN(2,2);
const double clhs341 =             clhs288 + clhs340*mu;
const double clhs342 =             DN(1,2)*N[2];
const double clhs343 =             DN(3,0)*clhs337;
const double clhs344 =             DN(3,1)*clhs337;
const double clhs345 =             DN(1,2)*DN(3,2);
const double clhs346 =             clhs294 + clhs345*mu;
const double clhs347 =             DN(1,2)*N[3];
const double clhs348 =             DN(4,0)*clhs337;
const double clhs349 =             DN(4,1)*clhs337;
const double clhs350 =             DN(1,2)*DN(4,2);
const double clhs351 =             clhs300 + clhs350*mu;
const double clhs352 =             DN(1,2)*N[4];
const double clhs353 =             DN(5,0)*clhs337;
const double clhs354 =             DN(5,1)*clhs337;
const double clhs355 =             DN(1,2)*DN(5,2);
const double clhs356 =             clhs306 + clhs355*mu;
const double clhs357 =             DN(1,2)*N[5];
const double clhs358 =             DN(2,0)*N[1];
const double clhs359 =             DN(2,1)*N[1];
const double clhs360 =             DN(2,2)*N[1];
const double clhs361 =             clhs257*(clhs286 + clhs316 + clhs340);
const double clhs362 =             DN(3,0)*N[1];
const double clhs363 =             DN(3,1)*N[1];
const double clhs364 =             DN(3,2)*N[1];
const double clhs365 =             clhs257*(clhs293 + clhs321 + clhs345);
const double clhs366 =             DN(4,0)*N[1];
const double clhs367 =             DN(4,1)*N[1];
const double clhs368 =             DN(4,2)*N[1];
const double clhs369 =             clhs257*(clhs299 + clhs326 + clhs350);
const double clhs370 =             DN(5,0)*N[1];
const double clhs371 =             DN(5,1)*N[1];
const double clhs372 =             DN(5,2)*N[1];
const double clhs373 =             clhs257*(clhs305 + clhs331 + clhs355);
const double clhs374 =             pow(DN(2,0), 2);
const double clhs375 =             pow(N[2], 2)*clhs1;
const double clhs376 =             DN(2,0)*mu;
const double clhs377 =             DN(2,1)*clhs376;
const double clhs378 =             DN(2,2)*clhs376;
const double clhs379 =             DN(2,0)*N[2];
const double clhs380 =             DN(2,0)*DN(3,0);
const double clhs381 =             N[2]*clhs1;
const double clhs382 =             N[3]*clhs381;
const double clhs383 =             clhs380*mu + clhs382;
const double clhs384 =             DN(3,1)*clhs376;
const double clhs385 =             DN(3,2)*clhs376;
const double clhs386 =             DN(2,0)*N[3];
const double clhs387 =             DN(2,0)*DN(4,0);
const double clhs388 =             N[4]*clhs381;
const double clhs389 =             clhs387*mu + clhs388;
const double clhs390 =             DN(4,1)*clhs376;
const double clhs391 =             DN(4,2)*clhs376;
const double clhs392 =             DN(2,0)*N[4];
const double clhs393 =             DN(2,0)*DN(5,0);
const double clhs394 =             N[5]*clhs381;
const double clhs395 =             clhs393*mu + clhs394;
const double clhs396 =             DN(5,1)*clhs376;
const double clhs397 =             DN(5,2)*clhs376;
const double clhs398 =             DN(2,0)*N[5];
const double clhs399 =             pow(DN(2,1), 2);
const double clhs400 =             DN(2,1)*mu;
const double clhs401 =             DN(2,2)*clhs400;
const double clhs402 =             DN(2,1)*N[2];
const double clhs403 =             DN(3,0)*clhs400;
const double clhs404 =             DN(2,1)*DN(3,1);
const double clhs405 =             clhs382 + clhs404*mu;
const double clhs406 =             DN(3,2)*clhs400;
const double clhs407 =             DN(2,1)*N[3];
const double clhs408 =             DN(4,0)*clhs400;
const double clhs409 =             DN(2,1)*DN(4,1);
const double clhs410 =             clhs388 + clhs409*mu;
const double clhs411 =             DN(4,2)*clhs400;
const double clhs412 =             DN(2,1)*N[4];
const double clhs413 =             DN(5,0)*clhs400;
const double clhs414 =             DN(2,1)*DN(5,1);
const double clhs415 =             clhs394 + clhs414*mu;
const double clhs416 =             DN(5,2)*clhs400;
const double clhs417 =             DN(2,1)*N[5];
const double clhs418 =             pow(DN(2,2), 2);
const double clhs419 =             DN(2,2)*N[2];
const double clhs420 =             DN(2,2)*mu;
const double clhs421 =             DN(3,0)*clhs420;
const double clhs422 =             DN(3,1)*clhs420;
const double clhs423 =             DN(2,2)*DN(3,2);
const double clhs424 =             clhs382 + clhs423*mu;
const double clhs425 =             DN(2,2)*N[3];
const double clhs426 =             DN(4,0)*clhs420;
const double clhs427 =             DN(4,1)*clhs420;
const double clhs428 =             DN(2,2)*DN(4,2);
const double clhs429 =             clhs388 + clhs428*mu;
const double clhs430 =             DN(2,2)*N[4];
const double clhs431 =             DN(5,0)*clhs420;
const double clhs432 =             DN(5,1)*clhs420;
const double clhs433 =             DN(2,2)*DN(5,2);
const double clhs434 =             clhs394 + clhs433*mu;
const double clhs435 =             DN(2,2)*N[5];
const double clhs436 =             DN(3,0)*N[2];
const double clhs437 =             DN(3,1)*N[2];
const double clhs438 =             DN(3,2)*N[2];
const double clhs439 =             clhs257*(clhs380 + clhs404 + clhs423);
const double clhs440 =             DN(4,0)*N[2];
const double clhs441 =             DN(4,1)*N[2];
const double clhs442 =             DN(4,2)*N[2];
const double clhs443 =             clhs257*(clhs387 + clhs409 + clhs428);
const double clhs444 =             DN(5,0)*N[2];
const double clhs445 =             DN(5,1)*N[2];
const double clhs446 =             DN(5,2)*N[2];
const double clhs447 =             clhs257*(clhs393 + clhs414 + clhs433);
const double clhs448 =             pow(DN(3,0), 2);
const double clhs449 =             pow(N[3], 2)*clhs1;
const double clhs450 =             DN(3,0)*mu;
const double clhs451 =             DN(3,1)*clhs450;
const double clhs452 =             DN(3,2)*clhs450;
const double clhs453 =             DN(3,0)*N[3];
const double clhs454 =             DN(3,0)*DN(4,0);
const double clhs455 =             N[3]*clhs1;
const double clhs456 =             N[4]*clhs455;
const double clhs457 =             clhs454*mu + clhs456;
const double clhs458 =             DN(4,1)*clhs450;
const double clhs459 =             DN(4,2)*clhs450;
const double clhs460 =             DN(3,0)*N[4];
const double clhs461 =             DN(3,0)*DN(5,0);
const double clhs462 =             N[5]*clhs455;
const double clhs463 =             clhs461*mu + clhs462;
const double clhs464 =             DN(5,1)*clhs450;
const double clhs465 =             DN(5,2)*clhs450;
const double clhs466 =             DN(3,0)*N[5];
const double clhs467 =             pow(DN(3,1), 2);
const double clhs468 =             DN(3,1)*mu;
const double clhs469 =             DN(3,2)*clhs468;
const double clhs470 =             DN(3,1)*N[3];
const double clhs471 =             DN(4,0)*clhs468;
const double clhs472 =             DN(3,1)*DN(4,1);
const double clhs473 =             clhs456 + clhs472*mu;
const double clhs474 =             DN(4,2)*clhs468;
const double clhs475 =             DN(3,1)*N[4];
const double clhs476 =             DN(5,0)*clhs468;
const double clhs477 =             DN(3,1)*DN(5,1);
const double clhs478 =             clhs462 + clhs477*mu;
const double clhs479 =             DN(5,2)*clhs468;
const double clhs480 =             DN(3,1)*N[5];
const double clhs481 =             pow(DN(3,2), 2);
const double clhs482 =             DN(3,2)*N[3];
const double clhs483 =             DN(3,2)*mu;
const double clhs484 =             DN(4,0)*clhs483;
const double clhs485 =             DN(4,1)*clhs483;
const double clhs486 =             DN(3,2)*DN(4,2);
const double clhs487 =             clhs456 + clhs486*mu;
const double clhs488 =             DN(3,2)*N[4];
const double clhs489 =             DN(5,0)*clhs483;
const double clhs490 =             DN(5,1)*clhs483;
const double clhs491 =             DN(3,2)*DN(5,2);
const double clhs492 =             clhs462 + clhs491*mu;
const double clhs493 =             DN(3,2)*N[5];
const double clhs494 =             DN(4,0)*N[3];
const double clhs495 =             DN(4,1)*N[3];
const double clhs496 =             DN(4,2)*N[3];
const double clhs497 =             clhs257*(clhs454 + clhs472 + clhs486);
const double clhs498 =             DN(5,0)*N[3];
const double clhs499 =             DN(5,1)*N[3];
const double clhs500 =             DN(5,2)*N[3];
const double clhs501 =             clhs257*(clhs461 + clhs477 + clhs491);
const double clhs502 =             pow(DN(4,0), 2);
const double clhs503 =             pow(N[4], 2)*clhs1;
const double clhs504 =             DN(4,0)*mu;
const double clhs505 =             DN(4,1)*clhs504;
const double clhs506 =             DN(4,2)*clhs504;
const double clhs507 =             DN(4,0)*N[4];
const double clhs508 =             DN(4,0)*DN(5,0);
const double clhs509 =             N[4]*N[5]*clhs1;
const double clhs510 =             clhs508*mu + clhs509;
const double clhs511 =             DN(5,1)*clhs504;
const double clhs512 =             DN(5,2)*clhs504;
const double clhs513 =             DN(4,0)*N[5];
const double clhs514 =             pow(DN(4,1), 2);
const double clhs515 =             DN(4,1)*mu;
const double clhs516 =             DN(4,2)*clhs515;
const double clhs517 =             DN(4,1)*N[4];
const double clhs518 =             DN(5,0)*clhs515;
const double clhs519 =             DN(4,1)*DN(5,1);
const double clhs520 =             clhs509 + clhs519*mu;
const double clhs521 =             DN(5,2)*clhs515;
const double clhs522 =             DN(4,1)*N[5];
const double clhs523 =             pow(DN(4,2), 2);
const double clhs524 =             DN(4,2)*N[4];
const double clhs525 =             DN(4,2)*mu;
const double clhs526 =             DN(5,0)*clhs525;
const double clhs527 =             DN(5,1)*clhs525;
const double clhs528 =             DN(4,2)*DN(5,2);
const double clhs529 =             clhs509 + clhs528*mu;
const double clhs530 =             DN(4,2)*N[5];
const double clhs531 =             DN(5,0)*N[4];
const double clhs532 =             DN(5,1)*N[4];
const double clhs533 =             DN(5,2)*N[4];
const double clhs534 =             clhs257*(clhs508 + clhs519 + clhs528);
const double clhs535 =             pow(DN(5,0), 2);
const double clhs536 =             pow(N[5], 2)*clhs1;
const double clhs537 =             DN(5,0)*mu;
const double clhs538 =             DN(5,1)*clhs537;
const double clhs539 =             DN(5,2)*clhs537;
const double clhs540 =             DN(5,0)*N[5];
const double clhs541 =             pow(DN(5,1), 2);
const double clhs542 =             DN(5,1)*DN(5,2)*mu;
const double clhs543 =             DN(5,1)*N[5];
const double clhs544 =             pow(DN(5,2), 2);
const double clhs545 =             DN(5,2)*N[5];
            lhs(0,0)=DN(0,0)*clhs3 + DN(0,1)*clhs5 + DN(0,2)*clhs7 + clhs0*mu + clhs2;
            lhs(0,1)=DN(0,0)*clhs10 + DN(0,1)*clhs12 + DN(0,2)*clhs15 + clhs9;
            lhs(0,2)=DN(0,0)*clhs17 + DN(0,1)*clhs19 + DN(0,2)*clhs21 + clhs16;
            lhs(0,3)=-clhs22;
            lhs(0,4)=DN(0,0)*clhs23 + DN(0,1)*clhs25 + DN(0,2)*clhs27 + clhs31;
            lhs(0,5)=DN(0,0)*clhs33 + DN(0,1)*clhs35 + DN(0,2)*clhs38 + clhs32;
            lhs(0,6)=DN(0,0)*clhs40 + DN(0,1)*clhs42 + DN(0,2)*clhs44 + clhs39;
            lhs(0,7)=-clhs45;
            lhs(0,8)=DN(0,0)*clhs46 + DN(0,1)*clhs48 + DN(0,2)*clhs50 + clhs53;
            lhs(0,9)=DN(0,0)*clhs55 + DN(0,1)*clhs57 + DN(0,2)*clhs60 + clhs54;
            lhs(0,10)=DN(0,0)*clhs62 + DN(0,1)*clhs64 + DN(0,2)*clhs66 + clhs61;
            lhs(0,11)=-clhs67;
            lhs(0,12)=DN(0,0)*clhs68 + DN(0,1)*clhs70 + DN(0,2)*clhs72 + clhs75;
            lhs(0,13)=DN(0,0)*clhs77 + DN(0,1)*clhs79 + DN(0,2)*clhs82 + clhs76;
            lhs(0,14)=DN(0,0)*clhs84 + DN(0,1)*clhs86 + DN(0,2)*clhs88 + clhs83;
            lhs(0,15)=-clhs89;
            lhs(0,16)=DN(0,0)*clhs90 + DN(0,1)*clhs92 + DN(0,2)*clhs94 + clhs97;
            lhs(0,17)=DN(0,0)*clhs99 + DN(0,1)*clhs101 + DN(0,2)*clhs104 + clhs98;
            lhs(0,18)=DN(0,0)*clhs106 + DN(0,1)*clhs108 + DN(0,2)*clhs110 + clhs105;
            lhs(0,19)=-clhs111;
            lhs(0,20)=DN(0,0)*clhs112 + DN(0,1)*clhs114 + DN(0,2)*clhs116 + clhs119;
            lhs(0,21)=DN(0,0)*clhs121 + DN(0,1)*clhs123 + DN(0,2)*clhs126 + clhs120;
            lhs(0,22)=DN(0,0)*clhs128 + DN(0,1)*clhs130 + DN(0,2)*clhs132 + clhs127;
            lhs(0,23)=-clhs133;
            lhs(1,0)=DN(0,0)*clhs5 + DN(0,1)*clhs134 + DN(0,2)*clhs135 + clhs9;
            lhs(1,1)=DN(0,0)*clhs12 + DN(0,1)*clhs137 + DN(0,2)*clhs139 + clhs136*mu + clhs2;
            lhs(1,2)=DN(0,0)*clhs19 + DN(0,1)*clhs142 + DN(0,2)*clhs144 + clhs141;
            lhs(1,3)=-clhs145;
            lhs(1,4)=DN(0,0)*clhs25 + DN(0,1)*clhs147 + DN(0,2)*clhs148 + clhs146;
            lhs(1,5)=DN(0,0)*clhs35 + DN(0,1)*clhs149 + DN(0,2)*clhs151 + clhs153;
            lhs(1,6)=DN(0,0)*clhs42 + DN(0,1)*clhs155 + DN(0,2)*clhs157 + clhs154;
            lhs(1,7)=-clhs158;
            lhs(1,8)=DN(0,0)*clhs48 + DN(0,1)*clhs160 + DN(0,2)*clhs161 + clhs159;
            lhs(1,9)=DN(0,0)*clhs57 + DN(0,1)*clhs162 + DN(0,2)*clhs164 + clhs166;
            lhs(1,10)=DN(0,0)*clhs64 + DN(0,1)*clhs168 + DN(0,2)*clhs170 + clhs167;
            lhs(1,11)=-clhs171;
            lhs(1,12)=DN(0,0)*clhs70 + DN(0,1)*clhs173 + DN(0,2)*clhs174 + clhs172;
            lhs(1,13)=DN(0,0)*clhs79 + DN(0,1)*clhs175 + DN(0,2)*clhs177 + clhs179;
            lhs(1,14)=DN(0,0)*clhs86 + DN(0,1)*clhs181 + DN(0,2)*clhs183 + clhs180;
            lhs(1,15)=-clhs184;
            lhs(1,16)=DN(0,0)*clhs92 + DN(0,1)*clhs186 + DN(0,2)*clhs187 + clhs185;
            lhs(1,17)=DN(0,0)*clhs101 + DN(0,1)*clhs188 + DN(0,2)*clhs190 + clhs192;
            lhs(1,18)=DN(0,0)*clhs108 + DN(0,1)*clhs194 + DN(0,2)*clhs196 + clhs193;
            lhs(1,19)=-clhs197;
            lhs(1,20)=DN(0,0)*clhs114 + DN(0,1)*clhs199 + DN(0,2)*clhs200 + clhs198;
            lhs(1,21)=DN(0,0)*clhs123 + DN(0,1)*clhs201 + DN(0,2)*clhs203 + clhs205;
            lhs(1,22)=DN(0,0)*clhs130 + DN(0,1)*clhs207 + DN(0,2)*clhs209 + clhs206;
            lhs(1,23)=-clhs210;
            lhs(2,0)=DN(0,0)*clhs7 + DN(0,1)*clhs135 + DN(0,2)*clhs211 + clhs16;
            lhs(2,1)=DN(0,0)*clhs15 + DN(0,1)*clhs139 + DN(0,2)*clhs212 + clhs141;
            lhs(2,2)=DN(0,0)*clhs21 + DN(0,1)*clhs144 + DN(0,2)*clhs214 + clhs2 + clhs213*mu;
            lhs(2,3)=-clhs215;
            lhs(2,4)=DN(0,0)*clhs27 + DN(0,1)*clhs148 + DN(0,2)*clhs218 + clhs217;
            lhs(2,5)=DN(0,0)*clhs38 + DN(0,1)*clhs151 + DN(0,2)*clhs220 + clhs219;
            lhs(2,6)=DN(0,0)*clhs44 + DN(0,1)*clhs157 + DN(0,2)*clhs221 + clhs223;
            lhs(2,7)=-clhs224;
            lhs(2,8)=DN(0,0)*clhs50 + DN(0,1)*clhs161 + DN(0,2)*clhs226 + clhs225;
            lhs(2,9)=DN(0,0)*clhs60 + DN(0,1)*clhs164 + DN(0,2)*clhs228 + clhs227;
            lhs(2,10)=DN(0,0)*clhs66 + DN(0,1)*clhs170 + DN(0,2)*clhs229 + clhs231;
            lhs(2,11)=-clhs232;
            lhs(2,12)=DN(0,0)*clhs72 + DN(0,1)*clhs174 + DN(0,2)*clhs234 + clhs233;
            lhs(2,13)=DN(0,0)*clhs82 + DN(0,1)*clhs177 + DN(0,2)*clhs236 + clhs235;
            lhs(2,14)=DN(0,0)*clhs88 + DN(0,1)*clhs183 + DN(0,2)*clhs237 + clhs239;
            lhs(2,15)=-clhs240;
            lhs(2,16)=DN(0,0)*clhs94 + DN(0,1)*clhs187 + DN(0,2)*clhs242 + clhs241;
            lhs(2,17)=DN(0,0)*clhs104 + DN(0,1)*clhs190 + DN(0,2)*clhs244 + clhs243;
            lhs(2,18)=DN(0,0)*clhs110 + DN(0,1)*clhs196 + DN(0,2)*clhs245 + clhs247;
            lhs(2,19)=-clhs248;
            lhs(2,20)=DN(0,0)*clhs116 + DN(0,1)*clhs200 + DN(0,2)*clhs250 + clhs249;
            lhs(2,21)=DN(0,0)*clhs126 + DN(0,1)*clhs203 + DN(0,2)*clhs252 + clhs251;
            lhs(2,22)=DN(0,0)*clhs132 + DN(0,1)*clhs209 + DN(0,2)*clhs253 + clhs255;
            lhs(2,23)=-clhs256;
            lhs(3,0)=clhs22*clhs259;
            lhs(3,1)=clhs145*clhs259;
            lhs(3,2)=clhs215*clhs259;
            lhs(3,3)=clhs257*(clhs0 + clhs136 + clhs213);
            lhs(3,4)=clhs258*clhs45 + clhs260;
            lhs(3,5)=clhs158*clhs258 + clhs261;
            lhs(3,6)=clhs224*clhs258 + clhs262;
            lhs(3,7)=clhs263;
            lhs(3,8)=clhs258*clhs67 + clhs264;
            lhs(3,9)=clhs171*clhs258 + clhs265;
            lhs(3,10)=clhs232*clhs258 + clhs266;
            lhs(3,11)=clhs267;
            lhs(3,12)=clhs258*clhs89 + clhs268;
            lhs(3,13)=clhs184*clhs258 + clhs269;
            lhs(3,14)=clhs240*clhs258 + clhs270;
            lhs(3,15)=clhs271;
            lhs(3,16)=clhs111*clhs258 + clhs272;
            lhs(3,17)=clhs197*clhs258 + clhs273;
            lhs(3,18)=clhs248*clhs258 + clhs274;
            lhs(3,19)=clhs275;
            lhs(3,20)=clhs133*clhs258 + clhs276;
            lhs(3,21)=clhs210*clhs258 + clhs277;
            lhs(3,22)=clhs256*clhs258 + clhs278;
            lhs(3,23)=clhs279;
            lhs(4,0)=DN(1,0)*clhs3 + DN(1,1)*clhs5 + DN(1,2)*clhs7 + clhs31;
            lhs(4,1)=DN(1,0)*clhs10 + DN(1,1)*clhs12 + DN(1,2)*clhs15 + clhs146;
            lhs(4,2)=DN(1,0)*clhs17 + DN(1,1)*clhs19 + DN(1,2)*clhs21 + clhs217;
            lhs(4,3)=-clhs260;
            lhs(4,4)=DN(1,0)*clhs23 + DN(1,1)*clhs25 + DN(1,2)*clhs27 + clhs280*mu + clhs281;
            lhs(4,5)=DN(1,0)*clhs33 + DN(1,1)*clhs35 + DN(1,2)*clhs38 + clhs283;
            lhs(4,6)=DN(1,0)*clhs40 + DN(1,1)*clhs42 + DN(1,2)*clhs44 + clhs284;
            lhs(4,7)=-clhs285;
            lhs(4,8)=DN(1,0)*clhs46 + DN(1,1)*clhs48 + DN(1,2)*clhs50 + clhs289;
            lhs(4,9)=DN(1,0)*clhs55 + DN(1,1)*clhs57 + DN(1,2)*clhs60 + clhs290;
            lhs(4,10)=DN(1,0)*clhs62 + DN(1,1)*clhs64 + DN(1,2)*clhs66 + clhs291;
            lhs(4,11)=-clhs292;
            lhs(4,12)=DN(1,0)*clhs68 + DN(1,1)*clhs70 + DN(1,2)*clhs72 + clhs295;
            lhs(4,13)=DN(1,0)*clhs77 + DN(1,1)*clhs79 + DN(1,2)*clhs82 + clhs296;
            lhs(4,14)=DN(1,0)*clhs84 + DN(1,1)*clhs86 + DN(1,2)*clhs88 + clhs297;
            lhs(4,15)=-clhs298;
            lhs(4,16)=DN(1,0)*clhs90 + DN(1,1)*clhs92 + DN(1,2)*clhs94 + clhs301;
            lhs(4,17)=DN(1,0)*clhs99 + DN(1,1)*clhs101 + DN(1,2)*clhs104 + clhs302;
            lhs(4,18)=DN(1,0)*clhs106 + DN(1,1)*clhs108 + DN(1,2)*clhs110 + clhs303;
            lhs(4,19)=-clhs304;
            lhs(4,20)=DN(1,0)*clhs112 + DN(1,1)*clhs114 + DN(1,2)*clhs116 + clhs307;
            lhs(4,21)=DN(1,0)*clhs121 + DN(1,1)*clhs123 + DN(1,2)*clhs126 + clhs308;
            lhs(4,22)=DN(1,0)*clhs128 + DN(1,1)*clhs130 + DN(1,2)*clhs132 + clhs309;
            lhs(4,23)=-clhs310;
            lhs(5,0)=DN(1,0)*clhs5 + DN(1,1)*clhs134 + DN(1,2)*clhs135 + clhs32;
            lhs(5,1)=DN(1,0)*clhs12 + DN(1,1)*clhs137 + DN(1,2)*clhs139 + clhs153;
            lhs(5,2)=DN(1,0)*clhs19 + DN(1,1)*clhs142 + DN(1,2)*clhs144 + clhs219;
            lhs(5,3)=-clhs261;
            lhs(5,4)=DN(1,0)*clhs25 + DN(1,1)*clhs147 + DN(1,2)*clhs148 + clhs283;
            lhs(5,5)=DN(1,0)*clhs35 + DN(1,1)*clhs149 + DN(1,2)*clhs151 + clhs281 + clhs311*mu;
            lhs(5,6)=DN(1,0)*clhs42 + DN(1,1)*clhs155 + DN(1,2)*clhs157 + clhs313;
            lhs(5,7)=-clhs314;
            lhs(5,8)=DN(1,0)*clhs48 + DN(1,1)*clhs160 + DN(1,2)*clhs161 + clhs315;
            lhs(5,9)=DN(1,0)*clhs57 + DN(1,1)*clhs162 + DN(1,2)*clhs164 + clhs317;
            lhs(5,10)=DN(1,0)*clhs64 + DN(1,1)*clhs168 + DN(1,2)*clhs170 + clhs318;
            lhs(5,11)=-clhs319;
            lhs(5,12)=DN(1,0)*clhs70 + DN(1,1)*clhs173 + DN(1,2)*clhs174 + clhs320;
            lhs(5,13)=DN(1,0)*clhs79 + DN(1,1)*clhs175 + DN(1,2)*clhs177 + clhs322;
            lhs(5,14)=DN(1,0)*clhs86 + DN(1,1)*clhs181 + DN(1,2)*clhs183 + clhs323;
            lhs(5,15)=-clhs324;
            lhs(5,16)=DN(1,0)*clhs92 + DN(1,1)*clhs186 + DN(1,2)*clhs187 + clhs325;
            lhs(5,17)=DN(1,0)*clhs101 + DN(1,1)*clhs188 + DN(1,2)*clhs190 + clhs327;
            lhs(5,18)=DN(1,0)*clhs108 + DN(1,1)*clhs194 + DN(1,2)*clhs196 + clhs328;
            lhs(5,19)=-clhs329;
            lhs(5,20)=DN(1,0)*clhs114 + DN(1,1)*clhs199 + DN(1,2)*clhs200 + clhs330;
            lhs(5,21)=DN(1,0)*clhs123 + DN(1,1)*clhs201 + DN(1,2)*clhs203 + clhs332;
            lhs(5,22)=DN(1,0)*clhs130 + DN(1,1)*clhs207 + DN(1,2)*clhs209 + clhs333;
            lhs(5,23)=-clhs334;
            lhs(6,0)=DN(1,0)*clhs7 + DN(1,1)*clhs135 + DN(1,2)*clhs211 + clhs39;
            lhs(6,1)=DN(1,0)*clhs15 + DN(1,1)*clhs139 + DN(1,2)*clhs212 + clhs154;
            lhs(6,2)=DN(1,0)*clhs21 + DN(1,1)*clhs144 + DN(1,2)*clhs214 + clhs223;
            lhs(6,3)=-clhs262;
            lhs(6,4)=DN(1,0)*clhs27 + DN(1,1)*clhs148 + DN(1,2)*clhs218 + clhs284;
            lhs(6,5)=DN(1,0)*clhs38 + DN(1,1)*clhs151 + DN(1,2)*clhs220 + clhs313;
            lhs(6,6)=DN(1,0)*clhs44 + DN(1,1)*clhs157 + DN(1,2)*clhs221 + clhs281 + clhs335*mu;
            lhs(6,7)=-clhs336;
            lhs(6,8)=DN(1,0)*clhs50 + DN(1,1)*clhs161 + DN(1,2)*clhs226 + clhs338;
            lhs(6,9)=DN(1,0)*clhs60 + DN(1,1)*clhs164 + DN(1,2)*clhs228 + clhs339;
            lhs(6,10)=DN(1,0)*clhs66 + DN(1,1)*clhs170 + DN(1,2)*clhs229 + clhs341;
            lhs(6,11)=-clhs342;
            lhs(6,12)=DN(1,0)*clhs72 + DN(1,1)*clhs174 + DN(1,2)*clhs234 + clhs343;
            lhs(6,13)=DN(1,0)*clhs82 + DN(1,1)*clhs177 + DN(1,2)*clhs236 + clhs344;
            lhs(6,14)=DN(1,0)*clhs88 + DN(1,1)*clhs183 + DN(1,2)*clhs237 + clhs346;
            lhs(6,15)=-clhs347;
            lhs(6,16)=DN(1,0)*clhs94 + DN(1,1)*clhs187 + DN(1,2)*clhs242 + clhs348;
            lhs(6,17)=DN(1,0)*clhs104 + DN(1,1)*clhs190 + DN(1,2)*clhs244 + clhs349;
            lhs(6,18)=DN(1,0)*clhs110 + DN(1,1)*clhs196 + DN(1,2)*clhs245 + clhs351;
            lhs(6,19)=-clhs352;
            lhs(6,20)=DN(1,0)*clhs116 + DN(1,1)*clhs200 + DN(1,2)*clhs250 + clhs353;
            lhs(6,21)=DN(1,0)*clhs126 + DN(1,1)*clhs203 + DN(1,2)*clhs252 + clhs354;
            lhs(6,22)=DN(1,0)*clhs132 + DN(1,1)*clhs209 + DN(1,2)*clhs253 + clhs356;
            lhs(6,23)=-clhs357;
            lhs(7,0)=clhs258*clhs260 + clhs45;
            lhs(7,1)=clhs158 + clhs258*clhs261;
            lhs(7,2)=clhs224 + clhs258*clhs262;
            lhs(7,3)=clhs263;
            lhs(7,4)=clhs259*clhs285;
            lhs(7,5)=clhs259*clhs314;
            lhs(7,6)=clhs259*clhs336;
            lhs(7,7)=clhs257*(clhs280 + clhs311 + clhs335);
            lhs(7,8)=clhs258*clhs292 + clhs358;
            lhs(7,9)=clhs258*clhs319 + clhs359;
            lhs(7,10)=clhs258*clhs342 + clhs360;
            lhs(7,11)=clhs361;
            lhs(7,12)=clhs258*clhs298 + clhs362;
            lhs(7,13)=clhs258*clhs324 + clhs363;
            lhs(7,14)=clhs258*clhs347 + clhs364;
            lhs(7,15)=clhs365;
            lhs(7,16)=clhs258*clhs304 + clhs366;
            lhs(7,17)=clhs258*clhs329 + clhs367;
            lhs(7,18)=clhs258*clhs352 + clhs368;
            lhs(7,19)=clhs369;
            lhs(7,20)=clhs258*clhs310 + clhs370;
            lhs(7,21)=clhs258*clhs334 + clhs371;
            lhs(7,22)=clhs258*clhs357 + clhs372;
            lhs(7,23)=clhs373;
            lhs(8,0)=DN(2,0)*clhs3 + DN(2,1)*clhs5 + DN(2,2)*clhs7 + clhs53;
            lhs(8,1)=DN(2,0)*clhs10 + DN(2,1)*clhs12 + DN(2,2)*clhs15 + clhs159;
            lhs(8,2)=DN(2,0)*clhs17 + DN(2,1)*clhs19 + DN(2,2)*clhs21 + clhs225;
            lhs(8,3)=-clhs264;
            lhs(8,4)=DN(2,0)*clhs23 + DN(2,1)*clhs25 + DN(2,2)*clhs27 + clhs289;
            lhs(8,5)=DN(2,0)*clhs33 + DN(2,1)*clhs35 + DN(2,2)*clhs38 + clhs315;
            lhs(8,6)=DN(2,0)*clhs40 + DN(2,1)*clhs42 + DN(2,2)*clhs44 + clhs338;
            lhs(8,7)=-clhs358;
            lhs(8,8)=DN(2,0)*clhs46 + DN(2,1)*clhs48 + DN(2,2)*clhs50 + clhs374*mu + clhs375;
            lhs(8,9)=DN(2,0)*clhs55 + DN(2,1)*clhs57 + DN(2,2)*clhs60 + clhs377;
            lhs(8,10)=DN(2,0)*clhs62 + DN(2,1)*clhs64 + DN(2,2)*clhs66 + clhs378;
            lhs(8,11)=-clhs379;
            lhs(8,12)=DN(2,0)*clhs68 + DN(2,1)*clhs70 + DN(2,2)*clhs72 + clhs383;
            lhs(8,13)=DN(2,0)*clhs77 + DN(2,1)*clhs79 + DN(2,2)*clhs82 + clhs384;
            lhs(8,14)=DN(2,0)*clhs84 + DN(2,1)*clhs86 + DN(2,2)*clhs88 + clhs385;
            lhs(8,15)=-clhs386;
            lhs(8,16)=DN(2,0)*clhs90 + DN(2,1)*clhs92 + DN(2,2)*clhs94 + clhs389;
            lhs(8,17)=DN(2,0)*clhs99 + DN(2,1)*clhs101 + DN(2,2)*clhs104 + clhs390;
            lhs(8,18)=DN(2,0)*clhs106 + DN(2,1)*clhs108 + DN(2,2)*clhs110 + clhs391;
            lhs(8,19)=-clhs392;
            lhs(8,20)=DN(2,0)*clhs112 + DN(2,1)*clhs114 + DN(2,2)*clhs116 + clhs395;
            lhs(8,21)=DN(2,0)*clhs121 + DN(2,1)*clhs123 + DN(2,2)*clhs126 + clhs396;
            lhs(8,22)=DN(2,0)*clhs128 + DN(2,1)*clhs130 + DN(2,2)*clhs132 + clhs397;
            lhs(8,23)=-clhs398;
            lhs(9,0)=DN(2,0)*clhs5 + DN(2,1)*clhs134 + DN(2,2)*clhs135 + clhs54;
            lhs(9,1)=DN(2,0)*clhs12 + DN(2,1)*clhs137 + DN(2,2)*clhs139 + clhs166;
            lhs(9,2)=DN(2,0)*clhs19 + DN(2,1)*clhs142 + DN(2,2)*clhs144 + clhs227;
            lhs(9,3)=-clhs265;
            lhs(9,4)=DN(2,0)*clhs25 + DN(2,1)*clhs147 + DN(2,2)*clhs148 + clhs290;
            lhs(9,5)=DN(2,0)*clhs35 + DN(2,1)*clhs149 + DN(2,2)*clhs151 + clhs317;
            lhs(9,6)=DN(2,0)*clhs42 + DN(2,1)*clhs155 + DN(2,2)*clhs157 + clhs339;
            lhs(9,7)=-clhs359;
            lhs(9,8)=DN(2,0)*clhs48 + DN(2,1)*clhs160 + DN(2,2)*clhs161 + clhs377;
            lhs(9,9)=DN(2,0)*clhs57 + DN(2,1)*clhs162 + DN(2,2)*clhs164 + clhs375 + clhs399*mu;
            lhs(9,10)=DN(2,0)*clhs64 + DN(2,1)*clhs168 + DN(2,2)*clhs170 + clhs401;
            lhs(9,11)=-clhs402;
            lhs(9,12)=DN(2,0)*clhs70 + DN(2,1)*clhs173 + DN(2,2)*clhs174 + clhs403;
            lhs(9,13)=DN(2,0)*clhs79 + DN(2,1)*clhs175 + DN(2,2)*clhs177 + clhs405;
            lhs(9,14)=DN(2,0)*clhs86 + DN(2,1)*clhs181 + DN(2,2)*clhs183 + clhs406;
            lhs(9,15)=-clhs407;
            lhs(9,16)=DN(2,0)*clhs92 + DN(2,1)*clhs186 + DN(2,2)*clhs187 + clhs408;
            lhs(9,17)=DN(2,0)*clhs101 + DN(2,1)*clhs188 + DN(2,2)*clhs190 + clhs410;
            lhs(9,18)=DN(2,0)*clhs108 + DN(2,1)*clhs194 + DN(2,2)*clhs196 + clhs411;
            lhs(9,19)=-clhs412;
            lhs(9,20)=DN(2,0)*clhs114 + DN(2,1)*clhs199 + DN(2,2)*clhs200 + clhs413;
            lhs(9,21)=DN(2,0)*clhs123 + DN(2,1)*clhs201 + DN(2,2)*clhs203 + clhs415;
            lhs(9,22)=DN(2,0)*clhs130 + DN(2,1)*clhs207 + DN(2,2)*clhs209 + clhs416;
            lhs(9,23)=-clhs417;
            lhs(10,0)=DN(2,0)*clhs7 + DN(2,1)*clhs135 + DN(2,2)*clhs211 + clhs61;
            lhs(10,1)=DN(2,0)*clhs15 + DN(2,1)*clhs139 + DN(2,2)*clhs212 + clhs167;
            lhs(10,2)=DN(2,0)*clhs21 + DN(2,1)*clhs144 + DN(2,2)*clhs214 + clhs231;
            lhs(10,3)=-clhs266;
            lhs(10,4)=DN(2,0)*clhs27 + DN(2,1)*clhs148 + DN(2,2)*clhs218 + clhs291;
            lhs(10,5)=DN(2,0)*clhs38 + DN(2,1)*clhs151 + DN(2,2)*clhs220 + clhs318;
            lhs(10,6)=DN(2,0)*clhs44 + DN(2,1)*clhs157 + DN(2,2)*clhs221 + clhs341;
            lhs(10,7)=-clhs360;
            lhs(10,8)=DN(2,0)*clhs50 + DN(2,1)*clhs161 + DN(2,2)*clhs226 + clhs378;
            lhs(10,9)=DN(2,0)*clhs60 + DN(2,1)*clhs164 + DN(2,2)*clhs228 + clhs401;
            lhs(10,10)=DN(2,0)*clhs66 + DN(2,1)*clhs170 + DN(2,2)*clhs229 + clhs375 + clhs418*mu;
            lhs(10,11)=-clhs419;
            lhs(10,12)=DN(2,0)*clhs72 + DN(2,1)*clhs174 + DN(2,2)*clhs234 + clhs421;
            lhs(10,13)=DN(2,0)*clhs82 + DN(2,1)*clhs177 + DN(2,2)*clhs236 + clhs422;
            lhs(10,14)=DN(2,0)*clhs88 + DN(2,1)*clhs183 + DN(2,2)*clhs237 + clhs424;
            lhs(10,15)=-clhs425;
            lhs(10,16)=DN(2,0)*clhs94 + DN(2,1)*clhs187 + DN(2,2)*clhs242 + clhs426;
            lhs(10,17)=DN(2,0)*clhs104 + DN(2,1)*clhs190 + DN(2,2)*clhs244 + clhs427;
            lhs(10,18)=DN(2,0)*clhs110 + DN(2,1)*clhs196 + DN(2,2)*clhs245 + clhs429;
            lhs(10,19)=-clhs430;
            lhs(10,20)=DN(2,0)*clhs116 + DN(2,1)*clhs200 + DN(2,2)*clhs250 + clhs431;
            lhs(10,21)=DN(2,0)*clhs126 + DN(2,1)*clhs203 + DN(2,2)*clhs252 + clhs432;
            lhs(10,22)=DN(2,0)*clhs132 + DN(2,1)*clhs209 + DN(2,2)*clhs253 + clhs434;
            lhs(10,23)=-clhs435;
            lhs(11,0)=clhs258*clhs264 + clhs67;
            lhs(11,1)=clhs171 + clhs258*clhs265;
            lhs(11,2)=clhs232 + clhs258*clhs266;
            lhs(11,3)=clhs267;
            lhs(11,4)=clhs258*clhs358 + clhs292;
            lhs(11,5)=clhs258*clhs359 + clhs319;
            lhs(11,6)=clhs258*clhs360 + clhs342;
            lhs(11,7)=clhs361;
            lhs(11,8)=clhs259*clhs379;
            lhs(11,9)=clhs259*clhs402;
            lhs(11,10)=clhs259*clhs419;
            lhs(11,11)=clhs257*(clhs374 + clhs399 + clhs418);
            lhs(11,12)=clhs258*clhs386 + clhs436;
            lhs(11,13)=clhs258*clhs407 + clhs437;
            lhs(11,14)=clhs258*clhs425 + clhs438;
            lhs(11,15)=clhs439;
            lhs(11,16)=clhs258*clhs392 + clhs440;
            lhs(11,17)=clhs258*clhs412 + clhs441;
            lhs(11,18)=clhs258*clhs430 + clhs442;
            lhs(11,19)=clhs443;
            lhs(11,20)=clhs258*clhs398 + clhs444;
            lhs(11,21)=clhs258*clhs417 + clhs445;
            lhs(11,22)=clhs258*clhs435 + clhs446;
            lhs(11,23)=clhs447;
            lhs(12,0)=DN(3,0)*clhs3 + DN(3,1)*clhs5 + DN(3,2)*clhs7 + clhs75;
            lhs(12,1)=DN(3,0)*clhs10 + DN(3,1)*clhs12 + DN(3,2)*clhs15 + clhs172;
            lhs(12,2)=DN(3,0)*clhs17 + DN(3,1)*clhs19 + DN(3,2)*clhs21 + clhs233;
            lhs(12,3)=-clhs268;
            lhs(12,4)=DN(3,0)*clhs23 + DN(3,1)*clhs25 + DN(3,2)*clhs27 + clhs295;
            lhs(12,5)=DN(3,0)*clhs33 + DN(3,1)*clhs35 + DN(3,2)*clhs38 + clhs320;
            lhs(12,6)=DN(3,0)*clhs40 + DN(3,1)*clhs42 + DN(3,2)*clhs44 + clhs343;
            lhs(12,7)=-clhs362;
            lhs(12,8)=DN(3,0)*clhs46 + DN(3,1)*clhs48 + DN(3,2)*clhs50 + clhs383;
            lhs(12,9)=DN(3,0)*clhs55 + DN(3,1)*clhs57 + DN(3,2)*clhs60 + clhs403;
            lhs(12,10)=DN(3,0)*clhs62 + DN(3,1)*clhs64 + DN(3,2)*clhs66 + clhs421;
            lhs(12,11)=-clhs436;
            lhs(12,12)=DN(3,0)*clhs68 + DN(3,1)*clhs70 + DN(3,2)*clhs72 + clhs448*mu + clhs449;
            lhs(12,13)=DN(3,0)*clhs77 + DN(3,1)*clhs79 + DN(3,2)*clhs82 + clhs451;
            lhs(12,14)=DN(3,0)*clhs84 + DN(3,1)*clhs86 + DN(3,2)*clhs88 + clhs452;
            lhs(12,15)=-clhs453;
            lhs(12,16)=DN(3,0)*clhs90 + DN(3,1)*clhs92 + DN(3,2)*clhs94 + clhs457;
            lhs(12,17)=DN(3,0)*clhs99 + DN(3,1)*clhs101 + DN(3,2)*clhs104 + clhs458;
            lhs(12,18)=DN(3,0)*clhs106 + DN(3,1)*clhs108 + DN(3,2)*clhs110 + clhs459;
            lhs(12,19)=-clhs460;
            lhs(12,20)=DN(3,0)*clhs112 + DN(3,1)*clhs114 + DN(3,2)*clhs116 + clhs463;
            lhs(12,21)=DN(3,0)*clhs121 + DN(3,1)*clhs123 + DN(3,2)*clhs126 + clhs464;
            lhs(12,22)=DN(3,0)*clhs128 + DN(3,1)*clhs130 + DN(3,2)*clhs132 + clhs465;
            lhs(12,23)=-clhs466;
            lhs(13,0)=DN(3,0)*clhs5 + DN(3,1)*clhs134 + DN(3,2)*clhs135 + clhs76;
            lhs(13,1)=DN(3,0)*clhs12 + DN(3,1)*clhs137 + DN(3,2)*clhs139 + clhs179;
            lhs(13,2)=DN(3,0)*clhs19 + DN(3,1)*clhs142 + DN(3,2)*clhs144 + clhs235;
            lhs(13,3)=-clhs269;
            lhs(13,4)=DN(3,0)*clhs25 + DN(3,1)*clhs147 + DN(3,2)*clhs148 + clhs296;
            lhs(13,5)=DN(3,0)*clhs35 + DN(3,1)*clhs149 + DN(3,2)*clhs151 + clhs322;
            lhs(13,6)=DN(3,0)*clhs42 + DN(3,1)*clhs155 + DN(3,2)*clhs157 + clhs344;
            lhs(13,7)=-clhs363;
            lhs(13,8)=DN(3,0)*clhs48 + DN(3,1)*clhs160 + DN(3,2)*clhs161 + clhs384;
            lhs(13,9)=DN(3,0)*clhs57 + DN(3,1)*clhs162 + DN(3,2)*clhs164 + clhs405;
            lhs(13,10)=DN(3,0)*clhs64 + DN(3,1)*clhs168 + DN(3,2)*clhs170 + clhs422;
            lhs(13,11)=-clhs437;
            lhs(13,12)=DN(3,0)*clhs70 + DN(3,1)*clhs173 + DN(3,2)*clhs174 + clhs451;
            lhs(13,13)=DN(3,0)*clhs79 + DN(3,1)*clhs175 + DN(3,2)*clhs177 + clhs449 + clhs467*mu;
            lhs(13,14)=DN(3,0)*clhs86 + DN(3,1)*clhs181 + DN(3,2)*clhs183 + clhs469;
            lhs(13,15)=-clhs470;
            lhs(13,16)=DN(3,0)*clhs92 + DN(3,1)*clhs186 + DN(3,2)*clhs187 + clhs471;
            lhs(13,17)=DN(3,0)*clhs101 + DN(3,1)*clhs188 + DN(3,2)*clhs190 + clhs473;
            lhs(13,18)=DN(3,0)*clhs108 + DN(3,1)*clhs194 + DN(3,2)*clhs196 + clhs474;
            lhs(13,19)=-clhs475;
            lhs(13,20)=DN(3,0)*clhs114 + DN(3,1)*clhs199 + DN(3,2)*clhs200 + clhs476;
            lhs(13,21)=DN(3,0)*clhs123 + DN(3,1)*clhs201 + DN(3,2)*clhs203 + clhs478;
            lhs(13,22)=DN(3,0)*clhs130 + DN(3,1)*clhs207 + DN(3,2)*clhs209 + clhs479;
            lhs(13,23)=-clhs480;
            lhs(14,0)=DN(3,0)*clhs7 + DN(3,1)*clhs135 + DN(3,2)*clhs211 + clhs83;
            lhs(14,1)=DN(3,0)*clhs15 + DN(3,1)*clhs139 + DN(3,2)*clhs212 + clhs180;
            lhs(14,2)=DN(3,0)*clhs21 + DN(3,1)*clhs144 + DN(3,2)*clhs214 + clhs239;
            lhs(14,3)=-clhs270;
            lhs(14,4)=DN(3,0)*clhs27 + DN(3,1)*clhs148 + DN(3,2)*clhs218 + clhs297;
            lhs(14,5)=DN(3,0)*clhs38 + DN(3,1)*clhs151 + DN(3,2)*clhs220 + clhs323;
            lhs(14,6)=DN(3,0)*clhs44 + DN(3,1)*clhs157 + DN(3,2)*clhs221 + clhs346;
            lhs(14,7)=-clhs364;
            lhs(14,8)=DN(3,0)*clhs50 + DN(3,1)*clhs161 + DN(3,2)*clhs226 + clhs385;
            lhs(14,9)=DN(3,0)*clhs60 + DN(3,1)*clhs164 + DN(3,2)*clhs228 + clhs406;
            lhs(14,10)=DN(3,0)*clhs66 + DN(3,1)*clhs170 + DN(3,2)*clhs229 + clhs424;
            lhs(14,11)=-clhs438;
            lhs(14,12)=DN(3,0)*clhs72 + DN(3,1)*clhs174 + DN(3,2)*clhs234 + clhs452;
            lhs(14,13)=DN(3,0)*clhs82 + DN(3,1)*clhs177 + DN(3,2)*clhs236 + clhs469;
            lhs(14,14)=DN(3,0)*clhs88 + DN(3,1)*clhs183 + DN(3,2)*clhs237 + clhs449 + clhs481*mu;
            lhs(14,15)=-clhs482;
            lhs(14,16)=DN(3,0)*clhs94 + DN(3,1)*clhs187 + DN(3,2)*clhs242 + clhs484;
            lhs(14,17)=DN(3,0)*clhs104 + DN(3,1)*clhs190 + DN(3,2)*clhs244 + clhs485;
            lhs(14,18)=DN(3,0)*clhs110 + DN(3,1)*clhs196 + DN(3,2)*clhs245 + clhs487;
            lhs(14,19)=-clhs488;
            lhs(14,20)=DN(3,0)*clhs116 + DN(3,1)*clhs200 + DN(3,2)*clhs250 + clhs489;
            lhs(14,21)=DN(3,0)*clhs126 + DN(3,1)*clhs203 + DN(3,2)*clhs252 + clhs490;
            lhs(14,22)=DN(3,0)*clhs132 + DN(3,1)*clhs209 + DN(3,2)*clhs253 + clhs492;
            lhs(14,23)=-clhs493;
            lhs(15,0)=clhs258*clhs268 + clhs89;
            lhs(15,1)=clhs184 + clhs258*clhs269;
            lhs(15,2)=clhs240 + clhs258*clhs270;
            lhs(15,3)=clhs271;
            lhs(15,4)=clhs258*clhs362 + clhs298;
            lhs(15,5)=clhs258*clhs363 + clhs324;
            lhs(15,6)=clhs258*clhs364 + clhs347;
            lhs(15,7)=clhs365;
            lhs(15,8)=clhs258*clhs436 + clhs386;
            lhs(15,9)=clhs258*clhs437 + clhs407;
            lhs(15,10)=clhs258*clhs438 + clhs425;
            lhs(15,11)=clhs439;
            lhs(15,12)=clhs259*clhs453;
            lhs(15,13)=clhs259*clhs470;
            lhs(15,14)=clhs259*clhs482;
            lhs(15,15)=clhs257*(clhs448 + clhs467 + clhs481);
            lhs(15,16)=clhs258*clhs460 + clhs494;
            lhs(15,17)=clhs258*clhs475 + clhs495;
            lhs(15,18)=clhs258*clhs488 + clhs496;
            lhs(15,19)=clhs497;
            lhs(15,20)=clhs258*clhs466 + clhs498;
            lhs(15,21)=clhs258*clhs480 + clhs499;
            lhs(15,22)=clhs258*clhs493 + clhs500;
            lhs(15,23)=clhs501;
            lhs(16,0)=DN(4,0)*clhs3 + DN(4,1)*clhs5 + DN(4,2)*clhs7 + clhs97;
            lhs(16,1)=DN(4,0)*clhs10 + DN(4,1)*clhs12 + DN(4,2)*clhs15 + clhs185;
            lhs(16,2)=DN(4,0)*clhs17 + DN(4,1)*clhs19 + DN(4,2)*clhs21 + clhs241;
            lhs(16,3)=-clhs272;
            lhs(16,4)=DN(4,0)*clhs23 + DN(4,1)*clhs25 + DN(4,2)*clhs27 + clhs301;
            lhs(16,5)=DN(4,0)*clhs33 + DN(4,1)*clhs35 + DN(4,2)*clhs38 + clhs325;
            lhs(16,6)=DN(4,0)*clhs40 + DN(4,1)*clhs42 + DN(4,2)*clhs44 + clhs348;
            lhs(16,7)=-clhs366;
            lhs(16,8)=DN(4,0)*clhs46 + DN(4,1)*clhs48 + DN(4,2)*clhs50 + clhs389;
            lhs(16,9)=DN(4,0)*clhs55 + DN(4,1)*clhs57 + DN(4,2)*clhs60 + clhs408;
            lhs(16,10)=DN(4,0)*clhs62 + DN(4,1)*clhs64 + DN(4,2)*clhs66 + clhs426;
            lhs(16,11)=-clhs440;
            lhs(16,12)=DN(4,0)*clhs68 + DN(4,1)*clhs70 + DN(4,2)*clhs72 + clhs457;
            lhs(16,13)=DN(4,0)*clhs77 + DN(4,1)*clhs79 + DN(4,2)*clhs82 + clhs471;
            lhs(16,14)=DN(4,0)*clhs84 + DN(4,1)*clhs86 + DN(4,2)*clhs88 + clhs484;
            lhs(16,15)=-clhs494;
            lhs(16,16)=DN(4,0)*clhs90 + DN(4,1)*clhs92 + DN(4,2)*clhs94 + clhs502*mu + clhs503;
            lhs(16,17)=DN(4,0)*clhs99 + DN(4,1)*clhs101 + DN(4,2)*clhs104 + clhs505;
            lhs(16,18)=DN(4,0)*clhs106 + DN(4,1)*clhs108 + DN(4,2)*clhs110 + clhs506;
            lhs(16,19)=-clhs507;
            lhs(16,20)=DN(4,0)*clhs112 + DN(4,1)*clhs114 + DN(4,2)*clhs116 + clhs510;
            lhs(16,21)=DN(4,0)*clhs121 + DN(4,1)*clhs123 + DN(4,2)*clhs126 + clhs511;
            lhs(16,22)=DN(4,0)*clhs128 + DN(4,1)*clhs130 + DN(4,2)*clhs132 + clhs512;
            lhs(16,23)=-clhs513;
            lhs(17,0)=DN(4,0)*clhs5 + DN(4,1)*clhs134 + DN(4,2)*clhs135 + clhs98;
            lhs(17,1)=DN(4,0)*clhs12 + DN(4,1)*clhs137 + DN(4,2)*clhs139 + clhs192;
            lhs(17,2)=DN(4,0)*clhs19 + DN(4,1)*clhs142 + DN(4,2)*clhs144 + clhs243;
            lhs(17,3)=-clhs273;
            lhs(17,4)=DN(4,0)*clhs25 + DN(4,1)*clhs147 + DN(4,2)*clhs148 + clhs302;
            lhs(17,5)=DN(4,0)*clhs35 + DN(4,1)*clhs149 + DN(4,2)*clhs151 + clhs327;
            lhs(17,6)=DN(4,0)*clhs42 + DN(4,1)*clhs155 + DN(4,2)*clhs157 + clhs349;
            lhs(17,7)=-clhs367;
            lhs(17,8)=DN(4,0)*clhs48 + DN(4,1)*clhs160 + DN(4,2)*clhs161 + clhs390;
            lhs(17,9)=DN(4,0)*clhs57 + DN(4,1)*clhs162 + DN(4,2)*clhs164 + clhs410;
            lhs(17,10)=DN(4,0)*clhs64 + DN(4,1)*clhs168 + DN(4,2)*clhs170 + clhs427;
            lhs(17,11)=-clhs441;
            lhs(17,12)=DN(4,0)*clhs70 + DN(4,1)*clhs173 + DN(4,2)*clhs174 + clhs458;
            lhs(17,13)=DN(4,0)*clhs79 + DN(4,1)*clhs175 + DN(4,2)*clhs177 + clhs473;
            lhs(17,14)=DN(4,0)*clhs86 + DN(4,1)*clhs181 + DN(4,2)*clhs183 + clhs485;
            lhs(17,15)=-clhs495;
            lhs(17,16)=DN(4,0)*clhs92 + DN(4,1)*clhs186 + DN(4,2)*clhs187 + clhs505;
            lhs(17,17)=DN(4,0)*clhs101 + DN(4,1)*clhs188 + DN(4,2)*clhs190 + clhs503 + clhs514*mu;
            lhs(17,18)=DN(4,0)*clhs108 + DN(4,1)*clhs194 + DN(4,2)*clhs196 + clhs516;
            lhs(17,19)=-clhs517;
            lhs(17,20)=DN(4,0)*clhs114 + DN(4,1)*clhs199 + DN(4,2)*clhs200 + clhs518;
            lhs(17,21)=DN(4,0)*clhs123 + DN(4,1)*clhs201 + DN(4,2)*clhs203 + clhs520;
            lhs(17,22)=DN(4,0)*clhs130 + DN(4,1)*clhs207 + DN(4,2)*clhs209 + clhs521;
            lhs(17,23)=-clhs522;
            lhs(18,0)=DN(4,0)*clhs7 + DN(4,1)*clhs135 + DN(4,2)*clhs211 + clhs105;
            lhs(18,1)=DN(4,0)*clhs15 + DN(4,1)*clhs139 + DN(4,2)*clhs212 + clhs193;
            lhs(18,2)=DN(4,0)*clhs21 + DN(4,1)*clhs144 + DN(4,2)*clhs214 + clhs247;
            lhs(18,3)=-clhs274;
            lhs(18,4)=DN(4,0)*clhs27 + DN(4,1)*clhs148 + DN(4,2)*clhs218 + clhs303;
            lhs(18,5)=DN(4,0)*clhs38 + DN(4,1)*clhs151 + DN(4,2)*clhs220 + clhs328;
            lhs(18,6)=DN(4,0)*clhs44 + DN(4,1)*clhs157 + DN(4,2)*clhs221 + clhs351;
            lhs(18,7)=-clhs368;
            lhs(18,8)=DN(4,0)*clhs50 + DN(4,1)*clhs161 + DN(4,2)*clhs226 + clhs391;
            lhs(18,9)=DN(4,0)*clhs60 + DN(4,1)*clhs164 + DN(4,2)*clhs228 + clhs411;
            lhs(18,10)=DN(4,0)*clhs66 + DN(4,1)*clhs170 + DN(4,2)*clhs229 + clhs429;
            lhs(18,11)=-clhs442;
            lhs(18,12)=DN(4,0)*clhs72 + DN(4,1)*clhs174 + DN(4,2)*clhs234 + clhs459;
            lhs(18,13)=DN(4,0)*clhs82 + DN(4,1)*clhs177 + DN(4,2)*clhs236 + clhs474;
            lhs(18,14)=DN(4,0)*clhs88 + DN(4,1)*clhs183 + DN(4,2)*clhs237 + clhs487;
            lhs(18,15)=-clhs496;
            lhs(18,16)=DN(4,0)*clhs94 + DN(4,1)*clhs187 + DN(4,2)*clhs242 + clhs506;
            lhs(18,17)=DN(4,0)*clhs104 + DN(4,1)*clhs190 + DN(4,2)*clhs244 + clhs516;
            lhs(18,18)=DN(4,0)*clhs110 + DN(4,1)*clhs196 + DN(4,2)*clhs245 + clhs503 + clhs523*mu;
            lhs(18,19)=-clhs524;
            lhs(18,20)=DN(4,0)*clhs116 + DN(4,1)*clhs200 + DN(4,2)*clhs250 + clhs526;
            lhs(18,21)=DN(4,0)*clhs126 + DN(4,1)*clhs203 + DN(4,2)*clhs252 + clhs527;
            lhs(18,22)=DN(4,0)*clhs132 + DN(4,1)*clhs209 + DN(4,2)*clhs253 + clhs529;
            lhs(18,23)=-clhs530;
            lhs(19,0)=clhs111 + clhs258*clhs272;
            lhs(19,1)=clhs197 + clhs258*clhs273;
            lhs(19,2)=clhs248 + clhs258*clhs274;
            lhs(19,3)=clhs275;
            lhs(19,4)=clhs258*clhs366 + clhs304;
            lhs(19,5)=clhs258*clhs367 + clhs329;
            lhs(19,6)=clhs258*clhs368 + clhs352;
            lhs(19,7)=clhs369;
            lhs(19,8)=clhs258*clhs440 + clhs392;
            lhs(19,9)=clhs258*clhs441 + clhs412;
            lhs(19,10)=clhs258*clhs442 + clhs430;
            lhs(19,11)=clhs443;
            lhs(19,12)=clhs258*clhs494 + clhs460;
            lhs(19,13)=clhs258*clhs495 + clhs475;
            lhs(19,14)=clhs258*clhs496 + clhs488;
            lhs(19,15)=clhs497;
            lhs(19,16)=clhs259*clhs507;
            lhs(19,17)=clhs259*clhs517;
            lhs(19,18)=clhs259*clhs524;
            lhs(19,19)=clhs257*(clhs502 + clhs514 + clhs523);
            lhs(19,20)=clhs258*clhs513 + clhs531;
            lhs(19,21)=clhs258*clhs522 + clhs532;
            lhs(19,22)=clhs258*clhs530 + clhs533;
            lhs(19,23)=clhs534;
            lhs(20,0)=DN(5,0)*clhs3 + DN(5,1)*clhs5 + DN(5,2)*clhs7 + clhs119;
            lhs(20,1)=DN(5,0)*clhs10 + DN(5,1)*clhs12 + DN(5,2)*clhs15 + clhs198;
            lhs(20,2)=DN(5,0)*clhs17 + DN(5,1)*clhs19 + DN(5,2)*clhs21 + clhs249;
            lhs(20,3)=-clhs276;
            lhs(20,4)=DN(5,0)*clhs23 + DN(5,1)*clhs25 + DN(5,2)*clhs27 + clhs307;
            lhs(20,5)=DN(5,0)*clhs33 + DN(5,1)*clhs35 + DN(5,2)*clhs38 + clhs330;
            lhs(20,6)=DN(5,0)*clhs40 + DN(5,1)*clhs42 + DN(5,2)*clhs44 + clhs353;
            lhs(20,7)=-clhs370;
            lhs(20,8)=DN(5,0)*clhs46 + DN(5,1)*clhs48 + DN(5,2)*clhs50 + clhs395;
            lhs(20,9)=DN(5,0)*clhs55 + DN(5,1)*clhs57 + DN(5,2)*clhs60 + clhs413;
            lhs(20,10)=DN(5,0)*clhs62 + DN(5,1)*clhs64 + DN(5,2)*clhs66 + clhs431;
            lhs(20,11)=-clhs444;
            lhs(20,12)=DN(5,0)*clhs68 + DN(5,1)*clhs70 + DN(5,2)*clhs72 + clhs463;
            lhs(20,13)=DN(5,0)*clhs77 + DN(5,1)*clhs79 + DN(5,2)*clhs82 + clhs476;
            lhs(20,14)=DN(5,0)*clhs84 + DN(5,1)*clhs86 + DN(5,2)*clhs88 + clhs489;
            lhs(20,15)=-clhs498;
            lhs(20,16)=DN(5,0)*clhs90 + DN(5,1)*clhs92 + DN(5,2)*clhs94 + clhs510;
            lhs(20,17)=DN(5,0)*clhs99 + DN(5,1)*clhs101 + DN(5,2)*clhs104 + clhs518;
            lhs(20,18)=DN(5,0)*clhs106 + DN(5,1)*clhs108 + DN(5,2)*clhs110 + clhs526;
            lhs(20,19)=-clhs531;
            lhs(20,20)=DN(5,0)*clhs112 + DN(5,1)*clhs114 + DN(5,2)*clhs116 + clhs535*mu + clhs536;
            lhs(20,21)=DN(5,0)*clhs121 + DN(5,1)*clhs123 + DN(5,2)*clhs126 + clhs538;
            lhs(20,22)=DN(5,0)*clhs128 + DN(5,1)*clhs130 + DN(5,2)*clhs132 + clhs539;
            lhs(20,23)=-clhs540;
            lhs(21,0)=DN(5,0)*clhs5 + DN(5,1)*clhs134 + DN(5,2)*clhs135 + clhs120;
            lhs(21,1)=DN(5,0)*clhs12 + DN(5,1)*clhs137 + DN(5,2)*clhs139 + clhs205;
            lhs(21,2)=DN(5,0)*clhs19 + DN(5,1)*clhs142 + DN(5,2)*clhs144 + clhs251;
            lhs(21,3)=-clhs277;
            lhs(21,4)=DN(5,0)*clhs25 + DN(5,1)*clhs147 + DN(5,2)*clhs148 + clhs308;
            lhs(21,5)=DN(5,0)*clhs35 + DN(5,1)*clhs149 + DN(5,2)*clhs151 + clhs332;
            lhs(21,6)=DN(5,0)*clhs42 + DN(5,1)*clhs155 + DN(5,2)*clhs157 + clhs354;
            lhs(21,7)=-clhs371;
            lhs(21,8)=DN(5,0)*clhs48 + DN(5,1)*clhs160 + DN(5,2)*clhs161 + clhs396;
            lhs(21,9)=DN(5,0)*clhs57 + DN(5,1)*clhs162 + DN(5,2)*clhs164 + clhs415;
            lhs(21,10)=DN(5,0)*clhs64 + DN(5,1)*clhs168 + DN(5,2)*clhs170 + clhs432;
            lhs(21,11)=-clhs445;
            lhs(21,12)=DN(5,0)*clhs70 + DN(5,1)*clhs173 + DN(5,2)*clhs174 + clhs464;
            lhs(21,13)=DN(5,0)*clhs79 + DN(5,1)*clhs175 + DN(5,2)*clhs177 + clhs478;
            lhs(21,14)=DN(5,0)*clhs86 + DN(5,1)*clhs181 + DN(5,2)*clhs183 + clhs490;
            lhs(21,15)=-clhs499;
            lhs(21,16)=DN(5,0)*clhs92 + DN(5,1)*clhs186 + DN(5,2)*clhs187 + clhs511;
            lhs(21,17)=DN(5,0)*clhs101 + DN(5,1)*clhs188 + DN(5,2)*clhs190 + clhs520;
            lhs(21,18)=DN(5,0)*clhs108 + DN(5,1)*clhs194 + DN(5,2)*clhs196 + clhs527;
            lhs(21,19)=-clhs532;
            lhs(21,20)=DN(5,0)*clhs114 + DN(5,1)*clhs199 + DN(5,2)*clhs200 + clhs538;
            lhs(21,21)=DN(5,0)*clhs123 + DN(5,1)*clhs201 + DN(5,2)*clhs203 + clhs536 + clhs541*mu;
            lhs(21,22)=DN(5,0)*clhs130 + DN(5,1)*clhs207 + DN(5,2)*clhs209 + clhs542;
            lhs(21,23)=-clhs543;
            lhs(22,0)=DN(5,0)*clhs7 + DN(5,1)*clhs135 + DN(5,2)*clhs211 + clhs127;
            lhs(22,1)=DN(5,0)*clhs15 + DN(5,1)*clhs139 + DN(5,2)*clhs212 + clhs206;
            lhs(22,2)=DN(5,0)*clhs21 + DN(5,1)*clhs144 + DN(5,2)*clhs214 + clhs255;
            lhs(22,3)=-clhs278;
            lhs(22,4)=DN(5,0)*clhs27 + DN(5,1)*clhs148 + DN(5,2)*clhs218 + clhs309;
            lhs(22,5)=DN(5,0)*clhs38 + DN(5,1)*clhs151 + DN(5,2)*clhs220 + clhs333;
            lhs(22,6)=DN(5,0)*clhs44 + DN(5,1)*clhs157 + DN(5,2)*clhs221 + clhs356;
            lhs(22,7)=-clhs372;
            lhs(22,8)=DN(5,0)*clhs50 + DN(5,1)*clhs161 + DN(5,2)*clhs226 + clhs397;
            lhs(22,9)=DN(5,0)*clhs60 + DN(5,1)*clhs164 + DN(5,2)*clhs228 + clhs416;
            lhs(22,10)=DN(5,0)*clhs66 + DN(5,1)*clhs170 + DN(5,2)*clhs229 + clhs434;
            lhs(22,11)=-clhs446;
            lhs(22,12)=DN(5,0)*clhs72 + DN(5,1)*clhs174 + DN(5,2)*clhs234 + clhs465;
            lhs(22,13)=DN(5,0)*clhs82 + DN(5,1)*clhs177 + DN(5,2)*clhs236 + clhs479;
            lhs(22,14)=DN(5,0)*clhs88 + DN(5,1)*clhs183 + DN(5,2)*clhs237 + clhs492;
            lhs(22,15)=-clhs500;
            lhs(22,16)=DN(5,0)*clhs94 + DN(5,1)*clhs187 + DN(5,2)*clhs242 + clhs512;
            lhs(22,17)=DN(5,0)*clhs104 + DN(5,1)*clhs190 + DN(5,2)*clhs244 + clhs521;
            lhs(22,18)=DN(5,0)*clhs110 + DN(5,1)*clhs196 + DN(5,2)*clhs245 + clhs529;
            lhs(22,19)=-clhs533;
            lhs(22,20)=DN(5,0)*clhs116 + DN(5,1)*clhs200 + DN(5,2)*clhs250 + clhs539;
            lhs(22,21)=DN(5,0)*clhs126 + DN(5,1)*clhs203 + DN(5,2)*clhs252 + clhs542;
            lhs(22,22)=DN(5,0)*clhs132 + DN(5,1)*clhs209 + DN(5,2)*clhs253 + clhs536 + clhs544*mu;
            lhs(22,23)=-clhs545;
            lhs(23,0)=clhs133 + clhs258*clhs276;
            lhs(23,1)=clhs210 + clhs258*clhs277;
            lhs(23,2)=clhs256 + clhs258*clhs278;
            lhs(23,3)=clhs279;
            lhs(23,4)=clhs258*clhs370 + clhs310;
            lhs(23,5)=clhs258*clhs371 + clhs334;
            lhs(23,6)=clhs258*clhs372 + clhs357;
            lhs(23,7)=clhs373;
            lhs(23,8)=clhs258*clhs444 + clhs398;
            lhs(23,9)=clhs258*clhs445 + clhs417;
            lhs(23,10)=clhs258*clhs446 + clhs435;
            lhs(23,11)=clhs447;
            lhs(23,12)=clhs258*clhs498 + clhs466;
            lhs(23,13)=clhs258*clhs499 + clhs480;
            lhs(23,14)=clhs258*clhs500 + clhs493;
            lhs(23,15)=clhs501;
            lhs(23,16)=clhs258*clhs531 + clhs513;
            lhs(23,17)=clhs258*clhs532 + clhs522;
            lhs(23,18)=clhs258*clhs533 + clhs530;
            lhs(23,19)=clhs534;
            lhs(23,20)=clhs259*clhs540;
            lhs(23,21)=clhs259*clhs543;
            lhs(23,22)=clhs259*clhs545;
            lhs(23,23)=clhs257*(clhs535 + clhs541 + clhs544);


    // Add intermediate results to local system
    noalias(rLHS) += lhs * rData.Weight;
}

template <>
void SymbolicStokes<SymbolicStokesData<3,8>>::ComputeGaussPointLHSContribution(
    SymbolicStokesData<3,8> &rData,
    MatrixType &rLHS)
{

    const double rho = rData.Density;
    const double mu = rData.EffectiveViscosity;

    const double h = rData.ElementSize;

    const double dt = rData.DeltaTime;
    const double bdf0 = rData.bdf0;

    const double dyn_tau = rData.DynamicTau;

    // Get constitutive matrix
    const Matrix &C = rData.C;

    // Get shape function values
    const auto &N = rData.N;
    const auto &DN = rData.DN_DX;

    // Stabilization parameters
    constexpr double stab_c1 = 4.0;
    constexpr double stab_c2 = 2.0;

    auto &lhs = rData.lhs;
    double dt_inv = 0.0;
    if (dt > 1e-09)
    {
        dt_inv = 1.0/dt;
    }
    if (std::abs(bdf0) < 1e-9)
    {
        dt_inv = 0.0;
    }

    const double clhs0 =             pow(DN(0,0), 2);
const double clhs1 =             bdf0*rho;
const double clhs2 =             pow(N[0], 2)*clhs1;
const double clhs3 =             C(0,0)*DN(0,0) + C(0,3)*DN(0,1) + C(0,5)*DN(0,2);
const double clhs4 =             C(0,3)*DN(0,0);
const double clhs5 =             C(3,3)*DN(0,1) + C(3,5)*DN(0,2) + clhs4;
const double clhs6 =             C(0,5)*DN(0,0);
const double clhs7 =             C(3,5)*DN(0,1) + C(5,5)*DN(0,2) + clhs6;
const double clhs8 =             DN(0,0)*mu;
const double clhs9 =             DN(0,1)*clhs8;
const double clhs10 =             C(0,1)*DN(0,1) + C(0,4)*DN(0,2) + clhs4;
const double clhs11 =             C(1,3)*DN(0,1);
const double clhs12 =             C(3,3)*DN(0,0) + C(3,4)*DN(0,2) + clhs11;
const double clhs13 =             C(3,5)*DN(0,0);
const double clhs14 =             C(4,5)*DN(0,2);
const double clhs15 =             C(1,5)*DN(0,1) + clhs13 + clhs14;
const double clhs16 =             DN(0,2)*clhs8;
const double clhs17 =             C(0,2)*DN(0,2) + C(0,4)*DN(0,1) + clhs6;
const double clhs18 =             C(3,4)*DN(0,1);
const double clhs19 =             C(2,3)*DN(0,2) + clhs13 + clhs18;
const double clhs20 =             C(2,5)*DN(0,2);
const double clhs21 =             C(4,5)*DN(0,1) + C(5,5)*DN(0,0) + clhs20;
const double clhs22 =             DN(0,0)*N[0];
const double clhs23 =             C(0,0)*DN(1,0) + C(0,3)*DN(1,1) + C(0,5)*DN(1,2);
const double clhs24 =             C(0,3)*DN(1,0);
const double clhs25 =             C(3,3)*DN(1,1) + C(3,5)*DN(1,2) + clhs24;
const double clhs26 =             C(0,5)*DN(1,0);
const double clhs27 =             C(3,5)*DN(1,1) + C(5,5)*DN(1,2) + clhs26;
const double clhs28 =             DN(0,0)*DN(1,0);
const double clhs29 =             N[0]*clhs1;
const double clhs30 =             N[1]*clhs29;
const double clhs31 =             clhs28*mu + clhs30;
const double clhs32 =             DN(1,1)*clhs8;
const double clhs33 =             C(0,1)*DN(1,1) + C(0,4)*DN(1,2) + clhs24;
const double clhs34 =             C(1,3)*DN(1,1);
const double clhs35 =             C(3,3)*DN(1,0) + C(3,4)*DN(1,2) + clhs34;
const double clhs36 =             C(3,5)*DN(1,0);
const double clhs37 =             C(4,5)*DN(1,2);
const double clhs38 =             C(1,5)*DN(1,1) + clhs36 + clhs37;
const double clhs39 =             DN(1,2)*clhs8;
const double clhs40 =             C(0,2)*DN(1,2) + C(0,4)*DN(1,1) + clhs26;
const double clhs41 =             C(3,4)*DN(1,1);
const double clhs42 =             C(2,3)*DN(1,2) + clhs36 + clhs41;
const double clhs43 =             C(2,5)*DN(1,2);
const double clhs44 =             C(4,5)*DN(1,1) + C(5,5)*DN(1,0) + clhs43;
const double clhs45 =             DN(0,0)*N[1];
const double clhs46 =             C(0,0)*DN(2,0) + C(0,3)*DN(2,1) + C(0,5)*DN(2,2);
const double clhs47 =             C(0,3)*DN(2,0);
const double clhs48 =             C(3,3)*DN(2,1) + C(3,5)*DN(2,2) + clhs47;
const double clhs49 =             C(0,5)*DN(2,0);
const double clhs50 =             C(3,5)*DN(2,1) + C(5,5)*DN(2,2) + clhs49;
const double clhs51 =             DN(0,0)*DN(2,0);
const double clhs52 =             N[2]*clhs29;
const double clhs53 =             clhs51*mu + clhs52;
const double clhs54 =             DN(2,1)*clhs8;
const double clhs55 =             C(0,1)*DN(2,1) + C(0,4)*DN(2,2) + clhs47;
const double clhs56 =             C(1,3)*DN(2,1);
const double clhs57 =             C(3,3)*DN(2,0) + C(3,4)*DN(2,2) + clhs56;
const double clhs58 =             C(3,5)*DN(2,0);
const double clhs59 =             C(4,5)*DN(2,2);
const double clhs60 =             C(1,5)*DN(2,1) + clhs58 + clhs59;
const double clhs61 =             DN(2,2)*clhs8;
const double clhs62 =             C(0,2)*DN(2,2) + C(0,4)*DN(2,1) + clhs49;
const double clhs63 =             C(3,4)*DN(2,1);
const double clhs64 =             C(2,3)*DN(2,2) + clhs58 + clhs63;
const double clhs65 =             C(2,5)*DN(2,2);
const double clhs66 =             C(4,5)*DN(2,1) + C(5,5)*DN(2,0) + clhs65;
const double clhs67 =             DN(0,0)*N[2];
const double clhs68 =             C(0,0)*DN(3,0) + C(0,3)*DN(3,1) + C(0,5)*DN(3,2);
const double clhs69 =             C(0,3)*DN(3,0);
const double clhs70 =             C(3,3)*DN(3,1) + C(3,5)*DN(3,2) + clhs69;
const double clhs71 =             C(0,5)*DN(3,0);
const double clhs72 =             C(3,5)*DN(3,1) + C(5,5)*DN(3,2) + clhs71;
const double clhs73 =             DN(0,0)*DN(3,0);
const double clhs74 =             N[3]*clhs29;
const double clhs75 =             clhs73*mu + clhs74;
const double clhs76 =             DN(3,1)*clhs8;
const double clhs77 =             C(0,1)*DN(3,1) + C(0,4)*DN(3,2) + clhs69;
const double clhs78 =             C(1,3)*DN(3,1);
const double clhs79 =             C(3,3)*DN(3,0) + C(3,4)*DN(3,2) + clhs78;
const double clhs80 =             C(3,5)*DN(3,0);
const double clhs81 =             C(4,5)*DN(3,2);
const double clhs82 =             C(1,5)*DN(3,1) + clhs80 + clhs81;
const double clhs83 =             DN(3,2)*clhs8;
const double clhs84 =             C(0,2)*DN(3,2) + C(0,4)*DN(3,1) + clhs71;
const double clhs85 =             C(3,4)*DN(3,1);
const double clhs86 =             C(2,3)*DN(3,2) + clhs80 + clhs85;
const double clhs87 =             C(2,5)*DN(3,2);
const double clhs88 =             C(4,5)*DN(3,1) + C(5,5)*DN(3,0) + clhs87;
const double clhs89 =             DN(0,0)*N[3];
const double clhs90 =             C(0,0)*DN(4,0) + C(0,3)*DN(4,1) + C(0,5)*DN(4,2);
const double clhs91 =             C(0,3)*DN(4,0);
const double clhs92 =             C(3,3)*DN(4,1) + C(3,5)*DN(4,2) + clhs91;
const double clhs93 =             C(0,5)*DN(4,0);
const double clhs94 =             C(3,5)*DN(4,1) + C(5,5)*DN(4,2) + clhs93;
const double clhs95 =             DN(0,0)*DN(4,0);
const double clhs96 =             N[4]*clhs29;
const double clhs97 =             clhs95*mu + clhs96;
const double clhs98 =             DN(4,1)*clhs8;
const double clhs99 =             C(0,1)*DN(4,1) + C(0,4)*DN(4,2) + clhs91;
const double clhs100 =             C(1,3)*DN(4,1);
const double clhs101 =             C(3,3)*DN(4,0) + C(3,4)*DN(4,2) + clhs100;
const double clhs102 =             C(3,5)*DN(4,0);
const double clhs103 =             C(4,5)*DN(4,2);
const double clhs104 =             C(1,5)*DN(4,1) + clhs102 + clhs103;
const double clhs105 =             DN(4,2)*clhs8;
const double clhs106 =             C(0,2)*DN(4,2) + C(0,4)*DN(4,1) + clhs93;
const double clhs107 =             C(3,4)*DN(4,1);
const double clhs108 =             C(2,3)*DN(4,2) + clhs102 + clhs107;
const double clhs109 =             C(2,5)*DN(4,2);
const double clhs110 =             C(4,5)*DN(4,1) + C(5,5)*DN(4,0) + clhs109;
const double clhs111 =             DN(0,0)*N[4];
const double clhs112 =             C(0,0)*DN(5,0) + C(0,3)*DN(5,1) + C(0,5)*DN(5,2);
const double clhs113 =             C(0,3)*DN(5,0);
const double clhs114 =             C(3,3)*DN(5,1) + C(3,5)*DN(5,2) + clhs113;
const double clhs115 =             C(0,5)*DN(5,0);
const double clhs116 =             C(3,5)*DN(5,1) + C(5,5)*DN(5,2) + clhs115;
const double clhs117 =             DN(0,0)*DN(5,0);
const double clhs118 =             N[5]*clhs29;
const double clhs119 =             clhs117*mu + clhs118;
const double clhs120 =             DN(5,1)*clhs8;
const double clhs121 =             C(0,1)*DN(5,1) + C(0,4)*DN(5,2) + clhs113;
const double clhs122 =             C(1,3)*DN(5,1);
const double clhs123 =             C(3,3)*DN(5,0) + C(3,4)*DN(5,2) + clhs122;
const double clhs124 =             C(3,5)*DN(5,0);
const double clhs125 =             C(4,5)*DN(5,2);
const double clhs126 =             C(1,5)*DN(5,1) + clhs124 + clhs125;
const double clhs127 =             DN(5,2)*clhs8;
const double clhs128 =             C(0,2)*DN(5,2) + C(0,4)*DN(5,1) + clhs115;
const double clhs129 =             C(3,4)*DN(5,1);
const double clhs130 =             C(2,3)*DN(5,2) + clhs124 + clhs129;
const double clhs131 =             C(2,5)*DN(5,2);
const double clhs132 =             C(4,5)*DN(5,1) + C(5,5)*DN(5,0) + clhs131;
const double clhs133 =             DN(0,0)*N[5];
const double clhs134 =             C(0,0)*DN(6,0) + C(0,3)*DN(6,1) + C(0,5)*DN(6,2);
const double clhs135 =             C(0,3)*DN(6,0);
const double clhs136 =             C(3,3)*DN(6,1) + C(3,5)*DN(6,2) + clhs135;
const double clhs137 =             C(0,5)*DN(6,0);
const double clhs138 =             C(3,5)*DN(6,1) + C(5,5)*DN(6,2) + clhs137;
const double clhs139 =             DN(0,0)*DN(6,0);
const double clhs140 =             N[6]*clhs29;
const double clhs141 =             clhs139*mu + clhs140;
const double clhs142 =             DN(6,1)*clhs8;
const double clhs143 =             C(0,1)*DN(6,1) + C(0,4)*DN(6,2) + clhs135;
const double clhs144 =             C(1,3)*DN(6,1);
const double clhs145 =             C(3,3)*DN(6,0) + C(3,4)*DN(6,2) + clhs144;
const double clhs146 =             C(3,5)*DN(6,0);
const double clhs147 =             C(4,5)*DN(6,2);
const double clhs148 =             C(1,5)*DN(6,1) + clhs146 + clhs147;
const double clhs149 =             DN(6,2)*clhs8;
const double clhs150 =             C(0,2)*DN(6,2) + C(0,4)*DN(6,1) + clhs137;
const double clhs151 =             C(3,4)*DN(6,1);
const double clhs152 =             C(2,3)*DN(6,2) + clhs146 + clhs151;
const double clhs153 =             C(2,5)*DN(6,2);
const double clhs154 =             C(4,5)*DN(6,1) + C(5,5)*DN(6,0) + clhs153;
const double clhs155 =             DN(0,0)*N[6];
const double clhs156 =             C(0,0)*DN(7,0) + C(0,3)*DN(7,1) + C(0,5)*DN(7,2);
const double clhs157 =             C(0,3)*DN(7,0);
const double clhs158 =             C(3,3)*DN(7,1) + C(3,5)*DN(7,2) + clhs157;
const double clhs159 =             C(0,5)*DN(7,0);
const double clhs160 =             C(3,5)*DN(7,1) + C(5,5)*DN(7,2) + clhs159;
const double clhs161 =             DN(0,0)*DN(7,0);
const double clhs162 =             N[7]*clhs29;
const double clhs163 =             clhs161*mu + clhs162;
const double clhs164 =             DN(7,1)*clhs8;
const double clhs165 =             C(0,1)*DN(7,1) + C(0,4)*DN(7,2) + clhs157;
const double clhs166 =             C(1,3)*DN(7,1);
const double clhs167 =             C(3,3)*DN(7,0) + C(3,4)*DN(7,2) + clhs166;
const double clhs168 =             C(3,5)*DN(7,0);
const double clhs169 =             C(4,5)*DN(7,2);
const double clhs170 =             C(1,5)*DN(7,1) + clhs168 + clhs169;
const double clhs171 =             DN(7,2)*clhs8;
const double clhs172 =             C(0,2)*DN(7,2) + C(0,4)*DN(7,1) + clhs159;
const double clhs173 =             C(3,4)*DN(7,1);
const double clhs174 =             C(2,3)*DN(7,2) + clhs168 + clhs173;
const double clhs175 =             C(2,5)*DN(7,2);
const double clhs176 =             C(4,5)*DN(7,1) + C(5,5)*DN(7,0) + clhs175;
const double clhs177 =             DN(0,0)*N[7];
const double clhs178 =             C(0,1)*DN(0,0) + C(1,5)*DN(0,2) + clhs11;
const double clhs179 =             C(0,4)*DN(0,0) + clhs14 + clhs18;
const double clhs180 =             pow(DN(0,1), 2);
const double clhs181 =             C(1,1)*DN(0,1) + C(1,3)*DN(0,0) + C(1,4)*DN(0,2);
const double clhs182 =             C(1,4)*DN(0,1);
const double clhs183 =             C(3,4)*DN(0,0) + C(4,4)*DN(0,2) + clhs182;
const double clhs184 =             DN(0,1)*mu;
const double clhs185 =             DN(0,2)*clhs184;
const double clhs186 =             C(1,2)*DN(0,2) + C(1,5)*DN(0,0) + clhs182;
const double clhs187 =             C(2,4)*DN(0,2);
const double clhs188 =             C(4,4)*DN(0,1) + C(4,5)*DN(0,0) + clhs187;
const double clhs189 =             DN(0,1)*N[0];
const double clhs190 =             DN(1,0)*clhs184;
const double clhs191 =             C(0,1)*DN(1,0) + C(1,5)*DN(1,2) + clhs34;
const double clhs192 =             C(0,4)*DN(1,0) + clhs37 + clhs41;
const double clhs193 =             C(1,1)*DN(1,1) + C(1,3)*DN(1,0) + C(1,4)*DN(1,2);
const double clhs194 =             C(1,4)*DN(1,1);
const double clhs195 =             C(3,4)*DN(1,0) + C(4,4)*DN(1,2) + clhs194;
const double clhs196 =             DN(0,1)*DN(1,1);
const double clhs197 =             clhs196*mu + clhs30;
const double clhs198 =             DN(1,2)*clhs184;
const double clhs199 =             C(1,2)*DN(1,2) + C(1,5)*DN(1,0) + clhs194;
const double clhs200 =             C(2,4)*DN(1,2);
const double clhs201 =             C(4,4)*DN(1,1) + C(4,5)*DN(1,0) + clhs200;
const double clhs202 =             DN(0,1)*N[1];
const double clhs203 =             DN(2,0)*clhs184;
const double clhs204 =             C(0,1)*DN(2,0) + C(1,5)*DN(2,2) + clhs56;
const double clhs205 =             C(0,4)*DN(2,0) + clhs59 + clhs63;
const double clhs206 =             C(1,1)*DN(2,1) + C(1,3)*DN(2,0) + C(1,4)*DN(2,2);
const double clhs207 =             C(1,4)*DN(2,1);
const double clhs208 =             C(3,4)*DN(2,0) + C(4,4)*DN(2,2) + clhs207;
const double clhs209 =             DN(0,1)*DN(2,1);
const double clhs210 =             clhs209*mu + clhs52;
const double clhs211 =             DN(2,2)*clhs184;
const double clhs212 =             C(1,2)*DN(2,2) + C(1,5)*DN(2,0) + clhs207;
const double clhs213 =             C(2,4)*DN(2,2);
const double clhs214 =             C(4,4)*DN(2,1) + C(4,5)*DN(2,0) + clhs213;
const double clhs215 =             DN(0,1)*N[2];
const double clhs216 =             DN(3,0)*clhs184;
const double clhs217 =             C(0,1)*DN(3,0) + C(1,5)*DN(3,2) + clhs78;
const double clhs218 =             C(0,4)*DN(3,0) + clhs81 + clhs85;
const double clhs219 =             C(1,1)*DN(3,1) + C(1,3)*DN(3,0) + C(1,4)*DN(3,2);
const double clhs220 =             C(1,4)*DN(3,1);
const double clhs221 =             C(3,4)*DN(3,0) + C(4,4)*DN(3,2) + clhs220;
const double clhs222 =             DN(0,1)*DN(3,1);
const double clhs223 =             clhs222*mu + clhs74;
const double clhs224 =             DN(3,2)*clhs184;
const double clhs225 =             C(1,2)*DN(3,2) + C(1,5)*DN(3,0) + clhs220;
const double clhs226 =             C(2,4)*DN(3,2);
const double clhs227 =             C(4,4)*DN(3,1) + C(4,5)*DN(3,0) + clhs226;
const double clhs228 =             DN(0,1)*N[3];
const double clhs229 =             DN(4,0)*clhs184;
const double clhs230 =             C(0,1)*DN(4,0) + C(1,5)*DN(4,2) + clhs100;
const double clhs231 =             C(0,4)*DN(4,0) + clhs103 + clhs107;
const double clhs232 =             C(1,1)*DN(4,1) + C(1,3)*DN(4,0) + C(1,4)*DN(4,2);
const double clhs233 =             C(1,4)*DN(4,1);
const double clhs234 =             C(3,4)*DN(4,0) + C(4,4)*DN(4,2) + clhs233;
const double clhs235 =             DN(0,1)*DN(4,1);
const double clhs236 =             clhs235*mu + clhs96;
const double clhs237 =             DN(4,2)*clhs184;
const double clhs238 =             C(1,2)*DN(4,2) + C(1,5)*DN(4,0) + clhs233;
const double clhs239 =             C(2,4)*DN(4,2);
const double clhs240 =             C(4,4)*DN(4,1) + C(4,5)*DN(4,0) + clhs239;
const double clhs241 =             DN(0,1)*N[4];
const double clhs242 =             DN(5,0)*clhs184;
const double clhs243 =             C(0,1)*DN(5,0) + C(1,5)*DN(5,2) + clhs122;
const double clhs244 =             C(0,4)*DN(5,0) + clhs125 + clhs129;
const double clhs245 =             C(1,1)*DN(5,1) + C(1,3)*DN(5,0) + C(1,4)*DN(5,2);
const double clhs246 =             C(1,4)*DN(5,1);
const double clhs247 =             C(3,4)*DN(5,0) + C(4,4)*DN(5,2) + clhs246;
const double clhs248 =             DN(0,1)*DN(5,1);
const double clhs249 =             clhs118 + clhs248*mu;
const double clhs250 =             DN(5,2)*clhs184;
const double clhs251 =             C(1,2)*DN(5,2) + C(1,5)*DN(5,0) + clhs246;
const double clhs252 =             C(2,4)*DN(5,2);
const double clhs253 =             C(4,4)*DN(5,1) + C(4,5)*DN(5,0) + clhs252;
const double clhs254 =             DN(0,1)*N[5];
const double clhs255 =             DN(6,0)*clhs184;
const double clhs256 =             C(0,1)*DN(6,0) + C(1,5)*DN(6,2) + clhs144;
const double clhs257 =             C(0,4)*DN(6,0) + clhs147 + clhs151;
const double clhs258 =             C(1,1)*DN(6,1) + C(1,3)*DN(6,0) + C(1,4)*DN(6,2);
const double clhs259 =             C(1,4)*DN(6,1);
const double clhs260 =             C(3,4)*DN(6,0) + C(4,4)*DN(6,2) + clhs259;
const double clhs261 =             DN(0,1)*DN(6,1);
const double clhs262 =             clhs140 + clhs261*mu;
const double clhs263 =             DN(6,2)*clhs184;
const double clhs264 =             C(1,2)*DN(6,2) + C(1,5)*DN(6,0) + clhs259;
const double clhs265 =             C(2,4)*DN(6,2);
const double clhs266 =             C(4,4)*DN(6,1) + C(4,5)*DN(6,0) + clhs265;
const double clhs267 =             DN(0,1)*N[6];
const double clhs268 =             DN(7,0)*clhs184;
const double clhs269 =             C(0,1)*DN(7,0) + C(1,5)*DN(7,2) + clhs166;
const double clhs270 =             C(0,4)*DN(7,0) + clhs169 + clhs173;
const double clhs271 =             C(1,1)*DN(7,1) + C(1,3)*DN(7,0) + C(1,4)*DN(7,2);
const double clhs272 =             C(1,4)*DN(7,1);
const double clhs273 =             C(3,4)*DN(7,0) + C(4,4)*DN(7,2) + clhs272;
const double clhs274 =             DN(0,1)*DN(7,1);
const double clhs275 =             clhs162 + clhs274*mu;
const double clhs276 =             DN(7,2)*clhs184;
const double clhs277 =             C(1,2)*DN(7,2) + C(1,5)*DN(7,0) + clhs272;
const double clhs278 =             C(2,4)*DN(7,2);
const double clhs279 =             C(4,4)*DN(7,1) + C(4,5)*DN(7,0) + clhs278;
const double clhs280 =             DN(0,1)*N[7];
const double clhs281 =             C(0,2)*DN(0,0) + C(2,3)*DN(0,1) + clhs20;
const double clhs282 =             C(1,2)*DN(0,1) + C(2,3)*DN(0,0) + clhs187;
const double clhs283 =             pow(DN(0,2), 2);
const double clhs284 =             C(2,2)*DN(0,2) + C(2,4)*DN(0,1) + C(2,5)*DN(0,0);
const double clhs285 =             DN(0,2)*N[0];
const double clhs286 =             DN(0,2)*mu;
const double clhs287 =             DN(1,0)*clhs286;
const double clhs288 =             C(0,2)*DN(1,0) + C(2,3)*DN(1,1) + clhs43;
const double clhs289 =             DN(1,1)*clhs286;
const double clhs290 =             C(1,2)*DN(1,1) + C(2,3)*DN(1,0) + clhs200;
const double clhs291 =             C(2,2)*DN(1,2) + C(2,4)*DN(1,1) + C(2,5)*DN(1,0);
const double clhs292 =             DN(0,2)*DN(1,2);
const double clhs293 =             clhs292*mu + clhs30;
const double clhs294 =             DN(0,2)*N[1];
const double clhs295 =             DN(2,0)*clhs286;
const double clhs296 =             C(0,2)*DN(2,0) + C(2,3)*DN(2,1) + clhs65;
const double clhs297 =             DN(2,1)*clhs286;
const double clhs298 =             C(1,2)*DN(2,1) + C(2,3)*DN(2,0) + clhs213;
const double clhs299 =             C(2,2)*DN(2,2) + C(2,4)*DN(2,1) + C(2,5)*DN(2,0);
const double clhs300 =             DN(0,2)*DN(2,2);
const double clhs301 =             clhs300*mu + clhs52;
const double clhs302 =             DN(0,2)*N[2];
const double clhs303 =             DN(3,0)*clhs286;
const double clhs304 =             C(0,2)*DN(3,0) + C(2,3)*DN(3,1) + clhs87;
const double clhs305 =             DN(3,1)*clhs286;
const double clhs306 =             C(1,2)*DN(3,1) + C(2,3)*DN(3,0) + clhs226;
const double clhs307 =             C(2,2)*DN(3,2) + C(2,4)*DN(3,1) + C(2,5)*DN(3,0);
const double clhs308 =             DN(0,2)*DN(3,2);
const double clhs309 =             clhs308*mu + clhs74;
const double clhs310 =             DN(0,2)*N[3];
const double clhs311 =             DN(4,0)*clhs286;
const double clhs312 =             C(0,2)*DN(4,0) + C(2,3)*DN(4,1) + clhs109;
const double clhs313 =             DN(4,1)*clhs286;
const double clhs314 =             C(1,2)*DN(4,1) + C(2,3)*DN(4,0) + clhs239;
const double clhs315 =             C(2,2)*DN(4,2) + C(2,4)*DN(4,1) + C(2,5)*DN(4,0);
const double clhs316 =             DN(0,2)*DN(4,2);
const double clhs317 =             clhs316*mu + clhs96;
const double clhs318 =             DN(0,2)*N[4];
const double clhs319 =             DN(5,0)*clhs286;
const double clhs320 =             C(0,2)*DN(5,0) + C(2,3)*DN(5,1) + clhs131;
const double clhs321 =             DN(5,1)*clhs286;
const double clhs322 =             C(1,2)*DN(5,1) + C(2,3)*DN(5,0) + clhs252;
const double clhs323 =             C(2,2)*DN(5,2) + C(2,4)*DN(5,1) + C(2,5)*DN(5,0);
const double clhs324 =             DN(0,2)*DN(5,2);
const double clhs325 =             clhs118 + clhs324*mu;
const double clhs326 =             DN(0,2)*N[5];
const double clhs327 =             DN(6,0)*clhs286;
const double clhs328 =             C(0,2)*DN(6,0) + C(2,3)*DN(6,1) + clhs153;
const double clhs329 =             DN(6,1)*clhs286;
const double clhs330 =             C(1,2)*DN(6,1) + C(2,3)*DN(6,0) + clhs265;
const double clhs331 =             C(2,2)*DN(6,2) + C(2,4)*DN(6,1) + C(2,5)*DN(6,0);
const double clhs332 =             DN(0,2)*DN(6,2);
const double clhs333 =             clhs140 + clhs332*mu;
const double clhs334 =             DN(0,2)*N[6];
const double clhs335 =             DN(7,0)*clhs286;
const double clhs336 =             C(0,2)*DN(7,0) + C(2,3)*DN(7,1) + clhs175;
const double clhs337 =             DN(7,1)*clhs286;
const double clhs338 =             C(1,2)*DN(7,1) + C(2,3)*DN(7,0) + clhs278;
const double clhs339 =             C(2,2)*DN(7,2) + C(2,4)*DN(7,1) + C(2,5)*DN(7,0);
const double clhs340 =             DN(0,2)*DN(7,2);
const double clhs341 =             clhs162 + clhs340*mu;
const double clhs342 =             DN(0,2)*N[7];
const double clhs343 =             1.0/(dt_inv*dyn_tau*rho + mu*stab_c1/pow(h, 2));
const double clhs344 =             clhs1*clhs343;
const double clhs345 =             clhs344 + 1;
const double clhs346 =             DN(1,0)*N[0];
const double clhs347 =             DN(1,1)*N[0];
const double clhs348 =             DN(1,2)*N[0];
const double clhs349 =             clhs343*(clhs196 + clhs28 + clhs292);
const double clhs350 =             DN(2,0)*N[0];
const double clhs351 =             DN(2,1)*N[0];
const double clhs352 =             DN(2,2)*N[0];
const double clhs353 =             clhs343*(clhs209 + clhs300 + clhs51);
const double clhs354 =             DN(3,0)*N[0];
const double clhs355 =             DN(3,1)*N[0];
const double clhs356 =             DN(3,2)*N[0];
const double clhs357 =             clhs343*(clhs222 + clhs308 + clhs73);
const double clhs358 =             DN(4,0)*N[0];
const double clhs359 =             DN(4,1)*N[0];
const double clhs360 =             DN(4,2)*N[0];
const double clhs361 =             clhs343*(clhs235 + clhs316 + clhs95);
const double clhs362 =             DN(5,0)*N[0];
const double clhs363 =             DN(5,1)*N[0];
const double clhs364 =             DN(5,2)*N[0];
const double clhs365 =             clhs343*(clhs117 + clhs248 + clhs324);
const double clhs366 =             DN(6,0)*N[0];
const double clhs367 =             DN(6,1)*N[0];
const double clhs368 =             DN(6,2)*N[0];
const double clhs369 =             clhs343*(clhs139 + clhs261 + clhs332);
const double clhs370 =             DN(7,0)*N[0];
const double clhs371 =             DN(7,1)*N[0];
const double clhs372 =             DN(7,2)*N[0];
const double clhs373 =             clhs343*(clhs161 + clhs274 + clhs340);
const double clhs374 =             pow(DN(1,0), 2);
const double clhs375 =             pow(N[1], 2)*clhs1;
const double clhs376 =             DN(1,0)*mu;
const double clhs377 =             DN(1,1)*clhs376;
const double clhs378 =             DN(1,2)*clhs376;
const double clhs379 =             DN(1,0)*N[1];
const double clhs380 =             DN(1,0)*DN(2,0);
const double clhs381 =             N[1]*clhs1;
const double clhs382 =             N[2]*clhs381;
const double clhs383 =             clhs380*mu + clhs382;
const double clhs384 =             DN(2,1)*clhs376;
const double clhs385 =             DN(2,2)*clhs376;
const double clhs386 =             DN(1,0)*N[2];
const double clhs387 =             DN(1,0)*DN(3,0);
const double clhs388 =             N[3]*clhs381;
const double clhs389 =             clhs387*mu + clhs388;
const double clhs390 =             DN(3,1)*clhs376;
const double clhs391 =             DN(3,2)*clhs376;
const double clhs392 =             DN(1,0)*N[3];
const double clhs393 =             DN(1,0)*DN(4,0);
const double clhs394 =             N[4]*clhs381;
const double clhs395 =             clhs393*mu + clhs394;
const double clhs396 =             DN(4,1)*clhs376;
const double clhs397 =             DN(4,2)*clhs376;
const double clhs398 =             DN(1,0)*N[4];
const double clhs399 =             DN(1,0)*DN(5,0);
const double clhs400 =             N[5]*clhs381;
const double clhs401 =             clhs399*mu + clhs400;
const double clhs402 =             DN(5,1)*clhs376;
const double clhs403 =             DN(5,2)*clhs376;
const double clhs404 =             DN(1,0)*N[5];
const double clhs405 =             DN(1,0)*DN(6,0);
const double clhs406 =             N[6]*clhs381;
const double clhs407 =             clhs405*mu + clhs406;
const double clhs408 =             DN(6,1)*clhs376;
const double clhs409 =             DN(6,2)*clhs376;
const double clhs410 =             DN(1,0)*N[6];
const double clhs411 =             DN(1,0)*DN(7,0);
const double clhs412 =             N[7]*clhs381;
const double clhs413 =             clhs411*mu + clhs412;
const double clhs414 =             DN(7,1)*clhs376;
const double clhs415 =             DN(7,2)*clhs376;
const double clhs416 =             DN(1,0)*N[7];
const double clhs417 =             pow(DN(1,1), 2);
const double clhs418 =             DN(1,1)*mu;
const double clhs419 =             DN(1,2)*clhs418;
const double clhs420 =             DN(1,1)*N[1];
const double clhs421 =             DN(2,0)*clhs418;
const double clhs422 =             DN(1,1)*DN(2,1);
const double clhs423 =             clhs382 + clhs422*mu;
const double clhs424 =             DN(2,2)*clhs418;
const double clhs425 =             DN(1,1)*N[2];
const double clhs426 =             DN(3,0)*clhs418;
const double clhs427 =             DN(1,1)*DN(3,1);
const double clhs428 =             clhs388 + clhs427*mu;
const double clhs429 =             DN(3,2)*clhs418;
const double clhs430 =             DN(1,1)*N[3];
const double clhs431 =             DN(4,0)*clhs418;
const double clhs432 =             DN(1,1)*DN(4,1);
const double clhs433 =             clhs394 + clhs432*mu;
const double clhs434 =             DN(4,2)*clhs418;
const double clhs435 =             DN(1,1)*N[4];
const double clhs436 =             DN(5,0)*clhs418;
const double clhs437 =             DN(1,1)*DN(5,1);
const double clhs438 =             clhs400 + clhs437*mu;
const double clhs439 =             DN(5,2)*clhs418;
const double clhs440 =             DN(1,1)*N[5];
const double clhs441 =             DN(6,0)*clhs418;
const double clhs442 =             DN(1,1)*DN(6,1);
const double clhs443 =             clhs406 + clhs442*mu;
const double clhs444 =             DN(6,2)*clhs418;
const double clhs445 =             DN(1,1)*N[6];
const double clhs446 =             DN(7,0)*clhs418;
const double clhs447 =             DN(1,1)*DN(7,1);
const double clhs448 =             clhs412 + clhs447*mu;
const double clhs449 =             DN(7,2)*clhs418;
const double clhs450 =             DN(1,1)*N[7];
const double clhs451 =             pow(DN(1,2), 2);
const double clhs452 =             DN(1,2)*N[1];
const double clhs453 =             DN(1,2)*mu;
const double clhs454 =             DN(2,0)*clhs453;
const double clhs455 =             DN(2,1)*clhs453;
const double clhs456 =             DN(1,2)*DN(2,2);
const double clhs457 =             clhs382 + clhs456*mu;
const double clhs458 =             DN(1,2)*N[2];
const double clhs459 =             DN(3,0)*clhs453;
const double clhs460 =             DN(3,1)*clhs453;
const double clhs461 =             DN(1,2)*DN(3,2);
const double clhs462 =             clhs388 + clhs461*mu;
const double clhs463 =             DN(1,2)*N[3];
const double clhs464 =             DN(4,0)*clhs453;
const double clhs465 =             DN(4,1)*clhs453;
const double clhs466 =             DN(1,2)*DN(4,2);
const double clhs467 =             clhs394 + clhs466*mu;
const double clhs468 =             DN(1,2)*N[4];
const double clhs469 =             DN(5,0)*clhs453;
const double clhs470 =             DN(5,1)*clhs453;
const double clhs471 =             DN(1,2)*DN(5,2);
const double clhs472 =             clhs400 + clhs471*mu;
const double clhs473 =             DN(1,2)*N[5];
const double clhs474 =             DN(6,0)*clhs453;
const double clhs475 =             DN(6,1)*clhs453;
const double clhs476 =             DN(1,2)*DN(6,2);
const double clhs477 =             clhs406 + clhs476*mu;
const double clhs478 =             DN(1,2)*N[6];
const double clhs479 =             DN(7,0)*clhs453;
const double clhs480 =             DN(7,1)*clhs453;
const double clhs481 =             DN(1,2)*DN(7,2);
const double clhs482 =             clhs412 + clhs481*mu;
const double clhs483 =             DN(1,2)*N[7];
const double clhs484 =             DN(2,0)*N[1];
const double clhs485 =             DN(2,1)*N[1];
const double clhs486 =             DN(2,2)*N[1];
const double clhs487 =             clhs343*(clhs380 + clhs422 + clhs456);
const double clhs488 =             DN(3,0)*N[1];
const double clhs489 =             DN(3,1)*N[1];
const double clhs490 =             DN(3,2)*N[1];
const double clhs491 =             clhs343*(clhs387 + clhs427 + clhs461);
const double clhs492 =             DN(4,0)*N[1];
const double clhs493 =             DN(4,1)*N[1];
const double clhs494 =             DN(4,2)*N[1];
const double clhs495 =             clhs343*(clhs393 + clhs432 + clhs466);
const double clhs496 =             DN(5,0)*N[1];
const double clhs497 =             DN(5,1)*N[1];
const double clhs498 =             DN(5,2)*N[1];
const double clhs499 =             clhs343*(clhs399 + clhs437 + clhs471);
const double clhs500 =             DN(6,0)*N[1];
const double clhs501 =             DN(6,1)*N[1];
const double clhs502 =             DN(6,2)*N[1];
const double clhs503 =             clhs343*(clhs405 + clhs442 + clhs476);
const double clhs504 =             DN(7,0)*N[1];
const double clhs505 =             DN(7,1)*N[1];
const double clhs506 =             DN(7,2)*N[1];
const double clhs507 =             clhs343*(clhs411 + clhs447 + clhs481);
const double clhs508 =             pow(DN(2,0), 2);
const double clhs509 =             pow(N[2], 2)*clhs1;
const double clhs510 =             DN(2,0)*mu;
const double clhs511 =             DN(2,1)*clhs510;
const double clhs512 =             DN(2,2)*clhs510;
const double clhs513 =             DN(2,0)*N[2];
const double clhs514 =             DN(2,0)*DN(3,0);
const double clhs515 =             N[2]*clhs1;
const double clhs516 =             N[3]*clhs515;
const double clhs517 =             clhs514*mu + clhs516;
const double clhs518 =             DN(3,1)*clhs510;
const double clhs519 =             DN(3,2)*clhs510;
const double clhs520 =             DN(2,0)*N[3];
const double clhs521 =             DN(2,0)*DN(4,0);
const double clhs522 =             N[4]*clhs515;
const double clhs523 =             clhs521*mu + clhs522;
const double clhs524 =             DN(4,1)*clhs510;
const double clhs525 =             DN(4,2)*clhs510;
const double clhs526 =             DN(2,0)*N[4];
const double clhs527 =             DN(2,0)*DN(5,0);
const double clhs528 =             N[5]*clhs515;
const double clhs529 =             clhs527*mu + clhs528;
const double clhs530 =             DN(5,1)*clhs510;
const double clhs531 =             DN(5,2)*clhs510;
const double clhs532 =             DN(2,0)*N[5];
const double clhs533 =             DN(2,0)*DN(6,0);
const double clhs534 =             N[6]*clhs515;
const double clhs535 =             clhs533*mu + clhs534;
const double clhs536 =             DN(6,1)*clhs510;
const double clhs537 =             DN(6,2)*clhs510;
const double clhs538 =             DN(2,0)*N[6];
const double clhs539 =             DN(2,0)*DN(7,0);
const double clhs540 =             N[7]*clhs515;
const double clhs541 =             clhs539*mu + clhs540;
const double clhs542 =             DN(7,1)*clhs510;
const double clhs543 =             DN(7,2)*clhs510;
const double clhs544 =             DN(2,0)*N[7];
const double clhs545 =             pow(DN(2,1), 2);
const double clhs546 =             DN(2,1)*mu;
const double clhs547 =             DN(2,2)*clhs546;
const double clhs548 =             DN(2,1)*N[2];
const double clhs549 =             DN(3,0)*clhs546;
const double clhs550 =             DN(2,1)*DN(3,1);
const double clhs551 =             clhs516 + clhs550*mu;
const double clhs552 =             DN(3,2)*clhs546;
const double clhs553 =             DN(2,1)*N[3];
const double clhs554 =             DN(4,0)*clhs546;
const double clhs555 =             DN(2,1)*DN(4,1);
const double clhs556 =             clhs522 + clhs555*mu;
const double clhs557 =             DN(4,2)*clhs546;
const double clhs558 =             DN(2,1)*N[4];
const double clhs559 =             DN(5,0)*clhs546;
const double clhs560 =             DN(2,1)*DN(5,1);
const double clhs561 =             clhs528 + clhs560*mu;
const double clhs562 =             DN(5,2)*clhs546;
const double clhs563 =             DN(2,1)*N[5];
const double clhs564 =             DN(6,0)*clhs546;
const double clhs565 =             DN(2,1)*DN(6,1);
const double clhs566 =             clhs534 + clhs565*mu;
const double clhs567 =             DN(6,2)*clhs546;
const double clhs568 =             DN(2,1)*N[6];
const double clhs569 =             DN(7,0)*clhs546;
const double clhs570 =             DN(2,1)*DN(7,1);
const double clhs571 =             clhs540 + clhs570*mu;
const double clhs572 =             DN(7,2)*clhs546;
const double clhs573 =             DN(2,1)*N[7];
const double clhs574 =             pow(DN(2,2), 2);
const double clhs575 =             DN(2,2)*N[2];
const double clhs576 =             DN(2,2)*mu;
const double clhs577 =             DN(3,0)*clhs576;
const double clhs578 =             DN(3,1)*clhs576;
const double clhs579 =             DN(2,2)*DN(3,2);
const double clhs580 =             clhs516 + clhs579*mu;
const double clhs581 =             DN(2,2)*N[3];
const double clhs582 =             DN(4,0)*clhs576;
const double clhs583 =             DN(4,1)*clhs576;
const double clhs584 =             DN(2,2)*DN(4,2);
const double clhs585 =             clhs522 + clhs584*mu;
const double clhs586 =             DN(2,2)*N[4];
const double clhs587 =             DN(5,0)*clhs576;
const double clhs588 =             DN(5,1)*clhs576;
const double clhs589 =             DN(2,2)*DN(5,2);
const double clhs590 =             clhs528 + clhs589*mu;
const double clhs591 =             DN(2,2)*N[5];
const double clhs592 =             DN(6,0)*clhs576;
const double clhs593 =             DN(6,1)*clhs576;
const double clhs594 =             DN(2,2)*DN(6,2);
const double clhs595 =             clhs534 + clhs594*mu;
const double clhs596 =             DN(2,2)*N[6];
const double clhs597 =             DN(7,0)*clhs576;
const double clhs598 =             DN(7,1)*clhs576;
const double clhs599 =             DN(2,2)*DN(7,2);
const double clhs600 =             clhs540 + clhs599*mu;
const double clhs601 =             DN(2,2)*N[7];
const double clhs602 =             DN(3,0)*N[2];
const double clhs603 =             DN(3,1)*N[2];
const double clhs604 =             DN(3,2)*N[2];
const double clhs605 =             clhs343*(clhs514 + clhs550 + clhs579);
const double clhs606 =             DN(4,0)*N[2];
const double clhs607 =             DN(4,1)*N[2];
const double clhs608 =             DN(4,2)*N[2];
const double clhs609 =             clhs343*(clhs521 + clhs555 + clhs584);
const double clhs610 =             DN(5,0)*N[2];
const double clhs611 =             DN(5,1)*N[2];
const double clhs612 =             DN(5,2)*N[2];
const double clhs613 =             clhs343*(clhs527 + clhs560 + clhs589);
const double clhs614 =             DN(6,0)*N[2];
const double clhs615 =             DN(6,1)*N[2];
const double clhs616 =             DN(6,2)*N[2];
const double clhs617 =             clhs343*(clhs533 + clhs565 + clhs594);
const double clhs618 =             DN(7,0)*N[2];
const double clhs619 =             DN(7,1)*N[2];
const double clhs620 =             DN(7,2)*N[2];
const double clhs621 =             clhs343*(clhs539 + clhs570 + clhs599);
const double clhs622 =             pow(DN(3,0), 2);
const double clhs623 =             pow(N[3], 2)*clhs1;
const double clhs624 =             DN(3,0)*mu;
const double clhs625 =             DN(3,1)*clhs624;
const double clhs626 =             DN(3,2)*clhs624;
const double clhs627 =             DN(3,0)*N[3];
const double clhs628 =             DN(3,0)*DN(4,0);
const double clhs629 =             N[3]*clhs1;
const double clhs630 =             N[4]*clhs629;
const double clhs631 =             clhs628*mu + clhs630;
const double clhs632 =             DN(4,1)*clhs624;
const double clhs633 =             DN(4,2)*clhs624;
const double clhs634 =             DN(3,0)*N[4];
const double clhs635 =             DN(3,0)*DN(5,0);
const double clhs636 =             N[5]*clhs629;
const double clhs637 =             clhs635*mu + clhs636;
const double clhs638 =             DN(5,1)*clhs624;
const double clhs639 =             DN(5,2)*clhs624;
const double clhs640 =             DN(3,0)*N[5];
const double clhs641 =             DN(3,0)*DN(6,0);
const double clhs642 =             N[6]*clhs629;
const double clhs643 =             clhs641*mu + clhs642;
const double clhs644 =             DN(6,1)*clhs624;
const double clhs645 =             DN(6,2)*clhs624;
const double clhs646 =             DN(3,0)*N[6];
const double clhs647 =             DN(3,0)*DN(7,0);
const double clhs648 =             N[7]*clhs629;
const double clhs649 =             clhs647*mu + clhs648;
const double clhs650 =             DN(7,1)*clhs624;
const double clhs651 =             DN(7,2)*clhs624;
const double clhs652 =             DN(3,0)*N[7];
const double clhs653 =             pow(DN(3,1), 2);
const double clhs654 =             DN(3,1)*mu;
const double clhs655 =             DN(3,2)*clhs654;
const double clhs656 =             DN(3,1)*N[3];
const double clhs657 =             DN(4,0)*clhs654;
const double clhs658 =             DN(3,1)*DN(4,1);
const double clhs659 =             clhs630 + clhs658*mu;
const double clhs660 =             DN(4,2)*clhs654;
const double clhs661 =             DN(3,1)*N[4];
const double clhs662 =             DN(5,0)*clhs654;
const double clhs663 =             DN(3,1)*DN(5,1);
const double clhs664 =             clhs636 + clhs663*mu;
const double clhs665 =             DN(5,2)*clhs654;
const double clhs666 =             DN(3,1)*N[5];
const double clhs667 =             DN(6,0)*clhs654;
const double clhs668 =             DN(3,1)*DN(6,1);
const double clhs669 =             clhs642 + clhs668*mu;
const double clhs670 =             DN(6,2)*clhs654;
const double clhs671 =             DN(3,1)*N[6];
const double clhs672 =             DN(7,0)*clhs654;
const double clhs673 =             DN(3,1)*DN(7,1);
const double clhs674 =             clhs648 + clhs673*mu;
const double clhs675 =             DN(7,2)*clhs654;
const double clhs676 =             DN(3,1)*N[7];
const double clhs677 =             pow(DN(3,2), 2);
const double clhs678 =             DN(3,2)*N[3];
const double clhs679 =             DN(3,2)*mu;
const double clhs680 =             DN(4,0)*clhs679;
const double clhs681 =             DN(4,1)*clhs679;
const double clhs682 =             DN(3,2)*DN(4,2);
const double clhs683 =             clhs630 + clhs682*mu;
const double clhs684 =             DN(3,2)*N[4];
const double clhs685 =             DN(5,0)*clhs679;
const double clhs686 =             DN(5,1)*clhs679;
const double clhs687 =             DN(3,2)*DN(5,2);
const double clhs688 =             clhs636 + clhs687*mu;
const double clhs689 =             DN(3,2)*N[5];
const double clhs690 =             DN(6,0)*clhs679;
const double clhs691 =             DN(6,1)*clhs679;
const double clhs692 =             DN(3,2)*DN(6,2);
const double clhs693 =             clhs642 + clhs692*mu;
const double clhs694 =             DN(3,2)*N[6];
const double clhs695 =             DN(7,0)*clhs679;
const double clhs696 =             DN(7,1)*clhs679;
const double clhs697 =             DN(3,2)*DN(7,2);
const double clhs698 =             clhs648 + clhs697*mu;
const double clhs699 =             DN(3,2)*N[7];
const double clhs700 =             DN(4,0)*N[3];
const double clhs701 =             DN(4,1)*N[3];
const double clhs702 =             DN(4,2)*N[3];
const double clhs703 =             clhs343*(clhs628 + clhs658 + clhs682);
const double clhs704 =             DN(5,0)*N[3];
const double clhs705 =             DN(5,1)*N[3];
const double clhs706 =             DN(5,2)*N[3];
const double clhs707 =             clhs343*(clhs635 + clhs663 + clhs687);
const double clhs708 =             DN(6,0)*N[3];
const double clhs709 =             DN(6,1)*N[3];
const double clhs710 =             DN(6,2)*N[3];
const double clhs711 =             clhs343*(clhs641 + clhs668 + clhs692);
const double clhs712 =             DN(7,0)*N[3];
const double clhs713 =             DN(7,1)*N[3];
const double clhs714 =             DN(7,2)*N[3];
const double clhs715 =             clhs343*(clhs647 + clhs673 + clhs697);
const double clhs716 =             pow(DN(4,0), 2);
const double clhs717 =             pow(N[4], 2)*clhs1;
const double clhs718 =             DN(4,0)*mu;
const double clhs719 =             DN(4,1)*clhs718;
const double clhs720 =             DN(4,2)*clhs718;
const double clhs721 =             DN(4,0)*N[4];
const double clhs722 =             DN(4,0)*DN(5,0);
const double clhs723 =             N[4]*clhs1;
const double clhs724 =             N[5]*clhs723;
const double clhs725 =             clhs722*mu + clhs724;
const double clhs726 =             DN(5,1)*clhs718;
const double clhs727 =             DN(5,2)*clhs718;
const double clhs728 =             DN(4,0)*N[5];
const double clhs729 =             DN(4,0)*DN(6,0);
const double clhs730 =             N[6]*clhs723;
const double clhs731 =             clhs729*mu + clhs730;
const double clhs732 =             DN(6,1)*clhs718;
const double clhs733 =             DN(6,2)*clhs718;
const double clhs734 =             DN(4,0)*N[6];
const double clhs735 =             DN(4,0)*DN(7,0);
const double clhs736 =             N[7]*clhs723;
const double clhs737 =             clhs735*mu + clhs736;
const double clhs738 =             DN(7,1)*clhs718;
const double clhs739 =             DN(7,2)*clhs718;
const double clhs740 =             DN(4,0)*N[7];
const double clhs741 =             pow(DN(4,1), 2);
const double clhs742 =             DN(4,1)*mu;
const double clhs743 =             DN(4,2)*clhs742;
const double clhs744 =             DN(4,1)*N[4];
const double clhs745 =             DN(5,0)*clhs742;
const double clhs746 =             DN(4,1)*DN(5,1);
const double clhs747 =             clhs724 + clhs746*mu;
const double clhs748 =             DN(5,2)*clhs742;
const double clhs749 =             DN(4,1)*N[5];
const double clhs750 =             DN(6,0)*clhs742;
const double clhs751 =             DN(4,1)*DN(6,1);
const double clhs752 =             clhs730 + clhs751*mu;
const double clhs753 =             DN(6,2)*clhs742;
const double clhs754 =             DN(4,1)*N[6];
const double clhs755 =             DN(7,0)*clhs742;
const double clhs756 =             DN(4,1)*DN(7,1);
const double clhs757 =             clhs736 + clhs756*mu;
const double clhs758 =             DN(7,2)*clhs742;
const double clhs759 =             DN(4,1)*N[7];
const double clhs760 =             pow(DN(4,2), 2);
const double clhs761 =             DN(4,2)*N[4];
const double clhs762 =             DN(4,2)*mu;
const double clhs763 =             DN(5,0)*clhs762;
const double clhs764 =             DN(5,1)*clhs762;
const double clhs765 =             DN(4,2)*DN(5,2);
const double clhs766 =             clhs724 + clhs765*mu;
const double clhs767 =             DN(4,2)*N[5];
const double clhs768 =             DN(6,0)*clhs762;
const double clhs769 =             DN(6,1)*clhs762;
const double clhs770 =             DN(4,2)*DN(6,2);
const double clhs771 =             clhs730 + clhs770*mu;
const double clhs772 =             DN(4,2)*N[6];
const double clhs773 =             DN(7,0)*clhs762;
const double clhs774 =             DN(7,1)*clhs762;
const double clhs775 =             DN(4,2)*DN(7,2);
const double clhs776 =             clhs736 + clhs775*mu;
const double clhs777 =             DN(4,2)*N[7];
const double clhs778 =             DN(5,0)*N[4];
const double clhs779 =             DN(5,1)*N[4];
const double clhs780 =             DN(5,2)*N[4];
const double clhs781 =             clhs343*(clhs722 + clhs746 + clhs765);
const double clhs782 =             DN(6,0)*N[4];
const double clhs783 =             DN(6,1)*N[4];
const double clhs784 =             DN(6,2)*N[4];
const double clhs785 =             clhs343*(clhs729 + clhs751 + clhs770);
const double clhs786 =             DN(7,0)*N[4];
const double clhs787 =             DN(7,1)*N[4];
const double clhs788 =             DN(7,2)*N[4];
const double clhs789 =             clhs343*(clhs735 + clhs756 + clhs775);
const double clhs790 =             pow(DN(5,0), 2);
const double clhs791 =             pow(N[5], 2)*clhs1;
const double clhs792 =             DN(5,0)*mu;
const double clhs793 =             DN(5,1)*clhs792;
const double clhs794 =             DN(5,2)*clhs792;
const double clhs795 =             DN(5,0)*N[5];
const double clhs796 =             DN(5,0)*DN(6,0);
const double clhs797 =             N[5]*clhs1;
const double clhs798 =             N[6]*clhs797;
const double clhs799 =             clhs796*mu + clhs798;
const double clhs800 =             DN(6,1)*clhs792;
const double clhs801 =             DN(6,2)*clhs792;
const double clhs802 =             DN(5,0)*N[6];
const double clhs803 =             DN(5,0)*DN(7,0);
const double clhs804 =             N[7]*clhs797;
const double clhs805 =             clhs803*mu + clhs804;
const double clhs806 =             DN(7,1)*clhs792;
const double clhs807 =             DN(7,2)*clhs792;
const double clhs808 =             DN(5,0)*N[7];
const double clhs809 =             pow(DN(5,1), 2);
const double clhs810 =             DN(5,1)*mu;
const double clhs811 =             DN(5,2)*clhs810;
const double clhs812 =             DN(5,1)*N[5];
const double clhs813 =             DN(6,0)*clhs810;
const double clhs814 =             DN(5,1)*DN(6,1);
const double clhs815 =             clhs798 + clhs814*mu;
const double clhs816 =             DN(6,2)*clhs810;
const double clhs817 =             DN(5,1)*N[6];
const double clhs818 =             DN(7,0)*clhs810;
const double clhs819 =             DN(5,1)*DN(7,1);
const double clhs820 =             clhs804 + clhs819*mu;
const double clhs821 =             DN(7,2)*clhs810;
const double clhs822 =             DN(5,1)*N[7];
const double clhs823 =             pow(DN(5,2), 2);
const double clhs824 =             DN(5,2)*N[5];
const double clhs825 =             DN(5,2)*mu;
const double clhs826 =             DN(6,0)*clhs825;
const double clhs827 =             DN(6,1)*clhs825;
const double clhs828 =             DN(5,2)*DN(6,2);
const double clhs829 =             clhs798 + clhs828*mu;
const double clhs830 =             DN(5,2)*N[6];
const double clhs831 =             DN(7,0)*clhs825;
const double clhs832 =             DN(7,1)*clhs825;
const double clhs833 =             DN(5,2)*DN(7,2);
const double clhs834 =             clhs804 + clhs833*mu;
const double clhs835 =             DN(5,2)*N[7];
const double clhs836 =             DN(6,0)*N[5];
const double clhs837 =             DN(6,1)*N[5];
const double clhs838 =             DN(6,2)*N[5];
const double clhs839 =             clhs343*(clhs796 + clhs814 + clhs828);
const double clhs840 =             DN(7,0)*N[5];
const double clhs841 =             DN(7,1)*N[5];
const double clhs842 =             DN(7,2)*N[5];
const double clhs843 =             clhs343*(clhs803 + clhs819 + clhs833);
const double clhs844 =             pow(DN(6,0), 2);
const double clhs845 =             pow(N[6], 2)*clhs1;
const double clhs846 =             DN(6,0)*mu;
const double clhs847 =             DN(6,1)*clhs846;
const double clhs848 =             DN(6,2)*clhs846;
const double clhs849 =             DN(6,0)*N[6];
const double clhs850 =             DN(6,0)*DN(7,0);
const double clhs851 =             N[6]*N[7]*clhs1;
const double clhs852 =             clhs850*mu + clhs851;
const double clhs853 =             DN(7,1)*clhs846;
const double clhs854 =             DN(7,2)*clhs846;
const double clhs855 =             DN(6,0)*N[7];
const double clhs856 =             pow(DN(6,1), 2);
const double clhs857 =             DN(6,1)*mu;
const double clhs858 =             DN(6,2)*clhs857;
const double clhs859 =             DN(6,1)*N[6];
const double clhs860 =             DN(7,0)*clhs857;
const double clhs861 =             DN(6,1)*DN(7,1);
const double clhs862 =             clhs851 + clhs861*mu;
const double clhs863 =             DN(7,2)*clhs857;
const double clhs864 =             DN(6,1)*N[7];
const double clhs865 =             pow(DN(6,2), 2);
const double clhs866 =             DN(6,2)*N[6];
const double clhs867 =             DN(6,2)*mu;
const double clhs868 =             DN(7,0)*clhs867;
const double clhs869 =             DN(7,1)*clhs867;
const double clhs870 =             DN(6,2)*DN(7,2);
const double clhs871 =             clhs851 + clhs870*mu;
const double clhs872 =             DN(6,2)*N[7];
const double clhs873 =             DN(7,0)*N[6];
const double clhs874 =             DN(7,1)*N[6];
const double clhs875 =             DN(7,2)*N[6];
const double clhs876 =             clhs343*(clhs850 + clhs861 + clhs870);
const double clhs877 =             pow(DN(7,0), 2);
const double clhs878 =             pow(N[7], 2)*clhs1;
const double clhs879 =             DN(7,0)*mu;
const double clhs880 =             DN(7,1)*clhs879;
const double clhs881 =             DN(7,2)*clhs879;
const double clhs882 =             DN(7,0)*N[7];
const double clhs883 =             pow(DN(7,1), 2);
const double clhs884 =             DN(7,1)*DN(7,2)*mu;
const double clhs885 =             DN(7,1)*N[7];
const double clhs886 =             pow(DN(7,2), 2);
const double clhs887 =             DN(7,2)*N[7];
            lhs(0,0)=DN(0,0)*clhs3 + DN(0,1)*clhs5 + DN(0,2)*clhs7 + clhs0*mu + clhs2;
            lhs(0,1)=DN(0,0)*clhs10 + DN(0,1)*clhs12 + DN(0,2)*clhs15 + clhs9;
            lhs(0,2)=DN(0,0)*clhs17 + DN(0,1)*clhs19 + DN(0,2)*clhs21 + clhs16;
            lhs(0,3)=-clhs22;
            lhs(0,4)=DN(0,0)*clhs23 + DN(0,1)*clhs25 + DN(0,2)*clhs27 + clhs31;
            lhs(0,5)=DN(0,0)*clhs33 + DN(0,1)*clhs35 + DN(0,2)*clhs38 + clhs32;
            lhs(0,6)=DN(0,0)*clhs40 + DN(0,1)*clhs42 + DN(0,2)*clhs44 + clhs39;
            lhs(0,7)=-clhs45;
            lhs(0,8)=DN(0,0)*clhs46 + DN(0,1)*clhs48 + DN(0,2)*clhs50 + clhs53;
            lhs(0,9)=DN(0,0)*clhs55 + DN(0,1)*clhs57 + DN(0,2)*clhs60 + clhs54;
            lhs(0,10)=DN(0,0)*clhs62 + DN(0,1)*clhs64 + DN(0,2)*clhs66 + clhs61;
            lhs(0,11)=-clhs67;
            lhs(0,12)=DN(0,0)*clhs68 + DN(0,1)*clhs70 + DN(0,2)*clhs72 + clhs75;
            lhs(0,13)=DN(0,0)*clhs77 + DN(0,1)*clhs79 + DN(0,2)*clhs82 + clhs76;
            lhs(0,14)=DN(0,0)*clhs84 + DN(0,1)*clhs86 + DN(0,2)*clhs88 + clhs83;
            lhs(0,15)=-clhs89;
            lhs(0,16)=DN(0,0)*clhs90 + DN(0,1)*clhs92 + DN(0,2)*clhs94 + clhs97;
            lhs(0,17)=DN(0,0)*clhs99 + DN(0,1)*clhs101 + DN(0,2)*clhs104 + clhs98;
            lhs(0,18)=DN(0,0)*clhs106 + DN(0,1)*clhs108 + DN(0,2)*clhs110 + clhs105;
            lhs(0,19)=-clhs111;
            lhs(0,20)=DN(0,0)*clhs112 + DN(0,1)*clhs114 + DN(0,2)*clhs116 + clhs119;
            lhs(0,21)=DN(0,0)*clhs121 + DN(0,1)*clhs123 + DN(0,2)*clhs126 + clhs120;
            lhs(0,22)=DN(0,0)*clhs128 + DN(0,1)*clhs130 + DN(0,2)*clhs132 + clhs127;
            lhs(0,23)=-clhs133;
            lhs(0,24)=DN(0,0)*clhs134 + DN(0,1)*clhs136 + DN(0,2)*clhs138 + clhs141;
            lhs(0,25)=DN(0,0)*clhs143 + DN(0,1)*clhs145 + DN(0,2)*clhs148 + clhs142;
            lhs(0,26)=DN(0,0)*clhs150 + DN(0,1)*clhs152 + DN(0,2)*clhs154 + clhs149;
            lhs(0,27)=-clhs155;
            lhs(0,28)=DN(0,0)*clhs156 + DN(0,1)*clhs158 + DN(0,2)*clhs160 + clhs163;
            lhs(0,29)=DN(0,0)*clhs165 + DN(0,1)*clhs167 + DN(0,2)*clhs170 + clhs164;
            lhs(0,30)=DN(0,0)*clhs172 + DN(0,1)*clhs174 + DN(0,2)*clhs176 + clhs171;
            lhs(0,31)=-clhs177;
            lhs(1,0)=DN(0,0)*clhs5 + DN(0,1)*clhs178 + DN(0,2)*clhs179 + clhs9;
            lhs(1,1)=DN(0,0)*clhs12 + DN(0,1)*clhs181 + DN(0,2)*clhs183 + clhs180*mu + clhs2;
            lhs(1,2)=DN(0,0)*clhs19 + DN(0,1)*clhs186 + DN(0,2)*clhs188 + clhs185;
            lhs(1,3)=-clhs189;
            lhs(1,4)=DN(0,0)*clhs25 + DN(0,1)*clhs191 + DN(0,2)*clhs192 + clhs190;
            lhs(1,5)=DN(0,0)*clhs35 + DN(0,1)*clhs193 + DN(0,2)*clhs195 + clhs197;
            lhs(1,6)=DN(0,0)*clhs42 + DN(0,1)*clhs199 + DN(0,2)*clhs201 + clhs198;
            lhs(1,7)=-clhs202;
            lhs(1,8)=DN(0,0)*clhs48 + DN(0,1)*clhs204 + DN(0,2)*clhs205 + clhs203;
            lhs(1,9)=DN(0,0)*clhs57 + DN(0,1)*clhs206 + DN(0,2)*clhs208 + clhs210;
            lhs(1,10)=DN(0,0)*clhs64 + DN(0,1)*clhs212 + DN(0,2)*clhs214 + clhs211;
            lhs(1,11)=-clhs215;
            lhs(1,12)=DN(0,0)*clhs70 + DN(0,1)*clhs217 + DN(0,2)*clhs218 + clhs216;
            lhs(1,13)=DN(0,0)*clhs79 + DN(0,1)*clhs219 + DN(0,2)*clhs221 + clhs223;
            lhs(1,14)=DN(0,0)*clhs86 + DN(0,1)*clhs225 + DN(0,2)*clhs227 + clhs224;
            lhs(1,15)=-clhs228;
            lhs(1,16)=DN(0,0)*clhs92 + DN(0,1)*clhs230 + DN(0,2)*clhs231 + clhs229;
            lhs(1,17)=DN(0,0)*clhs101 + DN(0,1)*clhs232 + DN(0,2)*clhs234 + clhs236;
            lhs(1,18)=DN(0,0)*clhs108 + DN(0,1)*clhs238 + DN(0,2)*clhs240 + clhs237;
            lhs(1,19)=-clhs241;
            lhs(1,20)=DN(0,0)*clhs114 + DN(0,1)*clhs243 + DN(0,2)*clhs244 + clhs242;
            lhs(1,21)=DN(0,0)*clhs123 + DN(0,1)*clhs245 + DN(0,2)*clhs247 + clhs249;
            lhs(1,22)=DN(0,0)*clhs130 + DN(0,1)*clhs251 + DN(0,2)*clhs253 + clhs250;
            lhs(1,23)=-clhs254;
            lhs(1,24)=DN(0,0)*clhs136 + DN(0,1)*clhs256 + DN(0,2)*clhs257 + clhs255;
            lhs(1,25)=DN(0,0)*clhs145 + DN(0,1)*clhs258 + DN(0,2)*clhs260 + clhs262;
            lhs(1,26)=DN(0,0)*clhs152 + DN(0,1)*clhs264 + DN(0,2)*clhs266 + clhs263;
            lhs(1,27)=-clhs267;
            lhs(1,28)=DN(0,0)*clhs158 + DN(0,1)*clhs269 + DN(0,2)*clhs270 + clhs268;
            lhs(1,29)=DN(0,0)*clhs167 + DN(0,1)*clhs271 + DN(0,2)*clhs273 + clhs275;
            lhs(1,30)=DN(0,0)*clhs174 + DN(0,1)*clhs277 + DN(0,2)*clhs279 + clhs276;
            lhs(1,31)=-clhs280;
            lhs(2,0)=DN(0,0)*clhs7 + DN(0,1)*clhs179 + DN(0,2)*clhs281 + clhs16;
            lhs(2,1)=DN(0,0)*clhs15 + DN(0,1)*clhs183 + DN(0,2)*clhs282 + clhs185;
            lhs(2,2)=DN(0,0)*clhs21 + DN(0,1)*clhs188 + DN(0,2)*clhs284 + clhs2 + clhs283*mu;
            lhs(2,3)=-clhs285;
            lhs(2,4)=DN(0,0)*clhs27 + DN(0,1)*clhs192 + DN(0,2)*clhs288 + clhs287;
            lhs(2,5)=DN(0,0)*clhs38 + DN(0,1)*clhs195 + DN(0,2)*clhs290 + clhs289;
            lhs(2,6)=DN(0,0)*clhs44 + DN(0,1)*clhs201 + DN(0,2)*clhs291 + clhs293;
            lhs(2,7)=-clhs294;
            lhs(2,8)=DN(0,0)*clhs50 + DN(0,1)*clhs205 + DN(0,2)*clhs296 + clhs295;
            lhs(2,9)=DN(0,0)*clhs60 + DN(0,1)*clhs208 + DN(0,2)*clhs298 + clhs297;
            lhs(2,10)=DN(0,0)*clhs66 + DN(0,1)*clhs214 + DN(0,2)*clhs299 + clhs301;
            lhs(2,11)=-clhs302;
            lhs(2,12)=DN(0,0)*clhs72 + DN(0,1)*clhs218 + DN(0,2)*clhs304 + clhs303;
            lhs(2,13)=DN(0,0)*clhs82 + DN(0,1)*clhs221 + DN(0,2)*clhs306 + clhs305;
            lhs(2,14)=DN(0,0)*clhs88 + DN(0,1)*clhs227 + DN(0,2)*clhs307 + clhs309;
            lhs(2,15)=-clhs310;
            lhs(2,16)=DN(0,0)*clhs94 + DN(0,1)*clhs231 + DN(0,2)*clhs312 + clhs311;
            lhs(2,17)=DN(0,0)*clhs104 + DN(0,1)*clhs234 + DN(0,2)*clhs314 + clhs313;
            lhs(2,18)=DN(0,0)*clhs110 + DN(0,1)*clhs240 + DN(0,2)*clhs315 + clhs317;
            lhs(2,19)=-clhs318;
            lhs(2,20)=DN(0,0)*clhs116 + DN(0,1)*clhs244 + DN(0,2)*clhs320 + clhs319;
            lhs(2,21)=DN(0,0)*clhs126 + DN(0,1)*clhs247 + DN(0,2)*clhs322 + clhs321;
            lhs(2,22)=DN(0,0)*clhs132 + DN(0,1)*clhs253 + DN(0,2)*clhs323 + clhs325;
            lhs(2,23)=-clhs326;
            lhs(2,24)=DN(0,0)*clhs138 + DN(0,1)*clhs257 + DN(0,2)*clhs328 + clhs327;
            lhs(2,25)=DN(0,0)*clhs148 + DN(0,1)*clhs260 + DN(0,2)*clhs330 + clhs329;
            lhs(2,26)=DN(0,0)*clhs154 + DN(0,1)*clhs266 + DN(0,2)*clhs331 + clhs333;
            lhs(2,27)=-clhs334;
            lhs(2,28)=DN(0,0)*clhs160 + DN(0,1)*clhs270 + DN(0,2)*clhs336 + clhs335;
            lhs(2,29)=DN(0,0)*clhs170 + DN(0,1)*clhs273 + DN(0,2)*clhs338 + clhs337;
            lhs(2,30)=DN(0,0)*clhs176 + DN(0,1)*clhs279 + DN(0,2)*clhs339 + clhs341;
            lhs(2,31)=-clhs342;
            lhs(3,0)=clhs22*clhs345;
            lhs(3,1)=clhs189*clhs345;
            lhs(3,2)=clhs285*clhs345;
            lhs(3,3)=clhs343*(clhs0 + clhs180 + clhs283);
            lhs(3,4)=clhs344*clhs45 + clhs346;
            lhs(3,5)=clhs202*clhs344 + clhs347;
            lhs(3,6)=clhs294*clhs344 + clhs348;
            lhs(3,7)=clhs349;
            lhs(3,8)=clhs344*clhs67 + clhs350;
            lhs(3,9)=clhs215*clhs344 + clhs351;
            lhs(3,10)=clhs302*clhs344 + clhs352;
            lhs(3,11)=clhs353;
            lhs(3,12)=clhs344*clhs89 + clhs354;
            lhs(3,13)=clhs228*clhs344 + clhs355;
            lhs(3,14)=clhs310*clhs344 + clhs356;
            lhs(3,15)=clhs357;
            lhs(3,16)=clhs111*clhs344 + clhs358;
            lhs(3,17)=clhs241*clhs344 + clhs359;
            lhs(3,18)=clhs318*clhs344 + clhs360;
            lhs(3,19)=clhs361;
            lhs(3,20)=clhs133*clhs344 + clhs362;
            lhs(3,21)=clhs254*clhs344 + clhs363;
            lhs(3,22)=clhs326*clhs344 + clhs364;
            lhs(3,23)=clhs365;
            lhs(3,24)=clhs155*clhs344 + clhs366;
            lhs(3,25)=clhs267*clhs344 + clhs367;
            lhs(3,26)=clhs334*clhs344 + clhs368;
            lhs(3,27)=clhs369;
            lhs(3,28)=clhs177*clhs344 + clhs370;
            lhs(3,29)=clhs280*clhs344 + clhs371;
            lhs(3,30)=clhs342*clhs344 + clhs372;
            lhs(3,31)=clhs373;
            lhs(4,0)=DN(1,0)*clhs3 + DN(1,1)*clhs5 + DN(1,2)*clhs7 + clhs31;
            lhs(4,1)=DN(1,0)*clhs10 + DN(1,1)*clhs12 + DN(1,2)*clhs15 + clhs190;
            lhs(4,2)=DN(1,0)*clhs17 + DN(1,1)*clhs19 + DN(1,2)*clhs21 + clhs287;
            lhs(4,3)=-clhs346;
            lhs(4,4)=DN(1,0)*clhs23 + DN(1,1)*clhs25 + DN(1,2)*clhs27 + clhs374*mu + clhs375;
            lhs(4,5)=DN(1,0)*clhs33 + DN(1,1)*clhs35 + DN(1,2)*clhs38 + clhs377;
            lhs(4,6)=DN(1,0)*clhs40 + DN(1,1)*clhs42 + DN(1,2)*clhs44 + clhs378;
            lhs(4,7)=-clhs379;
            lhs(4,8)=DN(1,0)*clhs46 + DN(1,1)*clhs48 + DN(1,2)*clhs50 + clhs383;
            lhs(4,9)=DN(1,0)*clhs55 + DN(1,1)*clhs57 + DN(1,2)*clhs60 + clhs384;
            lhs(4,10)=DN(1,0)*clhs62 + DN(1,1)*clhs64 + DN(1,2)*clhs66 + clhs385;
            lhs(4,11)=-clhs386;
            lhs(4,12)=DN(1,0)*clhs68 + DN(1,1)*clhs70 + DN(1,2)*clhs72 + clhs389;
            lhs(4,13)=DN(1,0)*clhs77 + DN(1,1)*clhs79 + DN(1,2)*clhs82 + clhs390;
            lhs(4,14)=DN(1,0)*clhs84 + DN(1,1)*clhs86 + DN(1,2)*clhs88 + clhs391;
            lhs(4,15)=-clhs392;
            lhs(4,16)=DN(1,0)*clhs90 + DN(1,1)*clhs92 + DN(1,2)*clhs94 + clhs395;
            lhs(4,17)=DN(1,0)*clhs99 + DN(1,1)*clhs101 + DN(1,2)*clhs104 + clhs396;
            lhs(4,18)=DN(1,0)*clhs106 + DN(1,1)*clhs108 + DN(1,2)*clhs110 + clhs397;
            lhs(4,19)=-clhs398;
            lhs(4,20)=DN(1,0)*clhs112 + DN(1,1)*clhs114 + DN(1,2)*clhs116 + clhs401;
            lhs(4,21)=DN(1,0)*clhs121 + DN(1,1)*clhs123 + DN(1,2)*clhs126 + clhs402;
            lhs(4,22)=DN(1,0)*clhs128 + DN(1,1)*clhs130 + DN(1,2)*clhs132 + clhs403;
            lhs(4,23)=-clhs404;
            lhs(4,24)=DN(1,0)*clhs134 + DN(1,1)*clhs136 + DN(1,2)*clhs138 + clhs407;
            lhs(4,25)=DN(1,0)*clhs143 + DN(1,1)*clhs145 + DN(1,2)*clhs148 + clhs408;
            lhs(4,26)=DN(1,0)*clhs150 + DN(1,1)*clhs152 + DN(1,2)*clhs154 + clhs409;
            lhs(4,27)=-clhs410;
            lhs(4,28)=DN(1,0)*clhs156 + DN(1,1)*clhs158 + DN(1,2)*clhs160 + clhs413;
            lhs(4,29)=DN(1,0)*clhs165 + DN(1,1)*clhs167 + DN(1,2)*clhs170 + clhs414;
            lhs(4,30)=DN(1,0)*clhs172 + DN(1,1)*clhs174 + DN(1,2)*clhs176 + clhs415;
            lhs(4,31)=-clhs416;
            lhs(5,0)=DN(1,0)*clhs5 + DN(1,1)*clhs178 + DN(1,2)*clhs179 + clhs32;
            lhs(5,1)=DN(1,0)*clhs12 + DN(1,1)*clhs181 + DN(1,2)*clhs183 + clhs197;
            lhs(5,2)=DN(1,0)*clhs19 + DN(1,1)*clhs186 + DN(1,2)*clhs188 + clhs289;
            lhs(5,3)=-clhs347;
            lhs(5,4)=DN(1,0)*clhs25 + DN(1,1)*clhs191 + DN(1,2)*clhs192 + clhs377;
            lhs(5,5)=DN(1,0)*clhs35 + DN(1,1)*clhs193 + DN(1,2)*clhs195 + clhs375 + clhs417*mu;
            lhs(5,6)=DN(1,0)*clhs42 + DN(1,1)*clhs199 + DN(1,2)*clhs201 + clhs419;
            lhs(5,7)=-clhs420;
            lhs(5,8)=DN(1,0)*clhs48 + DN(1,1)*clhs204 + DN(1,2)*clhs205 + clhs421;
            lhs(5,9)=DN(1,0)*clhs57 + DN(1,1)*clhs206 + DN(1,2)*clhs208 + clhs423;
            lhs(5,10)=DN(1,0)*clhs64 + DN(1,1)*clhs212 + DN(1,2)*clhs214 + clhs424;
            lhs(5,11)=-clhs425;
            lhs(5,12)=DN(1,0)*clhs70 + DN(1,1)*clhs217 + DN(1,2)*clhs218 + clhs426;
            lhs(5,13)=DN(1,0)*clhs79 + DN(1,1)*clhs219 + DN(1,2)*clhs221 + clhs428;
            lhs(5,14)=DN(1,0)*clhs86 + DN(1,1)*clhs225 + DN(1,2)*clhs227 + clhs429;
            lhs(5,15)=-clhs430;
            lhs(5,16)=DN(1,0)*clhs92 + DN(1,1)*clhs230 + DN(1,2)*clhs231 + clhs431;
            lhs(5,17)=DN(1,0)*clhs101 + DN(1,1)*clhs232 + DN(1,2)*clhs234 + clhs433;
            lhs(5,18)=DN(1,0)*clhs108 + DN(1,1)*clhs238 + DN(1,2)*clhs240 + clhs434;
            lhs(5,19)=-clhs435;
            lhs(5,20)=DN(1,0)*clhs114 + DN(1,1)*clhs243 + DN(1,2)*clhs244 + clhs436;
            lhs(5,21)=DN(1,0)*clhs123 + DN(1,1)*clhs245 + DN(1,2)*clhs247 + clhs438;
            lhs(5,22)=DN(1,0)*clhs130 + DN(1,1)*clhs251 + DN(1,2)*clhs253 + clhs439;
            lhs(5,23)=-clhs440;
            lhs(5,24)=DN(1,0)*clhs136 + DN(1,1)*clhs256 + DN(1,2)*clhs257 + clhs441;
            lhs(5,25)=DN(1,0)*clhs145 + DN(1,1)*clhs258 + DN(1,2)*clhs260 + clhs443;
            lhs(5,26)=DN(1,0)*clhs152 + DN(1,1)*clhs264 + DN(1,2)*clhs266 + clhs444;
            lhs(5,27)=-clhs445;
            lhs(5,28)=DN(1,0)*clhs158 + DN(1,1)*clhs269 + DN(1,2)*clhs270 + clhs446;
            lhs(5,29)=DN(1,0)*clhs167 + DN(1,1)*clhs271 + DN(1,2)*clhs273 + clhs448;
            lhs(5,30)=DN(1,0)*clhs174 + DN(1,1)*clhs277 + DN(1,2)*clhs279 + clhs449;
            lhs(5,31)=-clhs450;
            lhs(6,0)=DN(1,0)*clhs7 + DN(1,1)*clhs179 + DN(1,2)*clhs281 + clhs39;
            lhs(6,1)=DN(1,0)*clhs15 + DN(1,1)*clhs183 + DN(1,2)*clhs282 + clhs198;
            lhs(6,2)=DN(1,0)*clhs21 + DN(1,1)*clhs188 + DN(1,2)*clhs284 + clhs293;
            lhs(6,3)=-clhs348;
            lhs(6,4)=DN(1,0)*clhs27 + DN(1,1)*clhs192 + DN(1,2)*clhs288 + clhs378;
            lhs(6,5)=DN(1,0)*clhs38 + DN(1,1)*clhs195 + DN(1,2)*clhs290 + clhs419;
            lhs(6,6)=DN(1,0)*clhs44 + DN(1,1)*clhs201 + DN(1,2)*clhs291 + clhs375 + clhs451*mu;
            lhs(6,7)=-clhs452;
            lhs(6,8)=DN(1,0)*clhs50 + DN(1,1)*clhs205 + DN(1,2)*clhs296 + clhs454;
            lhs(6,9)=DN(1,0)*clhs60 + DN(1,1)*clhs208 + DN(1,2)*clhs298 + clhs455;
            lhs(6,10)=DN(1,0)*clhs66 + DN(1,1)*clhs214 + DN(1,2)*clhs299 + clhs457;
            lhs(6,11)=-clhs458;
            lhs(6,12)=DN(1,0)*clhs72 + DN(1,1)*clhs218 + DN(1,2)*clhs304 + clhs459;
            lhs(6,13)=DN(1,0)*clhs82 + DN(1,1)*clhs221 + DN(1,2)*clhs306 + clhs460;
            lhs(6,14)=DN(1,0)*clhs88 + DN(1,1)*clhs227 + DN(1,2)*clhs307 + clhs462;
            lhs(6,15)=-clhs463;
            lhs(6,16)=DN(1,0)*clhs94 + DN(1,1)*clhs231 + DN(1,2)*clhs312 + clhs464;
            lhs(6,17)=DN(1,0)*clhs104 + DN(1,1)*clhs234 + DN(1,2)*clhs314 + clhs465;
            lhs(6,18)=DN(1,0)*clhs110 + DN(1,1)*clhs240 + DN(1,2)*clhs315 + clhs467;
            lhs(6,19)=-clhs468;
            lhs(6,20)=DN(1,0)*clhs116 + DN(1,1)*clhs244 + DN(1,2)*clhs320 + clhs469;
            lhs(6,21)=DN(1,0)*clhs126 + DN(1,1)*clhs247 + DN(1,2)*clhs322 + clhs470;
            lhs(6,22)=DN(1,0)*clhs132 + DN(1,1)*clhs253 + DN(1,2)*clhs323 + clhs472;
            lhs(6,23)=-clhs473;
            lhs(6,24)=DN(1,0)*clhs138 + DN(1,1)*clhs257 + DN(1,2)*clhs328 + clhs474;
            lhs(6,25)=DN(1,0)*clhs148 + DN(1,1)*clhs260 + DN(1,2)*clhs330 + clhs475;
            lhs(6,26)=DN(1,0)*clhs154 + DN(1,1)*clhs266 + DN(1,2)*clhs331 + clhs477;
            lhs(6,27)=-clhs478;
            lhs(6,28)=DN(1,0)*clhs160 + DN(1,1)*clhs270 + DN(1,2)*clhs336 + clhs479;
            lhs(6,29)=DN(1,0)*clhs170 + DN(1,1)*clhs273 + DN(1,2)*clhs338 + clhs480;
            lhs(6,30)=DN(1,0)*clhs176 + DN(1,1)*clhs279 + DN(1,2)*clhs339 + clhs482;
            lhs(6,31)=-clhs483;
            lhs(7,0)=clhs344*clhs346 + clhs45;
            lhs(7,1)=clhs202 + clhs344*clhs347;
            lhs(7,2)=clhs294 + clhs344*clhs348;
            lhs(7,3)=clhs349;
            lhs(7,4)=clhs345*clhs379;
            lhs(7,5)=clhs345*clhs420;
            lhs(7,6)=clhs345*clhs452;
            lhs(7,7)=clhs343*(clhs374 + clhs417 + clhs451);
            lhs(7,8)=clhs344*clhs386 + clhs484;
            lhs(7,9)=clhs344*clhs425 + clhs485;
            lhs(7,10)=clhs344*clhs458 + clhs486;
            lhs(7,11)=clhs487;
            lhs(7,12)=clhs344*clhs392 + clhs488;
            lhs(7,13)=clhs344*clhs430 + clhs489;
            lhs(7,14)=clhs344*clhs463 + clhs490;
            lhs(7,15)=clhs491;
            lhs(7,16)=clhs344*clhs398 + clhs492;
            lhs(7,17)=clhs344*clhs435 + clhs493;
            lhs(7,18)=clhs344*clhs468 + clhs494;
            lhs(7,19)=clhs495;
            lhs(7,20)=clhs344*clhs404 + clhs496;
            lhs(7,21)=clhs344*clhs440 + clhs497;
            lhs(7,22)=clhs344*clhs473 + clhs498;
            lhs(7,23)=clhs499;
            lhs(7,24)=clhs344*clhs410 + clhs500;
            lhs(7,25)=clhs344*clhs445 + clhs501;
            lhs(7,26)=clhs344*clhs478 + clhs502;
            lhs(7,27)=clhs503;
            lhs(7,28)=clhs344*clhs416 + clhs504;
            lhs(7,29)=clhs344*clhs450 + clhs505;
            lhs(7,30)=clhs344*clhs483 + clhs506;
            lhs(7,31)=clhs507;
            lhs(8,0)=DN(2,0)*clhs3 + DN(2,1)*clhs5 + DN(2,2)*clhs7 + clhs53;
            lhs(8,1)=DN(2,0)*clhs10 + DN(2,1)*clhs12 + DN(2,2)*clhs15 + clhs203;
            lhs(8,2)=DN(2,0)*clhs17 + DN(2,1)*clhs19 + DN(2,2)*clhs21 + clhs295;
            lhs(8,3)=-clhs350;
            lhs(8,4)=DN(2,0)*clhs23 + DN(2,1)*clhs25 + DN(2,2)*clhs27 + clhs383;
            lhs(8,5)=DN(2,0)*clhs33 + DN(2,1)*clhs35 + DN(2,2)*clhs38 + clhs421;
            lhs(8,6)=DN(2,0)*clhs40 + DN(2,1)*clhs42 + DN(2,2)*clhs44 + clhs454;
            lhs(8,7)=-clhs484;
            lhs(8,8)=DN(2,0)*clhs46 + DN(2,1)*clhs48 + DN(2,2)*clhs50 + clhs508*mu + clhs509;
            lhs(8,9)=DN(2,0)*clhs55 + DN(2,1)*clhs57 + DN(2,2)*clhs60 + clhs511;
            lhs(8,10)=DN(2,0)*clhs62 + DN(2,1)*clhs64 + DN(2,2)*clhs66 + clhs512;
            lhs(8,11)=-clhs513;
            lhs(8,12)=DN(2,0)*clhs68 + DN(2,1)*clhs70 + DN(2,2)*clhs72 + clhs517;
            lhs(8,13)=DN(2,0)*clhs77 + DN(2,1)*clhs79 + DN(2,2)*clhs82 + clhs518;
            lhs(8,14)=DN(2,0)*clhs84 + DN(2,1)*clhs86 + DN(2,2)*clhs88 + clhs519;
            lhs(8,15)=-clhs520;
            lhs(8,16)=DN(2,0)*clhs90 + DN(2,1)*clhs92 + DN(2,2)*clhs94 + clhs523;
            lhs(8,17)=DN(2,0)*clhs99 + DN(2,1)*clhs101 + DN(2,2)*clhs104 + clhs524;
            lhs(8,18)=DN(2,0)*clhs106 + DN(2,1)*clhs108 + DN(2,2)*clhs110 + clhs525;
            lhs(8,19)=-clhs526;
            lhs(8,20)=DN(2,0)*clhs112 + DN(2,1)*clhs114 + DN(2,2)*clhs116 + clhs529;
            lhs(8,21)=DN(2,0)*clhs121 + DN(2,1)*clhs123 + DN(2,2)*clhs126 + clhs530;
            lhs(8,22)=DN(2,0)*clhs128 + DN(2,1)*clhs130 + DN(2,2)*clhs132 + clhs531;
            lhs(8,23)=-clhs532;
            lhs(8,24)=DN(2,0)*clhs134 + DN(2,1)*clhs136 + DN(2,2)*clhs138 + clhs535;
            lhs(8,25)=DN(2,0)*clhs143 + DN(2,1)*clhs145 + DN(2,2)*clhs148 + clhs536;
            lhs(8,26)=DN(2,0)*clhs150 + DN(2,1)*clhs152 + DN(2,2)*clhs154 + clhs537;
            lhs(8,27)=-clhs538;
            lhs(8,28)=DN(2,0)*clhs156 + DN(2,1)*clhs158 + DN(2,2)*clhs160 + clhs541;
            lhs(8,29)=DN(2,0)*clhs165 + DN(2,1)*clhs167 + DN(2,2)*clhs170 + clhs542;
            lhs(8,30)=DN(2,0)*clhs172 + DN(2,1)*clhs174 + DN(2,2)*clhs176 + clhs543;
            lhs(8,31)=-clhs544;
            lhs(9,0)=DN(2,0)*clhs5 + DN(2,1)*clhs178 + DN(2,2)*clhs179 + clhs54;
            lhs(9,1)=DN(2,0)*clhs12 + DN(2,1)*clhs181 + DN(2,2)*clhs183 + clhs210;
            lhs(9,2)=DN(2,0)*clhs19 + DN(2,1)*clhs186 + DN(2,2)*clhs188 + clhs297;
            lhs(9,3)=-clhs351;
            lhs(9,4)=DN(2,0)*clhs25 + DN(2,1)*clhs191 + DN(2,2)*clhs192 + clhs384;
            lhs(9,5)=DN(2,0)*clhs35 + DN(2,1)*clhs193 + DN(2,2)*clhs195 + clhs423;
            lhs(9,6)=DN(2,0)*clhs42 + DN(2,1)*clhs199 + DN(2,2)*clhs201 + clhs455;
            lhs(9,7)=-clhs485;
            lhs(9,8)=DN(2,0)*clhs48 + DN(2,1)*clhs204 + DN(2,2)*clhs205 + clhs511;
            lhs(9,9)=DN(2,0)*clhs57 + DN(2,1)*clhs206 + DN(2,2)*clhs208 + clhs509 + clhs545*mu;
            lhs(9,10)=DN(2,0)*clhs64 + DN(2,1)*clhs212 + DN(2,2)*clhs214 + clhs547;
            lhs(9,11)=-clhs548;
            lhs(9,12)=DN(2,0)*clhs70 + DN(2,1)*clhs217 + DN(2,2)*clhs218 + clhs549;
            lhs(9,13)=DN(2,0)*clhs79 + DN(2,1)*clhs219 + DN(2,2)*clhs221 + clhs551;
            lhs(9,14)=DN(2,0)*clhs86 + DN(2,1)*clhs225 + DN(2,2)*clhs227 + clhs552;
            lhs(9,15)=-clhs553;
            lhs(9,16)=DN(2,0)*clhs92 + DN(2,1)*clhs230 + DN(2,2)*clhs231 + clhs554;
            lhs(9,17)=DN(2,0)*clhs101 + DN(2,1)*clhs232 + DN(2,2)*clhs234 + clhs556;
            lhs(9,18)=DN(2,0)*clhs108 + DN(2,1)*clhs238 + DN(2,2)*clhs240 + clhs557;
            lhs(9,19)=-clhs558;
            lhs(9,20)=DN(2,0)*clhs114 + DN(2,1)*clhs243 + DN(2,2)*clhs244 + clhs559;
            lhs(9,21)=DN(2,0)*clhs123 + DN(2,1)*clhs245 + DN(2,2)*clhs247 + clhs561;
            lhs(9,22)=DN(2,0)*clhs130 + DN(2,1)*clhs251 + DN(2,2)*clhs253 + clhs562;
            lhs(9,23)=-clhs563;
            lhs(9,24)=DN(2,0)*clhs136 + DN(2,1)*clhs256 + DN(2,2)*clhs257 + clhs564;
            lhs(9,25)=DN(2,0)*clhs145 + DN(2,1)*clhs258 + DN(2,2)*clhs260 + clhs566;
            lhs(9,26)=DN(2,0)*clhs152 + DN(2,1)*clhs264 + DN(2,2)*clhs266 + clhs567;
            lhs(9,27)=-clhs568;
            lhs(9,28)=DN(2,0)*clhs158 + DN(2,1)*clhs269 + DN(2,2)*clhs270 + clhs569;
            lhs(9,29)=DN(2,0)*clhs167 + DN(2,1)*clhs271 + DN(2,2)*clhs273 + clhs571;
            lhs(9,30)=DN(2,0)*clhs174 + DN(2,1)*clhs277 + DN(2,2)*clhs279 + clhs572;
            lhs(9,31)=-clhs573;
            lhs(10,0)=DN(2,0)*clhs7 + DN(2,1)*clhs179 + DN(2,2)*clhs281 + clhs61;
            lhs(10,1)=DN(2,0)*clhs15 + DN(2,1)*clhs183 + DN(2,2)*clhs282 + clhs211;
            lhs(10,2)=DN(2,0)*clhs21 + DN(2,1)*clhs188 + DN(2,2)*clhs284 + clhs301;
            lhs(10,3)=-clhs352;
            lhs(10,4)=DN(2,0)*clhs27 + DN(2,1)*clhs192 + DN(2,2)*clhs288 + clhs385;
            lhs(10,5)=DN(2,0)*clhs38 + DN(2,1)*clhs195 + DN(2,2)*clhs290 + clhs424;
            lhs(10,6)=DN(2,0)*clhs44 + DN(2,1)*clhs201 + DN(2,2)*clhs291 + clhs457;
            lhs(10,7)=-clhs486;
            lhs(10,8)=DN(2,0)*clhs50 + DN(2,1)*clhs205 + DN(2,2)*clhs296 + clhs512;
            lhs(10,9)=DN(2,0)*clhs60 + DN(2,1)*clhs208 + DN(2,2)*clhs298 + clhs547;
            lhs(10,10)=DN(2,0)*clhs66 + DN(2,1)*clhs214 + DN(2,2)*clhs299 + clhs509 + clhs574*mu;
            lhs(10,11)=-clhs575;
            lhs(10,12)=DN(2,0)*clhs72 + DN(2,1)*clhs218 + DN(2,2)*clhs304 + clhs577;
            lhs(10,13)=DN(2,0)*clhs82 + DN(2,1)*clhs221 + DN(2,2)*clhs306 + clhs578;
            lhs(10,14)=DN(2,0)*clhs88 + DN(2,1)*clhs227 + DN(2,2)*clhs307 + clhs580;
            lhs(10,15)=-clhs581;
            lhs(10,16)=DN(2,0)*clhs94 + DN(2,1)*clhs231 + DN(2,2)*clhs312 + clhs582;
            lhs(10,17)=DN(2,0)*clhs104 + DN(2,1)*clhs234 + DN(2,2)*clhs314 + clhs583;
            lhs(10,18)=DN(2,0)*clhs110 + DN(2,1)*clhs240 + DN(2,2)*clhs315 + clhs585;
            lhs(10,19)=-clhs586;
            lhs(10,20)=DN(2,0)*clhs116 + DN(2,1)*clhs244 + DN(2,2)*clhs320 + clhs587;
            lhs(10,21)=DN(2,0)*clhs126 + DN(2,1)*clhs247 + DN(2,2)*clhs322 + clhs588;
            lhs(10,22)=DN(2,0)*clhs132 + DN(2,1)*clhs253 + DN(2,2)*clhs323 + clhs590;
            lhs(10,23)=-clhs591;
            lhs(10,24)=DN(2,0)*clhs138 + DN(2,1)*clhs257 + DN(2,2)*clhs328 + clhs592;
            lhs(10,25)=DN(2,0)*clhs148 + DN(2,1)*clhs260 + DN(2,2)*clhs330 + clhs593;
            lhs(10,26)=DN(2,0)*clhs154 + DN(2,1)*clhs266 + DN(2,2)*clhs331 + clhs595;
            lhs(10,27)=-clhs596;
            lhs(10,28)=DN(2,0)*clhs160 + DN(2,1)*clhs270 + DN(2,2)*clhs336 + clhs597;
            lhs(10,29)=DN(2,0)*clhs170 + DN(2,1)*clhs273 + DN(2,2)*clhs338 + clhs598;
            lhs(10,30)=DN(2,0)*clhs176 + DN(2,1)*clhs279 + DN(2,2)*clhs339 + clhs600;
            lhs(10,31)=-clhs601;
            lhs(11,0)=clhs344*clhs350 + clhs67;
            lhs(11,1)=clhs215 + clhs344*clhs351;
            lhs(11,2)=clhs302 + clhs344*clhs352;
            lhs(11,3)=clhs353;
            lhs(11,4)=clhs344*clhs484 + clhs386;
            lhs(11,5)=clhs344*clhs485 + clhs425;
            lhs(11,6)=clhs344*clhs486 + clhs458;
            lhs(11,7)=clhs487;
            lhs(11,8)=clhs345*clhs513;
            lhs(11,9)=clhs345*clhs548;
            lhs(11,10)=clhs345*clhs575;
            lhs(11,11)=clhs343*(clhs508 + clhs545 + clhs574);
            lhs(11,12)=clhs344*clhs520 + clhs602;
            lhs(11,13)=clhs344*clhs553 + clhs603;
            lhs(11,14)=clhs344*clhs581 + clhs604;
            lhs(11,15)=clhs605;
            lhs(11,16)=clhs344*clhs526 + clhs606;
            lhs(11,17)=clhs344*clhs558 + clhs607;
            lhs(11,18)=clhs344*clhs586 + clhs608;
            lhs(11,19)=clhs609;
            lhs(11,20)=clhs344*clhs532 + clhs610;
            lhs(11,21)=clhs344*clhs563 + clhs611;
            lhs(11,22)=clhs344*clhs591 + clhs612;
            lhs(11,23)=clhs613;
            lhs(11,24)=clhs344*clhs538 + clhs614;
            lhs(11,25)=clhs344*clhs568 + clhs615;
            lhs(11,26)=clhs344*clhs596 + clhs616;
            lhs(11,27)=clhs617;
            lhs(11,28)=clhs344*clhs544 + clhs618;
            lhs(11,29)=clhs344*clhs573 + clhs619;
            lhs(11,30)=clhs344*clhs601 + clhs620;
            lhs(11,31)=clhs621;
            lhs(12,0)=DN(3,0)*clhs3 + DN(3,1)*clhs5 + DN(3,2)*clhs7 + clhs75;
            lhs(12,1)=DN(3,0)*clhs10 + DN(3,1)*clhs12 + DN(3,2)*clhs15 + clhs216;
            lhs(12,2)=DN(3,0)*clhs17 + DN(3,1)*clhs19 + DN(3,2)*clhs21 + clhs303;
            lhs(12,3)=-clhs354;
            lhs(12,4)=DN(3,0)*clhs23 + DN(3,1)*clhs25 + DN(3,2)*clhs27 + clhs389;
            lhs(12,5)=DN(3,0)*clhs33 + DN(3,1)*clhs35 + DN(3,2)*clhs38 + clhs426;
            lhs(12,6)=DN(3,0)*clhs40 + DN(3,1)*clhs42 + DN(3,2)*clhs44 + clhs459;
            lhs(12,7)=-clhs488;
            lhs(12,8)=DN(3,0)*clhs46 + DN(3,1)*clhs48 + DN(3,2)*clhs50 + clhs517;
            lhs(12,9)=DN(3,0)*clhs55 + DN(3,1)*clhs57 + DN(3,2)*clhs60 + clhs549;
            lhs(12,10)=DN(3,0)*clhs62 + DN(3,1)*clhs64 + DN(3,2)*clhs66 + clhs577;
            lhs(12,11)=-clhs602;
            lhs(12,12)=DN(3,0)*clhs68 + DN(3,1)*clhs70 + DN(3,2)*clhs72 + clhs622*mu + clhs623;
            lhs(12,13)=DN(3,0)*clhs77 + DN(3,1)*clhs79 + DN(3,2)*clhs82 + clhs625;
            lhs(12,14)=DN(3,0)*clhs84 + DN(3,1)*clhs86 + DN(3,2)*clhs88 + clhs626;
            lhs(12,15)=-clhs627;
            lhs(12,16)=DN(3,0)*clhs90 + DN(3,1)*clhs92 + DN(3,2)*clhs94 + clhs631;
            lhs(12,17)=DN(3,0)*clhs99 + DN(3,1)*clhs101 + DN(3,2)*clhs104 + clhs632;
            lhs(12,18)=DN(3,0)*clhs106 + DN(3,1)*clhs108 + DN(3,2)*clhs110 + clhs633;
            lhs(12,19)=-clhs634;
            lhs(12,20)=DN(3,0)*clhs112 + DN(3,1)*clhs114 + DN(3,2)*clhs116 + clhs637;
            lhs(12,21)=DN(3,0)*clhs121 + DN(3,1)*clhs123 + DN(3,2)*clhs126 + clhs638;
            lhs(12,22)=DN(3,0)*clhs128 + DN(3,1)*clhs130 + DN(3,2)*clhs132 + clhs639;
            lhs(12,23)=-clhs640;
            lhs(12,24)=DN(3,0)*clhs134 + DN(3,1)*clhs136 + DN(3,2)*clhs138 + clhs643;
            lhs(12,25)=DN(3,0)*clhs143 + DN(3,1)*clhs145 + DN(3,2)*clhs148 + clhs644;
            lhs(12,26)=DN(3,0)*clhs150 + DN(3,1)*clhs152 + DN(3,2)*clhs154 + clhs645;
            lhs(12,27)=-clhs646;
            lhs(12,28)=DN(3,0)*clhs156 + DN(3,1)*clhs158 + DN(3,2)*clhs160 + clhs649;
            lhs(12,29)=DN(3,0)*clhs165 + DN(3,1)*clhs167 + DN(3,2)*clhs170 + clhs650;
            lhs(12,30)=DN(3,0)*clhs172 + DN(3,1)*clhs174 + DN(3,2)*clhs176 + clhs651;
            lhs(12,31)=-clhs652;
            lhs(13,0)=DN(3,0)*clhs5 + DN(3,1)*clhs178 + DN(3,2)*clhs179 + clhs76;
            lhs(13,1)=DN(3,0)*clhs12 + DN(3,1)*clhs181 + DN(3,2)*clhs183 + clhs223;
            lhs(13,2)=DN(3,0)*clhs19 + DN(3,1)*clhs186 + DN(3,2)*clhs188 + clhs305;
            lhs(13,3)=-clhs355;
            lhs(13,4)=DN(3,0)*clhs25 + DN(3,1)*clhs191 + DN(3,2)*clhs192 + clhs390;
            lhs(13,5)=DN(3,0)*clhs35 + DN(3,1)*clhs193 + DN(3,2)*clhs195 + clhs428;
            lhs(13,6)=DN(3,0)*clhs42 + DN(3,1)*clhs199 + DN(3,2)*clhs201 + clhs460;
            lhs(13,7)=-clhs489;
            lhs(13,8)=DN(3,0)*clhs48 + DN(3,1)*clhs204 + DN(3,2)*clhs205 + clhs518;
            lhs(13,9)=DN(3,0)*clhs57 + DN(3,1)*clhs206 + DN(3,2)*clhs208 + clhs551;
            lhs(13,10)=DN(3,0)*clhs64 + DN(3,1)*clhs212 + DN(3,2)*clhs214 + clhs578;
            lhs(13,11)=-clhs603;
            lhs(13,12)=DN(3,0)*clhs70 + DN(3,1)*clhs217 + DN(3,2)*clhs218 + clhs625;
            lhs(13,13)=DN(3,0)*clhs79 + DN(3,1)*clhs219 + DN(3,2)*clhs221 + clhs623 + clhs653*mu;
            lhs(13,14)=DN(3,0)*clhs86 + DN(3,1)*clhs225 + DN(3,2)*clhs227 + clhs655;
            lhs(13,15)=-clhs656;
            lhs(13,16)=DN(3,0)*clhs92 + DN(3,1)*clhs230 + DN(3,2)*clhs231 + clhs657;
            lhs(13,17)=DN(3,0)*clhs101 + DN(3,1)*clhs232 + DN(3,2)*clhs234 + clhs659;
            lhs(13,18)=DN(3,0)*clhs108 + DN(3,1)*clhs238 + DN(3,2)*clhs240 + clhs660;
            lhs(13,19)=-clhs661;
            lhs(13,20)=DN(3,0)*clhs114 + DN(3,1)*clhs243 + DN(3,2)*clhs244 + clhs662;
            lhs(13,21)=DN(3,0)*clhs123 + DN(3,1)*clhs245 + DN(3,2)*clhs247 + clhs664;
            lhs(13,22)=DN(3,0)*clhs130 + DN(3,1)*clhs251 + DN(3,2)*clhs253 + clhs665;
            lhs(13,23)=-clhs666;
            lhs(13,24)=DN(3,0)*clhs136 + DN(3,1)*clhs256 + DN(3,2)*clhs257 + clhs667;
            lhs(13,25)=DN(3,0)*clhs145 + DN(3,1)*clhs258 + DN(3,2)*clhs260 + clhs669;
            lhs(13,26)=DN(3,0)*clhs152 + DN(3,1)*clhs264 + DN(3,2)*clhs266 + clhs670;
            lhs(13,27)=-clhs671;
            lhs(13,28)=DN(3,0)*clhs158 + DN(3,1)*clhs269 + DN(3,2)*clhs270 + clhs672;
            lhs(13,29)=DN(3,0)*clhs167 + DN(3,1)*clhs271 + DN(3,2)*clhs273 + clhs674;
            lhs(13,30)=DN(3,0)*clhs174 + DN(3,1)*clhs277 + DN(3,2)*clhs279 + clhs675;
            lhs(13,31)=-clhs676;
            lhs(14,0)=DN(3,0)*clhs7 + DN(3,1)*clhs179 + DN(3,2)*clhs281 + clhs83;
            lhs(14,1)=DN(3,0)*clhs15 + DN(3,1)*clhs183 + DN(3,2)*clhs282 + clhs224;
            lhs(14,2)=DN(3,0)*clhs21 + DN(3,1)*clhs188 + DN(3,2)*clhs284 + clhs309;
            lhs(14,3)=-clhs356;
            lhs(14,4)=DN(3,0)*clhs27 + DN(3,1)*clhs192 + DN(3,2)*clhs288 + clhs391;
            lhs(14,5)=DN(3,0)*clhs38 + DN(3,1)*clhs195 + DN(3,2)*clhs290 + clhs429;
            lhs(14,6)=DN(3,0)*clhs44 + DN(3,1)*clhs201 + DN(3,2)*clhs291 + clhs462;
            lhs(14,7)=-clhs490;
            lhs(14,8)=DN(3,0)*clhs50 + DN(3,1)*clhs205 + DN(3,2)*clhs296 + clhs519;
            lhs(14,9)=DN(3,0)*clhs60 + DN(3,1)*clhs208 + DN(3,2)*clhs298 + clhs552;
            lhs(14,10)=DN(3,0)*clhs66 + DN(3,1)*clhs214 + DN(3,2)*clhs299 + clhs580;
            lhs(14,11)=-clhs604;
            lhs(14,12)=DN(3,0)*clhs72 + DN(3,1)*clhs218 + DN(3,2)*clhs304 + clhs626;
            lhs(14,13)=DN(3,0)*clhs82 + DN(3,1)*clhs221 + DN(3,2)*clhs306 + clhs655;
            lhs(14,14)=DN(3,0)*clhs88 + DN(3,1)*clhs227 + DN(3,2)*clhs307 + clhs623 + clhs677*mu;
            lhs(14,15)=-clhs678;
            lhs(14,16)=DN(3,0)*clhs94 + DN(3,1)*clhs231 + DN(3,2)*clhs312 + clhs680;
            lhs(14,17)=DN(3,0)*clhs104 + DN(3,1)*clhs234 + DN(3,2)*clhs314 + clhs681;
            lhs(14,18)=DN(3,0)*clhs110 + DN(3,1)*clhs240 + DN(3,2)*clhs315 + clhs683;
            lhs(14,19)=-clhs684;
            lhs(14,20)=DN(3,0)*clhs116 + DN(3,1)*clhs244 + DN(3,2)*clhs320 + clhs685;
            lhs(14,21)=DN(3,0)*clhs126 + DN(3,1)*clhs247 + DN(3,2)*clhs322 + clhs686;
            lhs(14,22)=DN(3,0)*clhs132 + DN(3,1)*clhs253 + DN(3,2)*clhs323 + clhs688;
            lhs(14,23)=-clhs689;
            lhs(14,24)=DN(3,0)*clhs138 + DN(3,1)*clhs257 + DN(3,2)*clhs328 + clhs690;
            lhs(14,25)=DN(3,0)*clhs148 + DN(3,1)*clhs260 + DN(3,2)*clhs330 + clhs691;
            lhs(14,26)=DN(3,0)*clhs154 + DN(3,1)*clhs266 + DN(3,2)*clhs331 + clhs693;
            lhs(14,27)=-clhs694;
            lhs(14,28)=DN(3,0)*clhs160 + DN(3,1)*clhs270 + DN(3,2)*clhs336 + clhs695;
            lhs(14,29)=DN(3,0)*clhs170 + DN(3,1)*clhs273 + DN(3,2)*clhs338 + clhs696;
            lhs(14,30)=DN(3,0)*clhs176 + DN(3,1)*clhs279 + DN(3,2)*clhs339 + clhs698;
            lhs(14,31)=-clhs699;
            lhs(15,0)=clhs344*clhs354 + clhs89;
            lhs(15,1)=clhs228 + clhs344*clhs355;
            lhs(15,2)=clhs310 + clhs344*clhs356;
            lhs(15,3)=clhs357;
            lhs(15,4)=clhs344*clhs488 + clhs392;
            lhs(15,5)=clhs344*clhs489 + clhs430;
            lhs(15,6)=clhs344*clhs490 + clhs463;
            lhs(15,7)=clhs491;
            lhs(15,8)=clhs344*clhs602 + clhs520;
            lhs(15,9)=clhs344*clhs603 + clhs553;
            lhs(15,10)=clhs344*clhs604 + clhs581;
            lhs(15,11)=clhs605;
            lhs(15,12)=clhs345*clhs627;
            lhs(15,13)=clhs345*clhs656;
            lhs(15,14)=clhs345*clhs678;
            lhs(15,15)=clhs343*(clhs622 + clhs653 + clhs677);
            lhs(15,16)=clhs344*clhs634 + clhs700;
            lhs(15,17)=clhs344*clhs661 + clhs701;
            lhs(15,18)=clhs344*clhs684 + clhs702;
            lhs(15,19)=clhs703;
            lhs(15,20)=clhs344*clhs640 + clhs704;
            lhs(15,21)=clhs344*clhs666 + clhs705;
            lhs(15,22)=clhs344*clhs689 + clhs706;
            lhs(15,23)=clhs707;
            lhs(15,24)=clhs344*clhs646 + clhs708;
            lhs(15,25)=clhs344*clhs671 + clhs709;
            lhs(15,26)=clhs344*clhs694 + clhs710;
            lhs(15,27)=clhs711;
            lhs(15,28)=clhs344*clhs652 + clhs712;
            lhs(15,29)=clhs344*clhs676 + clhs713;
            lhs(15,30)=clhs344*clhs699 + clhs714;
            lhs(15,31)=clhs715;
            lhs(16,0)=DN(4,0)*clhs3 + DN(4,1)*clhs5 + DN(4,2)*clhs7 + clhs97;
            lhs(16,1)=DN(4,0)*clhs10 + DN(4,1)*clhs12 + DN(4,2)*clhs15 + clhs229;
            lhs(16,2)=DN(4,0)*clhs17 + DN(4,1)*clhs19 + DN(4,2)*clhs21 + clhs311;
            lhs(16,3)=-clhs358;
            lhs(16,4)=DN(4,0)*clhs23 + DN(4,1)*clhs25 + DN(4,2)*clhs27 + clhs395;
            lhs(16,5)=DN(4,0)*clhs33 + DN(4,1)*clhs35 + DN(4,2)*clhs38 + clhs431;
            lhs(16,6)=DN(4,0)*clhs40 + DN(4,1)*clhs42 + DN(4,2)*clhs44 + clhs464;
            lhs(16,7)=-clhs492;
            lhs(16,8)=DN(4,0)*clhs46 + DN(4,1)*clhs48 + DN(4,2)*clhs50 + clhs523;
            lhs(16,9)=DN(4,0)*clhs55 + DN(4,1)*clhs57 + DN(4,2)*clhs60 + clhs554;
            lhs(16,10)=DN(4,0)*clhs62 + DN(4,1)*clhs64 + DN(4,2)*clhs66 + clhs582;
            lhs(16,11)=-clhs606;
            lhs(16,12)=DN(4,0)*clhs68 + DN(4,1)*clhs70 + DN(4,2)*clhs72 + clhs631;
            lhs(16,13)=DN(4,0)*clhs77 + DN(4,1)*clhs79 + DN(4,2)*clhs82 + clhs657;
            lhs(16,14)=DN(4,0)*clhs84 + DN(4,1)*clhs86 + DN(4,2)*clhs88 + clhs680;
            lhs(16,15)=-clhs700;
            lhs(16,16)=DN(4,0)*clhs90 + DN(4,1)*clhs92 + DN(4,2)*clhs94 + clhs716*mu + clhs717;
            lhs(16,17)=DN(4,0)*clhs99 + DN(4,1)*clhs101 + DN(4,2)*clhs104 + clhs719;
            lhs(16,18)=DN(4,0)*clhs106 + DN(4,1)*clhs108 + DN(4,2)*clhs110 + clhs720;
            lhs(16,19)=-clhs721;
            lhs(16,20)=DN(4,0)*clhs112 + DN(4,1)*clhs114 + DN(4,2)*clhs116 + clhs725;
            lhs(16,21)=DN(4,0)*clhs121 + DN(4,1)*clhs123 + DN(4,2)*clhs126 + clhs726;
            lhs(16,22)=DN(4,0)*clhs128 + DN(4,1)*clhs130 + DN(4,2)*clhs132 + clhs727;
            lhs(16,23)=-clhs728;
            lhs(16,24)=DN(4,0)*clhs134 + DN(4,1)*clhs136 + DN(4,2)*clhs138 + clhs731;
            lhs(16,25)=DN(4,0)*clhs143 + DN(4,1)*clhs145 + DN(4,2)*clhs148 + clhs732;
            lhs(16,26)=DN(4,0)*clhs150 + DN(4,1)*clhs152 + DN(4,2)*clhs154 + clhs733;
            lhs(16,27)=-clhs734;
            lhs(16,28)=DN(4,0)*clhs156 + DN(4,1)*clhs158 + DN(4,2)*clhs160 + clhs737;
            lhs(16,29)=DN(4,0)*clhs165 + DN(4,1)*clhs167 + DN(4,2)*clhs170 + clhs738;
            lhs(16,30)=DN(4,0)*clhs172 + DN(4,1)*clhs174 + DN(4,2)*clhs176 + clhs739;
            lhs(16,31)=-clhs740;
            lhs(17,0)=DN(4,0)*clhs5 + DN(4,1)*clhs178 + DN(4,2)*clhs179 + clhs98;
            lhs(17,1)=DN(4,0)*clhs12 + DN(4,1)*clhs181 + DN(4,2)*clhs183 + clhs236;
            lhs(17,2)=DN(4,0)*clhs19 + DN(4,1)*clhs186 + DN(4,2)*clhs188 + clhs313;
            lhs(17,3)=-clhs359;
            lhs(17,4)=DN(4,0)*clhs25 + DN(4,1)*clhs191 + DN(4,2)*clhs192 + clhs396;
            lhs(17,5)=DN(4,0)*clhs35 + DN(4,1)*clhs193 + DN(4,2)*clhs195 + clhs433;
            lhs(17,6)=DN(4,0)*clhs42 + DN(4,1)*clhs199 + DN(4,2)*clhs201 + clhs465;
            lhs(17,7)=-clhs493;
            lhs(17,8)=DN(4,0)*clhs48 + DN(4,1)*clhs204 + DN(4,2)*clhs205 + clhs524;
            lhs(17,9)=DN(4,0)*clhs57 + DN(4,1)*clhs206 + DN(4,2)*clhs208 + clhs556;
            lhs(17,10)=DN(4,0)*clhs64 + DN(4,1)*clhs212 + DN(4,2)*clhs214 + clhs583;
            lhs(17,11)=-clhs607;
            lhs(17,12)=DN(4,0)*clhs70 + DN(4,1)*clhs217 + DN(4,2)*clhs218 + clhs632;
            lhs(17,13)=DN(4,0)*clhs79 + DN(4,1)*clhs219 + DN(4,2)*clhs221 + clhs659;
            lhs(17,14)=DN(4,0)*clhs86 + DN(4,1)*clhs225 + DN(4,2)*clhs227 + clhs681;
            lhs(17,15)=-clhs701;
            lhs(17,16)=DN(4,0)*clhs92 + DN(4,1)*clhs230 + DN(4,2)*clhs231 + clhs719;
            lhs(17,17)=DN(4,0)*clhs101 + DN(4,1)*clhs232 + DN(4,2)*clhs234 + clhs717 + clhs741*mu;
            lhs(17,18)=DN(4,0)*clhs108 + DN(4,1)*clhs238 + DN(4,2)*clhs240 + clhs743;
            lhs(17,19)=-clhs744;
            lhs(17,20)=DN(4,0)*clhs114 + DN(4,1)*clhs243 + DN(4,2)*clhs244 + clhs745;
            lhs(17,21)=DN(4,0)*clhs123 + DN(4,1)*clhs245 + DN(4,2)*clhs247 + clhs747;
            lhs(17,22)=DN(4,0)*clhs130 + DN(4,1)*clhs251 + DN(4,2)*clhs253 + clhs748;
            lhs(17,23)=-clhs749;
            lhs(17,24)=DN(4,0)*clhs136 + DN(4,1)*clhs256 + DN(4,2)*clhs257 + clhs750;
            lhs(17,25)=DN(4,0)*clhs145 + DN(4,1)*clhs258 + DN(4,2)*clhs260 + clhs752;
            lhs(17,26)=DN(4,0)*clhs152 + DN(4,1)*clhs264 + DN(4,2)*clhs266 + clhs753;
            lhs(17,27)=-clhs754;
            lhs(17,28)=DN(4,0)*clhs158 + DN(4,1)*clhs269 + DN(4,2)*clhs270 + clhs755;
            lhs(17,29)=DN(4,0)*clhs167 + DN(4,1)*clhs271 + DN(4,2)*clhs273 + clhs757;
            lhs(17,30)=DN(4,0)*clhs174 + DN(4,1)*clhs277 + DN(4,2)*clhs279 + clhs758;
            lhs(17,31)=-clhs759;
            lhs(18,0)=DN(4,0)*clhs7 + DN(4,1)*clhs179 + DN(4,2)*clhs281 + clhs105;
            lhs(18,1)=DN(4,0)*clhs15 + DN(4,1)*clhs183 + DN(4,2)*clhs282 + clhs237;
            lhs(18,2)=DN(4,0)*clhs21 + DN(4,1)*clhs188 + DN(4,2)*clhs284 + clhs317;
            lhs(18,3)=-clhs360;
            lhs(18,4)=DN(4,0)*clhs27 + DN(4,1)*clhs192 + DN(4,2)*clhs288 + clhs397;
            lhs(18,5)=DN(4,0)*clhs38 + DN(4,1)*clhs195 + DN(4,2)*clhs290 + clhs434;
            lhs(18,6)=DN(4,0)*clhs44 + DN(4,1)*clhs201 + DN(4,2)*clhs291 + clhs467;
            lhs(18,7)=-clhs494;
            lhs(18,8)=DN(4,0)*clhs50 + DN(4,1)*clhs205 + DN(4,2)*clhs296 + clhs525;
            lhs(18,9)=DN(4,0)*clhs60 + DN(4,1)*clhs208 + DN(4,2)*clhs298 + clhs557;
            lhs(18,10)=DN(4,0)*clhs66 + DN(4,1)*clhs214 + DN(4,2)*clhs299 + clhs585;
            lhs(18,11)=-clhs608;
            lhs(18,12)=DN(4,0)*clhs72 + DN(4,1)*clhs218 + DN(4,2)*clhs304 + clhs633;
            lhs(18,13)=DN(4,0)*clhs82 + DN(4,1)*clhs221 + DN(4,2)*clhs306 + clhs660;
            lhs(18,14)=DN(4,0)*clhs88 + DN(4,1)*clhs227 + DN(4,2)*clhs307 + clhs683;
            lhs(18,15)=-clhs702;
            lhs(18,16)=DN(4,0)*clhs94 + DN(4,1)*clhs231 + DN(4,2)*clhs312 + clhs720;
            lhs(18,17)=DN(4,0)*clhs104 + DN(4,1)*clhs234 + DN(4,2)*clhs314 + clhs743;
            lhs(18,18)=DN(4,0)*clhs110 + DN(4,1)*clhs240 + DN(4,2)*clhs315 + clhs717 + clhs760*mu;
            lhs(18,19)=-clhs761;
            lhs(18,20)=DN(4,0)*clhs116 + DN(4,1)*clhs244 + DN(4,2)*clhs320 + clhs763;
            lhs(18,21)=DN(4,0)*clhs126 + DN(4,1)*clhs247 + DN(4,2)*clhs322 + clhs764;
            lhs(18,22)=DN(4,0)*clhs132 + DN(4,1)*clhs253 + DN(4,2)*clhs323 + clhs766;
            lhs(18,23)=-clhs767;
            lhs(18,24)=DN(4,0)*clhs138 + DN(4,1)*clhs257 + DN(4,2)*clhs328 + clhs768;
            lhs(18,25)=DN(4,0)*clhs148 + DN(4,1)*clhs260 + DN(4,2)*clhs330 + clhs769;
            lhs(18,26)=DN(4,0)*clhs154 + DN(4,1)*clhs266 + DN(4,2)*clhs331 + clhs771;
            lhs(18,27)=-clhs772;
            lhs(18,28)=DN(4,0)*clhs160 + DN(4,1)*clhs270 + DN(4,2)*clhs336 + clhs773;
            lhs(18,29)=DN(4,0)*clhs170 + DN(4,1)*clhs273 + DN(4,2)*clhs338 + clhs774;
            lhs(18,30)=DN(4,0)*clhs176 + DN(4,1)*clhs279 + DN(4,2)*clhs339 + clhs776;
            lhs(18,31)=-clhs777;
            lhs(19,0)=clhs111 + clhs344*clhs358;
            lhs(19,1)=clhs241 + clhs344*clhs359;
            lhs(19,2)=clhs318 + clhs344*clhs360;
            lhs(19,3)=clhs361;
            lhs(19,4)=clhs344*clhs492 + clhs398;
            lhs(19,5)=clhs344*clhs493 + clhs435;
            lhs(19,6)=clhs344*clhs494 + clhs468;
            lhs(19,7)=clhs495;
            lhs(19,8)=clhs344*clhs606 + clhs526;
            lhs(19,9)=clhs344*clhs607 + clhs558;
            lhs(19,10)=clhs344*clhs608 + clhs586;
            lhs(19,11)=clhs609;
            lhs(19,12)=clhs344*clhs700 + clhs634;
            lhs(19,13)=clhs344*clhs701 + clhs661;
            lhs(19,14)=clhs344*clhs702 + clhs684;
            lhs(19,15)=clhs703;
            lhs(19,16)=clhs345*clhs721;
            lhs(19,17)=clhs345*clhs744;
            lhs(19,18)=clhs345*clhs761;
            lhs(19,19)=clhs343*(clhs716 + clhs741 + clhs760);
            lhs(19,20)=clhs344*clhs728 + clhs778;
            lhs(19,21)=clhs344*clhs749 + clhs779;
            lhs(19,22)=clhs344*clhs767 + clhs780;
            lhs(19,23)=clhs781;
            lhs(19,24)=clhs344*clhs734 + clhs782;
            lhs(19,25)=clhs344*clhs754 + clhs783;
            lhs(19,26)=clhs344*clhs772 + clhs784;
            lhs(19,27)=clhs785;
            lhs(19,28)=clhs344*clhs740 + clhs786;
            lhs(19,29)=clhs344*clhs759 + clhs787;
            lhs(19,30)=clhs344*clhs777 + clhs788;
            lhs(19,31)=clhs789;
            lhs(20,0)=DN(5,0)*clhs3 + DN(5,1)*clhs5 + DN(5,2)*clhs7 + clhs119;
            lhs(20,1)=DN(5,0)*clhs10 + DN(5,1)*clhs12 + DN(5,2)*clhs15 + clhs242;
            lhs(20,2)=DN(5,0)*clhs17 + DN(5,1)*clhs19 + DN(5,2)*clhs21 + clhs319;
            lhs(20,3)=-clhs362;
            lhs(20,4)=DN(5,0)*clhs23 + DN(5,1)*clhs25 + DN(5,2)*clhs27 + clhs401;
            lhs(20,5)=DN(5,0)*clhs33 + DN(5,1)*clhs35 + DN(5,2)*clhs38 + clhs436;
            lhs(20,6)=DN(5,0)*clhs40 + DN(5,1)*clhs42 + DN(5,2)*clhs44 + clhs469;
            lhs(20,7)=-clhs496;
            lhs(20,8)=DN(5,0)*clhs46 + DN(5,1)*clhs48 + DN(5,2)*clhs50 + clhs529;
            lhs(20,9)=DN(5,0)*clhs55 + DN(5,1)*clhs57 + DN(5,2)*clhs60 + clhs559;
            lhs(20,10)=DN(5,0)*clhs62 + DN(5,1)*clhs64 + DN(5,2)*clhs66 + clhs587;
            lhs(20,11)=-clhs610;
            lhs(20,12)=DN(5,0)*clhs68 + DN(5,1)*clhs70 + DN(5,2)*clhs72 + clhs637;
            lhs(20,13)=DN(5,0)*clhs77 + DN(5,1)*clhs79 + DN(5,2)*clhs82 + clhs662;
            lhs(20,14)=DN(5,0)*clhs84 + DN(5,1)*clhs86 + DN(5,2)*clhs88 + clhs685;
            lhs(20,15)=-clhs704;
            lhs(20,16)=DN(5,0)*clhs90 + DN(5,1)*clhs92 + DN(5,2)*clhs94 + clhs725;
            lhs(20,17)=DN(5,0)*clhs99 + DN(5,1)*clhs101 + DN(5,2)*clhs104 + clhs745;
            lhs(20,18)=DN(5,0)*clhs106 + DN(5,1)*clhs108 + DN(5,2)*clhs110 + clhs763;
            lhs(20,19)=-clhs778;
            lhs(20,20)=DN(5,0)*clhs112 + DN(5,1)*clhs114 + DN(5,2)*clhs116 + clhs790*mu + clhs791;
            lhs(20,21)=DN(5,0)*clhs121 + DN(5,1)*clhs123 + DN(5,2)*clhs126 + clhs793;
            lhs(20,22)=DN(5,0)*clhs128 + DN(5,1)*clhs130 + DN(5,2)*clhs132 + clhs794;
            lhs(20,23)=-clhs795;
            lhs(20,24)=DN(5,0)*clhs134 + DN(5,1)*clhs136 + DN(5,2)*clhs138 + clhs799;
            lhs(20,25)=DN(5,0)*clhs143 + DN(5,1)*clhs145 + DN(5,2)*clhs148 + clhs800;
            lhs(20,26)=DN(5,0)*clhs150 + DN(5,1)*clhs152 + DN(5,2)*clhs154 + clhs801;
            lhs(20,27)=-clhs802;
            lhs(20,28)=DN(5,0)*clhs156 + DN(5,1)*clhs158 + DN(5,2)*clhs160 + clhs805;
            lhs(20,29)=DN(5,0)*clhs165 + DN(5,1)*clhs167 + DN(5,2)*clhs170 + clhs806;
            lhs(20,30)=DN(5,0)*clhs172 + DN(5,1)*clhs174 + DN(5,2)*clhs176 + clhs807;
            lhs(20,31)=-clhs808;
            lhs(21,0)=DN(5,0)*clhs5 + DN(5,1)*clhs178 + DN(5,2)*clhs179 + clhs120;
            lhs(21,1)=DN(5,0)*clhs12 + DN(5,1)*clhs181 + DN(5,2)*clhs183 + clhs249;
            lhs(21,2)=DN(5,0)*clhs19 + DN(5,1)*clhs186 + DN(5,2)*clhs188 + clhs321;
            lhs(21,3)=-clhs363;
            lhs(21,4)=DN(5,0)*clhs25 + DN(5,1)*clhs191 + DN(5,2)*clhs192 + clhs402;
            lhs(21,5)=DN(5,0)*clhs35 + DN(5,1)*clhs193 + DN(5,2)*clhs195 + clhs438;
            lhs(21,6)=DN(5,0)*clhs42 + DN(5,1)*clhs199 + DN(5,2)*clhs201 + clhs470;
            lhs(21,7)=-clhs497;
            lhs(21,8)=DN(5,0)*clhs48 + DN(5,1)*clhs204 + DN(5,2)*clhs205 + clhs530;
            lhs(21,9)=DN(5,0)*clhs57 + DN(5,1)*clhs206 + DN(5,2)*clhs208 + clhs561;
            lhs(21,10)=DN(5,0)*clhs64 + DN(5,1)*clhs212 + DN(5,2)*clhs214 + clhs588;
            lhs(21,11)=-clhs611;
            lhs(21,12)=DN(5,0)*clhs70 + DN(5,1)*clhs217 + DN(5,2)*clhs218 + clhs638;
            lhs(21,13)=DN(5,0)*clhs79 + DN(5,1)*clhs219 + DN(5,2)*clhs221 + clhs664;
            lhs(21,14)=DN(5,0)*clhs86 + DN(5,1)*clhs225 + DN(5,2)*clhs227 + clhs686;
            lhs(21,15)=-clhs705;
            lhs(21,16)=DN(5,0)*clhs92 + DN(5,1)*clhs230 + DN(5,2)*clhs231 + clhs726;
            lhs(21,17)=DN(5,0)*clhs101 + DN(5,1)*clhs232 + DN(5,2)*clhs234 + clhs747;
            lhs(21,18)=DN(5,0)*clhs108 + DN(5,1)*clhs238 + DN(5,2)*clhs240 + clhs764;
            lhs(21,19)=-clhs779;
            lhs(21,20)=DN(5,0)*clhs114 + DN(5,1)*clhs243 + DN(5,2)*clhs244 + clhs793;
            lhs(21,21)=DN(5,0)*clhs123 + DN(5,1)*clhs245 + DN(5,2)*clhs247 + clhs791 + clhs809*mu;
            lhs(21,22)=DN(5,0)*clhs130 + DN(5,1)*clhs251 + DN(5,2)*clhs253 + clhs811;
            lhs(21,23)=-clhs812;
            lhs(21,24)=DN(5,0)*clhs136 + DN(5,1)*clhs256 + DN(5,2)*clhs257 + clhs813;
            lhs(21,25)=DN(5,0)*clhs145 + DN(5,1)*clhs258 + DN(5,2)*clhs260 + clhs815;
            lhs(21,26)=DN(5,0)*clhs152 + DN(5,1)*clhs264 + DN(5,2)*clhs266 + clhs816;
            lhs(21,27)=-clhs817;
            lhs(21,28)=DN(5,0)*clhs158 + DN(5,1)*clhs269 + DN(5,2)*clhs270 + clhs818;
            lhs(21,29)=DN(5,0)*clhs167 + DN(5,1)*clhs271 + DN(5,2)*clhs273 + clhs820;
            lhs(21,30)=DN(5,0)*clhs174 + DN(5,1)*clhs277 + DN(5,2)*clhs279 + clhs821;
            lhs(21,31)=-clhs822;
            lhs(22,0)=DN(5,0)*clhs7 + DN(5,1)*clhs179 + DN(5,2)*clhs281 + clhs127;
            lhs(22,1)=DN(5,0)*clhs15 + DN(5,1)*clhs183 + DN(5,2)*clhs282 + clhs250;
            lhs(22,2)=DN(5,0)*clhs21 + DN(5,1)*clhs188 + DN(5,2)*clhs284 + clhs325;
            lhs(22,3)=-clhs364;
            lhs(22,4)=DN(5,0)*clhs27 + DN(5,1)*clhs192 + DN(5,2)*clhs288 + clhs403;
            lhs(22,5)=DN(5,0)*clhs38 + DN(5,1)*clhs195 + DN(5,2)*clhs290 + clhs439;
            lhs(22,6)=DN(5,0)*clhs44 + DN(5,1)*clhs201 + DN(5,2)*clhs291 + clhs472;
            lhs(22,7)=-clhs498;
            lhs(22,8)=DN(5,0)*clhs50 + DN(5,1)*clhs205 + DN(5,2)*clhs296 + clhs531;
            lhs(22,9)=DN(5,0)*clhs60 + DN(5,1)*clhs208 + DN(5,2)*clhs298 + clhs562;
            lhs(22,10)=DN(5,0)*clhs66 + DN(5,1)*clhs214 + DN(5,2)*clhs299 + clhs590;
            lhs(22,11)=-clhs612;
            lhs(22,12)=DN(5,0)*clhs72 + DN(5,1)*clhs218 + DN(5,2)*clhs304 + clhs639;
            lhs(22,13)=DN(5,0)*clhs82 + DN(5,1)*clhs221 + DN(5,2)*clhs306 + clhs665;
            lhs(22,14)=DN(5,0)*clhs88 + DN(5,1)*clhs227 + DN(5,2)*clhs307 + clhs688;
            lhs(22,15)=-clhs706;
            lhs(22,16)=DN(5,0)*clhs94 + DN(5,1)*clhs231 + DN(5,2)*clhs312 + clhs727;
            lhs(22,17)=DN(5,0)*clhs104 + DN(5,1)*clhs234 + DN(5,2)*clhs314 + clhs748;
            lhs(22,18)=DN(5,0)*clhs110 + DN(5,1)*clhs240 + DN(5,2)*clhs315 + clhs766;
            lhs(22,19)=-clhs780;
            lhs(22,20)=DN(5,0)*clhs116 + DN(5,1)*clhs244 + DN(5,2)*clhs320 + clhs794;
            lhs(22,21)=DN(5,0)*clhs126 + DN(5,1)*clhs247 + DN(5,2)*clhs322 + clhs811;
            lhs(22,22)=DN(5,0)*clhs132 + DN(5,1)*clhs253 + DN(5,2)*clhs323 + clhs791 + clhs823*mu;
            lhs(22,23)=-clhs824;
            lhs(22,24)=DN(5,0)*clhs138 + DN(5,1)*clhs257 + DN(5,2)*clhs328 + clhs826;
            lhs(22,25)=DN(5,0)*clhs148 + DN(5,1)*clhs260 + DN(5,2)*clhs330 + clhs827;
            lhs(22,26)=DN(5,0)*clhs154 + DN(5,1)*clhs266 + DN(5,2)*clhs331 + clhs829;
            lhs(22,27)=-clhs830;
            lhs(22,28)=DN(5,0)*clhs160 + DN(5,1)*clhs270 + DN(5,2)*clhs336 + clhs831;
            lhs(22,29)=DN(5,0)*clhs170 + DN(5,1)*clhs273 + DN(5,2)*clhs338 + clhs832;
            lhs(22,30)=DN(5,0)*clhs176 + DN(5,1)*clhs279 + DN(5,2)*clhs339 + clhs834;
            lhs(22,31)=-clhs835;
            lhs(23,0)=clhs133 + clhs344*clhs362;
            lhs(23,1)=clhs254 + clhs344*clhs363;
            lhs(23,2)=clhs326 + clhs344*clhs364;
            lhs(23,3)=clhs365;
            lhs(23,4)=clhs344*clhs496 + clhs404;
            lhs(23,5)=clhs344*clhs497 + clhs440;
            lhs(23,6)=clhs344*clhs498 + clhs473;
            lhs(23,7)=clhs499;
            lhs(23,8)=clhs344*clhs610 + clhs532;
            lhs(23,9)=clhs344*clhs611 + clhs563;
            lhs(23,10)=clhs344*clhs612 + clhs591;
            lhs(23,11)=clhs613;
            lhs(23,12)=clhs344*clhs704 + clhs640;
            lhs(23,13)=clhs344*clhs705 + clhs666;
            lhs(23,14)=clhs344*clhs706 + clhs689;
            lhs(23,15)=clhs707;
            lhs(23,16)=clhs344*clhs778 + clhs728;
            lhs(23,17)=clhs344*clhs779 + clhs749;
            lhs(23,18)=clhs344*clhs780 + clhs767;
            lhs(23,19)=clhs781;
            lhs(23,20)=clhs345*clhs795;
            lhs(23,21)=clhs345*clhs812;
            lhs(23,22)=clhs345*clhs824;
            lhs(23,23)=clhs343*(clhs790 + clhs809 + clhs823);
            lhs(23,24)=clhs344*clhs802 + clhs836;
            lhs(23,25)=clhs344*clhs817 + clhs837;
            lhs(23,26)=clhs344*clhs830 + clhs838;
            lhs(23,27)=clhs839;
            lhs(23,28)=clhs344*clhs808 + clhs840;
            lhs(23,29)=clhs344*clhs822 + clhs841;
            lhs(23,30)=clhs344*clhs835 + clhs842;
            lhs(23,31)=clhs843;
            lhs(24,0)=DN(6,0)*clhs3 + DN(6,1)*clhs5 + DN(6,2)*clhs7 + clhs141;
            lhs(24,1)=DN(6,0)*clhs10 + DN(6,1)*clhs12 + DN(6,2)*clhs15 + clhs255;
            lhs(24,2)=DN(6,0)*clhs17 + DN(6,1)*clhs19 + DN(6,2)*clhs21 + clhs327;
            lhs(24,3)=-clhs366;
            lhs(24,4)=DN(6,0)*clhs23 + DN(6,1)*clhs25 + DN(6,2)*clhs27 + clhs407;
            lhs(24,5)=DN(6,0)*clhs33 + DN(6,1)*clhs35 + DN(6,2)*clhs38 + clhs441;
            lhs(24,6)=DN(6,0)*clhs40 + DN(6,1)*clhs42 + DN(6,2)*clhs44 + clhs474;
            lhs(24,7)=-clhs500;
            lhs(24,8)=DN(6,0)*clhs46 + DN(6,1)*clhs48 + DN(6,2)*clhs50 + clhs535;
            lhs(24,9)=DN(6,0)*clhs55 + DN(6,1)*clhs57 + DN(6,2)*clhs60 + clhs564;
            lhs(24,10)=DN(6,0)*clhs62 + DN(6,1)*clhs64 + DN(6,2)*clhs66 + clhs592;
            lhs(24,11)=-clhs614;
            lhs(24,12)=DN(6,0)*clhs68 + DN(6,1)*clhs70 + DN(6,2)*clhs72 + clhs643;
            lhs(24,13)=DN(6,0)*clhs77 + DN(6,1)*clhs79 + DN(6,2)*clhs82 + clhs667;
            lhs(24,14)=DN(6,0)*clhs84 + DN(6,1)*clhs86 + DN(6,2)*clhs88 + clhs690;
            lhs(24,15)=-clhs708;
            lhs(24,16)=DN(6,0)*clhs90 + DN(6,1)*clhs92 + DN(6,2)*clhs94 + clhs731;
            lhs(24,17)=DN(6,0)*clhs99 + DN(6,1)*clhs101 + DN(6,2)*clhs104 + clhs750;
            lhs(24,18)=DN(6,0)*clhs106 + DN(6,1)*clhs108 + DN(6,2)*clhs110 + clhs768;
            lhs(24,19)=-clhs782;
            lhs(24,20)=DN(6,0)*clhs112 + DN(6,1)*clhs114 + DN(6,2)*clhs116 + clhs799;
            lhs(24,21)=DN(6,0)*clhs121 + DN(6,1)*clhs123 + DN(6,2)*clhs126 + clhs813;
            lhs(24,22)=DN(6,0)*clhs128 + DN(6,1)*clhs130 + DN(6,2)*clhs132 + clhs826;
            lhs(24,23)=-clhs836;
            lhs(24,24)=DN(6,0)*clhs134 + DN(6,1)*clhs136 + DN(6,2)*clhs138 + clhs844*mu + clhs845;
            lhs(24,25)=DN(6,0)*clhs143 + DN(6,1)*clhs145 + DN(6,2)*clhs148 + clhs847;
            lhs(24,26)=DN(6,0)*clhs150 + DN(6,1)*clhs152 + DN(6,2)*clhs154 + clhs848;
            lhs(24,27)=-clhs849;
            lhs(24,28)=DN(6,0)*clhs156 + DN(6,1)*clhs158 + DN(6,2)*clhs160 + clhs852;
            lhs(24,29)=DN(6,0)*clhs165 + DN(6,1)*clhs167 + DN(6,2)*clhs170 + clhs853;
            lhs(24,30)=DN(6,0)*clhs172 + DN(6,1)*clhs174 + DN(6,2)*clhs176 + clhs854;
            lhs(24,31)=-clhs855;
            lhs(25,0)=DN(6,0)*clhs5 + DN(6,1)*clhs178 + DN(6,2)*clhs179 + clhs142;
            lhs(25,1)=DN(6,0)*clhs12 + DN(6,1)*clhs181 + DN(6,2)*clhs183 + clhs262;
            lhs(25,2)=DN(6,0)*clhs19 + DN(6,1)*clhs186 + DN(6,2)*clhs188 + clhs329;
            lhs(25,3)=-clhs367;
            lhs(25,4)=DN(6,0)*clhs25 + DN(6,1)*clhs191 + DN(6,2)*clhs192 + clhs408;
            lhs(25,5)=DN(6,0)*clhs35 + DN(6,1)*clhs193 + DN(6,2)*clhs195 + clhs443;
            lhs(25,6)=DN(6,0)*clhs42 + DN(6,1)*clhs199 + DN(6,2)*clhs201 + clhs475;
            lhs(25,7)=-clhs501;
            lhs(25,8)=DN(6,0)*clhs48 + DN(6,1)*clhs204 + DN(6,2)*clhs205 + clhs536;
            lhs(25,9)=DN(6,0)*clhs57 + DN(6,1)*clhs206 + DN(6,2)*clhs208 + clhs566;
            lhs(25,10)=DN(6,0)*clhs64 + DN(6,1)*clhs212 + DN(6,2)*clhs214 + clhs593;
            lhs(25,11)=-clhs615;
            lhs(25,12)=DN(6,0)*clhs70 + DN(6,1)*clhs217 + DN(6,2)*clhs218 + clhs644;
            lhs(25,13)=DN(6,0)*clhs79 + DN(6,1)*clhs219 + DN(6,2)*clhs221 + clhs669;
            lhs(25,14)=DN(6,0)*clhs86 + DN(6,1)*clhs225 + DN(6,2)*clhs227 + clhs691;
            lhs(25,15)=-clhs709;
            lhs(25,16)=DN(6,0)*clhs92 + DN(6,1)*clhs230 + DN(6,2)*clhs231 + clhs732;
            lhs(25,17)=DN(6,0)*clhs101 + DN(6,1)*clhs232 + DN(6,2)*clhs234 + clhs752;
            lhs(25,18)=DN(6,0)*clhs108 + DN(6,1)*clhs238 + DN(6,2)*clhs240 + clhs769;
            lhs(25,19)=-clhs783;
            lhs(25,20)=DN(6,0)*clhs114 + DN(6,1)*clhs243 + DN(6,2)*clhs244 + clhs800;
            lhs(25,21)=DN(6,0)*clhs123 + DN(6,1)*clhs245 + DN(6,2)*clhs247 + clhs815;
            lhs(25,22)=DN(6,0)*clhs130 + DN(6,1)*clhs251 + DN(6,2)*clhs253 + clhs827;
            lhs(25,23)=-clhs837;
            lhs(25,24)=DN(6,0)*clhs136 + DN(6,1)*clhs256 + DN(6,2)*clhs257 + clhs847;
            lhs(25,25)=DN(6,0)*clhs145 + DN(6,1)*clhs258 + DN(6,2)*clhs260 + clhs845 + clhs856*mu;
            lhs(25,26)=DN(6,0)*clhs152 + DN(6,1)*clhs264 + DN(6,2)*clhs266 + clhs858;
            lhs(25,27)=-clhs859;
            lhs(25,28)=DN(6,0)*clhs158 + DN(6,1)*clhs269 + DN(6,2)*clhs270 + clhs860;
            lhs(25,29)=DN(6,0)*clhs167 + DN(6,1)*clhs271 + DN(6,2)*clhs273 + clhs862;
            lhs(25,30)=DN(6,0)*clhs174 + DN(6,1)*clhs277 + DN(6,2)*clhs279 + clhs863;
            lhs(25,31)=-clhs864;
            lhs(26,0)=DN(6,0)*clhs7 + DN(6,1)*clhs179 + DN(6,2)*clhs281 + clhs149;
            lhs(26,1)=DN(6,0)*clhs15 + DN(6,1)*clhs183 + DN(6,2)*clhs282 + clhs263;
            lhs(26,2)=DN(6,0)*clhs21 + DN(6,1)*clhs188 + DN(6,2)*clhs284 + clhs333;
            lhs(26,3)=-clhs368;
            lhs(26,4)=DN(6,0)*clhs27 + DN(6,1)*clhs192 + DN(6,2)*clhs288 + clhs409;
            lhs(26,5)=DN(6,0)*clhs38 + DN(6,1)*clhs195 + DN(6,2)*clhs290 + clhs444;
            lhs(26,6)=DN(6,0)*clhs44 + DN(6,1)*clhs201 + DN(6,2)*clhs291 + clhs477;
            lhs(26,7)=-clhs502;
            lhs(26,8)=DN(6,0)*clhs50 + DN(6,1)*clhs205 + DN(6,2)*clhs296 + clhs537;
            lhs(26,9)=DN(6,0)*clhs60 + DN(6,1)*clhs208 + DN(6,2)*clhs298 + clhs567;
            lhs(26,10)=DN(6,0)*clhs66 + DN(6,1)*clhs214 + DN(6,2)*clhs299 + clhs595;
            lhs(26,11)=-clhs616;
            lhs(26,12)=DN(6,0)*clhs72 + DN(6,1)*clhs218 + DN(6,2)*clhs304 + clhs645;
            lhs(26,13)=DN(6,0)*clhs82 + DN(6,1)*clhs221 + DN(6,2)*clhs306 + clhs670;
            lhs(26,14)=DN(6,0)*clhs88 + DN(6,1)*clhs227 + DN(6,2)*clhs307 + clhs693;
            lhs(26,15)=-clhs710;
            lhs(26,16)=DN(6,0)*clhs94 + DN(6,1)*clhs231 + DN(6,2)*clhs312 + clhs733;
            lhs(26,17)=DN(6,0)*clhs104 + DN(6,1)*clhs234 + DN(6,2)*clhs314 + clhs753;
            lhs(26,18)=DN(6,0)*clhs110 + DN(6,1)*clhs240 + DN(6,2)*clhs315 + clhs771;
            lhs(26,19)=-clhs784;
            lhs(26,20)=DN(6,0)*clhs116 + DN(6,1)*clhs244 + DN(6,2)*clhs320 + clhs801;
            lhs(26,21)=DN(6,0)*clhs126 + DN(6,1)*clhs247 + DN(6,2)*clhs322 + clhs816;
            lhs(26,22)=DN(6,0)*clhs132 + DN(6,1)*clhs253 + DN(6,2)*clhs323 + clhs829;
            lhs(26,23)=-clhs838;
            lhs(26,24)=DN(6,0)*clhs138 + DN(6,1)*clhs257 + DN(6,2)*clhs328 + clhs848;
            lhs(26,25)=DN(6,0)*clhs148 + DN(6,1)*clhs260 + DN(6,2)*clhs330 + clhs858;
            lhs(26,26)=DN(6,0)*clhs154 + DN(6,1)*clhs266 + DN(6,2)*clhs331 + clhs845 + clhs865*mu;
            lhs(26,27)=-clhs866;
            lhs(26,28)=DN(6,0)*clhs160 + DN(6,1)*clhs270 + DN(6,2)*clhs336 + clhs868;
            lhs(26,29)=DN(6,0)*clhs170 + DN(6,1)*clhs273 + DN(6,2)*clhs338 + clhs869;
            lhs(26,30)=DN(6,0)*clhs176 + DN(6,1)*clhs279 + DN(6,2)*clhs339 + clhs871;
            lhs(26,31)=-clhs872;
            lhs(27,0)=clhs155 + clhs344*clhs366;
            lhs(27,1)=clhs267 + clhs344*clhs367;
            lhs(27,2)=clhs334 + clhs344*clhs368;
            lhs(27,3)=clhs369;
            lhs(27,4)=clhs344*clhs500 + clhs410;
            lhs(27,5)=clhs344*clhs501 + clhs445;
            lhs(27,6)=clhs344*clhs502 + clhs478;
            lhs(27,7)=clhs503;
            lhs(27,8)=clhs344*clhs614 + clhs538;
            lhs(27,9)=clhs344*clhs615 + clhs568;
            lhs(27,10)=clhs344*clhs616 + clhs596;
            lhs(27,11)=clhs617;
            lhs(27,12)=clhs344*clhs708 + clhs646;
            lhs(27,13)=clhs344*clhs709 + clhs671;
            lhs(27,14)=clhs344*clhs710 + clhs694;
            lhs(27,15)=clhs711;
            lhs(27,16)=clhs344*clhs782 + clhs734;
            lhs(27,17)=clhs344*clhs783 + clhs754;
            lhs(27,18)=clhs344*clhs784 + clhs772;
            lhs(27,19)=clhs785;
            lhs(27,20)=clhs344*clhs836 + clhs802;
            lhs(27,21)=clhs344*clhs837 + clhs817;
            lhs(27,22)=clhs344*clhs838 + clhs830;
            lhs(27,23)=clhs839;
            lhs(27,24)=clhs345*clhs849;
            lhs(27,25)=clhs345*clhs859;
            lhs(27,26)=clhs345*clhs866;
            lhs(27,27)=clhs343*(clhs844 + clhs856 + clhs865);
            lhs(27,28)=clhs344*clhs855 + clhs873;
            lhs(27,29)=clhs344*clhs864 + clhs874;
            lhs(27,30)=clhs344*clhs872 + clhs875;
            lhs(27,31)=clhs876;
            lhs(28,0)=DN(7,0)*clhs3 + DN(7,1)*clhs5 + DN(7,2)*clhs7 + clhs163;
            lhs(28,1)=DN(7,0)*clhs10 + DN(7,1)*clhs12 + DN(7,2)*clhs15 + clhs268;
            lhs(28,2)=DN(7,0)*clhs17 + DN(7,1)*clhs19 + DN(7,2)*clhs21 + clhs335;
            lhs(28,3)=-clhs370;
            lhs(28,4)=DN(7,0)*clhs23 + DN(7,1)*clhs25 + DN(7,2)*clhs27 + clhs413;
            lhs(28,5)=DN(7,0)*clhs33 + DN(7,1)*clhs35 + DN(7,2)*clhs38 + clhs446;
            lhs(28,6)=DN(7,0)*clhs40 + DN(7,1)*clhs42 + DN(7,2)*clhs44 + clhs479;
            lhs(28,7)=-clhs504;
            lhs(28,8)=DN(7,0)*clhs46 + DN(7,1)*clhs48 + DN(7,2)*clhs50 + clhs541;
            lhs(28,9)=DN(7,0)*clhs55 + DN(7,1)*clhs57 + DN(7,2)*clhs60 + clhs569;
            lhs(28,10)=DN(7,0)*clhs62 + DN(7,1)*clhs64 + DN(7,2)*clhs66 + clhs597;
            lhs(28,11)=-clhs618;
            lhs(28,12)=DN(7,0)*clhs68 + DN(7,1)*clhs70 + DN(7,2)*clhs72 + clhs649;
            lhs(28,13)=DN(7,0)*clhs77 + DN(7,1)*clhs79 + DN(7,2)*clhs82 + clhs672;
            lhs(28,14)=DN(7,0)*clhs84 + DN(7,1)*clhs86 + DN(7,2)*clhs88 + clhs695;
            lhs(28,15)=-clhs712;
            lhs(28,16)=DN(7,0)*clhs90 + DN(7,1)*clhs92 + DN(7,2)*clhs94 + clhs737;
            lhs(28,17)=DN(7,0)*clhs99 + DN(7,1)*clhs101 + DN(7,2)*clhs104 + clhs755;
            lhs(28,18)=DN(7,0)*clhs106 + DN(7,1)*clhs108 + DN(7,2)*clhs110 + clhs773;
            lhs(28,19)=-clhs786;
            lhs(28,20)=DN(7,0)*clhs112 + DN(7,1)*clhs114 + DN(7,2)*clhs116 + clhs805;
            lhs(28,21)=DN(7,0)*clhs121 + DN(7,1)*clhs123 + DN(7,2)*clhs126 + clhs818;
            lhs(28,22)=DN(7,0)*clhs128 + DN(7,1)*clhs130 + DN(7,2)*clhs132 + clhs831;
            lhs(28,23)=-clhs840;
            lhs(28,24)=DN(7,0)*clhs134 + DN(7,1)*clhs136 + DN(7,2)*clhs138 + clhs852;
            lhs(28,25)=DN(7,0)*clhs143 + DN(7,1)*clhs145 + DN(7,2)*clhs148 + clhs860;
            lhs(28,26)=DN(7,0)*clhs150 + DN(7,1)*clhs152 + DN(7,2)*clhs154 + clhs868;
            lhs(28,27)=-clhs873;
            lhs(28,28)=DN(7,0)*clhs156 + DN(7,1)*clhs158 + DN(7,2)*clhs160 + clhs877*mu + clhs878;
            lhs(28,29)=DN(7,0)*clhs165 + DN(7,1)*clhs167 + DN(7,2)*clhs170 + clhs880;
            lhs(28,30)=DN(7,0)*clhs172 + DN(7,1)*clhs174 + DN(7,2)*clhs176 + clhs881;
            lhs(28,31)=-clhs882;
            lhs(29,0)=DN(7,0)*clhs5 + DN(7,1)*clhs178 + DN(7,2)*clhs179 + clhs164;
            lhs(29,1)=DN(7,0)*clhs12 + DN(7,1)*clhs181 + DN(7,2)*clhs183 + clhs275;
            lhs(29,2)=DN(7,0)*clhs19 + DN(7,1)*clhs186 + DN(7,2)*clhs188 + clhs337;
            lhs(29,3)=-clhs371;
            lhs(29,4)=DN(7,0)*clhs25 + DN(7,1)*clhs191 + DN(7,2)*clhs192 + clhs414;
            lhs(29,5)=DN(7,0)*clhs35 + DN(7,1)*clhs193 + DN(7,2)*clhs195 + clhs448;
            lhs(29,6)=DN(7,0)*clhs42 + DN(7,1)*clhs199 + DN(7,2)*clhs201 + clhs480;
            lhs(29,7)=-clhs505;
            lhs(29,8)=DN(7,0)*clhs48 + DN(7,1)*clhs204 + DN(7,2)*clhs205 + clhs542;
            lhs(29,9)=DN(7,0)*clhs57 + DN(7,1)*clhs206 + DN(7,2)*clhs208 + clhs571;
            lhs(29,10)=DN(7,0)*clhs64 + DN(7,1)*clhs212 + DN(7,2)*clhs214 + clhs598;
            lhs(29,11)=-clhs619;
            lhs(29,12)=DN(7,0)*clhs70 + DN(7,1)*clhs217 + DN(7,2)*clhs218 + clhs650;
            lhs(29,13)=DN(7,0)*clhs79 + DN(7,1)*clhs219 + DN(7,2)*clhs221 + clhs674;
            lhs(29,14)=DN(7,0)*clhs86 + DN(7,1)*clhs225 + DN(7,2)*clhs227 + clhs696;
            lhs(29,15)=-clhs713;
            lhs(29,16)=DN(7,0)*clhs92 + DN(7,1)*clhs230 + DN(7,2)*clhs231 + clhs738;
            lhs(29,17)=DN(7,0)*clhs101 + DN(7,1)*clhs232 + DN(7,2)*clhs234 + clhs757;
            lhs(29,18)=DN(7,0)*clhs108 + DN(7,1)*clhs238 + DN(7,2)*clhs240 + clhs774;
            lhs(29,19)=-clhs787;
            lhs(29,20)=DN(7,0)*clhs114 + DN(7,1)*clhs243 + DN(7,2)*clhs244 + clhs806;
            lhs(29,21)=DN(7,0)*clhs123 + DN(7,1)*clhs245 + DN(7,2)*clhs247 + clhs820;
            lhs(29,22)=DN(7,0)*clhs130 + DN(7,1)*clhs251 + DN(7,2)*clhs253 + clhs832;
            lhs(29,23)=-clhs841;
            lhs(29,24)=DN(7,0)*clhs136 + DN(7,1)*clhs256 + DN(7,2)*clhs257 + clhs853;
            lhs(29,25)=DN(7,0)*clhs145 + DN(7,1)*clhs258 + DN(7,2)*clhs260 + clhs862;
            lhs(29,26)=DN(7,0)*clhs152 + DN(7,1)*clhs264 + DN(7,2)*clhs266 + clhs869;
            lhs(29,27)=-clhs874;
            lhs(29,28)=DN(7,0)*clhs158 + DN(7,1)*clhs269 + DN(7,2)*clhs270 + clhs880;
            lhs(29,29)=DN(7,0)*clhs167 + DN(7,1)*clhs271 + DN(7,2)*clhs273 + clhs878 + clhs883*mu;
            lhs(29,30)=DN(7,0)*clhs174 + DN(7,1)*clhs277 + DN(7,2)*clhs279 + clhs884;
            lhs(29,31)=-clhs885;
            lhs(30,0)=DN(7,0)*clhs7 + DN(7,1)*clhs179 + DN(7,2)*clhs281 + clhs171;
            lhs(30,1)=DN(7,0)*clhs15 + DN(7,1)*clhs183 + DN(7,2)*clhs282 + clhs276;
            lhs(30,2)=DN(7,0)*clhs21 + DN(7,1)*clhs188 + DN(7,2)*clhs284 + clhs341;
            lhs(30,3)=-clhs372;
            lhs(30,4)=DN(7,0)*clhs27 + DN(7,1)*clhs192 + DN(7,2)*clhs288 + clhs415;
            lhs(30,5)=DN(7,0)*clhs38 + DN(7,1)*clhs195 + DN(7,2)*clhs290 + clhs449;
            lhs(30,6)=DN(7,0)*clhs44 + DN(7,1)*clhs201 + DN(7,2)*clhs291 + clhs482;
            lhs(30,7)=-clhs506;
            lhs(30,8)=DN(7,0)*clhs50 + DN(7,1)*clhs205 + DN(7,2)*clhs296 + clhs543;
            lhs(30,9)=DN(7,0)*clhs60 + DN(7,1)*clhs208 + DN(7,2)*clhs298 + clhs572;
            lhs(30,10)=DN(7,0)*clhs66 + DN(7,1)*clhs214 + DN(7,2)*clhs299 + clhs600;
            lhs(30,11)=-clhs620;
            lhs(30,12)=DN(7,0)*clhs72 + DN(7,1)*clhs218 + DN(7,2)*clhs304 + clhs651;
            lhs(30,13)=DN(7,0)*clhs82 + DN(7,1)*clhs221 + DN(7,2)*clhs306 + clhs675;
            lhs(30,14)=DN(7,0)*clhs88 + DN(7,1)*clhs227 + DN(7,2)*clhs307 + clhs698;
            lhs(30,15)=-clhs714;
            lhs(30,16)=DN(7,0)*clhs94 + DN(7,1)*clhs231 + DN(7,2)*clhs312 + clhs739;
            lhs(30,17)=DN(7,0)*clhs104 + DN(7,1)*clhs234 + DN(7,2)*clhs314 + clhs758;
            lhs(30,18)=DN(7,0)*clhs110 + DN(7,1)*clhs240 + DN(7,2)*clhs315 + clhs776;
            lhs(30,19)=-clhs788;
            lhs(30,20)=DN(7,0)*clhs116 + DN(7,1)*clhs244 + DN(7,2)*clhs320 + clhs807;
            lhs(30,21)=DN(7,0)*clhs126 + DN(7,1)*clhs247 + DN(7,2)*clhs322 + clhs821;
            lhs(30,22)=DN(7,0)*clhs132 + DN(7,1)*clhs253 + DN(7,2)*clhs323 + clhs834;
            lhs(30,23)=-clhs842;
            lhs(30,24)=DN(7,0)*clhs138 + DN(7,1)*clhs257 + DN(7,2)*clhs328 + clhs854;
            lhs(30,25)=DN(7,0)*clhs148 + DN(7,1)*clhs260 + DN(7,2)*clhs330 + clhs863;
            lhs(30,26)=DN(7,0)*clhs154 + DN(7,1)*clhs266 + DN(7,2)*clhs331 + clhs871;
            lhs(30,27)=-clhs875;
            lhs(30,28)=DN(7,0)*clhs160 + DN(7,1)*clhs270 + DN(7,2)*clhs336 + clhs881;
            lhs(30,29)=DN(7,0)*clhs170 + DN(7,1)*clhs273 + DN(7,2)*clhs338 + clhs884;
            lhs(30,30)=DN(7,0)*clhs176 + DN(7,1)*clhs279 + DN(7,2)*clhs339 + clhs878 + clhs886*mu;
            lhs(30,31)=-clhs887;
            lhs(31,0)=clhs177 + clhs344*clhs370;
            lhs(31,1)=clhs280 + clhs344*clhs371;
            lhs(31,2)=clhs342 + clhs344*clhs372;
            lhs(31,3)=clhs373;
            lhs(31,4)=clhs344*clhs504 + clhs416;
            lhs(31,5)=clhs344*clhs505 + clhs450;
            lhs(31,6)=clhs344*clhs506 + clhs483;
            lhs(31,7)=clhs507;
            lhs(31,8)=clhs344*clhs618 + clhs544;
            lhs(31,9)=clhs344*clhs619 + clhs573;
            lhs(31,10)=clhs344*clhs620 + clhs601;
            lhs(31,11)=clhs621;
            lhs(31,12)=clhs344*clhs712 + clhs652;
            lhs(31,13)=clhs344*clhs713 + clhs676;
            lhs(31,14)=clhs344*clhs714 + clhs699;
            lhs(31,15)=clhs715;
            lhs(31,16)=clhs344*clhs786 + clhs740;
            lhs(31,17)=clhs344*clhs787 + clhs759;
            lhs(31,18)=clhs344*clhs788 + clhs777;
            lhs(31,19)=clhs789;
            lhs(31,20)=clhs344*clhs840 + clhs808;
            lhs(31,21)=clhs344*clhs841 + clhs822;
            lhs(31,22)=clhs344*clhs842 + clhs835;
            lhs(31,23)=clhs843;
            lhs(31,24)=clhs344*clhs873 + clhs855;
            lhs(31,25)=clhs344*clhs874 + clhs864;
            lhs(31,26)=clhs344*clhs875 + clhs872;
            lhs(31,27)=clhs876;
            lhs(31,28)=clhs345*clhs882;
            lhs(31,29)=clhs345*clhs885;
            lhs(31,30)=clhs345*clhs887;
            lhs(31,31)=clhs343*(clhs877 + clhs883 + clhs886);


    // Add intermediate results to local system
    noalias(rLHS) += lhs * rData.Weight;
}

template <>
void SymbolicStokes<SymbolicStokesData<2,3>>::ComputeGaussPointRHSContribution(
    SymbolicStokesData<2,3> &rData,
    VectorType &rRHS)
{

    const double rho = rData.Density;
    const double mu = rData.EffectiveViscosity;

    const double h = rData.ElementSize;

    const double dt = rData.DeltaTime;
    const double bdf0 = rData.bdf0;
    const double bdf1 = rData.bdf1;
    const double bdf2 = rData.bdf2;

    const double dyn_tau = rData.DynamicTau;

    const auto &v = rData.Velocity;
    const auto &vn = rData.Velocity_OldStep1;
    const auto &vnn = rData.Velocity_OldStep2;
    const auto &f = rData.BodyForce;
    const auto &p = rData.Pressure;
    const auto &stress = rData.ShearStress;

    // Get shape function values
    const auto &N = rData.N;
    const auto &DN = rData.DN_DX;

    // Stabilization parameters
    constexpr double stab_c1 = 4.0;
    constexpr double stab_c2 = 2.0;

    auto &rhs = rData.rhs;
    double dt_inv = 0.0;
    if (dt > 1e-09)
    {
        dt_inv = 1.0/dt;
    }
    if (std::abs(bdf0) < 1e-9)
    {
        dt_inv = 0.0;
    }

    const double crhs0 =             N[0]*p[0] + N[1]*p[1] + N[2]*p[2];
const double crhs1 =             rho*(N[0]*f(0,0) + N[1]*f(1,0) + N[2]*f(2,0));
const double crhs2 =             DN(0,0)*v(0,0) + DN(0,1)*v(0,1) + DN(1,0)*v(1,0) + DN(1,1)*v(1,1) + DN(2,0)*v(2,0) + DN(2,1)*v(2,1);
const double crhs3 =             crhs2*mu;
const double crhs4 =             rho*(N[0]*(bdf0*v(0,0) + bdf1*vn(0,0) + bdf2*vnn(0,0)) + N[1]*(bdf0*v(1,0) + bdf1*vn(1,0) + bdf2*vnn(1,0)) + N[2]*(bdf0*v(2,0) + bdf1*vn(2,0) + bdf2*vnn(2,0)));
const double crhs5 =             rho*(N[0]*f(0,1) + N[1]*f(1,1) + N[2]*f(2,1));
const double crhs6 =             rho*(N[0]*(bdf0*v(0,1) + bdf1*vn(0,1) + bdf2*vnn(0,1)) + N[1]*(bdf0*v(1,1) + bdf1*vn(1,1) + bdf2*vnn(1,1)) + N[2]*(bdf0*v(2,1) + bdf1*vn(2,1) + bdf2*vnn(2,1)));
const double crhs7 =             1.0/(dt_inv*dyn_tau*rho + mu*stab_c1/pow(h, 2));
const double crhs8 =             crhs7*(DN(0,0)*p[0] + DN(1,0)*p[1] + DN(2,0)*p[2] - crhs1 + crhs4);
const double crhs9 =             crhs7*(DN(0,1)*p[0] + DN(1,1)*p[1] + DN(2,1)*p[2] - crhs5 + crhs6);
            rhs[0]=DN(0,0)*crhs0 - DN(0,0)*crhs3 - DN(0,0)*stress[0] - DN(0,1)*stress[2] + N[0]*crhs1 - N[0]*crhs4;
            rhs[1]=-DN(0,0)*stress[2] + DN(0,1)*crhs0 - DN(0,1)*crhs3 - DN(0,1)*stress[1] + N[0]*crhs5 - N[0]*crhs6;
            rhs[2]=-DN(0,0)*crhs8 - DN(0,1)*crhs9 - N[0]*crhs2;
            rhs[3]=DN(1,0)*crhs0 - DN(1,0)*crhs3 - DN(1,0)*stress[0] - DN(1,1)*stress[2] + N[1]*crhs1 - N[1]*crhs4;
            rhs[4]=-DN(1,0)*stress[2] + DN(1,1)*crhs0 - DN(1,1)*crhs3 - DN(1,1)*stress[1] + N[1]*crhs5 - N[1]*crhs6;
            rhs[5]=-DN(1,0)*crhs8 - DN(1,1)*crhs9 - N[1]*crhs2;
            rhs[6]=DN(2,0)*crhs0 - DN(2,0)*crhs3 - DN(2,0)*stress[0] - DN(2,1)*stress[2] + N[2]*crhs1 - N[2]*crhs4;
            rhs[7]=-DN(2,0)*stress[2] + DN(2,1)*crhs0 - DN(2,1)*crhs3 - DN(2,1)*stress[1] + N[2]*crhs5 - N[2]*crhs6;
            rhs[8]=-DN(2,0)*crhs8 - DN(2,1)*crhs9 - N[2]*crhs2;


    noalias(rRHS) += rData.Weight * rhs;
}

template <>
void SymbolicStokes<SymbolicStokesData<2,4>>::ComputeGaussPointRHSContribution(
    SymbolicStokesData<2,4> &rData,
    VectorType &rRHS)
{

    const double rho = rData.Density;
    const double mu = rData.EffectiveViscosity;

    const double h = rData.ElementSize;

    const double dt = rData.DeltaTime;
    const double bdf0 = rData.bdf0;
    const double bdf1 = rData.bdf1;
    const double bdf2 = rData.bdf2;

    const double dyn_tau = rData.DynamicTau;

    const auto &v = rData.Velocity;
    const auto &vn = rData.Velocity_OldStep1;
    const auto &vnn = rData.Velocity_OldStep2;
    const auto &f = rData.BodyForce;
    const auto &p = rData.Pressure;
    const auto &stress = rData.ShearStress;

    // Get shape function values
    const auto &N = rData.N;
    const auto &DN = rData.DN_DX;

    // Stabilization parameters
    constexpr double stab_c1 = 4.0;
    constexpr double stab_c2 = 2.0;

    auto &rhs = rData.rhs;
    double dt_inv = 0.0;
    if (dt > 1e-09)
    {
        dt_inv = 1.0/dt;
    }
    if (std::abs(bdf0) < 1e-9)
    {
        dt_inv = 0.0;
    }
    const double crhs0 =             N[0]*p[0] + N[1]*p[1] + N[2]*p[2] + N[3]*p[3];
const double crhs1 =             rho*(N[0]*f(0,0) + N[1]*f(1,0) + N[2]*f(2,0) + N[3]*f(3,0));
const double crhs2 =             DN(0,0)*v(0,0) + DN(0,1)*v(0,1) + DN(1,0)*v(1,0) + DN(1,1)*v(1,1) + DN(2,0)*v(2,0) + DN(2,1)*v(2,1) + DN(3,0)*v(3,0) + DN(3,1)*v(3,1);
const double crhs3 =             crhs2*mu;
const double crhs4 =             rho*(N[0]*(bdf0*v(0,0) + bdf1*vn(0,0) + bdf2*vnn(0,0)) + N[1]*(bdf0*v(1,0) + bdf1*vn(1,0) + bdf2*vnn(1,0)) + N[2]*(bdf0*v(2,0) + bdf1*vn(2,0) + bdf2*vnn(2,0)) + N[3]*(bdf0*v(3,0) + bdf1*vn(3,0) + bdf2*vnn(3,0)));
const double crhs5 =             rho*(N[0]*f(0,1) + N[1]*f(1,1) + N[2]*f(2,1) + N[3]*f(3,1));
const double crhs6 =             rho*(N[0]*(bdf0*v(0,1) + bdf1*vn(0,1) + bdf2*vnn(0,1)) + N[1]*(bdf0*v(1,1) + bdf1*vn(1,1) + bdf2*vnn(1,1)) + N[2]*(bdf0*v(2,1) + bdf1*vn(2,1) + bdf2*vnn(2,1)) + N[3]*(bdf0*v(3,1) + bdf1*vn(3,1) + bdf2*vnn(3,1)));
const double crhs7 =             1.0/(dt_inv*dyn_tau*rho + mu*stab_c1/pow(h, 2));
const double crhs8 =             crhs7*(DN(0,0)*p[0] + DN(1,0)*p[1] + DN(2,0)*p[2] + DN(3,0)*p[3] - crhs1 + crhs4);
const double crhs9 =             crhs7*(DN(0,1)*p[0] + DN(1,1)*p[1] + DN(2,1)*p[2] + DN(3,1)*p[3] - crhs5 + crhs6);
            rhs[0]=DN(0,0)*crhs0 - DN(0,0)*crhs3 - DN(0,0)*stress[0] - DN(0,1)*stress[2] + N[0]*crhs1 - N[0]*crhs4;
            rhs[1]=-DN(0,0)*stress[2] + DN(0,1)*crhs0 - DN(0,1)*crhs3 - DN(0,1)*stress[1] + N[0]*crhs5 - N[0]*crhs6;
            rhs[2]=-DN(0,0)*crhs8 - DN(0,1)*crhs9 - N[0]*crhs2;
            rhs[3]=DN(1,0)*crhs0 - DN(1,0)*crhs3 - DN(1,0)*stress[0] - DN(1,1)*stress[2] + N[1]*crhs1 - N[1]*crhs4;
            rhs[4]=-DN(1,0)*stress[2] + DN(1,1)*crhs0 - DN(1,1)*crhs3 - DN(1,1)*stress[1] + N[1]*crhs5 - N[1]*crhs6;
            rhs[5]=-DN(1,0)*crhs8 - DN(1,1)*crhs9 - N[1]*crhs2;
            rhs[6]=DN(2,0)*crhs0 - DN(2,0)*crhs3 - DN(2,0)*stress[0] - DN(2,1)*stress[2] + N[2]*crhs1 - N[2]*crhs4;
            rhs[7]=-DN(2,0)*stress[2] + DN(2,1)*crhs0 - DN(2,1)*crhs3 - DN(2,1)*stress[1] + N[2]*crhs5 - N[2]*crhs6;
            rhs[8]=-DN(2,0)*crhs8 - DN(2,1)*crhs9 - N[2]*crhs2;
            rhs[9]=DN(3,0)*crhs0 - DN(3,0)*crhs3 - DN(3,0)*stress[0] - DN(3,1)*stress[2] + N[3]*crhs1 - N[3]*crhs4;
            rhs[10]=-DN(3,0)*stress[2] + DN(3,1)*crhs0 - DN(3,1)*crhs3 - DN(3,1)*stress[1] + N[3]*crhs5 - N[3]*crhs6;
            rhs[11]=-DN(3,0)*crhs8 - DN(3,1)*crhs9 - N[3]*crhs2;


    noalias(rRHS) += rData.Weight * rhs;
}

template <>
void SymbolicStokes<SymbolicStokesData<3,4>>::ComputeGaussPointRHSContribution(
    SymbolicStokesData<3,4> &rData,
    VectorType &rRHS)
{

    const double rho = rData.Density;
    const double mu = rData.EffectiveViscosity;

    const double h = rData.ElementSize;

    const double dt = rData.DeltaTime;
    const double bdf0 = rData.bdf0;
    const double bdf1 = rData.bdf1;
    const double bdf2 = rData.bdf2;

    const double dyn_tau = rData.DynamicTau;

    const auto &v = rData.Velocity;
    const auto &vn = rData.Velocity_OldStep1;
    const auto &vnn = rData.Velocity_OldStep2;
    const auto &f = rData.BodyForce;
    const auto &p = rData.Pressure;
    const auto &stress = rData.ShearStress;

    // Get shape function values
    const auto &N = rData.N;
    const auto &DN = rData.DN_DX;

    // Stabilization parameters
    constexpr double stab_c1 = 4.0;
    constexpr double stab_c2 = 2.0;

    auto &rhs = rData.rhs;
    double dt_inv = 0.0;
    if (dt > 1e-09)
    {
        dt_inv = 1.0/dt;
    }
    if (std::abs(bdf0) < 1e-9)
    {
        dt_inv = 0.0;
    }

    const double crhs0 =             N[0]*p[0] + N[1]*p[1] + N[2]*p[2] + N[3]*p[3];
const double crhs1 =             rho*(N[0]*f(0,0) + N[1]*f(1,0) + N[2]*f(2,0) + N[3]*f(3,0));
const double crhs2 =             DN(0,0)*v(0,0) + DN(0,1)*v(0,1) + DN(0,2)*v(0,2) + DN(1,0)*v(1,0) + DN(1,1)*v(1,1) + DN(1,2)*v(1,2) + DN(2,0)*v(2,0) + DN(2,1)*v(2,1) + DN(2,2)*v(2,2) + DN(3,0)*v(3,0) + DN(3,1)*v(3,1) + DN(3,2)*v(3,2);
const double crhs3 =             crhs2*mu;
const double crhs4 =             rho*(N[0]*(bdf0*v(0,0) + bdf1*vn(0,0) + bdf2*vnn(0,0)) + N[1]*(bdf0*v(1,0) + bdf1*vn(1,0) + bdf2*vnn(1,0)) + N[2]*(bdf0*v(2,0) + bdf1*vn(2,0) + bdf2*vnn(2,0)) + N[3]*(bdf0*v(3,0) + bdf1*vn(3,0) + bdf2*vnn(3,0)));
const double crhs5 =             rho*(N[0]*f(0,1) + N[1]*f(1,1) + N[2]*f(2,1) + N[3]*f(3,1));
const double crhs6 =             rho*(N[0]*(bdf0*v(0,1) + bdf1*vn(0,1) + bdf2*vnn(0,1)) + N[1]*(bdf0*v(1,1) + bdf1*vn(1,1) + bdf2*vnn(1,1)) + N[2]*(bdf0*v(2,1) + bdf1*vn(2,1) + bdf2*vnn(2,1)) + N[3]*(bdf0*v(3,1) + bdf1*vn(3,1) + bdf2*vnn(3,1)));
const double crhs7 =             rho*(N[0]*f(0,2) + N[1]*f(1,2) + N[2]*f(2,2) + N[3]*f(3,2));
const double crhs8 =             rho*(N[0]*(bdf0*v(0,2) + bdf1*vn(0,2) + bdf2*vnn(0,2)) + N[1]*(bdf0*v(1,2) + bdf1*vn(1,2) + bdf2*vnn(1,2)) + N[2]*(bdf0*v(2,2) + bdf1*vn(2,2) + bdf2*vnn(2,2)) + N[3]*(bdf0*v(3,2) + bdf1*vn(3,2) + bdf2*vnn(3,2)));
const double crhs9 =             1.0/(dt_inv*dyn_tau*rho + mu*stab_c1/pow(h, 2));
const double crhs10 =             crhs9*(DN(0,0)*p[0] + DN(1,0)*p[1] + DN(2,0)*p[2] + DN(3,0)*p[3] - crhs1 + crhs4);
const double crhs11 =             crhs9*(DN(0,1)*p[0] + DN(1,1)*p[1] + DN(2,1)*p[2] + DN(3,1)*p[3] - crhs5 + crhs6);
const double crhs12 =             crhs9*(DN(0,2)*p[0] + DN(1,2)*p[1] + DN(2,2)*p[2] + DN(3,2)*p[3] - crhs7 + crhs8);
            rhs[0]=DN(0,0)*crhs0 - DN(0,0)*crhs3 - DN(0,0)*stress[0] - DN(0,1)*stress[3] - DN(0,2)*stress[5] + N[0]*crhs1 - N[0]*crhs4;
            rhs[1]=-DN(0,0)*stress[3] + DN(0,1)*crhs0 - DN(0,1)*crhs3 - DN(0,1)*stress[1] - DN(0,2)*stress[4] + N[0]*crhs5 - N[0]*crhs6;
            rhs[2]=-DN(0,0)*stress[5] - DN(0,1)*stress[4] + DN(0,2)*crhs0 - DN(0,2)*crhs3 - DN(0,2)*stress[2] + N[0]*crhs7 - N[0]*crhs8;
            rhs[3]=-DN(0,0)*crhs10 - DN(0,1)*crhs11 - DN(0,2)*crhs12 - N[0]*crhs2;
            rhs[4]=DN(1,0)*crhs0 - DN(1,0)*crhs3 - DN(1,0)*stress[0] - DN(1,1)*stress[3] - DN(1,2)*stress[5] + N[1]*crhs1 - N[1]*crhs4;
            rhs[5]=-DN(1,0)*stress[3] + DN(1,1)*crhs0 - DN(1,1)*crhs3 - DN(1,1)*stress[1] - DN(1,2)*stress[4] + N[1]*crhs5 - N[1]*crhs6;
            rhs[6]=-DN(1,0)*stress[5] - DN(1,1)*stress[4] + DN(1,2)*crhs0 - DN(1,2)*crhs3 - DN(1,2)*stress[2] + N[1]*crhs7 - N[1]*crhs8;
            rhs[7]=-DN(1,0)*crhs10 - DN(1,1)*crhs11 - DN(1,2)*crhs12 - N[1]*crhs2;
            rhs[8]=DN(2,0)*crhs0 - DN(2,0)*crhs3 - DN(2,0)*stress[0] - DN(2,1)*stress[3] - DN(2,2)*stress[5] + N[2]*crhs1 - N[2]*crhs4;
            rhs[9]=-DN(2,0)*stress[3] + DN(2,1)*crhs0 - DN(2,1)*crhs3 - DN(2,1)*stress[1] - DN(2,2)*stress[4] + N[2]*crhs5 - N[2]*crhs6;
            rhs[10]=-DN(2,0)*stress[5] - DN(2,1)*stress[4] + DN(2,2)*crhs0 - DN(2,2)*crhs3 - DN(2,2)*stress[2] + N[2]*crhs7 - N[2]*crhs8;
            rhs[11]=-DN(2,0)*crhs10 - DN(2,1)*crhs11 - DN(2,2)*crhs12 - N[2]*crhs2;
            rhs[12]=DN(3,0)*crhs0 - DN(3,0)*crhs3 - DN(3,0)*stress[0] - DN(3,1)*stress[3] - DN(3,2)*stress[5] + N[3]*crhs1 - N[3]*crhs4;
            rhs[13]=-DN(3,0)*stress[3] + DN(3,1)*crhs0 - DN(3,1)*crhs3 - DN(3,1)*stress[1] - DN(3,2)*stress[4] + N[3]*crhs5 - N[3]*crhs6;
            rhs[14]=-DN(3,0)*stress[5] - DN(3,1)*stress[4] + DN(3,2)*crhs0 - DN(3,2)*crhs3 - DN(3,2)*stress[2] + N[3]*crhs7 - N[3]*crhs8;
            rhs[15]=-DN(3,0)*crhs10 - DN(3,1)*crhs11 - DN(3,2)*crhs12 - N[3]*crhs2;


    noalias(rRHS) += rData.Weight * rhs;
}

template <>
void SymbolicStokes<SymbolicStokesData<3,6>>::ComputeGaussPointRHSContribution(
    SymbolicStokesData<3,6> &rData,
    VectorType &rRHS)
{

    const double rho = rData.Density;
    const double mu = rData.EffectiveViscosity;

    const double h = rData.ElementSize;

    const double dt = rData.DeltaTime;
    const double bdf0 = rData.bdf0;
    const double bdf1 = rData.bdf1;
    const double bdf2 = rData.bdf2;

    const double dyn_tau = rData.DynamicTau;

    const auto &v = rData.Velocity;
    const auto &vn = rData.Velocity_OldStep1;
    const auto &vnn = rData.Velocity_OldStep2;
    const auto &f = rData.BodyForce;
    const auto &p = rData.Pressure;
    const auto &stress = rData.ShearStress;

    // Get shape function values
    const auto &N = rData.N;
    const auto &DN = rData.DN_DX;

    // Stabilization parameters
    constexpr double stab_c1 = 4.0;
    constexpr double stab_c2 = 2.0;

    auto &rhs = rData.rhs;
    double dt_inv = 0.0;
    if (dt > 1e-09)
    {
        dt_inv = 1.0/dt;
    }
    if (std::abs(bdf0) < 1e-9)
    {
        dt_inv = 0.0;
    }

    const double crhs0 =             N[0]*p[0] + N[1]*p[1] + N[2]*p[2] + N[3]*p[3] + N[4]*p[4] + N[5]*p[5];
const double crhs1 =             rho*(N[0]*f(0,0) + N[1]*f(1,0) + N[2]*f(2,0) + N[3]*f(3,0) + N[4]*f(4,0) + N[5]*f(5,0));
const double crhs2 =             DN(0,0)*v(0,0) + DN(0,1)*v(0,1) + DN(0,2)*v(0,2) + DN(1,0)*v(1,0) + DN(1,1)*v(1,1) + DN(1,2)*v(1,2) + DN(2,0)*v(2,0) + DN(2,1)*v(2,1) + DN(2,2)*v(2,2) + DN(3,0)*v(3,0) + DN(3,1)*v(3,1) + DN(3,2)*v(3,2) + DN(4,0)*v(4,0) + DN(4,1)*v(4,1) + DN(4,2)*v(4,2) + DN(5,0)*v(5,0) + DN(5,1)*v(5,1) + DN(5,2)*v(5,2);
const double crhs3 =             crhs2*mu;
const double crhs4 =             rho*(N[0]*(bdf0*v(0,0) + bdf1*vn(0,0) + bdf2*vnn(0,0)) + N[1]*(bdf0*v(1,0) + bdf1*vn(1,0) + bdf2*vnn(1,0)) + N[2]*(bdf0*v(2,0) + bdf1*vn(2,0) + bdf2*vnn(2,0)) + N[3]*(bdf0*v(3,0) + bdf1*vn(3,0) + bdf2*vnn(3,0)) + N[4]*(bdf0*v(4,0) + bdf1*vn(4,0) + bdf2*vnn(4,0)) + N[5]*(bdf0*v(5,0) + bdf1*vn(5,0) + bdf2*vnn(5,0)));
const double crhs5 =             rho*(N[0]*f(0,1) + N[1]*f(1,1) + N[2]*f(2,1) + N[3]*f(3,1) + N[4]*f(4,1) + N[5]*f(5,1));
const double crhs6 =             rho*(N[0]*(bdf0*v(0,1) + bdf1*vn(0,1) + bdf2*vnn(0,1)) + N[1]*(bdf0*v(1,1) + bdf1*vn(1,1) + bdf2*vnn(1,1)) + N[2]*(bdf0*v(2,1) + bdf1*vn(2,1) + bdf2*vnn(2,1)) + N[3]*(bdf0*v(3,1) + bdf1*vn(3,1) + bdf2*vnn(3,1)) + N[4]*(bdf0*v(4,1) + bdf1*vn(4,1) + bdf2*vnn(4,1)) + N[5]*(bdf0*v(5,1) + bdf1*vn(5,1) + bdf2*vnn(5,1)));
const double crhs7 =             rho*(N[0]*f(0,2) + N[1]*f(1,2) + N[2]*f(2,2) + N[3]*f(3,2) + N[4]*f(4,2) + N[5]*f(5,2));
const double crhs8 =             rho*(N[0]*(bdf0*v(0,2) + bdf1*vn(0,2) + bdf2*vnn(0,2)) + N[1]*(bdf0*v(1,2) + bdf1*vn(1,2) + bdf2*vnn(1,2)) + N[2]*(bdf0*v(2,2) + bdf1*vn(2,2) + bdf2*vnn(2,2)) + N[3]*(bdf0*v(3,2) + bdf1*vn(3,2) + bdf2*vnn(3,2)) + N[4]*(bdf0*v(4,2) + bdf1*vn(4,2) + bdf2*vnn(4,2)) + N[5]*(bdf0*v(5,2) + bdf1*vn(5,2) + bdf2*vnn(5,2)));
const double crhs9 =             1.0/(dt_inv*dyn_tau*rho + mu*stab_c1/pow(h, 2));
const double crhs10 =             crhs9*(DN(0,0)*p[0] + DN(1,0)*p[1] + DN(2,0)*p[2] + DN(3,0)*p[3] + DN(4,0)*p[4] + DN(5,0)*p[5] - crhs1 + crhs4);
const double crhs11 =             crhs9*(DN(0,1)*p[0] + DN(1,1)*p[1] + DN(2,1)*p[2] + DN(3,1)*p[3] + DN(4,1)*p[4] + DN(5,1)*p[5] - crhs5 + crhs6);
const double crhs12 =             crhs9*(DN(0,2)*p[0] + DN(1,2)*p[1] + DN(2,2)*p[2] + DN(3,2)*p[3] + DN(4,2)*p[4] + DN(5,2)*p[5] - crhs7 + crhs8);
            rhs[0]=DN(0,0)*crhs0 - DN(0,0)*crhs3 - DN(0,0)*stress[0] - DN(0,1)*stress[3] - DN(0,2)*stress[5] + N[0]*crhs1 - N[0]*crhs4;
            rhs[1]=-DN(0,0)*stress[3] + DN(0,1)*crhs0 - DN(0,1)*crhs3 - DN(0,1)*stress[1] - DN(0,2)*stress[4] + N[0]*crhs5 - N[0]*crhs6;
            rhs[2]=-DN(0,0)*stress[5] - DN(0,1)*stress[4] + DN(0,2)*crhs0 - DN(0,2)*crhs3 - DN(0,2)*stress[2] + N[0]*crhs7 - N[0]*crhs8;
            rhs[3]=-DN(0,0)*crhs10 - DN(0,1)*crhs11 - DN(0,2)*crhs12 - N[0]*crhs2;
            rhs[4]=DN(1,0)*crhs0 - DN(1,0)*crhs3 - DN(1,0)*stress[0] - DN(1,1)*stress[3] - DN(1,2)*stress[5] + N[1]*crhs1 - N[1]*crhs4;
            rhs[5]=-DN(1,0)*stress[3] + DN(1,1)*crhs0 - DN(1,1)*crhs3 - DN(1,1)*stress[1] - DN(1,2)*stress[4] + N[1]*crhs5 - N[1]*crhs6;
            rhs[6]=-DN(1,0)*stress[5] - DN(1,1)*stress[4] + DN(1,2)*crhs0 - DN(1,2)*crhs3 - DN(1,2)*stress[2] + N[1]*crhs7 - N[1]*crhs8;
            rhs[7]=-DN(1,0)*crhs10 - DN(1,1)*crhs11 - DN(1,2)*crhs12 - N[1]*crhs2;
            rhs[8]=DN(2,0)*crhs0 - DN(2,0)*crhs3 - DN(2,0)*stress[0] - DN(2,1)*stress[3] - DN(2,2)*stress[5] + N[2]*crhs1 - N[2]*crhs4;
            rhs[9]=-DN(2,0)*stress[3] + DN(2,1)*crhs0 - DN(2,1)*crhs3 - DN(2,1)*stress[1] - DN(2,2)*stress[4] + N[2]*crhs5 - N[2]*crhs6;
            rhs[10]=-DN(2,0)*stress[5] - DN(2,1)*stress[4] + DN(2,2)*crhs0 - DN(2,2)*crhs3 - DN(2,2)*stress[2] + N[2]*crhs7 - N[2]*crhs8;
            rhs[11]=-DN(2,0)*crhs10 - DN(2,1)*crhs11 - DN(2,2)*crhs12 - N[2]*crhs2;
            rhs[12]=DN(3,0)*crhs0 - DN(3,0)*crhs3 - DN(3,0)*stress[0] - DN(3,1)*stress[3] - DN(3,2)*stress[5] + N[3]*crhs1 - N[3]*crhs4;
            rhs[13]=-DN(3,0)*stress[3] + DN(3,1)*crhs0 - DN(3,1)*crhs3 - DN(3,1)*stress[1] - DN(3,2)*stress[4] + N[3]*crhs5 - N[3]*crhs6;
            rhs[14]=-DN(3,0)*stress[5] - DN(3,1)*stress[4] + DN(3,2)*crhs0 - DN(3,2)*crhs3 - DN(3,2)*stress[2] + N[3]*crhs7 - N[3]*crhs8;
            rhs[15]=-DN(3,0)*crhs10 - DN(3,1)*crhs11 - DN(3,2)*crhs12 - N[3]*crhs2;
            rhs[16]=DN(4,0)*crhs0 - DN(4,0)*crhs3 - DN(4,0)*stress[0] - DN(4,1)*stress[3] - DN(4,2)*stress[5] + N[4]*crhs1 - N[4]*crhs4;
            rhs[17]=-DN(4,0)*stress[3] + DN(4,1)*crhs0 - DN(4,1)*crhs3 - DN(4,1)*stress[1] - DN(4,2)*stress[4] + N[4]*crhs5 - N[4]*crhs6;
            rhs[18]=-DN(4,0)*stress[5] - DN(4,1)*stress[4] + DN(4,2)*crhs0 - DN(4,2)*crhs3 - DN(4,2)*stress[2] + N[4]*crhs7 - N[4]*crhs8;
            rhs[19]=-DN(4,0)*crhs10 - DN(4,1)*crhs11 - DN(4,2)*crhs12 - N[4]*crhs2;
            rhs[20]=DN(5,0)*crhs0 - DN(5,0)*crhs3 - DN(5,0)*stress[0] - DN(5,1)*stress[3] - DN(5,2)*stress[5] + N[5]*crhs1 - N[5]*crhs4;
            rhs[21]=-DN(5,0)*stress[3] + DN(5,1)*crhs0 - DN(5,1)*crhs3 - DN(5,1)*stress[1] - DN(5,2)*stress[4] + N[5]*crhs5 - N[5]*crhs6;
            rhs[22]=-DN(5,0)*stress[5] - DN(5,1)*stress[4] + DN(5,2)*crhs0 - DN(5,2)*crhs3 - DN(5,2)*stress[2] + N[5]*crhs7 - N[5]*crhs8;
            rhs[23]=-DN(5,0)*crhs10 - DN(5,1)*crhs11 - DN(5,2)*crhs12 - N[5]*crhs2;


    noalias(rRHS) += rData.Weight * rhs;
}

template <>
void SymbolicStokes<SymbolicStokesData<3,8>>::ComputeGaussPointRHSContribution(
    SymbolicStokesData<3,8> &rData,
    VectorType &rRHS)
{

    const double rho = rData.Density;
    const double mu = rData.EffectiveViscosity;

    const double h = rData.ElementSize;

    const double dt = rData.DeltaTime;
    const double bdf0 = rData.bdf0;
    const double bdf1 = rData.bdf1;
    const double bdf2 = rData.bdf2;

    const double dyn_tau = rData.DynamicTau;

    const auto &v = rData.Velocity;
    const auto &vn = rData.Velocity_OldStep1;
    const auto &vnn = rData.Velocity_OldStep2;
    const auto &f = rData.BodyForce;
    const auto &p = rData.Pressure;
    const auto &stress = rData.ShearStress;

    // Get shape function values
    const auto &N = rData.N;
    const auto &DN = rData.DN_DX;

    // Stabilization parameters
    constexpr double stab_c1 = 4.0;
    constexpr double stab_c2 = 2.0;

    auto &rhs = rData.rhs;
    double dt_inv = 0.0;
    if (dt > 1e-09)
    {
        dt_inv = 1.0/dt;
    }
    if (std::abs(bdf0) < 1e-9)
    {
        dt_inv = 0.0;
    }

    const double crhs0 =             N[0]*p[0] + N[1]*p[1] + N[2]*p[2] + N[3]*p[3] + N[4]*p[4] + N[5]*p[5] + N[6]*p[6] + N[7]*p[7];
const double crhs1 =             rho*(N[0]*f(0,0) + N[1]*f(1,0) + N[2]*f(2,0) + N[3]*f(3,0) + N[4]*f(4,0) + N[5]*f(5,0) + N[6]*f(6,0) + N[7]*f(7,0));
const double crhs2 =             DN(0,0)*v(0,0) + DN(0,1)*v(0,1) + DN(0,2)*v(0,2) + DN(1,0)*v(1,0) + DN(1,1)*v(1,1) + DN(1,2)*v(1,2) + DN(2,0)*v(2,0) + DN(2,1)*v(2,1) + DN(2,2)*v(2,2) + DN(3,0)*v(3,0) + DN(3,1)*v(3,1) + DN(3,2)*v(3,2) + DN(4,0)*v(4,0) + DN(4,1)*v(4,1) + DN(4,2)*v(4,2) + DN(5,0)*v(5,0) + DN(5,1)*v(5,1) + DN(5,2)*v(5,2) + DN(6,0)*v(6,0) + DN(6,1)*v(6,1) + DN(6,2)*v(6,2) + DN(7,0)*v(7,0) + DN(7,1)*v(7,1) + DN(7,2)*v(7,2);
const double crhs3 =             crhs2*mu;
const double crhs4 =             rho*(N[0]*(bdf0*v(0,0) + bdf1*vn(0,0) + bdf2*vnn(0,0)) + N[1]*(bdf0*v(1,0) + bdf1*vn(1,0) + bdf2*vnn(1,0)) + N[2]*(bdf0*v(2,0) + bdf1*vn(2,0) + bdf2*vnn(2,0)) + N[3]*(bdf0*v(3,0) + bdf1*vn(3,0) + bdf2*vnn(3,0)) + N[4]*(bdf0*v(4,0) + bdf1*vn(4,0) + bdf2*vnn(4,0)) + N[5]*(bdf0*v(5,0) + bdf1*vn(5,0) + bdf2*vnn(5,0)) + N[6]*(bdf0*v(6,0) + bdf1*vn(6,0) + bdf2*vnn(6,0)) + N[7]*(bdf0*v(7,0) + bdf1*vn(7,0) + bdf2*vnn(7,0)));
const double crhs5 =             rho*(N[0]*f(0,1) + N[1]*f(1,1) + N[2]*f(2,1) + N[3]*f(3,1) + N[4]*f(4,1) + N[5]*f(5,1) + N[6]*f(6,1) + N[7]*f(7,1));
const double crhs6 =             rho*(N[0]*(bdf0*v(0,1) + bdf1*vn(0,1) + bdf2*vnn(0,1)) + N[1]*(bdf0*v(1,1) + bdf1*vn(1,1) + bdf2*vnn(1,1)) + N[2]*(bdf0*v(2,1) + bdf1*vn(2,1) + bdf2*vnn(2,1)) + N[3]*(bdf0*v(3,1) + bdf1*vn(3,1) + bdf2*vnn(3,1)) + N[4]*(bdf0*v(4,1) + bdf1*vn(4,1) + bdf2*vnn(4,1)) + N[5]*(bdf0*v(5,1) + bdf1*vn(5,1) + bdf2*vnn(5,1)) + N[6]*(bdf0*v(6,1) + bdf1*vn(6,1) + bdf2*vnn(6,1)) + N[7]*(bdf0*v(7,1) + bdf1*vn(7,1) + bdf2*vnn(7,1)));
const double crhs7 =             rho*(N[0]*f(0,2) + N[1]*f(1,2) + N[2]*f(2,2) + N[3]*f(3,2) + N[4]*f(4,2) + N[5]*f(5,2) + N[6]*f(6,2) + N[7]*f(7,2));
const double crhs8 =             rho*(N[0]*(bdf0*v(0,2) + bdf1*vn(0,2) + bdf2*vnn(0,2)) + N[1]*(bdf0*v(1,2) + bdf1*vn(1,2) + bdf2*vnn(1,2)) + N[2]*(bdf0*v(2,2) + bdf1*vn(2,2) + bdf2*vnn(2,2)) + N[3]*(bdf0*v(3,2) + bdf1*vn(3,2) + bdf2*vnn(3,2)) + N[4]*(bdf0*v(4,2) + bdf1*vn(4,2) + bdf2*vnn(4,2)) + N[5]*(bdf0*v(5,2) + bdf1*vn(5,2) + bdf2*vnn(5,2)) + N[6]*(bdf0*v(6,2) + bdf1*vn(6,2) + bdf2*vnn(6,2)) + N[7]*(bdf0*v(7,2) + bdf1*vn(7,2) + bdf2*vnn(7,2)));
const double crhs9 =             1.0/(dt_inv*dyn_tau*rho + mu*stab_c1/pow(h, 2));
const double crhs10 =             crhs9*(DN(0,0)*p[0] + DN(1,0)*p[1] + DN(2,0)*p[2] + DN(3,0)*p[3] + DN(4,0)*p[4] + DN(5,0)*p[5] + DN(6,0)*p[6] + DN(7,0)*p[7] - crhs1 + crhs4);
const double crhs11 =             crhs9*(DN(0,1)*p[0] + DN(1,1)*p[1] + DN(2,1)*p[2] + DN(3,1)*p[3] + DN(4,1)*p[4] + DN(5,1)*p[5] + DN(6,1)*p[6] + DN(7,1)*p[7] - crhs5 + crhs6);
const double crhs12 =             crhs9*(DN(0,2)*p[0] + DN(1,2)*p[1] + DN(2,2)*p[2] + DN(3,2)*p[3] + DN(4,2)*p[4] + DN(5,2)*p[5] + DN(6,2)*p[6] + DN(7,2)*p[7] - crhs7 + crhs8);
            rhs[0]=DN(0,0)*crhs0 - DN(0,0)*crhs3 - DN(0,0)*stress[0] - DN(0,1)*stress[3] - DN(0,2)*stress[5] + N[0]*crhs1 - N[0]*crhs4;
            rhs[1]=-DN(0,0)*stress[3] + DN(0,1)*crhs0 - DN(0,1)*crhs3 - DN(0,1)*stress[1] - DN(0,2)*stress[4] + N[0]*crhs5 - N[0]*crhs6;
            rhs[2]=-DN(0,0)*stress[5] - DN(0,1)*stress[4] + DN(0,2)*crhs0 - DN(0,2)*crhs3 - DN(0,2)*stress[2] + N[0]*crhs7 - N[0]*crhs8;
            rhs[3]=-DN(0,0)*crhs10 - DN(0,1)*crhs11 - DN(0,2)*crhs12 - N[0]*crhs2;
            rhs[4]=DN(1,0)*crhs0 - DN(1,0)*crhs3 - DN(1,0)*stress[0] - DN(1,1)*stress[3] - DN(1,2)*stress[5] + N[1]*crhs1 - N[1]*crhs4;
            rhs[5]=-DN(1,0)*stress[3] + DN(1,1)*crhs0 - DN(1,1)*crhs3 - DN(1,1)*stress[1] - DN(1,2)*stress[4] + N[1]*crhs5 - N[1]*crhs6;
            rhs[6]=-DN(1,0)*stress[5] - DN(1,1)*stress[4] + DN(1,2)*crhs0 - DN(1,2)*crhs3 - DN(1,2)*stress[2] + N[1]*crhs7 - N[1]*crhs8;
            rhs[7]=-DN(1,0)*crhs10 - DN(1,1)*crhs11 - DN(1,2)*crhs12 - N[1]*crhs2;
            rhs[8]=DN(2,0)*crhs0 - DN(2,0)*crhs3 - DN(2,0)*stress[0] - DN(2,1)*stress[3] - DN(2,2)*stress[5] + N[2]*crhs1 - N[2]*crhs4;
            rhs[9]=-DN(2,0)*stress[3] + DN(2,1)*crhs0 - DN(2,1)*crhs3 - DN(2,1)*stress[1] - DN(2,2)*stress[4] + N[2]*crhs5 - N[2]*crhs6;
            rhs[10]=-DN(2,0)*stress[5] - DN(2,1)*stress[4] + DN(2,2)*crhs0 - DN(2,2)*crhs3 - DN(2,2)*stress[2] + N[2]*crhs7 - N[2]*crhs8;
            rhs[11]=-DN(2,0)*crhs10 - DN(2,1)*crhs11 - DN(2,2)*crhs12 - N[2]*crhs2;
            rhs[12]=DN(3,0)*crhs0 - DN(3,0)*crhs3 - DN(3,0)*stress[0] - DN(3,1)*stress[3] - DN(3,2)*stress[5] + N[3]*crhs1 - N[3]*crhs4;
            rhs[13]=-DN(3,0)*stress[3] + DN(3,1)*crhs0 - DN(3,1)*crhs3 - DN(3,1)*stress[1] - DN(3,2)*stress[4] + N[3]*crhs5 - N[3]*crhs6;
            rhs[14]=-DN(3,0)*stress[5] - DN(3,1)*stress[4] + DN(3,2)*crhs0 - DN(3,2)*crhs3 - DN(3,2)*stress[2] + N[3]*crhs7 - N[3]*crhs8;
            rhs[15]=-DN(3,0)*crhs10 - DN(3,1)*crhs11 - DN(3,2)*crhs12 - N[3]*crhs2;
            rhs[16]=DN(4,0)*crhs0 - DN(4,0)*crhs3 - DN(4,0)*stress[0] - DN(4,1)*stress[3] - DN(4,2)*stress[5] + N[4]*crhs1 - N[4]*crhs4;
            rhs[17]=-DN(4,0)*stress[3] + DN(4,1)*crhs0 - DN(4,1)*crhs3 - DN(4,1)*stress[1] - DN(4,2)*stress[4] + N[4]*crhs5 - N[4]*crhs6;
            rhs[18]=-DN(4,0)*stress[5] - DN(4,1)*stress[4] + DN(4,2)*crhs0 - DN(4,2)*crhs3 - DN(4,2)*stress[2] + N[4]*crhs7 - N[4]*crhs8;
            rhs[19]=-DN(4,0)*crhs10 - DN(4,1)*crhs11 - DN(4,2)*crhs12 - N[4]*crhs2;
            rhs[20]=DN(5,0)*crhs0 - DN(5,0)*crhs3 - DN(5,0)*stress[0] - DN(5,1)*stress[3] - DN(5,2)*stress[5] + N[5]*crhs1 - N[5]*crhs4;
            rhs[21]=-DN(5,0)*stress[3] + DN(5,1)*crhs0 - DN(5,1)*crhs3 - DN(5,1)*stress[1] - DN(5,2)*stress[4] + N[5]*crhs5 - N[5]*crhs6;
            rhs[22]=-DN(5,0)*stress[5] - DN(5,1)*stress[4] + DN(5,2)*crhs0 - DN(5,2)*crhs3 - DN(5,2)*stress[2] + N[5]*crhs7 - N[5]*crhs8;
            rhs[23]=-DN(5,0)*crhs10 - DN(5,1)*crhs11 - DN(5,2)*crhs12 - N[5]*crhs2;
            rhs[24]=DN(6,0)*crhs0 - DN(6,0)*crhs3 - DN(6,0)*stress[0] - DN(6,1)*stress[3] - DN(6,2)*stress[5] + N[6]*crhs1 - N[6]*crhs4;
            rhs[25]=-DN(6,0)*stress[3] + DN(6,1)*crhs0 - DN(6,1)*crhs3 - DN(6,1)*stress[1] - DN(6,2)*stress[4] + N[6]*crhs5 - N[6]*crhs6;
            rhs[26]=-DN(6,0)*stress[5] - DN(6,1)*stress[4] + DN(6,2)*crhs0 - DN(6,2)*crhs3 - DN(6,2)*stress[2] + N[6]*crhs7 - N[6]*crhs8;
            rhs[27]=-DN(6,0)*crhs10 - DN(6,1)*crhs11 - DN(6,2)*crhs12 - N[6]*crhs2;
            rhs[28]=DN(7,0)*crhs0 - DN(7,0)*crhs3 - DN(7,0)*stress[0] - DN(7,1)*stress[3] - DN(7,2)*stress[5] + N[7]*crhs1 - N[7]*crhs4;
            rhs[29]=-DN(7,0)*stress[3] + DN(7,1)*crhs0 - DN(7,1)*crhs3 - DN(7,1)*stress[1] - DN(7,2)*stress[4] + N[7]*crhs5 - N[7]*crhs6;
            rhs[30]=-DN(7,0)*stress[5] - DN(7,1)*stress[4] + DN(7,2)*crhs0 - DN(7,2)*crhs3 - DN(7,2)*stress[2] + N[7]*crhs7 - N[7]*crhs8;
            rhs[31]=-DN(7,0)*crhs10 - DN(7,1)*crhs11 - DN(7,2)*crhs12 - N[7]*crhs2;


    noalias(rRHS) += rData.Weight * rhs;
}


template <class TElementData>
void SymbolicStokes<TElementData>::save(Serializer &rSerializer) const
{
    using BaseType = FluidElement<TElementData>;
    KRATOS_SERIALIZE_SAVE_BASE_CLASS(rSerializer, BaseType);
}

template <class TElementData>
void SymbolicStokes<TElementData>::load(Serializer &rSerializer)
{
    using BaseType = FluidElement<TElementData>;
    KRATOS_SERIALIZE_LOAD_BASE_CLASS(rSerializer, BaseType);
}


template <class TElementData>
void SymbolicStokes<TElementData>::GetValueOnIntegrationPoints(   const Variable<double> &rVariable,
                                                                        std::vector<double> &rValues,
                                                                        const ProcessInfo &rCurrentProcessInfo )
{
    if (rVariable == DIVERGENCE){

        const auto& rGeom = this->GetGeometry();
        const GeometryType::IntegrationPointsArrayType& IntegrationPoints = rGeom.IntegrationPoints(GeometryData::GI_GAUSS_2);
        const unsigned int num_gauss = IntegrationPoints.size();

        if (rValues.size() != num_gauss){
            rValues.resize(num_gauss);
        }

        Vector gauss_pts_jacobian_determinant = ZeroVector(num_gauss);
        GeometryData::ShapeFunctionsGradientsType DN_DX;
        rGeom.ShapeFunctionsIntegrationPointsGradients(DN_DX, gauss_pts_jacobian_determinant, GeometryData::GI_GAUSS_2);

        for (unsigned int i_gauss = 0; i_gauss < num_gauss; i_gauss++){

            const Matrix gp_DN_DX = DN_DX[i_gauss];
            double DVi_DXi = 0.0;

            for(unsigned int nnode = 0; nnode < NumNodes; nnode++){

                const array_1d<double,3> vel = rGeom[nnode].GetSolutionStepValue(VELOCITY);
                for(unsigned int ndim = 0; ndim < Dim; ndim++){
                    DVi_DXi += gp_DN_DX(nnode, ndim) * vel[ndim];
                }
            }
            rValues[i_gauss] = DVi_DXi;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Class template instantiation

template class SymbolicStokes<SymbolicStokesData<2,3>>;
template class SymbolicStokes<SymbolicStokesData<2,4>>;
template class SymbolicStokes<SymbolicStokesData<3,4>>;
template class SymbolicStokes<SymbolicStokesData<3,6>>;
template class SymbolicStokes<SymbolicStokesData<3,8>>;

} // namespace Kratos