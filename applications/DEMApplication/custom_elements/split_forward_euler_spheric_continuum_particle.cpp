// System includes
#include <string>
#include <iostream>
#include <cmath>

#include <fstream>

// External includes

// Project includes
#include "split_forward_euler_spheric_continuum_particle.h"
#include "custom_utilities/GeometryFunctions.h"
#include "custom_utilities/AuxiliaryFunctions.h"
#include "custom_utilities/discrete_particle_configure.h"
#include "custom_strategies/schemes/glued_to_wall_scheme.h"


namespace Kratos
{
// using namespace GeometryFunctions;

SplitForwardEulerSphericContinuumParticle::SplitForwardEulerSphericContinuumParticle()
    : SphericContinuumParticle() {
}

SplitForwardEulerSphericContinuumParticle::SplitForwardEulerSphericContinuumParticle(IndexType NewId, GeometryType::Pointer pGeometry)
    : SphericContinuumParticle(NewId, pGeometry) {
}

SplitForwardEulerSphericContinuumParticle::SplitForwardEulerSphericContinuumParticle(IndexType NewId, GeometryType::Pointer pGeometry,  PropertiesType::Pointer pProperties)
    : SphericContinuumParticle(NewId, pGeometry, pProperties) {
}

SplitForwardEulerSphericContinuumParticle::SplitForwardEulerSphericContinuumParticle(IndexType NewId, NodesArrayType const& ThisNodes)
    : SphericContinuumParticle(NewId, ThisNodes) {
}

Element::Pointer SplitForwardEulerSphericContinuumParticle::Create(IndexType NewId, NodesArrayType const& ThisNodes, PropertiesType::Pointer pProperties) const
{
    return Element::Pointer(new SplitForwardEulerSphericContinuumParticle(NewId, GetGeometry().Create(ThisNodes), pProperties));
}

/// Destructor.
SplitForwardEulerSphericContinuumParticle::~SplitForwardEulerSphericContinuumParticle(){
}

// SplitForwardEulerSphericContinuumParticle& SplitForwardEulerSphericContinuumParticle::operator=(const SplitForwardEulerSphericContinuumParticle& rOther) {
//     SphericContinuumParticle::operator=(rOther);

//     return *this;
// }

void SplitForwardEulerSphericContinuumParticle::Initialize(const ProcessInfo& r_process_info)
{
    KRATOS_TRY

    SphericContinuumParticle::Initialize(r_process_info);

    NodeType& node = GetGeometry()[0];

    node.SetValue(NODAL_DAMPING,0.0);
    node.SetValue(NODAL_AUX_MASS,0.0);
    node.SetValue(NODAL_ROTATIONAL_DAMPING,0.0);
    node.SetValue(NODAL_AUX_INERTIA,0.0);

    KRATOS_CATCH( "" )
}

void SplitForwardEulerSphericContinuumParticle::CalculateRightHandSide(ProcessInfo& r_process_info, double dt, const array_1d<double,3>& gravity, int search_control)
{
    KRATOS_TRY

    // Creating a data buffer to store those variables that we want to reuse so that we can keep function parameter lists short

    SphericParticle::BufferPointerType p_buffer = CreateParticleDataBuffer(this); // all memory will be freed once this shared pointer goes out of scope
    SphericParticle::ParticleDataBuffer& data_buffer = *p_buffer;
    data_buffer.SetBoundingBox(r_process_info[DOMAIN_IS_PERIODIC], r_process_info[DOMAIN_MIN_CORNER], r_process_info[DOMAIN_MAX_CORNER]);

    NodeType& this_node = GetGeometry()[0];

    data_buffer.mDt = dt;
    data_buffer.mMultiStageRHS = false;

    array_1d<double, 3> additional_forces = ZeroVector(3);
    array_1d<double, 3> additionally_applied_moment = ZeroVector(3);
    array_1d<double, 3>& elastic_force       = this_node.FastGetSolutionStepValue(ELASTIC_FORCES);
    array_1d<double, 3>& contact_force       = this_node.FastGetSolutionStepValue(CONTACT_FORCES);
    array_1d<double, 3>& rigid_element_force = this_node.FastGetSolutionStepValue(RIGID_ELEMENT_FORCE);

    double nodal_stiffness;
    double& nodal_damping = this_node.GetValue(NODAL_DAMPING);
    const double& mass = this_node.FastGetSolutionStepValue(NODAL_MASS);
    double& nodal_aux_mass = this_node.GetValue(NODAL_AUX_MASS);
    double nodal_rotational_stiffness;
    double& nodal_rotational_damping = this_node.GetValue(NODAL_ROTATIONAL_DAMPING);
    const double& moment_of_inertia = this_node.FastGetSolutionStepValue(PARTICLE_MOMENT_OF_INERTIA);
    double& nodal_aux_inertia = this_node.GetValue(NODAL_AUX_INERTIA);

    mContactMoment.clear();
    elastic_force.clear();
    contact_force.clear();
    rigid_element_force.clear();
    nodal_stiffness = 0.0;
    nodal_damping = 0.0;
    nodal_rotational_stiffness = 0.0;
    nodal_rotational_damping = 0.0;

    InitializeForceComputation(r_process_info);

    double RollingResistance = 0.0;

    ComputeBallToBallContactForce(data_buffer, r_process_info, elastic_force, contact_force, RollingResistance);

    ComputeBallToRigidFaceContactForce(data_buffer, elastic_force, contact_force, RollingResistance, rigid_element_force, r_process_info, search_control);

    unsigned int num_sum = 0;

    ComputeBallToBallStiffnessAndDamping(data_buffer, num_sum, nodal_stiffness, nodal_damping, nodal_rotational_stiffness, nodal_rotational_damping);

    ComputeBallToRigidFaceStiffnessAndDamping(data_buffer, num_sum, nodal_stiffness, nodal_damping, nodal_rotational_stiffness, nodal_rotational_damping);

    if (num_sum > 0) {
        nodal_damping = nodal_damping / num_sum;
        nodal_rotational_damping = nodal_rotational_damping / num_sum;
    }
    // TODO: should I calculate the nodal_damping proportional to the mass in case that there are no neighbors ?

    if (nodal_damping > std::numeric_limits<double>::epsilon()) {
        nodal_aux_mass = r_process_info[INERTIAL_FACTOR] * mass/nodal_damping;
    }
    if (nodal_rotational_damping > std::numeric_limits<double>::epsilon()) {
        nodal_aux_inertia = r_process_info[INERTIAL_FACTOR] * moment_of_inertia/nodal_rotational_damping;
    }
    // KRATOS_WATCH(this->Id())
    // KRATOS_WATCH(nodal_stiffness)
    // KRATOS_WATCH(nodal_damping)
    // KRATOS_WATCH(mass)
    // KRATOS_WATCH(nodal_rotational_stiffness)
    // KRATOS_WATCH(nodal_rotational_damping)
    // KRATOS_WATCH(moment_of_inertia)
    // KRATOS_WATCH(r_process_info[INERTIAL_FACTOR])

    if (this->IsNot(DEMFlags::BELONGS_TO_A_CLUSTER)){
        ComputeAdditionalForces(additional_forces, additionally_applied_moment, r_process_info, gravity);
        #ifdef KRATOS_DEBUG
        DemDebugFunctions::CheckIfNan(additional_forces, "NAN in Additional Force in RHS of Ball");
        DemDebugFunctions::CheckIfNan(additionally_applied_moment, "NAN in Additional Torque in RHS of Ball");
        #endif
    }

    // ROLLING FRICTION
    if (this->Is(DEMFlags::HAS_ROTATION) && !data_buffer.mMultiStageRHS) {
        if (this->Is(DEMFlags::HAS_ROLLING_FRICTION) && !data_buffer.mMultiStageRHS) {
            array_1d<double, 3>& rolling_resistance_moment = this_node.FastGetSolutionStepValue(ROLLING_RESISTANCE_MOMENT);
            rolling_resistance_moment.clear();

            ComputeRollingFriction(rolling_resistance_moment, RollingResistance, data_buffer.mDt);
        }
    }

    array_1d<double,3>& total_forces = this_node.FastGetSolutionStepValue(TOTAL_FORCES);
    array_1d<double,3>& total_moment = this_node.FastGetSolutionStepValue(PARTICLE_MOMENT);

    total_forces[0] = contact_force[0] + additional_forces[0];
    total_forces[1] = contact_force[1] + additional_forces[1];
    total_forces[2] = contact_force[2] + additional_forces[2];

    total_moment[0] = mContactMoment[0] + additionally_applied_moment[0];
    total_moment[1] = mContactMoment[1] + additionally_applied_moment[1];
    total_moment[2] = mContactMoment[2] + additionally_applied_moment[2];

    // ApplyGlobalDampingToContactForcesAndMoments(total_forces, total_moment);

    #ifdef KRATOS_DEBUG
    DemDebugFunctions::CheckIfNan(total_forces, "NAN in Total Forces in RHS of Ball");
    DemDebugFunctions::CheckIfNan(total_moment, "NAN in Total Torque in RHS of Ball");
    #endif

    FinalizeForceComputation(data_buffer);
    KRATOS_CATCH("")
}

void SplitForwardEulerSphericContinuumParticle::ComputeBallToBallStiffnessAndDamping(SphericParticle::ParticleDataBuffer & data_buffer,
                                                                            unsigned int& r_num_sum,
                                                                            double& r_nodal_stiffness,
                                                                            double& r_nodal_damping,
                                                                            double& r_nodal_rotational_stiffness,
                                                                            double& r_nodal_rotational_damping)
{
    KRATOS_TRY

    Vector& cont_ini_neigh_area = this->GetValue(NEIGHBOURS_CONTACT_AREAS);

    for (int i = 0; data_buffer.SetNextNeighbourOrExit(i); ++i) {

        if (mNeighbourElements[i] == NULL) continue;
        if (this->Is(NEW_ENTITY) && mNeighbourElements[i]->Is(NEW_ENTITY)) continue;
        SphericContinuumParticle* neighbour_iterator = dynamic_cast<SphericContinuumParticle*>(mNeighbourElements[i]);
        data_buffer.mpOtherParticle = neighbour_iterator;

        noalias(data_buffer.mOtherToMeVector) = this->GetGeometry()[0].Coordinates() - data_buffer.mpOtherParticle->GetGeometry()[0].Coordinates();
        const double& other_radius = data_buffer.mpOtherParticle->GetRadius();
        data_buffer.mDistance = DEM_MODULUS_3(data_buffer.mOtherToMeVector);
        double radius_sum = GetRadius() + other_radius;
        double initial_delta = GetInitialDelta(i);
        double initial_dist = radius_sum - initial_delta;
        double indentation = initial_dist - data_buffer.mDistance;
        double myYoung = GetYoung();
        double myPoisson = GetPoisson();
        
        // Getting neighbor properties
        double other_young = data_buffer.mpOtherParticle->GetYoung();
        double other_poisson = data_buffer.mpOtherParticle->GetPoisson();
        double equiv_poisson;
        if ((myPoisson + other_poisson) != 0.0) { equiv_poisson = 2.0 * myPoisson * other_poisson / (myPoisson + other_poisson); }
        else { equiv_poisson = 0.0; }
        double equiv_young = 2.0 * myYoung * other_young / (myYoung + other_young);
        double calculation_area = 0.0;

        double normal_stiffness = 0.0;
        double tangential_stiffness = 0.0;
        double normal_damping_coeff = 0.0;
        double tangential_damping_coeff = 0.0;

        if (i < (int)mContinuumInitialNeighborsSize) {
            mContinuumConstitutiveLawArray[i]->GetContactArea(GetRadius(), other_radius, cont_ini_neigh_area, i, calculation_area);
            mContinuumConstitutiveLawArray[i]->CalculateElasticConstants(normal_stiffness, tangential_stiffness, initial_dist, equiv_young, equiv_poisson, calculation_area, this, neighbour_iterator);
            mContinuumConstitutiveLawArray[i]->CalculateViscoDampingCoeff(normal_damping_coeff, tangential_damping_coeff, this, neighbour_iterator, normal_stiffness, tangential_stiffness);
        }
        r_nodal_stiffness += CalculateStiffnessNorm(normal_stiffness,tangential_stiffness);
        r_nodal_damping += CalculateDampingNorm(normal_damping_coeff,tangential_damping_coeff);
        r_num_sum ++;

        if (this->Is(DEMFlags::HAS_ROTATION)) {
            if (i < (int)mContinuumInitialNeighborsSize && mIniNeighbourFailureId[i] == 0) {
                // TODO: is this right ?
                double arm_length = GetInteractionRadius() - indentation;
                double normal_rotational_stiffness = normal_stiffness * arm_length*1.0;
                double tangential_rotational_stiffness = tangential_stiffness * arm_length*1.0;
                double normal_rotational_damping_coeff = normal_damping_coeff * arm_length*1.0;
                double tangential_rotational_damping_coeff = tangential_damping_coeff * arm_length*1.0;

                r_nodal_rotational_stiffness += CalculateStiffnessNorm(normal_rotational_stiffness,tangential_rotational_stiffness);
                r_nodal_rotational_damping += CalculateDampingNorm(normal_rotational_damping_coeff,tangential_rotational_damping_coeff);
            }
        }
    }// for each neighbor

    KRATOS_CATCH("")
}

void SplitForwardEulerSphericContinuumParticle::ComputeBallToRigidFaceStiffnessAndDamping(SphericParticle::ParticleDataBuffer & data_buffer,
                                                                                unsigned int& r_num_sum,
                                                                                double& r_nodal_stiffness,
                                                                                double& r_nodal_damping,
                                                                                double& r_nodal_rotational_stiffness,
                                                                                double& r_nodal_rotational_damping)
{
    KRATOS_TRY

    std::vector<DEMWall*>& rNeighbours = this->mNeighbourRigidFaces;

    for (unsigned int i = 0; i < rNeighbours.size(); i++) {
        DEMWall* wall = rNeighbours[i];
        if(wall == NULL) continue;
        if(this->Is(DEMFlags::STICKY)) {
            DEMIntegrationScheme& dem_scheme = this->GetTranslationalIntegrationScheme();
            GluedToWallScheme* p_glued_scheme = dynamic_cast<GluedToWallScheme*>(&dem_scheme);
            Condition* p_condition = p_glued_scheme->pGetCondition();
            if(p_condition == wall) continue;
        }
        if(wall->IsPhantom()){
            wall->CheckSide(this);
            continue;
        }

        DEM_SET_COMPONENTS_TO_ZERO_3x3(data_buffer.mLocalCoordSystem)
        array_1d<double, 3> wall_delta_disp_at_contact_point = ZeroVector(3);
        array_1d<double, 3> wall_velocity_at_contact_point = ZeroVector(3);

        double ini_delta = GetInitialDeltaWithFEM(i);
        double DistPToB = 0.0;

        int ContactType = -1;
        array_1d<double, 4>& Weight = this->mContactConditionWeights[i];

        rNeighbours[i]->ComputeConditionRelativeData(i, this, data_buffer.mLocalCoordSystem, DistPToB, Weight, wall_delta_disp_at_contact_point, wall_velocity_at_contact_point, ContactType);

        if (ContactType == 1 || ContactType == 2 || ContactType == 3) {

            double indentation = -(DistPToB - GetInteractionRadius()) - ini_delta;
 
            if (indentation > 0.0) {

                double normal_stiffness, tangential_stiffness, normal_damping_coeff, tangential_damping_coeff;
                mDiscontinuumConstitutiveLaw->InitializeContactWithFEM(this,wall,indentation);
                normal_stiffness = mDiscontinuumConstitutiveLaw->mKn;
                tangential_stiffness = mDiscontinuumConstitutiveLaw->mKt;
                mDiscontinuumConstitutiveLaw->CalculateViscoDampingCoeffWithFEM(normal_damping_coeff, tangential_damping_coeff, this, wall,normal_stiffness,tangential_stiffness);

                r_nodal_stiffness += CalculateStiffnessNorm(normal_stiffness,tangential_stiffness);
                r_nodal_damping += CalculateDampingNorm(normal_damping_coeff,tangential_damping_coeff);
                r_num_sum ++;

                if (this->Is(DEMFlags::HAS_ROTATION)) {
                    // TODO: is this right ?
                    double arm_length = GetInteractionRadius() - indentation;
                    double normal_rotational_stiffness = normal_stiffness * arm_length*1.0;
                    double tangential_rotational_stiffness = tangential_stiffness * arm_length*1.0;
                    double normal_rotational_damping_coeff = normal_damping_coeff * arm_length*1.0;
                    double tangential_rotational_damping_coeff = tangential_damping_coeff * arm_length*1.0;

                    r_nodal_rotational_stiffness += CalculateStiffnessNorm(normal_rotational_stiffness,tangential_rotational_stiffness);
                    r_nodal_rotational_damping += CalculateDampingNorm(normal_rotational_damping_coeff,tangential_rotational_damping_coeff);
                }
            }
        } //ContactType if
    } //rNeighbours.size loop

    KRATOS_CATCH("")
}

double SplitForwardEulerSphericContinuumParticle::CalculateStiffnessNorm(const double& r_normal_stiffness, const double& r_tangential_stiffness) {
    return std::sqrt(r_normal_stiffness*r_normal_stiffness+2.0*r_tangential_stiffness*r_tangential_stiffness);
}

double SplitForwardEulerSphericContinuumParticle::CalculateDampingNorm(const double& r_normal_damping_coeff, const double& r_tangential_damping_coeff) {
    return std::sqrt(r_normal_damping_coeff*r_normal_damping_coeff+2.0*r_tangential_damping_coeff*r_tangential_damping_coeff);
}

void SplitForwardEulerSphericContinuumParticle::AddUpForcesAndProject(double OldCoordSystem[3][3],
                                            double LocalCoordSystem[3][3],
                                            double LocalContactForce[3],
                                            double LocalElasticContactForce[3],
                                            double LocalElasticExtraContactForce[3],
                                            double GlobalContactForce[3],
                                            double GlobalElasticContactForce[3],
                                            double GlobalElasticExtraContactForce[3],
                                            double TotalGlobalElasticContactForce[3],
                                            double ViscoDampingLocalContactForce[3],
                                            const double cohesive_force,
                                            array_1d<double, 3>& other_ball_to_ball_forces,
                                            array_1d<double, 3>& r_elastic_force,
                                            array_1d<double, 3>& r_contact_force,
                                            const unsigned int i_neighbour_count,
                                            ProcessInfo& r_process_info)
{

    for (unsigned int index = 0; index < 3; index++) {
        // NOTE: In this element we don't have damping forces
        // LocalContactForce[index] = LocalElasticContactForce[index] + ViscoDampingLocalContactForce[index] + other_ball_to_ball_forces[index];
        LocalContactForce[index] = LocalElasticContactForce[index] + other_ball_to_ball_forces[index];
    }
    LocalContactForce[2] -= cohesive_force;

    DEM_ADD_SECOND_TO_FIRST(LocalElasticContactForce, other_ball_to_ball_forces);

    GeometryFunctions::VectorLocal2Global(LocalCoordSystem, LocalElasticContactForce, GlobalElasticContactForce);
    GeometryFunctions::VectorLocal2Global(LocalCoordSystem, LocalContactForce, GlobalContactForce);
    GeometryFunctions::VectorLocal2Global(LocalCoordSystem, LocalElasticExtraContactForce, GlobalElasticExtraContactForce);

    // Saving contact forces (We need to, since tangential elastic force is history-dependent)
    DEM_COPY_SECOND_TO_FIRST_3(mNeighbourElasticContactForces[i_neighbour_count], GlobalElasticContactForce)
    DEM_COPY_SECOND_TO_FIRST_3(mNeighbourElasticExtraContactForces[i_neighbour_count], GlobalElasticExtraContactForce)

    TotalGlobalElasticContactForce[0] = GlobalElasticContactForce[0] + GlobalElasticExtraContactForce[0];
    TotalGlobalElasticContactForce[1] = GlobalElasticContactForce[1] + GlobalElasticExtraContactForce[1];
    TotalGlobalElasticContactForce[2] = GlobalElasticContactForce[2] + GlobalElasticExtraContactForce[2];
    DEM_ADD_SECOND_TO_FIRST(r_elastic_force, TotalGlobalElasticContactForce)

    double TotalGlobalContactForce[3];
    TotalGlobalContactForce[0] = GlobalContactForce[0] + GlobalElasticExtraContactForce[0];
    TotalGlobalContactForce[1] = GlobalContactForce[1] + GlobalElasticExtraContactForce[1];
    TotalGlobalContactForce[2] = GlobalContactForce[2] + GlobalElasticExtraContactForce[2];
    DEM_ADD_SECOND_TO_FIRST(r_contact_force, TotalGlobalContactForce )
}

void SplitForwardEulerSphericContinuumParticle::AddUpMomentsAndProject(double LocalCoordSystem[3][3],
                                             double LocalElasticRotationalMoment[3],
                                             double LocalViscoRotationalMoment[3]) {

    double LocalContactRotationalMoment[3] = {0.0};
    double GlobalContactRotationalMoment[3] = {0.0};

    for (unsigned int index = 0; index < 3; index++) {
        // NOTE: In this element we don't have damping forces
        // LocalContactRotationalMoment[index] = LocalElasticRotationalMoment[index] + LocalViscoRotationalMoment[index];
        LocalContactRotationalMoment[index] = LocalElasticRotationalMoment[index];
    }

    GeometryFunctions::VectorLocal2Global(LocalCoordSystem, LocalContactRotationalMoment, GlobalContactRotationalMoment);

    DEM_ADD_SECOND_TO_FIRST(mContactMoment, GlobalContactRotationalMoment)
}

void SplitForwardEulerSphericContinuumParticle::AddUpFEMForcesAndProject(double LocalCoordSystem[3][3],
                                               double LocalContactForce[3],
                                               double LocalElasticContactForce[3],
                                               double GlobalContactForce[3],
                                               double GlobalElasticContactForce[3],
                                               double ViscoDampingLocalContactForce[3],
                                               const double cohesive_force,
                                               array_1d<double, 3>& r_elastic_force,
                                               array_1d<double, 3>& r_contact_force,
                                               array_1d<double, 3>& elastic_force_backup,
                                               array_1d<double, 3>& total_force_backup) {
    for (unsigned int index = 0; index < 3; index++) {
        // NOTE: In this element we don't have damping forces
        // LocalContactForce[index] = LocalElasticContactForce[index] + ViscoDampingLocalContactForce[index];
        LocalContactForce[index] = LocalElasticContactForce[index];
    }
    LocalContactForce[2] -= cohesive_force;

    GeometryFunctions::VectorLocal2Global(LocalCoordSystem, LocalElasticContactForce, GlobalElasticContactForce);
    GeometryFunctions::VectorLocal2Global(LocalCoordSystem, LocalContactForce, GlobalContactForce);
    // Saving contact forces (We need to, since tangential elastic force is history-dependent)
    DEM_COPY_SECOND_TO_FIRST_3(elastic_force_backup,GlobalElasticContactForce)
    DEM_COPY_SECOND_TO_FIRST_3(total_force_backup,GlobalContactForce)
    DEM_ADD_SECOND_TO_FIRST(r_elastic_force, GlobalElasticContactForce)
    DEM_ADD_SECOND_TO_FIRST(r_contact_force, GlobalContactForce)

}

}  // namespace Kratos.