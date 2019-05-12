//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ `
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics
//
//  License:		 BSD License
//					 Kratos default license: kratos/license.txt
//
//  Main authors:    Riccardo Rossi
//


#if !defined(KRATOS_GLOBAL_POINTER_UTILITIES_H_INCLUDED )
#define  KRATOS_GLOBAL_POINTER_UTILITIES_H_INCLUDED


// System includes
#include <string>
#include <iostream>


// External includes


// Project includes
#include "includes/define.h"
#include "includes/data_communicator.h"
#include "includes/global_pointer.h"
#include "includes/mpi_serializer.h"
#include "containers/global_pointers_vector.h"
#include "containers/global_pointers_unordered_map.h"

#include "utilities/mpi_coloring_utilities.h"

namespace Kratos
{
///@addtogroup ApplicationNameApplication
///@{

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

/// Short class definition.
/** Detail class definition.
*/
template< class TContainerType >
class GlobalPointerUtilities
{
public:
    ///@name Type Definitions
    ///@{
    typedef GlobalPointer<typename TContainerType::value_type> GPType;

    /// Pointer definition of GlobalPointerUtilities
    KRATOS_CLASS_POINTER_DEFINITION(GlobalPointerUtilities);

    ///@}
    ///@name Life Cycle
    ///@{

    /// Default constructor.
    GlobalPointerUtilities()
    {}

    /// Destructor.
    virtual ~GlobalPointerUtilities() {}


