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

namespace
{
void assert_not_insane_or_disordered
    (std::vector<ValueInterval> const& intervals
    ,int                               years_to_maturity
    );

void assert_sane_and_ordered_partition
    (std::vector<ValueInterval> const& intervals
    ,int                               years_to_maturity
    );

void fill_interval_gaps
    (std::vector<ValueInterval> const& in
    ,std::vector<ValueInterval>      & out
    ,int                               years_to_maturity
    ,bool                              keywords_only
    ,std::string                const& default_keyword
    );
} // Unnamed namespace.

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
    :years_to_maturity_ (a_years_to_maturity)
    ,issue_age_         (a_issue_age)
    ,number_result_     (a_years_to_maturity)
    ,keyword_result_    (a_years_to_maturity, a_default_keyword)
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

    fill_interval_gaps
        (parser.intervals()
        ,intervals_
        ,a_years_to_maturity
        ,a_keywords_only
        ,a_default_keyword
        );

    realize_intervals();
    assert_sane_and_ordered_partition(intervals_, a_years_to_maturity);
}

/// Construct from vector: e.g, 1 1 1 2 2 --> 1[0,3); 2[3,4).
///
/// This is used, e.g., when interest rates obtained from an external
/// source vary from one year to the next, and it is desired to use
/// them as lmi input. It might seem that inserting semicolons between
/// elements would produce acceptable input, and that the only benefit
/// is saving space because of RLE. However, if the imported vector is
/// of length 20, with the last 19 elements the same, then pasting it
/// into lmi with semicolon delimiters would be an input error if
/// there are only 15 years until retirement, whereas the two-element
/// RLE representation would work correctly.

InputSequence::InputSequence(std::vector<double> const& v)
    :years_to_maturity_(v.size())
    ,number_result_    {v}
    ,keyword_result_   {}
{
    initialize_from_vector(v);
    assert_sane_and_ordered_partition(intervals_, years_to_maturity_);
}

/// Construct from vector: e.g, a a a b b --> a[0,3); b[3,4).
///
/// No actual need for this particular ctor has yet been found, but
/// one might be, someday.

