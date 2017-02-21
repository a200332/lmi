// Input sequences (e.g. 1 3; 7 5;0; --> 1 1 1 7 7 0...): parser
//
// Copyright (C) 2002, 2003, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017 Gregory W. Chicares.
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

// See documentation in main input-sequence header.

#ifndef input_sequence_parser_hpp
#define input_sequence_parser_hpp

#include "config.hpp"

#include "input_sequence_interval.hpp"
#include "obstruct_slicing.hpp"
#include "so_attributes.hpp"
#include "uncopyable_lmi.hpp"

#include <sstream>
#include <string>
#include <vector>

class SequenceParser
    :        private lmi::uncopyable <SequenceParser>
    ,virtual private obstruct_slicing<SequenceParser>
{
  public:
    SequenceParser
        (std::string const&              input_expression
        ,int                             a_years_to_maturity
        ,int                             a_issue_age
        ,int                             a_retirement_age
        ,int                             a_inforce_duration
        ,int                             a_effective_year
        ,std::vector<std::string> const& a_allowed_keywords
        ,bool                            a_keywords_only
        );

    ~SequenceParser();

    std::string diagnostics() const;
    std::vector<ValueInterval> const& intervals() const;

  private:
    enum token_type
        {e_eof             = 0
        ,e_major_separator = ';'
        ,e_minor_separator = ','
        ,e_begin_incl      = '['
        ,e_begin_excl      = '('
        ,e_end_incl        = ']'
        ,e_end_excl        = ')'
        ,e_age_prefix      = '@'
        ,e_cardinal_prefix = '#'
        ,e_number
        ,e_keyword
        ,e_startup
        };
    std::string token_type_name(token_type);

    void duration_scalar();
    void null_duration();
    void single_duration();
    void intervalic_duration();
    void validate_duration
        (int           tentative_begin_duration
        ,duration_mode tentative_begin_duration_mode
        ,int           tentative_end_duration
        ,duration_mode tentative_end_duration_mode
        );
    void duration();
    void value();
    void span();
    void sequence();
    token_type get_token();
    void match(token_type);

    void mark_diagnostic_context();

    std::istringstream input_stream_;

    // Copies of ctor args that are identical to class InputSequence's.
    int years_to_maturity_;
    int issue_age_;
    int retirement_age_;
    int inforce_duration_;
    int effective_year_;
    std::vector<std::string> allowed_keywords_;
    bool keywords_only_;

    token_type current_token_type_;
    double current_number_;
    std::string current_keyword_;
    int current_duration_scalar_;
    duration_mode previous_duration_scalar_mode_;
    duration_mode current_duration_scalar_mode_;
    ValueInterval current_interval_;
    int last_input_duration_;

    std::ostringstream diagnostics_;

    std::vector<ValueInterval> intervals_;
};

#endif // input_sequence_parser_hpp
