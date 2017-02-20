// Input sequences (e.g. 1 3; 7 5;0; --> 1 1 1 7 7 0...)
//
// Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Gregory W. Chicares.
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

// Extract the grammar from lines matching the regexp _// GRAMMAR_ .

#include "pchfile.hpp"

#include "input_sequence.hpp"

#include "alert.hpp"
#include "assert_lmi.hpp"
#include "contains.hpp"
#include "input_sequence_parser.hpp"
#include "value_cast.hpp"

#include <algorithm>                    // std::fill()
#include <sstream>
#include <stdexcept>
#include <type_traits>

InputSequence::InputSequence
    (std::string const&              input_expression
    ,int                             a_years_to_maturity
    ,int                             a_issue_age
    ,int                             a_retirement_age
    ,int                             a_inforce_duration
    ,int                             a_effective_year
    ,std::vector<std::string> const& a_allowed_keywords
    ,bool                            a_keywords_only
    ,std::string const&              a_default_keyword
    )
    :years_to_maturity_             (a_years_to_maturity)
    ,issue_age_                     (a_issue_age)
    ,retirement_age_                (a_retirement_age)
    ,inforce_duration_              (a_inforce_duration)
    ,effective_year_                (a_effective_year)
    ,allowed_keywords_              (a_allowed_keywords)
    ,keywords_only_                 (a_keywords_only)
    ,default_keyword_               (a_default_keyword)
{
    // A default keyword should be specified (i.e., nonempty) only for
    // keyword-only sequences (otherwise, the default is numeric), and
    // it should always be allowable even though other keywords may be
    // disallowed in context. As this is written in 2017-02, the only
    // UDTs with default keywords are:
    //   mode_sequence::default_keyword() // "annual"
    //   dbo_sequence::default_keyword()  // "a"
    // This assertion will provide useful guidance if, e.g., a new
    // policy form that forbids annual mode is implemented.
    LMI_ASSERT
        (  a_default_keyword.empty()
        || a_keywords_only && contains(a_allowed_keywords, a_default_keyword)
        );

    SequenceParser parser
        (input_expression
        ,a_years_to_maturity
        ,a_issue_age
        ,a_retirement_age
        ,a_inforce_duration
        ,a_effective_year
        ,a_allowed_keywords
        ,a_keywords_only
        );

    std::string const parser_diagnostics = parser.diagnostics();
    if(!parser_diagnostics.empty())
        {
        throw std::runtime_error(parser_diagnostics);
        }

    intervals_ = parser.intervals();

    // Inception and maturity endpoints exist, so the interval they
    // define must exist. However, parsing an empty expression
    // constructs zero intervals, so a default one must be created
    // to make the physical reality meet the conceptual requirement.
    if(intervals_.empty())
        {
        intervals_.push_back(ValueInterval());
        }

    // Extend the last interval's endpoint to maturity, replicating
    // the last element. (This doesn't need to be done by the ctors
    // that take vector arguments, because those arguments specify
    // each value in [inception, maturity) and deduce the terminal
    // (maturity) duration from size().)

    // This invariant has not yet been established, whether or not the
    // sequence was empty.
    intervals_.back().end_duration = a_years_to_maturity;
    // This invariant is established by realize_intervals(), but it
    // does no harm to repeat it here, and it would be confusing not
    // to do so in conjunction with the line above.
    intervals_.back().end_mode     = e_maturity;

    realize_intervals();
}

/// Construct from vector: e.g, 1 1 1 2 2 --> 1[0,3); 2[3,4).
///
/// This is used, e.g., when interest rates obtained from an external
/// source vary from one year to the next, and it is desired to use
/// them as lmi input. It might seem that inserting semicolons between
/// elements would produce acceptable input, and that the only benefit
/// is run-length encoding. However, if the imported vector is of
/// length 20, with the last 19 elements the same, then pasting it
/// into lmi with semicolon delimiters would be an input error if
/// there are only 15 years until retirement.

