// Create a PDF file from a ledger.
//
// Copyright (C) 2017, 2018 Gregory W. Chicares.
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

#include "pdf_command.hpp"

#include "callback.hpp"

namespace
{
    callback<pdf_command_fp_type> pdf_command_callback;
} // Unnamed namespace.

typedef pdf_command_fp_type FunctionPointer;
template<> FunctionPointer callback<FunctionPointer>::function_pointer_ = nullptr;

bool pdf_command_initialize(pdf_command_fp_type f)
{
    pdf_command_callback.initialize(f);
    return true;
}

void pdf_command(Ledger const& ledger, fs::path const& pdf_out_file)
{
    pdf_command_callback()(ledger, pdf_out_file);
}