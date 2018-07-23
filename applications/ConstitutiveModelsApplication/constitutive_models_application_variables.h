//--------------------------------------------------------------------
//    |  /           |                                               .
//    ' /   __| _` | __|  _ \   __|                                  .
//    . \  |   (   | |   (   |\__ \                                  .
//   _|\_\_|  \__,_|\__|\___/ ____/                                  .
//                        __  __      _           _      _           .
//   KRATOS CONSTITUTIVE |  \/  |__ _| |_ ___ _ _(_)__ _| |          .
//                       | |\/| / _` |  _/ -_) '_| / _` | |          .
//                       |_|  |_\__,_|\__\___|_| |_\__,_|_| MODELS   .
//			                                             .
//   License:(BSD)	  ConstitutiveModelsApplication/license.txt  .
//   Main authors:        Josep Maria Carbonell                      .
//                        ..                                         .
//--------------------------------------------------------------------
//
//   Project Name:        KratosConstitutiveModelsApplication $
//   Created by:          $Author:                JMCarbonell $
//   Last modified by:    $Co-Author:                         $
//   Date:                $Date:                   April 2017 $
//   Revision:            $Revision:                      0.0 $
//
//

#if !defined(KRATOS_CONSTITUTIVE_MODELS_APPLICATION_VARIABLES_H_INCLUDED )
#define  KRATOS_CONSTITUTIVE_MODELS_APPLICATION_VARIABLES_H_INCLUDED

// System includes

// External includes

// Project includes
#include "includes/define.h"
#include "includes/variables.h"
#include "includes/mat_variables.h"
#include "includes/kratos_application.h"

namespace Kratos
{
  //specific constitutive models variables must be DEFINED here
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, ALPHA )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, BETA )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, MF )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, CC )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, MM )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, RHOS )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, RHOT )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, KSIS )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, RHOM )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, KSIM )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, PC0 )

   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, VOID_RATIO )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, PS )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, PT )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, PM )
   KRATOS_DEFINE_APPLICATION_VARIABLE( UMAT_APPLICATION, double, PLASTIC_MULTIPLIER )
}

#endif	/* KRATOS_CONSTITUTIVE_MODELS_APPLICATION_VARIABLES_H_INCLUDED */
