
from . import co_simulation_tools as tools
# Other imports
cs_data_structure = tools.cs_data_structure
import numpy as np

## Class CouplingInterfaceData: Class to hold different properties of the data field contributed in
#                           CoSimulation.
#
class CouplingInterfaceData(object):
    def __init__(self, custom_config, solver):

        default_config = cs_data_structure.Parameters("""{
            "name"          : "default",
            "dimension"     : -1,
            "geometry_name" : "",
            "location"      : "node_historical"
        }""")
        custom_config.ValidateAndAssignDefaults(default_config)

        self.name = custom_config["name"].GetString()
        # self.operations = []
        self.solver = solver
        self.dimension = custom_config["dimension"].GetInt()
        self.location = custom_config["location"].GetString()
        self.geometry_name = custom_config["geometry_name"].GetString()
        self.origin_data      = None
        self.destination_data = None
        self.mapper_settings  = None
        # TODO check DOMAIN_SIZE against the dimension

    # def ExecuteOperations(self):
    #     for op in self.operations:
    #         op.Execute()

    def GetPythonList(self, solution_step_index=0):
        data_mesh = self.solver.model[self.geometry_name]
        data = [0]*len(data_mesh.Nodes)*self.dimension
        data_variable = cs_data_structure.KratosGlobals.GetVariable(self.name)
        node_index = 0
        for node in data_mesh.Nodes:
            data_value = node.GetSolutionStepValue(data_variable,solution_step_index) #TODO what if non-historical?
            for i in range(self.dimension):
                data[node_index*self.dimension + i] = data_value[i]
            node_index+=1
        return data

    def GetNumpyArray(self, solution_step_index=0):
        return np.asarray(self.GetPythonList(solution_step_index), dtype=np.float64)

    def ApplyUpdateToData(self, update):
        data_mesh = self.solver.model[self.geometry_name]
        data_variable = cs_data_structure.KratosGlobals.GetVariable(self.name)
        node_index = 0
        updated_value = [0]*3 # TODO this is a hack, find better solution! => check var_type
        for node in data_mesh.Nodes: # #TODO: local nodes to also work in MPI?
            # TODO: aditya the data might also be non-historical => GetValue
            for i in range(self.dimension):
                updated_value[i] = update[node_index*self.dimension + i]

            node.SetSolutionStepValue(data_variable, 0, updated_value)
            node_index += 1
