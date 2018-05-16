// KRATOS  ___|  |                   |                   |
//       \___ \  __|  __| |   |  __| __| |   |  __| _` | |
//             | |   |    |   | (    |   |   | |   (   | |
//       _____/ \__|_|   \__,_|\___|\__|\__,_|_|  \__,_|_| MECHANICS
//
//  License:		 BSD License
//					 license: structural_mechanics_application/license.txt
//
//  Main authors:    Vicente Mataix Ferrandiz
//

#if !defined(KRATOS_SKIN_DTEECTION_PROCESS_H_INCLUDED )
#define  KRATOS_SKIN_DTEECTION_PROCESS_H_INCLUDED

// System includes

// External includes
#include <unordered_map>

// Project includes
#include "processes/process.h"
#include "includes/key_hash.h"
#include "includes/model_part.h"
#include "includes/kratos_parameters.h"

namespace Kratos
{

///@name Kratos Globals
///@{

///@}
///@name Type Definitions
///@{

    // General geometry type definitions
    typedef Node<3>                                          NodeType;
    typedef Geometry<NodeType>                           GeometryType;

    /// The definition of the index type
    typedef std::size_t IndexType;

    /// The definition of the sizetype
    typedef std::size_t SizeType;

///@}
///@name  Enum's
///@{

///@}
///@name  Functions
///@{

///@}
///@name Kratos Classes
///@{

/**
 * @class SkinDetectionProcess
 * @ingroup StructuralMechanicsApplication
 * @brief An algorithm that looks for neighbour elements in a mesh and creates a submodelpart containing the skin of the disconnected elements (interface elements)
 * @details For that pourpose if builds an unordered map of the surrounding elements and nodes and performs different checks.
 * @warning Please check that you geometry is compatible, it will be assumed that a geometry has a neighbour geometries of the same class. If your geometries combines for example hexahedra, prism, pyramids and tetrahedra you will need to refactor this
 * @todo Solve the previous problem
 * @todo Move this to the core if people demands this
 * @tparam TDim The dimension where the problem is computed
 * @author Vicente Mataix Ferrandiz
*/
template<SizeType TDim>
class KRATOS_API(STRUCTURAL_MECHANICS_APPLICATION) SkinDetectionProcess
    : public Process
{
public:
    ///@name Type Definitions
    ///@{

    /// Pointer definition of SkinDetectionProcess
    KRATOS_CLASS_POINTER_DEFINITION(SkinDetectionProcess);

    // Containers definition
    typedef ModelPart::NodesContainerType              NodesArrayType;
    typedef ModelPart::ConditionsContainerType    ConditionsArrayType;
    typedef ModelPart::ElementsContainerType        ElementsArrayType;

    // Containers iterators definition
    typedef NodesArrayType::iterator                NodesIterarorType;
    typedef ConditionsArrayType::iterator      ConditionsIteratorType;
    typedef ElementsArrayType::iterator          ElementsIteratorType;

    // Weak pointers vectors types
    typedef WeakPointerVector<NodeType> NodePointerVector;
    typedef WeakPointerVector<Element> ElementPointerVector;

    /// Definition of the vector indexes considered
    typedef vector<IndexType> VectorIndexType;

    /// Definition of the hasher considered
    typedef VectorIndexHasher<VectorIndexType> VectorIndexHasherType;

    /// Definition of the key comparor considered
    typedef VectorIndexComparor<VectorIndexType> VectorIndexComparorType;

    /// Define the map considered for indexes
    typedef std::unordered_map<VectorIndexType, IndexType, VectorIndexHasherType, VectorIndexComparorType > HashMapVectorIntIntType;

    /// Define the HashMapVectorIntIntType iterator type
    typedef HashMapVectorIntIntType::iterator HashMapVectorIntIntIteratorType;

    /// Define the map considered for elemento pointers
    typedef std::unordered_map<VectorIndexType, Element::Pointer, VectorIndexHasherType, VectorIndexComparorType > HashMapVectorIntElementPointerType;

    /// Define the HashMapVectorIntElementPointerType iterator type
    typedef HashMapVectorIntElementPointerType::iterator HashMapVectorIntElementPointerIteratorType;

    ///@}
    ///@name Life Cycle
    ///@{

    /**
     * @brief Default constructor.
     * @param rModelPart The model part where the search of neighbours is performed
     * @param ThisParameters The parameters of configuration
     */
    SkinDetectionProcess(
        ModelPart& rModelPart,
        Parameters ThisParameters = Parameters(R"({})")
        );

    /// Destructor.
    virtual ~SkinDetectionProcess() {}

    ///@}
    ///@name Operators
    ///@{

    void operator()()
    {
        Execute();
    }

    ///@}
    ///@name Operations
    ///@{

    /**
     * @brief This method executes the algorithm that looks for neighbour nodes and elements in a  mesh of prismatic elements
     */
    void Execute() override;

    /**
     * @brief This function is designed for being called at the end of the computations right after reading the model and the groups
     */
    void ExecuteFinalize() override;

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
    virtual std::string Info() const override
    {
        return "SkinDetectionProcess";
    }

    /// Print information about this object.
    virtual void PrintInfo(std::ostream& rOStream) const override
    {
        rOStream << "SkinDetectionProcess";
    }

    /// Print object's data.
    virtual void PrintData(std::ostream& rOStream) const override
    {
    }

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

    ModelPart& mrModelPart;     /// The main model part
    Parameters mThisParameters; /// The parameters (can be used for general pourposes)

    ///@}
    ///@name Private Operators
    ///@{

    /**
     * @brief This method computes the required reserve size for neighbours
     * @param itElem The element iterator where to check the size and so on
     * @return The reserve size for the neighbour elements vector
     * @todo Check that EdgesNumber() and FacesNumber() are properly implemeted on the geometries of interest
     */
    SizeType ComputeReserveSize(ElementsIteratorType itElem);

    /**
     * @brief This method returns the subgeometry of the current geometry we are interested of
     * @param BaseGeometry The base geometry where we get the subgeometry
     * @param IndexSubGeometry The index of the corresponding subgeometry
     * @return The corresponding subgeometry
     */
    GeometryType& GetSubGeometry(
        GeometryType& BaseGeometry,
        const IndexType IndexSubGeometry
        );

    /**
     * @brief This method should be called in case that the current list of neighbour must be drop
     */
    void ClearNeighbours();

    /**
     * @brief This method add a unique weak pointer for any class
     * @param rPointerVector The vector containing the pointers of the defined class
     * @param Candidate The potential candidate  to add to the vector of pointers
     * @tparam TDataType The class type of the pointer
     * @todo Move this method to a common class (it is reused in the prism neighbour)
     */
    template< class TDataType >
    void  AddUniqueWeakPointer(
        WeakPointerVector< TDataType >& rPointerVector,
        const typename TDataType::WeakPointer Candidate
        )
    {
        typename WeakPointerVector< TDataType >::iterator beginit = rPointerVector.begin();
        typename WeakPointerVector< TDataType >::iterator endit   = rPointerVector.end();
        while ( beginit != endit && beginit->Id() != (Candidate.lock())->Id()) {
            beginit++;
        }
        if( beginit == endit ) {
            rPointerVector.push_back(Candidate);
        }

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
    SkinDetectionProcess& operator=(SkinDetectionProcess const& rOther);

    ///@}

}; // Class SkinDetectionProcess

///@}

///@name Type Definitions
///@{


///@}
///@name Input and output
///@{


/// input stream function
template<SizeType TDim>
inline std::istream& operator >> (std::istream& rIStream,
                                  SkinDetectionProcess<TDim>& rThis);

/// output stream function
template<SizeType TDim>
inline std::ostream& operator << (std::ostream& rOStream,
                                  const SkinDetectionProcess<TDim>& rThis)
{
    rThis.PrintInfo(rOStream);
    rOStream << std::endl;
    rThis.PrintData(rOStream);

    return rOStream;
}
///@}


}  // namespace Kratos.

#endif // KRATOS_SKIN_DTEECTION_PROCESS_H_INCLUDED  defined
