import os

# Import Kratos
from KratosMultiphysics import *

try: # test to import mpi
    from KratosMultiphysics.mpi import *
except:
    pass

# Import KratosUnittest
import KratosMultiphysics.KratosUnittest as KratosUnittest
from KratosExecuteNearestNeighborMapperTest import KratosExecuteNearestNeighborMapperTest

# This utiltiy will control the execution scope in case we need to acces files or we depend
# on specific relative locations of the files.

# TODO: Should we move this to KratosUnittest?
class controlledExecutionScope:
    def __init__(self, scope):
        self.currentPath = os.getcwd()
        self.scope = scope

    def __enter__(self):
        os.chdir(self.scope)

    def __exit__(self, type, value, traceback):
        os.chdir(self.currentPath)


class NearestNeighborMapperTestFactory(KratosUnittest.TestCase):

    def setUp(self):
        try:
            self.num_processors = mpi.size
        except:
            self.num_processors = 1

        # Within this location context:
        with controlledExecutionScope(os.path.dirname(os.path.realpath(__file__))):
            GiD_output = True
            if (self.num_processors == 1):
                self.test = KratosExecuteNearestNeighborMapperTest(GiD_output)
            else:
                from KratosExecuteNearestNeighborMapperTestMPI import KratosExecuteNearestNeighborMapperTestMPI
                self.test = KratosExecuteNearestNeighborMapperTestMPI(GiD_output)

    def test_execution(self):
        # Within this location context:
        with controlledExecutionScope(os.path.dirname(os.path.realpath(__file__))):
            if (self.num_processors == 1):
                self.serialTests()
            else:
                # run tests that are independent of number of processors
                self.parallelTests()

                # run tests that are depending on number of processors
                list_processors_TestMapNonConstantScalarValues = [2,4,8]
                if (self.num_processors in list_processors_TestMapNonConstantScalarValues):
                    self.test.TestMapNonConstantScalarValues(5.0)

                list_processors_TestInverseMapNonConstantScalarValues = [3,5,9]
                if (self.num_processors in list_processors_TestInverseMapNonConstantScalarValues):
                    self.test.TestInverseMapNonConstantScalarValues(6.0)


    def tearDown(self):
        pass

    def serialTests(self):
        self.test.TestMapConstantScalarValues(1.0)
        self.test.TestInverseMapConstantScalarValues(2.0)

        self.test.TestMapConstantVectorValues(3.0)
        self.test.TestInverseMapConstantVectorValues(4.0)

        self.test.TestMapNonConstantScalarValues(5.0)
        self.test.TestInverseMapNonConstantScalarValues(6.0)

    def parallelTests(self):
        # These tests run independent of the number of processors,
        # since only conatant fields are mapped
        self.test.TestMapConstantScalarValues(1.0)
        self.test.TestInverseMapConstantScalarValues(2.0)

        self.test.TestMapConstantVectorValues(3.0)
        self.test.TestInverseMapConstantVectorValues(4.0)


class NearestNeighborTest_1(NearestNeighborMapperTestFactory):
    file_name = "Mapper_Test_1/Mapper_Test_1"
