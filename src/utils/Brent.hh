/*
  Copyright 2010-202x held jointly by participating institutions.
  Amanzi is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors: Konstantin Lipnikov
*/

#ifndef UTILS_BRENT_HH_
#define UTILS_BRENT_HH_

#include <algorithm>
#include <iostream>

namespace Amanzi {
namespace Utils {

/* ******************************************************************
* Brent's method
****************************************************************** */
template <class F>
double
brent(F f, double a, double b, double tol, int* itr)
{
  int itr_max(*itr);
  double xtol(tol);
  double c, d, s, fa, fb, fc, fs;
  bool flag(true);

  fa = f(a);
  fb = f(b);
  if (fa * fb >= 0.0) {
    *itr = -1;
    return 0.0;
  }

  if (std::fabs(fa) < std::fabs(fb)) {
    std::swap(a, b);
    std::swap(fa, fb);
  }

  c = a;
  fc = fa;

  *itr = 0;
  while (*itr < itr_max) {
    (*itr)++;
    if (fa != fc && fb != fc) {
      s = a * fb * fc / ((fa - fb) * (fa - fc)) + b * fa * fc / ((fb - fa) * (fb - fc)) +
          c * fa * fb / ((fc - fa) * (fc - fb));
    } else {
      s = (fa * b - fb * a) / (fa - fb);
    }

    if ((s - (3 * a + b) / 4) * (s - b) >= 0.0 ||
        (flag && std::fabs(s - b) >= std::fabs(b - c) / 2) ||
        (!flag && std::fabs(s - b) >= std::fabs(c - d) / 2) ||
        (flag && std::fabs(b - c) < std::fabs(xtol)) ||
        (!flag && std::fabs(c - d) < std::fabs(xtol))) {
      s = (a + b) / 2;
      flag = true;
    } else {
      flag = false;
    }

    fs = f(s);
    d = c;
    c = b;
    fc = fb;

    if (fa * fs < 0.0) {
      b = s;
      fb = fs;
    } else {
      a = s;
      fa = fs;
    }

    if (std::fabs(fa) < std::fabs(fb)) {
      std::swap(a, b);
      std::swap(fa, fb);
    }
    if (fb == 0.0) return b;
    if (fs == 0.0) return s;
    if (std::fabs(b - a) < xtol) return s;
  }

  return 0.0; // default value
}

} // namespace Utils
} // namespace Amanzi

#endif