    std::unordered_map< int, GPType > RetrieveGlobalIndexedPointersMap(
        const TContainerType& container,
        const std::vector<int>& id_list,
        const DataCommunicator& rDataCommunicator)
    {
        std::unordered_map< int, GPType > global_pointers_list, empty_gp_list;
        const int current_rank = rDataCommunicator.Rank();
        const int world_size = rDataCommunicator.Size();
        std::vector<int> empty = {-1};

        std::vector<int> remote_ids;
        for(const int id : id_list )
        {
            const auto it = container.find(id);
            if( it != container.end()) //found locally
            {
                //if(container[id].FastGetSolutionStepValue(PARTITION_INDEX) == current_rank)
                if(IteratorIsLocal(it, current_rank))
                    global_pointers_list.emplace(id,GPType(&*it, current_rank));
                else //remote, but this is a lucky case since for those we know to which rank they  they belong
                    remote_ids.push_back(id); //TODO: optimize according to the comment just above
            }
            else //id not found and we have no clue of what node owns it
            {
                remote_ids.push_back(id);
            }
        }
        //gather everything onto master_rank processor
        int master_rank = 0;

        std::vector<int> all_remote_ids;
        std::vector< std::vector<int> > collected_remote_ids(world_size);
        std::unordered_map< int, GPType > all_non_local_gp_map;

        //STEP1 - here we send the id we need to the master_rank
        //NOTE: here we DO NOT use a collective since we need to keep distinguished the ids per rank
        for(int i=0; i<world_size; ++i)
        {
            if(i != master_rank)
            {
                if(current_rank == master_rank) //only master executes
                {
                    collected_remote_ids[i] = rDataCommunicator.SendRecv(empty,i,i);
                }
                else if(current_rank == i) //only processor i executes
                {
                    rDataCommunicator.SendRecv(remote_ids,master_rank,master_rank);
                }
            }
            else //no communication needed
            {
                if(current_rank == master_rank) //only master executes
                    collected_remote_ids[i] = remote_ids;
            }

            if(current_rank == master_rank)
            {
                for(const int id : collected_remote_ids[i])
                    all_remote_ids.push_back( id );
            }
        }

        // very useful for debugging. do not remove for now
        // if(current_rank == master_rank)
        // {
        //     std::cout << "collected ids " << std::endl;
        //     for(unsigned int rank=0; rank<collected_remote_ids.size(); ++rank)
        //     {
        //         std::cout << " r = " << rank << " - ";
        //         for(int id : collected_remote_ids[rank])
        //             std::cout << id << " " ;
        //         std::cout << std::endl;
        //     }

        //     std::cout << "all remote ids " << std::endl;
        //     for(int id : all_remote_ids)
        //         std::cout << id << " ";
        //     std::cout << std::endl;
        // }

        if(current_rank == master_rank)
        {
            std::sort(all_remote_ids.begin(), all_remote_ids.end());
            std::unique(all_remote_ids.begin(), all_remote_ids.end());
        }

        //communicate the size of all remote_ids and resize the vector accordingly
        int number_of_all_remote_ids = all_remote_ids.size();
        rDataCommunicator.Broadcast(number_of_all_remote_ids,master_rank);

        if(current_rank != master_rank)
            all_remote_ids.resize(number_of_all_remote_ids);

        //STEP2 - here we give to every processor the ids that are needed by someone
        rDataCommunicator.Broadcast(all_remote_ids,master_rank);

        //STEP3 - here we obtain the list of gps we own and we send it back to the master_rank
        //gather results on master_rank
        for(int i=0; i<world_size; ++i)
        {

            if(i != master_rank)
            {
                if(current_rank == master_rank)
                {
                    //TODO: here we could use separately send and recv
                    auto recv_gps = SendRecv(empty_gp_list,i,i,rDataCommunicator);

                    for(auto& it : recv_gps)
                        all_non_local_gp_map.emplace(it.first, it.second);
                }
                else if(current_rank == i)
                {
                    auto non_local_gp_map = ComputeGpMap(container, all_remote_ids, current_rank);
                    SendRecv(non_local_gp_map,master_rank,master_rank,rDataCommunicator);
                }
            }
            else
            {
                auto recv_gps = ComputeGpMap(container, all_remote_ids, current_rank);

                for(auto& it : recv_gps)
                    all_non_local_gp_map.emplace(it.first, it.second);
            }
        }

        //STEP4 - here we obtain from the master_rank the list of gps we need
        //extract data and send to everyone
        for(int i=0; i<world_size; ++i)
        {
            if(i != master_rank)
            {
                if(current_rank == master_rank) //only master executes
                {
                    auto gp_list = ExtractById(all_non_local_gp_map,collected_remote_ids[i]);

                    //TODO: here we could use separately send and recv
                    SendRecv(gp_list,i,i,rDataCommunicator);
                }
                else if(current_rank == i) //only processor i executes
                {
                    auto gp_list = SendRecv(empty_gp_list,master_rank, master_rank,rDataCommunicator);

                    for(auto& it : gp_list)
                        global_pointers_list.emplace(it.first, it.second);
                }

            }
            else
            {
                auto gp_list = ExtractById(all_non_local_gp_map,collected_remote_ids[i]);

                for(auto& it : gp_list)
                    global_pointers_list.emplace(it.first, it.second);
            }
        }

        return global_pointers_list;

    }

    GlobalPointersVector< typename TContainerType::value_type > RetrieveGlobalIndexedPointers(
        const TContainerType& container,
        const std::vector<int>& id_list,
        const DataCommunicator& rDataCommunicator
    )
    {

        auto global_pointers_list = RetrieveGlobalIndexedPointersMap(container, id_list, rDataCommunicator);

        int current_rank = rDataCommunicator.Rank();

        //compute final array
        GlobalPointersVector< typename TContainerType::value_type > result;
        result.reserve(id_list.size());
        for(unsigned int i=0; i<id_list.size(); ++i)
        {
            auto it = global_pointers_list.find(id_list[i]);
            if(it != global_pointers_list.end())
                result.push_back( it->second );
            else
                KRATOS_ERROR << "id " << id_list[i] << " not found for processor " << current_rank << std::endl;
        }

        return result;

    }

    ///@}
    ///@name Operators
    ///@{


    ///@}
    ///@name Operations
    ///@{


    ///@}
    ///@name Access
    ///@{


