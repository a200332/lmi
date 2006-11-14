// Configurable settings.
//
// Copyright (C) 2003, 2005, 2006 Gregory W. Chicares.
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

// $Id: configurable_settings.cpp,v 1.27 2006-11-14 04:35:27 chicares Exp $

#ifdef __BORLANDC__
#   include "pchfile.hpp"
#   pragma hdrstop
#endif // __BORLANDC__

#include "configurable_settings.hpp"

#include "alert.hpp"
#include "data_directory.hpp"     // AddDataDir()
#include "handle_exceptions.hpp"
#include "path_utility.hpp"
#include "platform_dependent.hpp" // access()
#include "xml_lmi.hpp"

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <stdexcept>

// TODO ?? Need unit tests.

namespace
{
    std::string const& configuration_filename()
    {
        static std::string s("configurable_settings.xml");
        return s;
    }

    /// Store the complete configuration-file path at startup, in case
    /// it's non-complete--as is typical msw usage.
    ///
    /// Look for the configuration file first where FHS would have it.
    /// To support non-FHS platforms, if it's not found there, then
    /// look in the data directory.
    ///
    /// TODO ?? CALCULATION_SUMMARY Should write access be checked
    /// here? What if the first file found is read-only, but the
    /// second is read-write?

    fs::path const& configuration_filepath()
    {
        static fs::path complete_path;
        if(!complete_path.empty())
            {
            return complete_path;
            }

        std::string filename = "/etc/opt/lmi/" + configuration_filename();
        if(access(filename.c_str(), R_OK))
            {
            filename = AddDataDir(configuration_filename());
            if(access(filename.c_str(), R_OK))
                {
                fatal_error()
                    << "No readable file '"
                    << configuration_filename()
                    << "' exists."
                    << LMI_FLUSH
                    ;
                }
            }
        validate_filepath(filename, "Configurable-settings file");
        complete_path = fs::system_complete(filename);
        return complete_path;
    }

    std::string const& default_calculation_summary_columns()
    {
        static std::string s
            ("Outlay"
            " AcctVal_Current"
            " CSVNet_Current"
            " EOYDeathBft_Current"
            );
        return s;
    }

    std::string const& xml_root_name()
    {
        static std::string s("configurable_settings");
        return s;
    }
} // Unnamed namespace.

configurable_settings::configurable_settings()
    :calculation_summary_columns_      (default_calculation_summary_columns())
    ,cgi_bin_log_filename_             ("cgi_bin.log"        )
    ,custom_input_filename_            ("custom.ini"         )
    ,custom_output_filename_           ("custom.out"         )
    ,default_input_filename_           ("etc/opt/default.ini")
    ,libraries_to_preload_             (""                   )
    ,offer_hobsons_choice_             (false                )
    ,print_directory_                  ("/var/opt/lmi/spool" )
    ,skin_filename_                    ("xml_notebook.xrc"   )
    ,spreadsheet_file_extension_       (".gnumeric"          )
    ,use_builtin_calculation_summary_  (false                )
    ,xml_schema_filename_              ("schema.xsd"         )
    ,xsl_fo_command_                   ("fo"                 )
    ,xsl_directory_                    (""                   )
    ,xslt_format_xml_filename_         ("format.xml"         )
    ,xslt_html_filename_               ("html.xsl"           )
    ,xslt_light_tab_delimited_filename_("cs_tsv.xsl"         )
    ,xslt_tab_delimited_filename_      ("tab_delimited.xsl"  )
{
    ascribe_members();
    load();
    // TODO ?? Something like this:
//    validate_directory(print_directory_, "Print directory");
    // might be appropriate here.
}

configurable_settings::~configurable_settings()
{}

configurable_settings& configurable_settings::instance()
{
    try
        {
        static configurable_settings z;
        return z;
        }
    catch(...)
        {
        report_exception();
        fatal_error() << "Instantiation failed." << LMI_FLUSH;
        throw std::logic_error("Unreachable"); // Silence compiler warning.
        }
}

