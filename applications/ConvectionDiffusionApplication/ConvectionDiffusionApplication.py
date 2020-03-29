# makes KratosMultiphysics backward compatible with python 2.6 and 2.7
from __future__ import print_function, absolute_import, division

# Application dependent names and paths
import KratosMultiphysics as KM
from KratosConvectionDiffusionApplication import *
application = KratosConvectionDiffusionApplication()
application_name = "KratosConvectionDiffusionApplication"

KM._ImportApplication(application, application_name)