InputSequence::InputSequence(std::vector<std::string> const& v)
    :years_to_maturity_(v.size())
    ,number_result_    {}
    ,keyword_result_   {v}
{
    initialize_from_vector(v);
    assert_sane_and_ordered_partition(intervals_, years_to_maturity_);
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

/// Canonical form of an input sequence
///
/// Consider the set S of all strings that are well formed wrt the
/// grammar defined by class SequenceParser. Some are equivalent:
/// e.g., "1 [0, maturity)" and "1" mean the same thing. Choosing one
/// member of each equivalence class defines a canonical form.
///
/// The canonical form need not be a string if a better representation
/// can be found. For example, it might appear obvious that any member
/// of S can be transformed into this member of the present class:
///   std::vector<ValueInterval> intervals_;
/// yet that is not necessarily the case: this sequence
///   0, @65; 10000
/// is well formed, but (as this is written in 2017-02) would not be
/// permitted for a 70-year-old. Even if that obstacle is overcome,
/// a std::vector<ValueInterval> seems a poor choice because it is
/// intricate and unwieldy: it would be uncouth to serialize that into
/// xml or ask humans to deal with it. A simple, compact, readable
/// string is wanted instead.
///
/// Another unsuccessful candidate for the canonical form would have
/// specified left-closed and right-open intervals in full. Thus,
///   0, retirement; 10000, #10; 0
/// would have been canonicalized as
///   0 [0, retirement); 10000 [retirement, #10); 0 [?, maturity)
/// But then the last interval must begin at "retirement + 10", which
/// is not allowed by the grammar and therefore cannot be canonical.
/// At first, it had seemed possible to work around this by changing
/// the parser, to forbid
///   e_number_of_years == ValueInterval.begin_mode
/// by replacing that mode with the most recent differing begin_mode:
/// thus, {"@50" + "#10"} and {"5" + "#10"} would become "@60" and 15
/// respectively. However, that experiment failed because "retirement"
/// is neither an age nor a duration. It might be deemed to signify an
/// age, but that would lose the variable nature of the retirement-age
/// ctor argument, inaptly treating the case above as equivalent to
///   0, retirement; 10000, @C; 0
/// where C is 75 for retirement at age 65, 72...at age 62, and so on.
/// Specifying the original string
///   0, retirement; 10000, #10; 0
/// at the group level, for a census with differing retirement ages,
/// encompasses that variation; no canonicalization that loses that
/// advantage is acceptable.

std::string InputSequence::canonical_form() const
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

        std::string s;
        switch(interval_i.end_mode)
            {
            case e_invalid_mode:
                {
                fatal_error() << "Invalid mode." << LMI_FLUSH;
                }
                break;
            case e_duration:
                {
                int const z = interval_i.end_duration;
                s = " " + value_cast<std::string>(z);
                }
                break;
            case e_attained_age:
                {
                int const z = interval_i.end_duration + issue_age_;
                s = " @" + value_cast<std::string>(z);
                }
                break;
            case e_number_of_years:
                {
                int const z = interval_i.end_duration - interval_i.begin_duration;
                s = " #" + value_cast<std::string>(z);
                }
                break;
            case e_inception:
                {
                fatal_error() << "Interval ended at inception." << LMI_FLUSH;
                }
                break;
            case e_inforce:
                {
                fatal_error() << "'e_inforce' not implemented." << LMI_FLUSH;
                }
                break;
            case e_retirement:
                {
                s = " retirement";
                }
                break;
            case e_maturity:
                {
                s = " maturity"; // Generally omitted.
                }
                break;
            }

        if(interval_i.end_duration != years_to_maturity_)
            {
            oss << s << "; ";
            }
        else
            {
            ; // Do nothing.
            }
        }
    return oss.str();
}

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
    for(auto const& interval_i : intervals_)
        {
        LMI_ASSERT(0 <= interval_i.begin_duration);
        LMI_ASSERT(interval_i.begin_duration <= interval_i.end_duration);
        LMI_ASSERT(interval_i.end_duration <= years_to_maturity_);
        if(interval_i.value_is_keyword)
            {
            std::fill
                (keyword_result_.begin() + interval_i.begin_duration
                ,keyword_result_.begin() + interval_i.end_duration
                ,interval_i.value_keyword
                );
            }
        else
            {
            std::fill
                (number_result_.begin() + interval_i.begin_duration
                ,number_result_.begin() + interval_i.end_duration
                ,interval_i.value_number
                );
            }
        }
}

