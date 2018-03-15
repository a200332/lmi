// Ledger values: common base class.
//
// Copyright (C) 1998, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018 Gregory W. Chicares.
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

#include "pchfile.hpp"

#include "ledger_base.hpp"

#include "alert.hpp"
#include "assert_lmi.hpp"
#include "crc32.hpp"
#include "miscellany.hpp"               // minmax
#include "value_cast.hpp"

#include <algorithm>                    // max(), min(), transform()
#include <cmath>                        // floor(), log10(), pow()
#include <functional>                   // multiplies

//============================================================================
LedgerBase::LedgerBase(int a_Length)
    :scale_factor_(1.0)
    ,scale_unit_  ("")
{
    Initialize(a_Length);
}

//============================================================================
LedgerBase::LedgerBase(LedgerBase const& obj)
    :scale_factor_(obj.scale_factor_)
    ,scale_unit_  (obj.scale_unit_)
{
    Initialize(obj.GetLength());
    Copy(obj);
}

//============================================================================
LedgerBase& LedgerBase::operator=(LedgerBase const& obj)
{
    if(this != &obj)
        {
        Initialize(obj.GetLength());
        Copy(obj);
        }
    return *this;
}

//============================================================================
void LedgerBase::Alloc()
{
    ScalableVectors.insert(BegYearVectors   .begin(), BegYearVectors    .end());
    ScalableVectors.insert(EndYearVectors   .begin(), EndYearVectors    .end());
    ScalableVectors.insert(ForborneVectors  .begin(), ForborneVectors   .end());

    AllVectors.insert(BegYearVectors        .begin(), BegYearVectors    .end());
    AllVectors.insert(EndYearVectors        .begin(), EndYearVectors    .end());
    AllVectors.insert(ForborneVectors       .begin(), ForborneVectors   .end());
    AllVectors.insert(OtherVectors          .begin(), OtherVectors      .end());

    AllScalars.insert(ScalableScalars       .begin(), ScalableScalars   .end());
    AllScalars.insert(OtherScalars          .begin(), OtherScalars      .end());
}

//============================================================================
void LedgerBase::Initialize(int a_Length)
{
    for(auto& i : AllVectors)
        {
        i.second->assign(a_Length, 0.0);
        }

    for(auto& i : AllScalars)
        {
        *i.second = 0.0;
        }
}

//============================================================================
void LedgerBase::Copy(LedgerBase const& obj)
{
    // We do not do this:
    // AllVectors       = obj.AllVectors; // DO NOT DO THIS
    // The reason is that map<> members are structural artifacts of the
    // design of this class, and are not information in and of themselves.
    // Rather, their contents are information that is added in by derived
    // classes.
    //
    // TODO ?? There has to be a way to abstract this.

    double_vector_map::const_iterator obj_svmi = obj.AllVectors.begin();
    for
        (double_vector_map::iterator svmi = AllVectors.begin()
        ;svmi != AllVectors.end()
        ;svmi++, obj_svmi++
        )
        {
        *(*svmi).second = *(*obj_svmi).second;
        }

    scalar_map::const_iterator obj_sci = obj.AllScalars.begin();
    for
        (scalar_map::iterator svmi = AllScalars.begin()
        ;svmi != AllScalars.end()
        ;svmi++, obj_sci++
        )
        {
        *(*svmi).second = *(*obj_sci).second;
        }

    string_map::const_iterator obj_sti = obj.Strings.begin();
    for
        (string_map::iterator svmi = Strings.begin()
        ;svmi != Strings.end()
        ;svmi++, obj_sti++
        )
        {
        *(*svmi).second = *(*obj_sti).second;
        }
}

//============================================================================
std::string LedgerBase::value_str(std::string const& map_key, int index) const
{
    double_vector_map::const_iterator found = AllVectors.find(map_key);
    if(AllVectors.end() != found)
        {
        return value_cast<std::string>((*(*found).second)[index]);
        }

    alarum() << "Map key '" << map_key << "' not found." << LMI_FLUSH;
    return "";
}

//============================================================================
std::string LedgerBase::value_str(std::string const& map_key) const
{
    string_map::const_iterator found_string = Strings.find(map_key);
    if(Strings.end() != found_string)
        {
        return *(*found_string).second;
        }

    scalar_map::const_iterator found_scalar = AllScalars.find(map_key);
    if(AllScalars.end() != found_scalar)
        {
        return value_cast<std::string>(*(*found_scalar).second);
        }

    alarum() << "Map key '" << map_key << "' not found." << LMI_FLUSH;
    return "";
}

//============================================================================
double_vector_map const& LedgerBase::all_vectors() const
{
    return AllVectors;
}

