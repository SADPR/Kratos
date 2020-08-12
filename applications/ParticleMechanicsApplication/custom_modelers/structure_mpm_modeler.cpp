//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ `
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics
//
//  License:         BSD License
//                   Kratos default license: kratos/license.txt
//


// Project includes
#include "structure_mpm_modeler.h"


namespace Kratos
{
    ///@name Stages
    ///@{

    void StructureMpmModeler::SetupGeometryModel()
    {
        CheckParameters();

        Model* p_model_mpm = (mIsOriginMpm) ? mpModelOrigin : mpModelDest;
        Model* p_model_fem = (mIsOriginMpm) ? mpModelDest : mpModelOrigin;

        ModelPart& coupling_model_part = (mpModelOrigin->HasModelPart("coupling"))
            ? mpModelOrigin->GetModelPart("coupling")
            : mpModelOrigin->CreateModelPart("coupling");

        std::string origin_interface_sub_model_part_name;
        std::string destination_interface_sub_model_part_name;
        if (mParameters["is_interface_sub_model_parts_specified"].GetBool())
        {
            origin_interface_sub_model_part_name = mParameters["origin_interface_sub_model_part_name"].GetString();
            destination_interface_sub_model_part_name = mParameters["destination_interface_sub_model_part_name"].GetString();
        }
        else
        {
            KRATOS_ERROR << "Automatic interface creation is not implemented yet" << std::endl;
            // Some future functionality to automatically determine interfaces?
            // (Create interface sub model parts in origin and destination modelparts)
            // (set strings to correct values)
            //origin_interface_sub_model_part_name = ???
            //destination_interface_sub_model_part_name = ???
        }

        ModelPart& background_grid_model_part = (p_model_mpm->HasModelPart("Background_Grid"))
            ? p_model_mpm->GetModelPart("Background_Grid")
            : p_model_mpm->CreateModelPart("Background_Grid");

        KRATOS_ERROR_IF(background_grid_model_part.NumberOfElements() == 0) << "Background_Grid model part has zero elements!\n";

        // create coupling conditions on interface depending on the dimension
        ModelPart& r_fem_interface = (mIsOriginMpm)
            ? p_model_fem->GetModelPart(destination_interface_sub_model_part_name)
            : p_model_fem->GetModelPart(origin_interface_sub_model_part_name);
        std::vector<GeometryPointerType> interface_geoms;
        const IndexType dim = 2;
        if (dim == 2)
        {
            CreateInterfaceLineCouplingConditions(r_fem_interface, interface_geoms);
            //CreateInterfaceLineCouplingConditions(mpModelMpm->GetModelPart(destination_interface_sub_model_part_name));
        }
        else
        {
            KRATOS_ERROR << "Not implemented yet" << std::endl;
        }

        std::vector<GeometryPointerType> quads_structure;
        CreateStructureQuadraturePointGeometries<
            std::vector<GeometryPointerType>, std::vector<GeometryPointerType>>(
                interface_geoms, quads_structure);
        std::vector<GeometryPointerType> quads_mpm(quads_structure.size());
        CreateMpmQuadraturePointGeometries<2, std::vector<GeometryPointerType>>(
            quads_structure, quads_mpm, background_grid_model_part);

        // Transfer everything into the coupling modelpart
        ModelPart& coupling_interface_origin = (coupling_model_part.HasSubModelPart("interface_origin"))
            ? coupling_model_part.GetSubModelPart("interface_origin")
            : coupling_model_part.CreateSubModelPart("interface_origin");

        ModelPart& coupling_interface_destination = (coupling_model_part.HasSubModelPart("interface_destination"))
            ? coupling_model_part.GetSubModelPart("interface_destination")
            : coupling_model_part.CreateSubModelPart("interface_destination");


        std::vector<GeometryPointerType>& p_quads_origin = (mIsOriginMpm) ? quads_mpm : quads_structure;
        std::vector<GeometryPointerType>& p_quads_dest = (mIsOriginMpm) ? quads_structure : quads_mpm;

        for (IndexType i = 0; i < p_quads_origin.size(); ++i) {
            for (IndexType j = 0; j < p_quads_origin[i]->size(); ++j) {
                coupling_interface_origin.AddNode(p_quads_origin[i]->pGetPoint(j));
            }
        }
        for (IndexType i = 0; i < p_quads_dest.size(); ++i) {
            for (IndexType j = 0; j < p_quads_dest[i]->size(); ++j) {
                coupling_interface_destination.AddNode(p_quads_dest[i]->pGetPoint(j));
            }
        }

        // Determine next condition number
        IndexType condition_id = (coupling_model_part.GetRootModelPart().NumberOfConditions() == 0)
            ? 1 : (coupling_model_part.GetRootModelPart().ConditionsEnd() - 1)->Id() + 1;

        for (IndexType i = 0; i < p_quads_origin.size(); ++i) {
            coupling_model_part.AddCondition(Kratos::make_intrusive<Condition>(
                condition_id + i, Kratos::make_shared<CouplingGeometry<Node<3>>>(p_quads_origin[i], p_quads_dest[i])));
        }
    }

