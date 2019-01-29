// Author: Guillermo Casas (gcasas@cimne.upc.edu)
// Date: February 2019

#if !defined(SDEM_DRAG_LAW_H_INCLUDED)
#define SDEM_DRAG_LAW_H_INCLUDED

#include <string>
#include <iostream>
#include "includes/define.h"
#include "includes/serializer.h"
#include "includes/model_part.h"
#include "containers/flags.h"

namespace Kratos {

    class KRATOS_API(SWIMMING_DEM_APPLICATION) DragLaw : public Flags {

    public:
        typedef Node <3> NodeType;
        KRATOS_CLASS_POINTER_DEFINITION(DragLaw);

        DragLaw(){}

        ~DragLaw(){}

        void Initialize(const ProcessInfo& r_process_info);

        std::string GetTypeOfLaw();

        void ComputeForce(NodeType& node,
                          double particle_radius,
                          double fluid_density,
                          double fluid_kinematic_viscosity,
                          array_1d<double, 3>& slip_velocity,
                          array_1d<double, 3>& drag_force,
                          const ProcessInfo& r_current_process_info);

    private:

        friend class Serializer;

        virtual void save(Serializer& rSerializer) const override {
            KRATOS_SERIALIZE_SAVE_BASE_CLASS(rSerializer, DragLaw)
        }

        virtual void load(Serializer& rSerializer) override {
            KRATOS_SERIALIZE_LOAD_BASE_CLASS(rSerializer, DragLaw)
        }

    }; //class DragLaw

} // Namespace Kratos

#endif /* SDEM_DRAG_LAW_H_INCLUDED  defined */