    ///@}
    ///@name Inquiry
    ///@{


    ///@}
    ///@name Input and output
    ///@{

    /// Turn back information as a string.
    virtual std::string Info() const
    {
        std::stringstream buffer;
        buffer << "GlobalPointerUtilities" ;
        return buffer.str();
    }

    /// Print information about this object.
    virtual void PrintInfo(std::ostream& rOStream) const
    {
        rOStream << "GlobalPointerUtilities";
    }

    /// Print object's data.
    virtual void PrintData(std::ostream& rOStream) const {}

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

    template< class TIteratorType >
    bool IteratorIsLocal(TIteratorType& it, const int CurrentRank)
    {
        return true; //if the iterator was found, then it is local!
    }

    //particularizing to the case of nodes
    bool IteratorIsLocal(ModelPart::NodesContainerType::iterator& it, const int CurrentRank)
    {
        if(it->FastGetSolutionStepValue(PARTITION_INDEX) == CurrentRank)
            return true;
        else
            return false;
    }

    ///@}
    ///@name Private Operators
    ///@{
    template< class TType>
    TType SendRecv(TType& send_buffer, int send_rank, int recv_rank,
                   const DataCommunicator& rDataCommunicator)
    {
        MpiSerializer send_serializer;
        send_serializer.save("data",send_buffer);
        std::string send_string = send_serializer.GetStringRepresentation();

        std::string recv_string = rDataCommunicator.SendRecv(send_string, send_rank, recv_rank);

        MpiSerializer recv_serializer(recv_string);

        TType recv_data;
        recv_serializer.load("data",recv_data);
        return recv_data;
    }

    std::unordered_map< int, GPType > ExtractById(
        std::unordered_map< int, GPType >& gp_list,
        const std::vector<int>& ids)
    {
        std::unordered_map< int, GPType > extracted_list;
        for(auto id : ids)
        {
            auto gp = gp_list[id];
            extracted_list[id] = gp;
        }
        return extracted_list;
    }

    std::unordered_map< int, GPType > ComputeGpMap(
        const TContainerType& container,
        const std::vector<int>& ids,
        int current_rank)
    {
        std::unordered_map< int, GPType > extracted_list;
        for(auto id : ids)
        {
            const auto it = container.find(id);
            if( it != container.end()) //found locally
            {
                //if(it->FastGetSolutionStepValue(PARTITION_INDEX) == current_rank)
                if(IteratorIsLocal(it, current_rank))
                    extracted_list.emplace(id, GPType(&*it, current_rank));
            }
        }
        return extracted_list;
    }

    ///@}
    ///@name Private Operations
    ///@{


    ///@}
    ///@name Private  Access
    ///@{


    ///@}
    ///@name Private Inquiry
    ///@{


    ///@}
    ///@name Un accessible methods
    ///@{

    /// Assignment operator.
    GlobalPointerUtilities& operator=(GlobalPointerUtilities const& rOther) = delete;

    /// Copy constructor.
    GlobalPointerUtilities(GlobalPointerUtilities const& rOther) = delete;

    ///@}

}; // Class GlobalPointerUtilities

///@}

///@name Type Definitions
///@{


///@}
///@name Input and output
///@{


/// input stream function
template< class TPointerDataType >
inline std::istream& operator >> (std::istream& rIStream,
                                  GlobalPointerUtilities<TPointerDataType>& rThis)
{
    return rIStream;
}

/// output stream function
template< class TPointerDataType >
inline std::ostream& operator << (std::ostream& rOStream,
                                  const GlobalPointerUtilities<TPointerDataType>& rThis)
{
    rThis.PrintInfo(rOStream);
    rOStream << std::endl;
    rThis.PrintData(rOStream);

    return rOStream;
}
///@}

///@} addtogroup block

}  // namespace Kratos.

#endif // KRATOS_GLOBAL_POINTER_UTILITIES_H_INCLUDED  defined