void configurable_settings::ascribe_members()
{
    ascribe("calculation_summary_columns"      ,&configurable_settings::calculation_summary_columns_      );
    ascribe("cgi_bin_log_filename"             ,&configurable_settings::cgi_bin_log_filename_             );
    ascribe("custom_input_filename"            ,&configurable_settings::custom_input_filename_            );
    ascribe("custom_output_filename"           ,&configurable_settings::custom_output_filename_           );
    ascribe("default_input_filename"           ,&configurable_settings::default_input_filename_           );
    ascribe("libraries_to_preload"             ,&configurable_settings::libraries_to_preload_             );
    ascribe("offer_hobsons_choice"             ,&configurable_settings::offer_hobsons_choice_             );
    ascribe("print_directory"                  ,&configurable_settings::print_directory_                  );
    ascribe("skin_filename"                    ,&configurable_settings::skin_filename_                    );
    ascribe("spreadsheet_file_extension"       ,&configurable_settings::spreadsheet_file_extension_       );
    ascribe("use_builtin_calculation_summary"  ,&configurable_settings::use_builtin_calculation_summary_  );
    ascribe("xml_schema_filename"              ,&configurable_settings::xml_schema_filename_              );
    ascribe("xsl_fo_command"                   ,&configurable_settings::xsl_fo_command_                   );
    ascribe("xsl_directory"                    ,&configurable_settings::xsl_directory_                    );
    ascribe("xslt_format_xml_filename"         ,&configurable_settings::xslt_format_xml_filename_         );
    ascribe("xslt_html_filename"               ,&configurable_settings::xslt_html_filename_               );
    ascribe("xslt_light_tab_delimited_filename",&configurable_settings::xslt_light_tab_delimited_filename_);
    ascribe("xslt_tab_delimited_filename"      ,&configurable_settings::xslt_tab_delimited_filename_      );
}

void configurable_settings::load()
{
    // TODO ?? CALCULATION_SUMMARY In 'gnome-xml-branch', all settings
    // were first reinitialized from hardcoded defaults. What problem
    // would that solve?

    xml_lmi::dom_parser parser(configuration_filepath().string());
    xml_lmi::Element const& root = parser.root_node(xml_root_name());
    xml_lmi::ElementContainer const elements(xml_lmi::child_elements(root));
    typedef xml_lmi::ElementContainer::const_iterator eci;
    for(eci i = elements.begin(); i != elements.end(); ++i)
        {
        operator[]((*i)->get_name()) = xml_lmi::get_content(**i);
        }
}

