// Miscellaneous mathematical operations as function objects.
//
// Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020 Gregory W. Chicares.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
//
// http://savannah.nongnu.org/projects/lmi
// email: <gchicares@sbcglobal.net>
// snail: Chicares, 186 Belle Woods Drive, Glastonbury CT 06033, USA

#ifndef math_functions_hpp
#define math_functions_hpp

#include "config.hpp"

#include "assert_lmi.hpp"
#include "et_vector.hpp"

#include <algorithm>                    // max(), min()
#include <cmath>                        // expm1(), log1p()
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

// TODO ?? Write functions here for other refactorable uses of
// std::pow() throughout lmi, to facilitate reuse and unit testing.

// Some of these provide the typedefs that std::unary_function or
// std::binary_function would have provided, because they're still
// required for std::binder1st() or std::binder2nd(), or for PETE.

template<typename T>
struct greater_of
{
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = T;
    T operator()(T const& x, T const& y) const
        {
        return std::max(x, y);
        }
};

template<typename T>
struct lesser_of
{
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = T;
    T operator()(T const& x, T const& y) const
        {
        return std::min(x, y);
        }
};

/// Arithmetic mean.
///
/// Calculate mean as
///   (half of x) plus (half of y)
/// instead of
///   half of (x plus y)
/// because the addition in the latter can overflow. Generally,
/// hardware deals better with underflow than with overflow.
///
/// The domain is restricted to floating point because integers would
/// give surprising results. For instance, the integer mean of one and
/// two would be truncated to one upon either returning an integer or
/// assigning the result to one. Returning a long double in all cases
/// is the best that could be done, but that seems unnatural.

template<typename T>
struct mean
{
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = T;
    static_assert(std::is_floating_point<T>::value);
    T operator()(T const& x, T const& y) const
        {return 0.5 * x + 0.5 * y;}
};

/// Divide integers, rounding away from zero.
///
/// This floating-point analogue may be useful for cross checking:
///   long double z = (long double)numerator / (long double)denominator;
///   return (T) (0 < z) ? std::ceil(z) : std::floor(z);

template<typename T>
inline T outward_quotient(T numerator, T denominator)
{
    static_assert(std::is_integral<T>::value);

    LMI_ASSERT(0 != denominator);

    // "INT_MIN / -1" would overflow; but "false/bool(-1)" would not,
    // hence the "T(-1) < 0" test.
    constexpr T min = std::numeric_limits<T>::min();
    LMI_ASSERT(!(min == numerator && T(-1) < 0 && T(-1) == denominator));

    T x = numerator / denominator;
    T y = 0 != numerator % denominator;
    return (0 < numerator == 0 < denominator) ? x + y : x - y;
}

// Actuarial functions.
//
// Some inputs are nonsense, like interest rates less than 100%.
// Contemporary compilers usually handle such situations without
// raising a hardware exception. Trapping invalid input would add a
// runtime overhead of about twenty percent (measured with gcc-3.4.2);
// this is judged not to be worthwhile.
//
// Typically, the period 'n' is a constant known at compile time, so
// it is makes sense for it to be a non-type template parameter. To
// support some old <functional> code, specializations for the most
// common case, where 'n' equals twelve, are provided with the
// typedefs that std::unary_function formerly provided.
//
// General preconditions: 0 < 'n'; -1.0 <= 'i'; T is floating point.
//
// Implementation note: greater accuracy and speed are obtained by
// applying the transformation
//   (1+i)^n - 1 <-> std::expm1(std::log1p(i) * n)
// to naive power-based formulas.

template<typename T, int n>
struct i_upper_n_over_n_from_i
{
    static_assert(std::is_floating_point<T>::value);
    static_assert(0 < n);
    T operator()(T const& i) const
        {
        if(i < -1.0)
            {
            throw std::domain_error("i is less than -100%.");
            }

        if(-1.0 == i)
            {
            return -1.0;
            }

        static long double const reciprocal_n = 1.0L / n;
        // naively:    (1+i)^(1/n) - 1
        // substitute: (1+i)^n - 1 <-> std::expm1(std::log1p(i) * n)
        long double z = std::expm1l(std::log1pl(i) * reciprocal_n);
        return static_cast<T>(z);
        }
};

template<typename T>
struct i_upper_12_over_12_from_i
{
    using argument_type = T;
    using result_type   = T;
    static_assert(std::is_floating_point<T>::value);
    T operator()(T const& i) const
        {
        return i_upper_n_over_n_from_i<T,12>()(i);
        }
};

template<typename T, int n>
struct i_from_i_upper_n_over_n
{
    static_assert(std::is_floating_point<T>::value);
    static_assert(0 < n);
    T operator()(T const& i) const
        {
        // naively:    (1+i)^n - 1
        // substitute: (1+i)^n - 1 <-> std::expm1(std::log1p(i) * n)
        long double z = std::expm1l(std::log1pl(i) * n);
        return static_cast<T>(z);
        }
};

