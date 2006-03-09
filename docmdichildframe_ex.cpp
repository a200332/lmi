// Customize implementation details of library class wxDocMDIChildFrame.
//
// Copyright (C) 2004, 2005, 2006 Gregory W. Chicares.
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

// $Id: docmdichildframe_ex.cpp,v 1.5 2006-03-09 01:58:18 chicares Exp $

#include "docmdichildframe_ex.hpp"

#include <wx/menu.h>

#include <stdexcept>

IMPLEMENT_CLASS(DocMDIChildFrameEx, wxDocMDIChildFrame)

BEGIN_EVENT_TABLE(DocMDIChildFrameEx, wxDocMDIChildFrame)
    EVT_ACTIVATE(DocMDIChildFrameEx::UponActivate)
    EVT_MENU_HIGHLIGHT_ALL(DocMDIChildFrameEx::UponMenuHighlight)
END_EVENT_TABLE()

DocMDIChildFrameEx::DocMDIChildFrameEx
    (wxDocument*       doc
    ,wxView*           view
    ,wxMDIParentFrame* parent
    ,wxWindowID        id
    ,wxString   const& title
    ,wxPoint    const& pos
    ,wxSize     const& size
    ,long int          style
    ,wxString   const& name
    )
    :wxDocMDIChildFrame(doc, view, parent, id, title, pos, size, style, name)
    ,status_bar_sought_from_menu_highlight_handler_(false)
{
}

DocMDIChildFrameEx::~DocMDIChildFrameEx()
{
}

wxStatusBar* DocMDIChildFrameEx::GetStatusBar() const
{
    if(!status_bar_sought_from_menu_highlight_handler_)
        {
        return wxDocMDIChildFrame::GetStatusBar();
        }

    wxStatusBar* status_bar = wxDocMDIChildFrame::GetStatusBar();
    if(status_bar)
        {
        return status_bar;
        }

    wxFrame* parent_frame = dynamic_cast<wxFrame*>(GetParent());
    if(parent_frame)
        {
        return parent_frame->GetStatusBar();
        }

    return 0;
}

/// This function merely augments DocMDIChildFrameEx::OnActivate(),
/// so it calls Skip() at the end.

void DocMDIChildFrameEx::UponActivate(wxActivateEvent& event)
{
    SetMdiWindowMenu();
    event.Skip();
}

/// This augments wxDocMDIChildFrame::OnMenuHighlight(), but isn't a
/// complete replacement. It calls that base-class function explicitly
/// because Skip() wouldn't work here.

void DocMDIChildFrameEx::UponMenuHighlight(wxMenuEvent& event)
{
    try
        {
        status_bar_sought_from_menu_highlight_handler_ = true;
        if(GetStatusBar())
            {
            wxDocMDIChildFrame::OnMenuHighlight(event);
            }
        status_bar_sought_from_menu_highlight_handler_ = false;
        }
    catch(...)
        {
        status_bar_sought_from_menu_highlight_handler_ = false;
        throw;
        }
}

// FSF !! Expunge this when we deprecate support for older versions
// than wx-2.5.4 .

#if wxCHECK_VERSION(2,5,4) || !defined LMI_MSW
void DocMDIChildFrameEx::SetMdiWindowMenu() const {}
#else // wx-msw prior to version 2.5.4 .
#   include <wx/msw/wrapwin.h>
void DocMDIChildFrameEx::SetMdiWindowMenu() const
{
    wxMDIParentFrame* parent_frame = dynamic_cast<wxMDIParentFrame*>(GetParent());
    if(!parent_frame)
        {
        throw std::runtime_error("MDI child frame has no parent.");
        }

    wxMDIClientWindow* client_window = parent_frame->GetClientWindow();
    if(!client_window)
        {
        throw std::runtime_error("Child frame's parent has no client window.");
        }
    HWND client_handle = (HWND)client_window->GetHandle();

    wxMenuBar* menu_bar = GetMenuBar();
    if(!menu_bar)
        {
        throw std::runtime_error("Child frame has no menubar.");
        return;
        }

    int window_menu_index = menu_bar->FindMenu("Window");
    if(wxNOT_FOUND == window_menu_index)
        {
        throw std::runtime_error("No 'Window' menu found.");
        return;
        }

    wxMenu* window_menu = menu_bar->GetMenu(window_menu_index);
    HMENU window_menu_handle = (HMENU)window_menu->GetHMenu();

    ::SendMessage(client_handle, WM_MDISETMENU, 0, (LPARAM)window_menu_handle);
    ::DrawMenuBar(client_handle);
}
#endif // wx-msw prior to version 2.5.4 .