namespace
{
// Special non-general helper function.
// The sole use of this function is to multiply y, a vector of values in
// a ledger, by z, a vector of inforce factors. For this sole intended use,
// we know that z is nonzero and nondecreasing; therefore, if it ever
// becomes zero, it remains zero. We can safely break at that point in the
// interest of speed because adding y times zero to z is a NOP. Note that
// the inforce factor becomes zero upon lapse.
    static void x_plus_eq_y_times_z
        (std::vector<double>& x
        ,std::vector<double> const& y
        ,std::vector<double> const& z
        )
    {
        std::vector<double>::iterator ix = x.begin();
        std::vector<double>::const_iterator iy = y.begin();
        std::vector<double>::const_iterator iz = z.begin();
        LMI_ASSERT(y.size() <= x.size());
        LMI_ASSERT(y.size() <= z.size());
        while(iy != y.end())
            {
            LMI_ASSERT(ix != x.end());
            LMI_ASSERT(iz != z.end());
            double mult = *iz++;
            if(0.0 == mult)
                {
                break;
                }
            // Don't waste time multiplying by one
            if(1.0 == mult)
                {
                *ix++ += *iy++;
                }
            else
                {
                *ix++ += *iy++ * mult;
                }
            }
    }
} // Unnamed namespace.

//============================================================================
// TODO ?? Adds cells by policy duration, not calendar duration: when
// cell issue dates differ, the result is valid only in that probably-
// unexpected sense.
LedgerBase& LedgerBase::PlusEq
    (LedgerBase          const& a_Addend
    ,std::vector<double> const& a_Inforce
    )
{
    LMI_ASSERT(0.0 != scale_factor_);
    if(scale_factor_ != a_Addend.scale_factor_)
        {
        alarum() << "Cannot add differently scaled ledgers." << LMI_FLUSH;
        }

    double_vector_map::const_iterator a_Addend_svmi;

    a_Addend_svmi = a_Addend.BegYearVectors.begin();
    std::vector<double> const BegYearInforce = a_Inforce;
    for
        (double_vector_map::iterator svmi = BegYearVectors.begin()
        ;svmi != BegYearVectors.end()
        ;svmi++, a_Addend_svmi++
        )
        {
        x_plus_eq_y_times_z
            (*(*svmi).second
            ,*(*a_Addend_svmi).second
            ,BegYearInforce
            );
        }
    LMI_ASSERT(a_Addend_svmi == a_Addend.BegYearVectors.end());

    std::vector<double> const EndYearInforce
        (a_Inforce.begin() + 1
        ,a_Inforce.end()
        );
    a_Addend_svmi = a_Addend.EndYearVectors.begin();
    for
        (double_vector_map::iterator svmi = EndYearVectors.begin()
        ;svmi != EndYearVectors.end()
        ;svmi++, a_Addend_svmi++
        )
        {
        x_plus_eq_y_times_z
            (*(*svmi).second
            ,*(*a_Addend_svmi).second
            ,EndYearInforce
            );
        }
    LMI_ASSERT(a_Addend_svmi == a_Addend.EndYearVectors.end());

    std::vector<double> const NumLivesIssued
        (a_Inforce.size()
        ,a_Inforce[0]
        );
    a_Addend_svmi = a_Addend.ForborneVectors.begin();
    for
        (double_vector_map::iterator svmi = ForborneVectors.begin()
        ;svmi != ForborneVectors.end()
        ;svmi++, a_Addend_svmi++
        )
        {
        x_plus_eq_y_times_z
            (*(*svmi).second
            ,*(*a_Addend_svmi).second
            ,NumLivesIssued
            );
        }
    LMI_ASSERT(a_Addend_svmi == a_Addend.ForborneVectors.end());

    scalar_map::const_iterator a_Addend_ssmi = a_Addend.ScalableScalars.begin();
    for
        (scalar_map::iterator ssmi = ScalableScalars.begin()
        ;ssmi != ScalableScalars.end()
        ;ssmi++, a_Addend_ssmi++
        )
        {
        *(*ssmi).second += *(*a_Addend_ssmi).second * a_Inforce[0];
        }
    LMI_ASSERT(a_Addend_ssmi == a_Addend.ScalableScalars.end());

    return *this;
}

