// Life insurance illustrations: command-line interface.
//
// Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007 Gregory W. Chicares.
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

// $Id: main_cli.cpp,v 1.39 2007-06-03 04:31:38 chicares Exp $

#ifdef __BORLANDC__
#   include "pchfile.hpp"
#   pragma hdrstop
#endif // __BORLANDC__

#include "alert.hpp"
#include "argv0.hpp"
#include "assert_lmi.hpp"
#include "calculate.hpp"
#include "custom_io_0.hpp"
#include "dev_null_stream_buffer.hpp"
#include "getopt.hpp"
#include "global_settings.hpp"
#include "group_values.hpp"
#include "handle_exceptions.hpp"
#include "illustrator.hpp"
#include "inputillus.hpp"
#include "ledger.hpp"
#include "ledger_text_formats.hpp"
#include "ledger_variant.hpp"
#include "ledgervalues.hpp"
#include "license.hpp"
#include "main_common.hpp"
#include "mc_enum.hpp"
#include "mc_enum_types.hpp"
#include "miscellany.hpp"
#include "multiple_cell_document.hpp"
#include "path_utility.hpp"
#include "so_attributes.hpp"
#include "timer.hpp"
#include "value_cast.hpp"

#include <boost/bind.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef> // NULL, std::size_t
#include <cstdio>  // std::printf()
#include <ios>
#include <iostream>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

// INELEGANT !! Prototype specified explicitly because the production
// and antediluvian branches define it identically but in different
// and incompatible headers.
void LMI_SO print_databases();

//============================================================================
void RegressionTestOneCensusFile(fs::directory_iterator i)
{
    std::cout << "Regression testing: " << i->string() << std::endl;
    multiple_cell_document doc(i->string());
    run_census::assert_consistency(doc.case_parms()[0], doc.cell_parms()[0]);
    run_census()(*i, mce_emit_test_data, doc.cell_parms());
}

//============================================================================
void RegressionTestOneIniFile(fs::directory_iterator i)
{
    std::cout << "Regression testing: " << i->string() << std::endl;
    IllusVal IV;
    IllusInputParms IP(false);
    SetSpecialInput(IP, i->string().c_str());
    IV.Run(&IP);
    fs::path out_file = fs::change_extension(*i, ".test0");
    PrintFormSpecial(IV.ledger(), out_file.string().c_str());
}

//============================================================================
void RegressionTest()
{
    global_settings::instance().set_regression_testing(true);
    fs::path test_dir(global_settings::instance().regression_test_directory());
    fs::directory_iterator i(test_dir);
    fs::directory_iterator end_i;
    for(; i != end_i; ++i)
        {
        try
            {
            if(is_directory(*i))
                {
                continue;
                }
            else if(".cns" == fs::extension(*i))
                {
                RegressionTestOneCensusFile(i);
                }
            else if(".ini" == fs::extension(*i))
                {
                RegressionTestOneIniFile(i);
                }
            else
                {
                // Do nothing. The test directory typically contains
                // many files of other types that are deliberately
                // ignored.
                }
            }
        catch(...)
            {
            report_exception();
            }
        }
}