InputSequence::InputSequence(std::vector<double> const& v)
    :years_to_maturity_(v.size())
{
    initialize_from_vector(v);
    realize_intervals();
}

/// Construct from vector: e.g, a a a b b --> a[0,3); b[3,4).
///
/// No actual need for this particular ctor has yet been found, but
/// one might be, someday.

InputSequence::InputSequence(std::vector<std::string> const& v)
    :years_to_maturity_(v.size())
{
    initialize_from_vector(v);
    realize_intervals();
}

namespace
{
// Naturally {value_number, value_keyword} constitute a discriminated
// union: perhaps std::variant when lmi someday requires C++17. See:
//   http://lists.nongnu.org/archive/html/lmi/2017-02/msg00025.html
// Until then...

void set_value(ValueInterval& v, double d)
{
    LMI_ASSERT(!v.value_is_keyword);
    v.value_number = d;
}

void set_value(ValueInterval& v, std::string const& s)
{
    LMI_ASSERT(v.value_is_keyword);
    v.value_keyword = s;
}
} // Unnamed namespace.

// Constructors taking only one (vector) argument are used to convert
// flat vectors with one value per year to input sequences, compacted
// with run-length encoding.
//
// The control constructs may appear nonobvious. This design treats
// the push_back operation as fundamental: push_back is called exactly
// when we know that a new interval must be added. This avoids special
// handling
//   when the vectors are of length zero, and
//   for the last interval.
// As a consequence, we always push_back a dummy interval exactly when
// we know that it will be needed, and then write to intervals_.back().
//
// An alternative design would work with a temporary interval and
// call push_back as needed. I tried that and concluded that this
// design is simpler.
//
// Strings in input vectors are not validated against a map of
// permissible strings: these constructors are designed for use only
// with vectors of strings generated by the program from known-valid
// input, and should not be used in any other situation.
// SOMEDAY !! Ideally, therefore, they should be protected from
// unintended use.

template<typename T>
void InputSequence::initialize_from_vector(std::vector<T> const& v)
{
    bool const T_is_double = std::is_same<T,double     >::value;
    bool const T_is_string = std::is_same<T,std::string>::value;
    static_assert(T_is_double || T_is_string, "");

    ValueInterval dummy;
    dummy.value_is_keyword = T_is_string;

    T prior_value = v.empty() ? T() : v.front();
    T current_value = prior_value;

    intervals_.push_back(dummy);
    set_value(intervals_.back(), current_value);

    for(auto const& vi : v)
        {
        current_value = vi;
        if(prior_value == current_value)
            {
            ++intervals_.back().end_duration;
            }
        else
            {
            int value_change_duration = intervals_.back().end_duration;
            intervals_.push_back(dummy);
            set_value(intervals_.back(), current_value);
            intervals_.back().begin_duration = value_change_duration;
            intervals_.back().end_duration = ++value_change_duration;
            prior_value = current_value;
            }
        }
}

InputSequence::~InputSequence() = default;

std::vector<double> const& InputSequence::linear_number_representation() const
{
    return number_result_;
}

std::vector<std::string> const& InputSequence::linear_keyword_representation() const
{
    return keyword_result_;
}

/// Regularized representation in [x,y) interval notation.
///
/// If there's only one interval, it must span all years, so depict it
/// as the simple scalar that it is, specifying no interval.
///
/// Use keyword 'maturity' for the last duration. This avoids
/// gratuitous differences between lives, e.g.
///   '10000 [20,55); 0' for a 45-year-old
/// and
///   '10000 [20,65); 0' for a 35-year-old
/// which the census GUI would treat as varying across cells, whereas
///   '10000 [20,65); maturity'
/// expresses the same sequence uniformly.
///
/// TODO ?? For the same reason, this representation should preserve
/// duration keywords such as 'retirement'.

