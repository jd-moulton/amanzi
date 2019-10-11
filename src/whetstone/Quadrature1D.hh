/*
  WhetStone, Version 2.2
  Release name: naka-to.

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL.
  Amanzi is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  Gauss quadrature on interval (0,1).
*/

#ifndef AMANZI_WHETSTONE_QUADRATURE_1D_HH_
#define AMANZI_WHETSTONE_QUADRATURE_1D_HH_

namespace Amanzi {
namespace WhetStone {

static const double q1d_weights[8][8] = { 1.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.5,
                                          0.5,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.277777777777778,
                                          0.444444444444444,
                                          0.277777777777778,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.173927422568727,
                                          0.326072577431273,
                                          0.326072577431273,
                                          0.173927422568727,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.118463442528095,
                                          0.239314335249683,
                                          0.284444444444444,
                                          0.239314335249683,
                                          0.118463442528095,
                                          0.0,
                                          0.0,
                                          0.0,
                                          0.0856622461895852,
                                          0.180380786524069,
                                          0.233956967286345,
                                          0.233956967286345,
                                          0.180380786524069,
                                          0.0856622461895852,
                                          0.0,
                                          0.0,
                                          0.0647424830844349,
                                          0.139852695744638,
                                          0.190915025252559,
                                          0.208979591836735,
                                          0.190915025252559,
                                          0.139852695744638,
                                          0.0647424830844349,
                                          0.0,
                                          0.0506142681451882,
                                          0.1111905172266873,
                                          0.1568533229389437,
                                          0.181341891689181,
                                          0.181341891689181,
                                          0.1568533229389437,
                                          0.1111905172266873,
                                          0.0506142681451882 };

static const double q1d_points[8][8] = { 0.5,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.211324865405187,
                                         0.788675134594813,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.112701665379258,
                                         0.5,
                                         0.887298334620742,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0694318442029737,
                                         0.330009478207572,
                                         0.669990521792428,
                                         0.930568155797026,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0469100770306680,
                                         0.230765344947158,
                                         0.5,
                                         0.769234655052841,
                                         0.953089922969332,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0337652428984240,
                                         0.169395306766868,
                                         0.380690406958402,
                                         0.619309593041598,
                                         0.830604693233132,
                                         0.966234757101576,
                                         0.0,
                                         0.0,
                                         0.0254460438286208,
                                         0.1292344072003028,
                                         0.297077424311301,
                                         0.5,
                                         0.702922575688699,
                                         0.870765592799697,
                                         0.974553956171379,
                                         0.0,
                                         0.0198550717512319,
                                         0.1016667612931866,
                                         0.2372337950418355,
                                         0.7627662049581645,
                                         0.4082826787521751,
                                         0.5917173212478249,
                                         0.8983332387068134,
                                         0.9801449282487682 };

} // namespace WhetStone
} // namespace Amanzi

#endif