//============================================================================
void SelfTest()
{
    IllusVal IV;
    IllusInputParms IP;
    IP["Gender"           ] = "Male";
    IP["Smoking"          ] = "Nonsmoker";
    IP["UnderwritingClass"] = "Standard";
    IP.GenAcctIntRate = "0.06";
    IP.propagate_changes_to_base_and_finalize();

    IP.EePremium.assign(IP.SpecAmt.size(), r_pmt(20000.0));
    IP.SpecAmt.assign(IP.SpecAmt.size(), r_spec_amt(1000000.0));
    double expected_value = 0.0;
    double observed_value = 0.0;

    IP["SolveType"] = "SolveNone";

    expected_value = 6305652.52;
    IV.Run(&IP);
    observed_value = IV.ledger().GetCurrFull().AcctVal.back();
    if(.005 < std::fabs(expected_value - observed_value))
        {
        warning()
            << "Value should be "
            << value_cast<std::string>(expected_value)
            << ", but is "
            << value_cast<std::string>(observed_value)
            << " ."
            << LMI_FLUSH
            ;
        }

    IP["SolveType"] = "SolveSpecAmt";
    expected_value = 1884064;
    observed_value = IV.Run(&IP);
    if(.005 < std::fabs(expected_value - observed_value))
        {
        warning()
            << "Value should be "
            << value_cast<std::string>(expected_value)
            << ", but is "
            << value_cast<std::string>(observed_value)
            << " .\n"
            << LMI_FLUSH
            ;
        }

    IP["SolveType"] = "SolveEePrem";
    expected_value = 10673.51;
    observed_value = IV.Run(&IP);
    if(.005 < std::fabs(expected_value - observed_value))
        {
        warning()
            << "Value should be "
            << value_cast<std::string>(expected_value)
            << ", but is "
            << value_cast<std::string>(observed_value)
            << " .\n"
            << LMI_FLUSH
            ;
        }

// TODO ?? This test segfaults:
//   /opt/lmi/bin/lmi_cli_shared -a -d /opt/lmi/data --selftest
// It serves only to test function object RunCensus, which is slated
// for removal, but cannot be removed until the segfault is tracked
// down and, if not due to a defect in RunCensus, fixed.
    multiple_cell_document census;
    std::vector<IllusInputParms> input_vector = census.cell_parms();
    input_vector.push_back(input_vector.front());
    static dev_null_stream_buffer<char> no_output;
    std::ostream dev_null_os(&no_output);
    RunCensus runner(dev_null_os);
    runner(input_vector);

std::cout << "? " << runner.XXXComposite.GetCurrFull().AcctVal.front() << std::endl;
std::cout << "? " << runner.XXXComposite.GetCurrFull().AcctVal.back() << std::endl;

std::cout << "? " << runner.XXXComposite.GetCurrFull().AcctVal[54] << std::endl;
std::cout << "? " << runner.XXXComposite.GetLedgerInvariant().GetInforceLives().front() << std::endl;
std::cout << "? " << runner.XXXComposite.GetLedgerInvariant().GetInforceLives().size() << std::endl;

    observed_value = runner.XXXComposite.GetLedgerInvariant().GrossPmt[0];
    expected_value = 12819.32;
    if(.005 < std::fabs(expected_value - observed_value))
        {
        warning()
            << "Value should be "
            << value_cast<std::string>(expected_value)
            << ", but is "
            << value_cast<std::string>(observed_value)
            << " .\n"
            << LMI_FLUSH
            ;
        }
// End of test that segfaults.

    std::cout
        << "Test solve speed: "
        << TimeAnAliquot(boost::bind(&IllusVal::Run, &IV, &IP), 5)
        << '\n'
        ;
}

//============================================================================
void Profile()
{
    for(int j = 0; j < 10; ++j)
        {
        SelfTest();
        }
}

