// Test case for creating new files of all types and opening them.
//
// Copyright (C) 2014 Gregory W. Chicares.
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

// $Id$

#ifdef __BORLANDC__
#   include "pchfile.hpp"
#   pragma hdrstop
#endif

#include "assert_lmi.hpp"
#include "mvc_controller.hpp"
#include "wx_test_case.hpp"
#include "version.hpp"

#include <wx/dialog.h>
#include <wx/scopeguard.h>
#include <wx/testing.h>
#include <wx/uiaction.h>

// Helper of test_new_file_and_save() which tests creating a new file of
// the type corresponding to the key argument, used to select this type in
// the "New" popup menu.
//
// The last argument indicates whether a dialog is shown when creating a
// new file of this type (e.g. true for illustrations, false for census).
// It affects this function behaviour in two ways: first, it needs to be
// ready for this dialog appearing and, second, "File|Save" menu command is
// disabled for the files created in this way and "File|Save as" needs to
// be used instead.
void do_test_create_open(int key, wxString const& file, bool uses_dialog)
{
    LMI_ASSERT(!wxFileExists(file));

    wxUIActionSimulator z;
    z.Char('n', wxMOD_CONTROL); // new file
    z.Char(key               ); // choose document type
    if (uses_dialog)
        {
        wxTEST_DIALOG
            (wxYield()
            ,wxExpectDismissableModal<MvcController>(wxID_OK)
            );
        }
    wxYield();

    z.Char(uses_dialog ? 'a' : 's', wxMOD_CONTROL); // save or save as
    wxTEST_DIALOG
        (wxYield()
        ,wxExpectModal<wxFileDialog>(file)
        );
    wxYield();

    LMI_ASSERT(wxFileExists(file));
    wxON_BLOCK_EXIT1(wxRemoveFile, file);

    z.Char('l', wxMOD_CONTROL); // close document
    wxYield();

    z.Char('o', wxMOD_CONTROL); // and open it again

    if (uses_dialog)
        {
        wxTEST_DIALOG
            (wxYield()
            ,wxExpectModal<wxFileDialog>(file)
            ,wxExpectDismissableModal<MvcController>(wxID_OK)
            );
        }
    else
        {
        wxTEST_DIALOG
            (wxYield()
            ,wxExpectModal<wxFileDialog>(file)
            );
        }
    wxYield();

    z.Char('l', wxMOD_CONTROL); // close it finally
    wxYield();
}

LMI_WX_TEST_CASE(create_open_census)
{
    do_test_create_open('c', "testfile.cns",  false);
}

LMI_WX_TEST_CASE(create_open_illustration)
{
    do_test_create_open('i', "testfile.ill",  true);
}

LMI_WX_TEST_CASE(create_open_database)
{
    do_test_create_open('d', "testfile.database", false);
}

LMI_WX_TEST_CASE(create_open_policy)
{
    do_test_create_open('p', "testfile.policy",  false);
}

LMI_WX_TEST_CASE(create_open_rounding)
{
    do_test_create_open('r', "testfile.rounding", false);
}

LMI_WX_TEST_CASE(create_open_strata)
{
    do_test_create_open('s', "testfile.strata", false);
}

LMI_WX_TEST_CASE(create_open_mec)
{
    do_test_create_open('m', "testfile.mec", true);
}

LMI_WX_TEST_CASE(create_open_gpt)
{
    do_test_create_open('g', "testfile.gpt", true);
}

LMI_WX_TEST_CASE(create_open_text)
{
    do_test_create_open('x', "testfile.txt", false);
}