    void StructureMpmModeler::UpdateGeometryModel()
    {
        Model* p_model_mpm = (mIsOriginMpm) ? mpModelOrigin : mpModelDest;
        Model* p_model_fem = (mIsOriginMpm) ? mpModelDest : mpModelOrigin;

        ModelPart& coupling_model_part = (mpModelOrigin->HasModelPart("coupling"))
            ? mpModelOrigin->GetModelPart("coupling")
            : mpModelOrigin->CreateModelPart("coupling");

        std::string origin_interface_sub_model_part_name;
        std::string destination_interface_sub_model_part_name;
        if (mParameters["is_interface_sub_model_parts_specified"].GetBool())
        {
            origin_interface_sub_model_part_name = mParameters["origin_interface_sub_model_part_name"].GetString();
            destination_interface_sub_model_part_name = mParameters["destination_interface_sub_model_part_name"].GetString();
        }
        else
        {
            KRATOS_ERROR << "Automatic interface creation is not implemented yet" << std::endl;
            // Some future functionality to automatically determine interfaces?
            // (Create interface sub model parts in origin and destination modelparts)
            // (set strings to correct values)
            //origin_interface_sub_model_part_name = ???
            //destination_interface_sub_model_part_name = ???
        }

        ModelPart& mpm_background_grid_model_part = (p_model_mpm->HasModelPart("Background_Grid"))
            ? p_model_mpm->GetModelPart("BackgroundGrid")
            : p_model_mpm->CreateModelPart("BackgroundGrid");

        std::vector<GeometryPointerType> quads_structure;
        UpdateMpmQuadraturePointGeometries<3,
            typename ModelPart::ConditionsContainerType>(
                coupling_model_part.Conditions(), mpm_background_grid_model_part);
    }

    void StructureMpmModeler::CheckParameters()
    {
        KRATOS_ERROR_IF_NOT(mParameters.Has("origin_model_part_name"))
            << "Missing \"origin_model_part_name\" in StructureMpmModeler Parameters." << std::endl;

        KRATOS_ERROR_IF_NOT(mParameters.Has("destination_model_part_name"))
            << "Missing \"destination_model_part_name\" in StructureMpmModeler Parameters." << std::endl;

        KRATOS_ERROR_IF_NOT(mParameters.Has("is_interface_sub_model_parts_specified"))
            << "Missing \"is_interface_sub_model_parts_specified\" in StructureMpmModeler Parameters." << std::endl;

        if (mParameters["is_interface_sub_model_parts_specified"].GetBool())
        {
            KRATOS_ERROR_IF_NOT(mParameters.Has("origin_interface_sub_model_part_name"))
                << "Missing \"origin_interface_sub_model_part_name\" in StructureMpmModeler Parameters." << std::endl;

            KRATOS_ERROR_IF_NOT(mParameters.Has("destination_interface_sub_model_part_name"))
                << "Missing \"destination_interface_sub_model_part_name\" in StructureMpmModeler Parameters." << std::endl;
        }
    }
	void StructureMpmModeler::CreateInterfaceLineCouplingConditions(
        ModelPart& rInterfaceModelPart,
        std::vector<GeometryPointerType>& rGeometries)
	{
        rInterfaceModelPart.CreateSubModelPart("coupling_conditions");
        ModelPart& coupling_geometries_model_part = rInterfaceModelPart.GetSubModelPart("coupling_conditions");
        const ModelPart& root_mp = rInterfaceModelPart.GetRootModelPart();

        IndexType interface_node_id;
        IndexType trial_interface_node_id;
        IndexType trial_geom_node_id;

        for (size_t node_index = 0; node_index < rInterfaceModelPart.NumberOfNodes() - 1; ++node_index)
        {
            interface_node_id = (rInterfaceModelPart.NodesBegin() + node_index)->Id();
            std::vector< GeometryPointerType> p_geom_vec;
            for (auto& ele_it : root_mp.Elements())
            {
                auto p_geom = ele_it.pGetGeometry();
                for (size_t i = 0; i < p_geom->size(); i++)
                {
                    if ((*p_geom)[i].Id() == interface_node_id)
                    {
                        p_geom_vec.push_back(p_geom);
                    }
                }
            }

            if (p_geom_vec.size() == 0) KRATOS_ERROR << "Interface node not found in modelpart geom\n";

            // Loop over all geometries that have nodes on the interface
            for (size_t interface_geom_index = 0; interface_geom_index < p_geom_vec.size(); interface_geom_index++)
            {
                GeometryType& r_interface_geom = *(p_geom_vec[interface_geom_index]);

                // Loop over remaining interface nodes, see if any of them are nodes in the interface geom
                for (size_t geom_node_index = 0; geom_node_index < r_interface_geom.size(); geom_node_index++)
                {
                    trial_geom_node_id = r_interface_geom[geom_node_index].Id();

                    for (size_t trial_index = node_index + 1; trial_index < rInterfaceModelPart.NumberOfNodes(); ++trial_index)
                    {
                        trial_interface_node_id = (rInterfaceModelPart.NodesBegin() + trial_index)->Id();
                        if (trial_geom_node_id == trial_interface_node_id)
                        {
                            // Another interface node was found in the same geom, make line condition between them
                            Line2D2<NodeType>::Pointer p_line = Kratos::make_shared<Line2D2<NodeType>>(
                                rInterfaceModelPart.pGetNode(interface_node_id), rInterfaceModelPart.pGetNode(trial_interface_node_id));
                            coupling_geometries_model_part.AddGeometry(p_line);
                            rGeometries.push_back(p_line);
                        }
                    }
                }
            }
        }
	}
}