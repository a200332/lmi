// Run an individual illustration, producing a ledger.
//
// Copyright (C) 1998, 2001, 2005, 2006, 2007, 2008 Gregory W. Chicares.
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
// email: <chicares@cox.net>
// snail: Chicares, 186 Belle Woods Drive, Glastonbury CT 06033, USA

// $Id: ledgervalues.hpp,v 1.27 2008-12-14 11:05:17 chicares Exp $

#ifndef ledgervalues_hpp
#define ledgervalues_hpp

#include "config.hpp"

#include "obstruct_slicing.hpp"
#include "so_attributes.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

#include <string>

class Input;
class Ledger;

/// Run an individual illustration, producing a ledger.
///
/// This class encapsulates a frequently-used series of operations.

class IllusVal
    :private boost::noncopyable
    ,virtual private obstruct_slicing<IllusVal>
{
  public:
    explicit IllusVal(std::string const& filename);
    ~IllusVal();

    double run(Input const&);

    boost::shared_ptr<Ledger const> ledger() const;

  private:
    std::string filename_;
    boost::shared_ptr<Ledger const> ledger_;
};

#endif // ledgervalues_hpp