template<typename T>
struct i_from_i_upper_12_over_12
{
    static_assert(std::is_floating_point<T>::value);
    T operator()(T const& i) const
        {
        return i_from_i_upper_n_over_n<T,12>()(i);
        }
};

template<typename T, int n>
struct d_upper_n_from_i
{
    static_assert(std::is_floating_point<T>::value);
    static_assert(0 < n);
    T operator()(T const& i) const
        {
        if(i < -1.0)
            {
            throw std::domain_error("i is less than -100%.");
            }

        if(-1.0 == i)
            {
            throw std::range_error("i equals -100%.");
            }

        static long double const reciprocal_n = 1.0L / n;
        // naively:    n * (1 - (1+i)^(-1/n))
        // substitute: (1+i)^n - 1 <-> std::expm1(std::log1p(i) * n)
        long double z = -n * std::expm1l(std::log1pl(i) * -reciprocal_n);
        return static_cast<T>(z);
        }
};

template<typename T>
struct d_upper_12_from_i
{
    static_assert(std::is_floating_point<T>::value);
    T operator()(T const& i) const
        {
        return d_upper_n_from_i<T,12>()(i);
        }
};

/// Annual net from annual gross rate, with two different kinds of
/// decrements. See the interest-rate class for the motivation.
///
/// Additional precondition: arguments are not such as to cause the
/// result to be less than -1.0 .

template<typename T, int n>
struct net_i_from_gross
{
    static_assert(std::is_floating_point<T>::value);
    static_assert(0 < n);
    T operator()(T const& i, T const& spread, T const& fee) const
        {
        static long double const reciprocal_n = 1.0L / n;
        // naively:
        //   (1
        //   +   (1+     i)^(1/n)
        //   -   (1+spread)^(1/n)
        //   -         fee *(1/n)
        //   )^n - 1
        // substitute: (1+i)^n - 1 <-> std::expm1(std::log1p(i) * n)
        long double z = std::expm1l
            (
            n * std::log1pl
                (   std::expm1l(reciprocal_n * std::log1pl(i))
                -   std::expm1l(reciprocal_n * std::log1pl(spread))
                -          reciprocal_n * fee
                )
            );
        return static_cast<T>(z);
        }
};

/// Convert q to a monthly COI rate.
///
/// The COI rate is the monthly equivalent of q divided by one minus
/// itself, because deducting the COI charge at the beginning of the
/// month increases the amount actually at risk--see:
///   http://lists.nongnu.org/archive/html/lmi/2009-09/msg00001.html
///
/// The value of 'q' might exceed unity, for example if guaranteed COI
/// rates for simplified issue are 125% of 1980 CSO, so that case is
/// accommodated. A value of zero might arise from a partial-mortality
/// multiplier that equals zero for some or all durations, and that
/// case arises often enough to merit a special optimization.
///
/// Preconditions:
///   'max_coi' is in [0.0, 1.0]
///   'q' is nonnegative
/// An exception is thrown if any precondition is violated.
///
/// If 'q' exceeds unity, then 'max_coi' is returned. Notionally, 'q'
/// is a probability and cannot exceed unity, but it doesn't seem
/// implausible to most actuaries to set q to 125% of 1980 CSO and
/// expect it to limit itself.

template<typename T>
struct coi_rate_from_q
{
    using first_argument_type  = T;
    using second_argument_type = T;
    using result_type          = T;
    static_assert(std::is_floating_point<T>::value);
    T operator()(T const& q, T const& max_coi) const
        {
        if(!(0.0 <= max_coi && max_coi <= 1.0))
            {
            throw std::runtime_error("Maximum COI rate out of range.");
            }

        if(q < 0.0)
            {
            throw std::domain_error("q is negative.");
            }

        if(0.0 == q)
            {
            return 0.0;
            }
        else if(1.0 <= q)
            {
            return max_coi;
            }
        else
            {
            static long double const reciprocal_12 = 1.0L / 12;
            // naively:    1 - (1-q)^(1/12)
            // substitute: (1+i)^n - 1 <-> std::expm1(std::log1p(i) * n)
            long double monthly_q = -std::expm1l(std::log1pl(-q) * reciprocal_12);
            if(1.0L == monthly_q)
                {
                throw std::logic_error("Monthly q equals unity.");
                }
            monthly_q = monthly_q / (1.0L - monthly_q);
            return std::min(max_coi, static_cast<T>(monthly_q));
            }
        }
};

/// Midpoint for illustration reg.
///
/// Section 7(C)(1)(c)(ii) prescribes an "average" without specifying
/// which average to use. The arithmetic mean is used here because
/// that seems to be the most common practice. On the other hand, a
/// strong case can be made for using the geometric mean, at least
/// with interest and mortality rates.

template<typename T>
void assign_midpoint
    (std::vector<T>      & out
    ,std::vector<T> const& in_0
    ,std::vector<T> const& in_1
    )
{
    LMI_ASSERT(in_0.size() == in_1.size());
    out.resize(in_0.size());
    assign(out, apply_binary(mean<T>(), in_0, in_1));
}

#endif // math_functions_hpp