namespace
{
void assert_not_insane_or_disordered
    (std::vector<ValueInterval> const& intervals
    ,int                               years_to_maturity
    )
{
    int prior_begin_duration = 0;
    for(auto const& interval_i : intervals)
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
        if(interval_i.begin_duration < 0)
            {
            fatal_error()
                << "Interval "
                << "[ " << interval_i.begin_duration << ", "
                << interval_i.end_duration << " )"
                << " is improper: it begins before duration zero."
                << LMI_FLUSH
                ;
            }
        if(interval_i.end_duration < interval_i.begin_duration)
            {
            fatal_error()
                << "Interval "
                << "[ " << interval_i.begin_duration << ", "
                << interval_i.end_duration << " )"
                << " is improper: it ends before it begins."
                << LMI_FLUSH
                ;
            }
        if(years_to_maturity < interval_i.end_duration)
            {
            fatal_error()
                << "Interval "
                << "[ " << interval_i.begin_duration << ", "
                << interval_i.end_duration << " )"
                << " is improper: it ends after maturity."
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
        }
}

/// Assert postconditions established by all ctors.
///
/// What is actually asserted here, for now at least, is only that the
/// intervals are contiguous--not that they truly partition the range
/// [0, years_to_maturity). Cf. fill_interval_gaps(), which similarly
/// establishes only this weaker invariant, which also happens to be
/// what InputSequenceEntry asserts.

void assert_sane_and_ordered_partition
    (std::vector<ValueInterval> const& intervals
    ,int                               years_to_maturity
    )
{
    assert_not_insane_or_disordered(intervals, years_to_maturity);

    LMI_ASSERT(!intervals.empty());

    LMI_ASSERT(0                 == intervals.front().begin_duration);
    LMI_ASSERT(e_inception       == intervals.front().begin_mode    );

    LMI_ASSERT(years_to_maturity == intervals.back().end_duration);
    LMI_ASSERT(e_maturity        == intervals.back().end_mode    );

    int prior_end_duration = 0;
    for(auto const& i : intervals)
        {
        if(i.begin_duration != prior_end_duration)
            {
            fatal_error()
                << "Interval "
                << "[ " << i.begin_duration << ", "
                << i.end_duration << " )"
                << " should begin at duration "
                << prior_end_duration
                << ", where the previous interval ended."
                << LMI_FLUSH
                ;
            }
        prior_end_duration = i.end_duration;
        }
}

/// Create a partition of [0, maturity) from parser output.
///
/// The last interval's endpoint is extended to maturity, replicating
/// the last value.
///
/// SequenceParser returns a set of intervals that may not constitute
/// a partition. Indeed, parsing an empty expression constructs zero
/// intervals, in which case a single interval must be created.
///
/// The for-statement does nothing if 'in' is empty. Otherwise, it
/// reads an element of 'in' and writes it to 'out', preceding it if
/// necessary with a synthesized interval to fill any preceding gap.
/// It has two main branches, controlled by its topmost if-else. The
/// first branch executes on the first iteration, and only then. (If
/// 'in' was empty, the for-statement does nothing; otherwise, 'out'
/// must be empty.)
///
/// The for-statement's second branch creates an improper interval if
/// the parsed expression had overlapping intervals. Alternatively, it
/// would create no such interval if its '!=' condition were replaced
/// by '<'; it is not obvious which way is better. For now at least,
/// assert_sane_and_ordered_partition() is called at the end of this
/// function to trap the anomaly.

void fill_interval_gaps
    (std::vector<ValueInterval> const& in
    ,std::vector<ValueInterval>      & out
    ,int                               years_to_maturity
    ,bool                              keywords_only
    ,std::string                const& default_keyword
    )
{
    assert_not_insane_or_disordered(in, years_to_maturity);

    LMI_ASSERT(out.empty());

    ValueInterval default_interval;
    default_interval.value_is_keyword = keywords_only;
    if(keywords_only)
        {
        default_interval.value_keyword = default_keyword;
        }

    if(in.empty())
        {
        out.push_back(default_interval);
        }

    // If in.empty(), then this loop iterates zero times.
    for(auto const& next : in)
        {
        if(out.empty()) // Iff first pass.
            {
            if(0 != next.begin_duration)
                {
                out.push_back(default_interval);
                out.back().end_mode     = next.begin_mode    ;
                out.back().end_duration = next.begin_duration;
                }
            out.push_back(next);
            }
        else // Iff not first pass.
            {
            auto const& last = out.back(); // Safe: 'out' cannot be empty.
            if(last.end_duration != next.begin_duration)
                {
                out.push_back(default_interval);
                out.back().begin_mode     = last.end_mode    ;
                out.back().begin_duration = last.end_duration;
                out.back().end_mode     = next.begin_mode    ;
                out.back().end_duration = next.begin_duration;
                }
            out.push_back(next);
            }
        }

    LMI_ASSERT(0 == out.front().begin_duration);
    out.front().begin_mode  = e_inception;

    out.back().end_duration = years_to_maturity;
    out.back().end_mode     = e_maturity;

    // This is necessary only to trap any improper interval that
    // may have been inserted.
    assert_sane_and_ordered_partition(out, years_to_maturity);
}
} // Unnamed namespace.

