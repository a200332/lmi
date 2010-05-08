// MVC Model for life-insurance illustrations--unit test.
//
// Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Gregory W. Chicares.
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
#endif // __BORLANDC__

// Facilities offered by all of these headers are tested here.
// Class product_database might appear not to belong, but it's
// intimately entwined with input.
#include "database.hpp"
#include "input.hpp"
#include "multiple_cell_document.hpp"
#include "single_cell_document.hpp"
#include "yare_input.hpp"
// End of headers tested here.

#include "assert_lmi.hpp"
#include "dbdict.hpp"
#include "dbnames.hpp"
#include "miscellany.hpp"
#include "test_tools.hpp"
#include "timer.hpp"
#include "xml_lmi.hpp"

#include <boost/bind.hpp>

#if defined BOOST_MSVC || defined __BORLANDC__
#   include <cfloat> // floating-point hardware control
#endif // defined BOOST_MSVC || defined __BORLANDC__
#include <cstdio> // std::remove()
#include <fstream>
#include <ios>
#if !defined LMI_USING_XML_SAVE_OPTION
#   include <sstream>
#endif // !defined LMI_USING_XML_SAVE_OPTION
#include <string>

class input_test
{
  public:
    static void test()
        {
        test_product_database();
        test_input_class();
        test_document_classes();
        assay_speed();
        }

  private:
    static void test_product_database();
    static void test_input_class();
    static void test_document_classes();
    static void assay_speed();

    template<typename DocumentClass>
    static void test_document_io
        (std::string const& original_filename
        ,std::string const& replica_filename
        ,char const*        file
        ,int                line
        ,bool               test_speed_only
        );

    static void mete_overhead();
    static void mete_read(xml::element& xml_data);
    static void mete_write();
    static void mete_cns_io();
    static void mete_ill_io();
};

void input_test::test_product_database()
{
    Input input;
    yare_input yi(input);
    product_database db(yi);
    std::vector<double> v;
    std::vector<double> w;

    // This vector's last element must be replicated.
    int dims_stat[database_entity::e_number_of_axes] = {1, 1, 1, 1, 1, 1, 10};
    double stat[10] = {0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.05};
    DBDictionary::instance().dictionary_[DB_StatVxQ] = database_entity
        (DB_StatVxQ
        ,database_entity::e_number_of_axes
        ,dims_stat
        ,stat
        );
    db.Query(v, DB_StatVxQ);
    w.assign(stat, stat + 10);
    w.insert(w.end(), db.length() - w.size(), w.back());
    BOOST_TEST(v == w);

    // This vector must be truncated.
    int dims_tax[database_entity::e_number_of_axes] = {1, 1, 1, 1, 1, 1, 100};
    double tax[100] =
        {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        ,0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1
        ,0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2
        ,0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3
        ,0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4, 0.4
        ,0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5
        ,0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6
        ,0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7, 0.7
        ,0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8
        ,0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9
        };
    DBDictionary::instance().dictionary_[DB_TaxVxQ] = database_entity
        (DB_TaxVxQ
        ,database_entity::e_number_of_axes
        ,dims_tax
        ,tax
        );
    db.Query(v, DB_TaxVxQ);
    w.assign(tax, tax + db.length());
    BOOST_TEST(v == w);

    // Scalar access is forbidden when entity varies by duration.
    BOOST_TEST_THROW
        (db.Query(DB_StatVxQ)
        ,std::runtime_error
        ,"Assertion '1 == v.GetLength()' failed."
        );

    std::cout
        << "\n  Database speed tests..."
        << "\n  initialize()      : " << TimeAnAliquot(boost::bind(&product_database::initialize,       &db))
        << "\n  Query(vector)     : " << TimeAnAliquot(boost::bind(&product_database::Query,            &db, v, DB_EndtAge))
        << "\n  Query(scalar)     : " << TimeAnAliquot(boost::bind(&product_database::Query,            &db, DB_EndtAge))
        << "\n  GetEntry()        : " << TimeAnAliquot(boost::bind(&product_database::GetEntry,         &db, DB_EndtAge))
        << '\n'
        ;

    BOOST_TEST_THROW
        (db.GetEntry(-1)
        ,std::runtime_error
        ,"Assertion 'DB_FIRST <= k && k < DB_LAST' failed."
        );

    database_entity const maturity = db.GetEntry(DB_EndtAge);

    // Maturity age must not vary by duration.
    DBDictionary::instance().dictionary_[DB_EndtAge] = database_entity
        (DB_StatVxQ
        ,database_entity::e_number_of_axes
        ,dims_stat
        ,stat
        );
    BOOST_TEST_THROW
        (db.initialize();
        ,std::runtime_error
        ,"Assertion '1 == v.GetLength()' failed."
        );
    DBDictionary::instance().dictionary_[DB_EndtAge] = maturity;

    DBDictionary::instance().dictionary_[1 + DB_LAST] = maturity;
    DBDictionary::instance().dictionary_.erase(DB_EndtAge);
    BOOST_TEST_THROW
        (db.GetEntry(DB_EndtAge)
        ,std::runtime_error
        ,"Assertion 'i != d.end()' failed."
        );
}