//============================================================================
// Multiplier to keep max < one billion units.
//
// TODO ?? It would be nicer to factor out
//   1000000000.0 (max width)
//   and 1.0E-18 (highest number we translate to words)
// and make them variables.
// PDF !! This seems not to be rigorously correct: $999,999,999.99 is
// less than one billion, but rounds to $1,000,000,000.
double LedgerBase::DetermineScaleFactor() const
{
    double min_val = 0.0;
    double max_val = 0.0;

    for(auto const& i : ScalableVectors)
        {
        minmax<double> extrema(*i.second);
        min_val = std::min(min_val, extrema.minimum());
        max_val = std::max(max_val, extrema.maximum());
        }

    // If minimum value is negative, it needs an extra character to
    // display the minus sign. So it needs as many characters as
    // ten times its absolute value.
    double widest = std::max
        (max_val
        ,min_val * -10
        );

    if(widest < 1000000000.0 || widest == 0)
        {
        return 1.0;
        }
    double d = std::log10(widest);
    d = 3.0 * std::floor(d / 3.0);
    d = std::pow(10.0, 6.0 - d);

    LMI_ASSERT(1.0E-18 <= d);
    LMI_ASSERT(d <= 1.0);

    return d;
}

namespace
{
    static std::string look_up_scale_unit(double a_ScalingFactor)
        {
        if(0.0 == a_ScalingFactor)
            {
            alarum() << "Scaling factor is zero." << LMI_FLUSH;
            return "ZERO";
            }

        double power = -std::log10(a_ScalingFactor);
        // Assert absolute equality of two floating-point quantities, because
        // they must both have integral values.
        // PDF !! Suppress the assertion because, with MinGW-w64 gcc-7.2.0,
        // it fails--apparently floor() gives the wrong answer, but trunc()
        // and static_cast<int>() give the right answer for the test case
        // described in the git commit message for 1c1bafa40. Obviously this
        // needs further work because the behavior in other cases is unknown,
        // and adding a small quantity to a near-integer to force it to round
        // "correctly" is surely bogus.
        // LMI_ASSERT(power == std::floor(power));
        int z = static_cast<int>(0.1 + power);

        // US names are used; UK names are different.
        // Assume that numbers over 999 quintillion (US) will not be needed.
        switch(z)
            {
            case 0:
                {
                return "";
                }
                //  break;
            case 3:
                {
                return "thousand";
                }
                //  break;
            case 6:
                {
                return "million";
                }
                //  break;
            case 9:
                {
                return "billion";
                }
                //  break;
            case 12:
                {
                return "trillion";
                }
                //  break;
            case 15:
                {
                return "quadrillion";
                }
                //  break;
            case 18:
                {
                return "quintillion";
                }
                //  break;
            default:
                {
                alarum() << "Case '" << z << "' not found." << LMI_FLUSH;
                throw "Unreachable--silences a compiler diagnostic.";
                }
            }
        }
} // Unnamed namespace.

//============================================================================
// Multiplies all scalable vectors by the factor from DetermineScaleFactor().
// Only columns are scaled, so we operate here only on vectors. A header
// that shows e.g. face amount should show the true face amount, unscaled.
void LedgerBase::ApplyScaleFactor(double a_Mult)
{
    LMI_ASSERT(0.0 != a_Mult);
    LMI_ASSERT(0.0 != scale_factor_);
    if(1.0 != scale_factor_)
        {
        alarum() << "Cannot scale the same ledger twice." << LMI_FLUSH;
        }

    scale_factor_ = a_Mult;
    if(1.0 == scale_factor_)
        {
        // Don't waste time multiplying all these vectors by one
        return;
        }
    scale_unit_ = look_up_scale_unit(scale_factor_);

    // ET !! *i.second *= M;
    std::vector<double>M(GetLength(), scale_factor_);
    for(auto& i : ScalableVectors)
        {
        std::vector<double>& v = *i.second;
        std::transform
            (v.begin()
            ,v.end()
            ,M.begin()
            ,v.begin()
            ,std::multiplies<double>()
            );
        }
}

//============================================================================
std::string const& LedgerBase::ScaleUnit() const
{
    return scale_unit_;
}

//============================================================================
double LedgerBase::ScaleFactor() const
{
    return scale_factor_;
}

//============================================================================
void LedgerBase::UpdateCRC(CRC& crc) const
{
    for(auto const& i : AllVectors)
        {
        crc += *i.second;
        }

    for(auto const& i : AllScalars)
        {
        crc += *i.second;
        }

    for(auto const& i : Strings)
        {
        crc += *i.second;
        }
}

//============================================================================
void LedgerBase::Spew(std::ostream& os) const
{
    for(auto const& i : AllVectors)
        {
        SpewVector(os, i.first, *i.second);
        }

    for(auto const& i : AllScalars)
        {
        os
            << i.first
            << "=="
            << std::setprecision(DECIMAL_DIG) << *i.second
            << '\n'
            ;
        }

    for(auto const& i : Strings)
        {
        os
            << i.first
            << "=="
            << *i.second
            << '\n'
            ;
        }
}