//============================================================================
void process_command_line(int argc, char* argv[])
{
    int c;
    int digit_optind = 0;
    int this_option_optind = 1;
    int option_index = 0;
//    static char const* vfile[] = {"file", "archive", 0};
//    static char const* vlist[] = {"one", "two", "three", 0};
//    static char const* vopt[] = {"optional", "alternative", 0};
    static struct Option long_options[] =
      {
        {"ash_nazg"  ,NO_ARG   ,0 ,001 ,0 ,"ash nazg durbatulūk"},
        {"ash_naz"   ,NO_ARG   ,0 ,003 ,0 ,"fraud"},
        {"mellon"    ,NO_ARG   ,0 ,002 ,0 ,"pedo mellon a minno"},
        {"mello"     ,NO_ARG   ,0 ,003 ,0 ,"fraud"},
        {"help"      ,NO_ARG   ,0 ,'h' ,0 ,"display this help and exit"},
        {"license"   ,NO_ARG   ,0 ,'l' ,0 ,"display license and exit"},
        {"accept"    ,NO_ARG   ,0 ,'a' ,0 ,"accept license (-l to display)"},
        {"selftest"  ,NO_ARG   ,0 ,'s' ,0 ,"perform self test and exit"},
        {"profile"   ,NO_ARG   ,0 ,'o' ,0 ,"set up for profiling and exit"},
        {"emit"      ,REQD_ARG ,0 ,'e' ,0 ,"choose what output to emit"},
        {"illfile"   ,REQD_ARG ,0 ,'i' ,0 ,"run illustration"},
        {"cnsfile"   ,REQD_ARG ,0 ,'c' ,0 ,"run census"},
        {"data_path" ,REQD_ARG ,0 ,'d' ,0 ,"path to data files"},
        {"print_db"  ,NO_ARG   ,0 ,'p', 0, "print product databases"},
        {"regress"   ,NO_ARG   ,0 ,'r' ,0 ,"run regression test"},
        {"test_path" ,REQD_ARG ,0 ,'t' ,0 ,"path to test files"},
//        {"list"    ,LIST_ARG, 0,   0, 0    , "list"},
//        {"opt"     ,OPT_ARG,  0,   0, 0    , "optional"},
//        {"alt"     ,ALT_ARG,  0,   0, 0    , "alternative"},
//        {"vfile"   ,REQD_ARG, 0,   0, vfile, "file type"},
//        {"vlist"   ,LIST_ARG, 0,   0, vlist, "list type"},
//        {"vopt"    ,OPT_ARG,  0,   0, vopt , "optional"},
//        {"valt"    ,ALT_ARG,  0,   0, vopt , "alternative"},
        {0         ,NO_ARG,   0,   0, 0    , ""}
      };
    bool license_accepted    = false;
    bool show_license        = false;
    bool show_help           = false;
    bool run_regression_test = false;
    bool run_selftest        = false;
    bool run_profile         = false;
    bool print_all_databases = false;
    bool run_illustration    = false;
    bool run_census          = false;

    e_emission emission(mce_emit_nothing);
    // Suppress enumerators for options not fully implemented.
    emission.allow(emission.ordinal("emit_pdf_file"      ), false);
    emission.allow(emission.ordinal("emit_pdf_to_printer"), false);
    emission.allow(emission.ordinal("emit_custom_0"      ), false);

    std::vector<std::string> ill_names;
    std::vector<std::string> cns_names;

    GetOpt getopt_long
        (argc
        ,argv
        ,""
        ,long_options
        ,&option_index
        ,1
        );

    while(EOF != (c = getopt_long()))
        {
        switch(c)
            {
            case 0:
                {
                char const* current_option = long_options[option_index].name;
                std::printf("option %s", current_option);
                if(getopt_long.optarg)
                    {
                    std::printf(" with arg %s", getopt_long.optarg);
                    }
                std::printf("\n");
                }
                break;

            case 001:
                {
                global_settings::instance().set_ash_nazg(true);
                }
                break;

            case 002:
                {
                global_settings::instance().set_mellon(true);
                }
                break;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                {
                if(digit_optind != 0 && digit_optind != this_option_optind)
                    {
                    std::printf("digits occur in two different argv-elements.\n");
                    }
                digit_optind = this_option_optind;
                std::printf("option %c\n", c);
                }
                break;

            case 'a':
                {
                license_accepted = true;
                }
                break;

            case 'b':
                {
                std::printf("option b\n");
                }
                break;

            case 'c':
                {
                run_census = true;
                cns_names.push_back(getopt_long.optarg);
                }
                break;

            case 'd':
                {
                global_settings::instance().set_data_directory
                    (getopt_long.optarg
                    );
                }
                break;

            case 'h':
                {
                show_help = true;
                }
                break;

            case 'i':
                {
                run_illustration = true;
                ill_names.push_back(getopt_long.optarg);
                }
                break;

            case 'l':
                {
                show_license = true;
                }
                break;

            case 'o':
                {
                run_profile = true;
                }
                break;

            case 'e':
                {
                int emission_suboptions = mce_emit_nothing;

                LMI_ASSERT(NULL != getopt_long.optarg);
                std::string const s(getopt_long.optarg);
                std::istringstream iss(s);
                for(;EOF != iss.peek();)
                    {
                    std::string token;
                    std::getline(iss, token, ',');
                    if(!token.empty())
                        {
                        try
                            {
                            e_emission z(token);
                            if(!emission.is_allowed(emission.ordinal(token)))
                                {
                                throw std::runtime_error(token);
                                }
                            emission_suboptions |= z.value();
                            }
                        catch(std::runtime_error const&)
                            {
                            std::cerr
                                << argv[0]
                                << ": unrecognized '--emit' suboption "
                                << "'" << token << "'"
                                << std::endl
                                ;
                            }
                        }
                    }
                emission = mcenum_emission(emission_suboptions);
                }
                break;

            case 'p':
                {
                print_all_databases = true;
                }
                break;

            case 'r':
                {
                run_regression_test = true;
                }
                break;

            case 's':
                {
                run_selftest = true;
                }
                break;

            case 't':
                {
                global_settings::instance().set_regression_test_directory
                    (getopt_long.optarg
                    );
                }
                break;

            case '?':
                {
                break;
                }

            default:
                {
                std::printf("? getopt returned character code 0%o ?\n", c);
                }
            }
        }

    if((c = getopt_long.optind) < argc)
        {
        std::printf("non-option ARGV-elements: ");
        while(c < argc)
            {
            std::printf("%s ", argv[c++]);
            }
        std::printf("\n");
        }

    if(!license_accepted)
        {
        std::cerr << license_notices_as_text() << "\n\n";
        }

    if(show_license)
        {
        std::cerr << license_as_text() << "\n\n";
        return;
        }

    if(show_help)
        {
        getopt_long.usage();

        std::cout << "Suboptions for '--emit':\n";
        for(std::size_t j = 0; j < emission.cardinality(); ++j)
            {
            if(emission.is_allowed(j))
                {
                std::cout << "  " << emission.str(j) << '\n';
                }
            }

        return;
        }

    if(run_selftest)
        {
        SelfTest();
        return;
        }

    if(run_regression_test)
        {
        RegressionTest();
        return;
        }

    if(run_profile)
        {
        Profile();
        return;
        }

    if(print_all_databases)
        {
        print_databases();
        return;
        }

    if(run_illustration)
        {
        RunIllustrationFromFile run_functor = std::for_each
            (ill_names.begin()
            ,ill_names.end()
            ,RunIllustrationFromFile(std::cout)
            );
        std::cerr
            << "File"
            << ((1U < ill_names.size()) ? "s" : "")
            << ":\n"
            ;
        std::copy
            (ill_names.begin()
            ,ill_names.end()
            ,std::ostream_iterator<std::string>(std::cerr, "\n")
            );
        std::cerr
            << "    Input:        "
            << Timer::elapsed_msec_str(run_functor.time_for_input)
            << '\n'
            ;
        std::cerr
            << "    Calculations: "
            << Timer::elapsed_msec_str(run_functor.time_for_calculations)
            << '\n'
            ;
        std::cerr
            << "    Output:       "
            << Timer::elapsed_msec_str(run_functor.time_for_output)
            << '\n'
            ;
        }

    if(run_census)
        {
        std::for_each
            (cns_names.begin()
            ,cns_names.end()
            ,illustrator(emission.value())
            );
        }
}

//============================================================================
int try_main(int argc, char* argv[])
{
    initialize_filesystem();
    process_command_line(argc, argv);
    return EXIT_SUCCESS;
}