void input_test::test_input_class()
{
    // Test << and >> operators.
    Input original;
    Input replica;

    std::ofstream os0("eraseme0.xml", ios_out_trunc_binary());
    BOOST_TEST(os0.good());

    original.AgentName           = "Herbert Cassidy";
    original.AgentPhone          = "123-4567";
    original.InsuredName         = "Full Name";
    original.Address             = "address";
    original.City                = "city";
//    original.FundAllocations     = "0.4 0.3 0.2 0.1";
    original.SeparateAccountRate = "0.03125";

    original.RealizeAllSequenceInput();
/*
    original.FundAllocs[0]       = 1.0;
    original.SeparateAccountRateRealized_[0] = .01234567890123456789;
    original.SeparateAccountRateRealized_[1] = .12345678901234567890;
    original.SeparateAccountRateRealized_[2] = .23456789012345678901;
    original.SeparateAccountRateRealized_[3] = .34567890123456789012;
    original.SeparateAccountRateRealized_[4] = .45678901234567890123;
    original.SeparateAccountRateRealized_[5] = .56789012345678901234;
    original.SeparateAccountRateRealized_[6] = .67890123456789012345;
    original.SeparateAccountRateRealized_[7] = .78901234567890123456;
    original.SeparateAccountRateRealized_[8] = .89012345678901234567;
    original.SeparateAccountRateRealized_[9] = .90123456789012345678;
*/

    xml_lmi::xml_document xml_document0("root");
    xml::element& xml_root0 = xml_document0.root_node();
    xml_root0 << original;
    os0 << xml_document0;
    os0.close();

    xml::node::const_iterator i = xml_root0.begin();
    LMI_ASSERT(!i->is_text());
    xml::element const& xml_node = *i;

    xml_node >> replica;
    std::ofstream os1("eraseme1.xml", ios_out_trunc_binary());
    BOOST_TEST(os1.good());

    xml_lmi::xml_document xml_document1("root");
    xml::element& xml_root1 = xml_document1.root_node();
    xml_root1 << replica;
    os1 << xml_document1;
    os1.close();

    BOOST_TEST(original == replica);
    bool okay = files_are_identical("eraseme0.xml", "eraseme1.xml");
    BOOST_TEST(okay);
    // Leave the files for analysis if they didn't match.
    if(okay)
        {
        BOOST_TEST(0 == std::remove("eraseme0.xml"));
        BOOST_TEST(0 == std::remove("eraseme1.xml"));
        }

    BOOST_TEST(0.03125 == original.SeparateAccountRateRealized_[0]);
    BOOST_TEST(replica.SeparateAccountRateRealized_.empty());
    replica.RealizeAllSequenceInput();
    BOOST_TEST(!replica.SeparateAccountRateRealized_.empty());
    BOOST_TEST(0.03125 == replica.SeparateAccountRateRealized_[0]);

/* TODO ?? The code this tests is defective--fix it someday.
    BOOST_TEST(0.4 == original.FundAllocs[0]);
    BOOST_TEST(0.4 == replica.FundAllocs[0]);
std::cout << "original.FundAllocs[0] is " << original.FundAllocs[0] << '\n';
std::cout << "replica.FundAllocs[0] is " << replica.FundAllocs[0] << '\n';

std::cout << "original.FundAllocs.size() is " << original.FundAllocs.size() << '\n';
std::cout << "replica.FundAllocs.size() is " << replica.FundAllocs.size() << '\n';
*/

    BOOST_TEST(0 == original.InforceYear);
    original["InforceYear"] = std::string("3");
    BOOST_TEST(3 == original.InforceYear);

// Fails--need to change initialization.
    BOOST_TEST(45 == original.IssueAge);
    original["IssueAge"] = std::string("57");
    BOOST_TEST(57 == original.IssueAge);

    // Test copy constructor.
    Input copy0(original);
    BOOST_TEST(original == copy0);
    copy0["InsuredName"] = "Claude Proulx";
    BOOST_TEST(std::string("Claude Proulx") == copy0   .InsuredName.value());
    BOOST_TEST(std::string("Full Name")     == original.InsuredName.value());

    // Test assignment operator.
    Input copy1;
    copy1 = original;
    BOOST_TEST(original == copy1);
    copy1["InsuredName"] = "Angela";
    BOOST_TEST(std::string("Angela")    == copy1   .InsuredName.value());
    BOOST_TEST(std::string("Full Name") == original.InsuredName.value());

    // For now at least, just test that this compiles and runs.
    yare_input y(original);
}

