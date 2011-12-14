/**
 * @file   PointRegion.cc
 * @author Rao Garimella
 * @date 
 * 
 * @brief  Implementation of PointRegion class
 * 
 * 
 */

#include "PointRegion.hh"
#include "dbc.hh"

namespace Amanzi {
namespace AmanziGeometry {

// -------------------------------------------------------------
//  class PointRegion
// -------------------------------------------------------------

// -------------------------------------------------------------
// PointRegion:: constructors / destructor
// -------------------------------------------------------------
PointRegion::PointRegion(const std::string name, 
			 const unsigned int id,
			 const Point& p)
  : Region(name,id,0), p_(p)
{
}

PointRegion::PointRegion(const char *name, const unsigned int id,
			 const Point& p)
  : Region(name,id,0), p_(p)
{
}

PointRegion::PointRegion(const PointRegion& old)
  : Region(old), p_(old.p_)
{
  // empty
}

PointRegion::~PointRegion(void)
{
  
}

// -------------------------------------------------------------
// PointRegion::inside -- check if input point is coincident with point
// -------------------------------------------------------------
bool
PointRegion::inside(const Point& p) const
{
  ASSERT(p.dim() == p_.dim());
  bool result(true);

  for (int i = 0; i < p.dim(); ++i) 
    {
      result = result & (fabs(p[i]-p_[i]) < 1e-12);
    }

  return result;
}

} // namespace AmanziGeometry
} // namespace Amanzi