// TODO ?? CALCULATION_SUMMARY This function must be rewritten.
void configurable_settings::save() const
{
// TODO ?? CALCULATION_SUMMARY If we want to do this up front, then
// it should be done in configuration_filepath(). It may be better,
// though, to detect failure at the bottom of the function than to
// predict it at the top.
#if 0
    if(access(configuration_filepath().c_str(), W_OK))
        {
        // User could choose to ignore this error and the only thing he risks
        // in that case is configurable settings not saved.
        fatal_error()
            << "Configuration settings file '"
            << configuration_filepath()
            << "' is not writeable."
            << LMI_FLUSH
            ;
        return; // unreachable code
        }
#endif // 0

    xml_lmi::dom_parser parser(configuration_filepath().string());
    xml_lmi::Document& document = const_cast<xml_lmi::Document&>(parser.document());
    xml_lmi::Element& root = const_cast<xml_lmi::Element&>(parser.root_node(xml_root_name()));

    std::vector<std::string>::const_iterator i;
    for(i = member_names().begin(); i != member_names().end(); ++i)
        {
        if("xml_schema_filename"               == *i) continue;
        if("xsl_directory"                     == *i) continue;
        if("xslt_format_xml_filename"          == *i) continue;
        if("xslt_html_filename"                == *i) continue;
        if("xslt_light_tab_delimited_filename" == *i) continue;
        if("xslt_tab_delimited_filename"       == *i) continue;

        std::string const& name = *i;
        std::string const content = operator[](name).str();
        xml_lmi::Element* element = NULL;

        xml_lmi::ElementContainer elements
            (xml_lmi::child_elements(root, name)
            );

        // Now write the new (or the same old) value to the last node with
        // the name. The motivation is to allow multiple nodes with the same
        // name in the file, while only the last one determines the value.
        // If no node exists we will need to create a new fresh node.
        if(elements.empty())
            {
            element = root.add_child(name);
            }
        else
            {
            // the element with the 'name' exist, we have to clear it up
            element = elements.back();
            xml_lmi::NodeContainer nodes = element->get_children();
            xml_lmi::NodeContainer::iterator it;
            for(it = nodes.begin(); it != nodes.end(); ++it)
                {
                element->remove_child(*it);
                }
            }

        if(!element)
            {
            fatal_error()
                << "Cannot create a new node '"
                << name
                << "' and write its value '"
                << content
                << "' into '"
                << configuration_filepath().string()
                << "'."
                << LMI_FLUSH
                ;
            }
        // finally put the new value into the element
        element->add_child_text(content);
        }
#if 0
    fs::ofstream ofs
        (configuration_filepath()
        ,std::ios_base::out | std::ios_base::trunc
        );
#else // not 0
    std::ofstream ofs
        (configuration_filepath().string().c_str()
        ,std::ios_base::out | std::ios_base::trunc
        );
#endif
    ofs << document;
    if(!ofs)
        {
        // User could choose to ignore this error and the only thing he risks
        // in that case is configurable settings not saved.
        warning()
            << "Configuration settings file '"
            << configuration_filepath().string()
            << "' is not writeable."
            << LMI_FLUSH
            ;
        }
}

/// Precondition: Argument is semantically valid; ultimately this will
/// be validated elsewhere.

void configurable_settings::calculation_summary_columns(std::string const& s)
{
    calculation_summary_columns_ = s;
}

void configurable_settings::use_builtin_calculation_summary(bool b)
{
    use_builtin_calculation_summary_ = b;
}

std::string const& configurable_settings::calculation_summary_columns() const
{
    return calculation_summary_columns_;
}

std::string const& configurable_settings::cgi_bin_log_filename() const
{
    return cgi_bin_log_filename_;
}

std::string const& configurable_settings::custom_input_filename() const
{
    return custom_input_filename_;
}

std::string const& configurable_settings::custom_output_filename() const
{
    return custom_output_filename_;
}

std::string const& configurable_settings::default_input_filename() const
{
    return default_input_filename_;
}

std::string const& configurable_settings::libraries_to_preload() const
{
    return libraries_to_preload_;
}

bool configurable_settings::offer_hobsons_choice() const
{
    return offer_hobsons_choice_;
}

std::string const& configurable_settings::print_directory() const
{
    return print_directory_;
}

std::string const& configurable_settings::skin_filename() const
{
    return skin_filename_;
}

std::string const& configurable_settings::spreadsheet_file_extension() const
{
    return spreadsheet_file_extension_;
}

bool configurable_settings::use_builtin_calculation_summary() const
{
    return use_builtin_calculation_summary_;
}

std::string const& configurable_settings::xml_schema_filename() const
{
    return xml_schema_filename_;
}

std::string const& configurable_settings::xsl_fo_command() const
{
    return xsl_fo_command_;
}

std::string const& configurable_settings::xsl_directory() const
{
    return xsl_directory_;
}

std::string const& configurable_settings::xslt_format_xml_filename() const
{
    return xslt_format_xml_filename_;
}

std::string const& configurable_settings::xslt_html_filename() const
{
    return xslt_html_filename_;
}

std::string const& configurable_settings::xslt_light_tab_delimited_filename() const
{
    return xslt_light_tab_delimited_filename_;
}

std::string const& configurable_settings::xslt_tab_delimited_filename() const
{
    return xslt_tab_delimited_filename_;
}

std::string const& effective_calculation_summary_columns()
{
    configurable_settings const& z = configurable_settings::instance();
    return
        z.use_builtin_calculation_summary()
        ? default_calculation_summary_columns()
        : z.calculation_summary_columns()
        ;
}