void input_test::test_document_classes()
{
    // TODO ?? Errors reported here with como seem to stem from
    // innocuous differences in the way floating-point exponents are
    // formatted. For instance, 'sample.ill', generated by MinGW
    // gcc-3.4.4 (due to a defect in the msvc runtime library),
    // contains
    //   <TotalSpecifiedAmount>1e+006</TotalSpecifiedAmount>
    // where a 'replica.ill' generated by como has
    //   <TotalSpecifiedAmount>1e+06</TotalSpecifiedAmount>
    // . They're equivalent, but como is correct: C99 7.19.6.1 says
    // "the exponent always contains at least two digits, and only as
    // many more digits as necessary to represent the exponent". This
    // spurious error report should be suppressed in a way that
    // doesn't block any actual error that may later develop.

    typedef multiple_cell_document M;
    test_document_io<M>("sample.cns", "replica.cns", __FILE__, __LINE__, false);
    typedef single_cell_document S;
    test_document_io<S>("sample.ill", "replica.ill", __FILE__, __LINE__, false);
}

void input_test::assay_speed()
{
    Input raw_data;
    xml_lmi::xml_document document("root");
    xml::element& root = document.root_node();
    root << raw_data;

    xml::node::const_iterator i = root.begin();
    LMI_ASSERT(!i->is_text());
    xml::element const& e = *i;

    std::cout
        << "\n  Input speed tests..."
        << "\n  Overhead: " << TimeAnAliquot(mete_overhead            )
        << "\n  Read    : " << TimeAnAliquot(boost::bind(mete_read, e))
        << "\n  Write   : " << TimeAnAliquot(mete_write               )
        << "\n  'cns' io: " << TimeAnAliquot(mete_cns_io              )
        << "\n  'ill' io: " << TimeAnAliquot(mete_ill_io              )
        << '\n'
        ;
}

template<typename DocumentClass>
void input_test::test_document_io
    (std::string const& original_filename
    ,std::string const& replica_filename
    ,char const*        file
    ,int                line
    ,bool               test_speed_only
    )
{
    DocumentClass document(original_filename);
    std::ofstream ofs(replica_filename.c_str(), ios_out_trunc_binary());
#if defined LMI_USING_XML_SAVE_OPTION
    document.write(ofs);
#else  // !defined LMI_USING_XML_SAVE_OPTION
// SOMEDAY !! XMLWRAPP !! Update 'xmlwrapp' to write empty elements as such:
//   http://mail.gnome.org/archives/xml/2007-May/msg00007.html
//   http://mail.gnome.org/archives/xml/2006-July/msg00048.html

    std::ostringstream oss0;
    document.write(oss0);
    std::string const s(oss0.str());

    std::istringstream iss(s);
    std::ostringstream oss1;
    for(;EOF != iss.peek();)
        {
        std::string line;
        std::getline(iss, line);
        std::string::size_type z = line.find("><");
        if(std::string::npos != z)
            {
            line.erase(z);
            line += "/>";
            }
        oss1 << line << '\n';
        }

    ofs << oss1.str();
#endif // !defined LMI_USING_XML_SAVE_OPTION
    if(test_speed_only)
        {
        return;
        }
    ofs.close();
    bool okay = files_are_identical(original_filename, replica_filename);
    INVOKE_BOOST_TEST(okay, file, line);
    // Leave the file for analysis if it didn't match.
    if(okay)
        {
        INVOKE_BOOST_TEST
            (0 == std::remove(replica_filename.c_str())
            ,file
            ,line
            );
        }
}

void input_test::mete_overhead()
{
    static Input raw_data;
    xml_lmi::xml_document document("root");
    xml::element& root = document.root_node();
    stifle_warning_for_unused_value(root);
}

void input_test::mete_read(xml::element& xml_data)
{
    static Input raw_data;
    xml_data >> raw_data;
    // Realizing sequence input might be done separately, but it must
    // somehow be done.
    raw_data.RealizeAllSequenceInput();
}

void input_test::mete_write()
{
    static Input raw_data;
    xml_lmi::xml_document document("root");
    xml::element& root = document.root_node();
    root << raw_data;
}

void input_test::mete_cns_io()
{
    typedef multiple_cell_document M;
    test_document_io<M>("sample.cns", "replica.cns", __FILE__, __LINE__, true);
}

void input_test::mete_ill_io()
{
    typedef single_cell_document S;
    test_document_io<S>("sample.ill", "replica.ill", __FILE__, __LINE__, true);
}

int test_main(int, char*[])
{
    input_test::test();
    return EXIT_SUCCESS;
}