std::string InputSequence::mathematical_representation() const
{
    std::ostringstream oss;
    for(auto const& interval_i : intervals_)
        {
        if(interval_i.value_is_keyword)
            {
            oss << interval_i.value_keyword;
            }
        else
            {
            oss << value_cast<std::string>(interval_i.value_number);
            }

        if(1 == intervals_.size())
            {
            break;
            }

        if(interval_i.end_duration != years_to_maturity_)
            {
            oss
                << " ["
                << interval_i.begin_duration
                << ", "
                << interval_i.end_duration
                << "); "
                ;
            }
        else
            {
            oss
                << " ["
                << interval_i.begin_duration
                << ", "
                << "maturity"
                << ")"
                ;
            }
        }
    return oss.str();
}

std::vector<ValueInterval> const& InputSequence::interval_representation() const
{
    return intervals_;
}

void InputSequence::realize_intervals()
{
    // Post-construction invariants.
    // Every ctor must already have established this...
    LMI_ASSERT(!intervals_.empty());
    // ...and this:
    LMI_ASSERT(years_to_maturity_ == intervals_.back().end_duration);
    // It cannot be assumed that all ctors have yet established this...
    intervals_.back().end_mode = e_maturity;
    // ...though now of course it has been established:
    LMI_ASSERT(e_maturity        == intervals_.back().end_mode    );

    std::vector<double>      r(years_to_maturity_);
    std::vector<std::string> s(years_to_maturity_, default_keyword_);
    number_result_  = r;
    keyword_result_ = s;

    int prior_begin_duration = 0;
    for(auto const& interval_i : intervals_)
        {
        if(interval_i.insane)
            {
            fatal_error()
                << "Untrapped parser error."
                << LMI_FLUSH
                ;
            }
        if(interval_i.value_is_keyword && "daft" == interval_i.value_keyword)
            {
            fatal_error()
                << "Interval "
                << "[ " << interval_i.begin_duration << ", "
                << interval_i.end_duration << " )"
                << " has invalid value_keyword."
                << LMI_FLUSH
                ;
            }
        if(e_invalid_mode == interval_i.begin_mode)
            {
            fatal_error()
                << "Interval "
                << "[ " << interval_i.begin_duration << ", "
                << interval_i.end_duration << " )"
                << " has invalid begin_mode."
                << LMI_FLUSH
                ;
            }
        if(e_invalid_mode == interval_i.end_mode)
            {
            fatal_error()
                << "Interval "
                << "[ " << interval_i.begin_duration << ", "
                << interval_i.end_duration << " )"
                << " has invalid end_mode."
                << LMI_FLUSH
                ;
            }
        if(interval_i.begin_duration < prior_begin_duration)
            {
            fatal_error()
                << "Previous interval began at duration "
                << prior_begin_duration
                << "; current interval "
                << "[ " << interval_i.begin_duration << ", "
                << interval_i.end_duration << " )"
                << " would begin before that."
                << LMI_FLUSH
                ;
            }
        prior_begin_duration = interval_i.begin_duration;
        bool interval_is_ok =
               0                         <= interval_i.begin_duration
            && interval_i.begin_duration <= interval_i.end_duration
            && interval_i.end_duration   <= years_to_maturity_
            ;
        if(!interval_is_ok)
            {
            fatal_error()
                << "Interval "
                << "[ " << interval_i.begin_duration << ", "
                << interval_i.end_duration << " )"
                << " not valid."
                << LMI_FLUSH
                ;
            }
        if(interval_i.value_is_keyword)
            {
            std::fill
                (s.begin() + interval_i.begin_duration
                ,s.begin() + interval_i.end_duration
                ,interval_i.value_keyword
                );
            }
        else
            {
            std::fill
                (r.begin() + interval_i.begin_duration
                ,r.begin() + interval_i.end_duration
                ,interval_i.value_number
                );
            }
        }

    number_result_  = r;
    keyword_result_ = s;
}

