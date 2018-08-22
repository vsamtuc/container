#pragma once

#include <cxxtest/TestSuite.h>
#include <jsoncpp/json/json.h>

#include <memory>
#include <type_traits>
#include <functional>
#include <unordered_set>
#include <iomanip>

#include "utilities.hh"
#include "resource.hh"

using namespace cdi;

using std::endl;
using std::cout;

using namespace cdi::utilities;

class UniqueStorageSuite : public CxxTest::TestSuite
{
public:

	void test_unique_storage()
	{
		using std::string;

		// init a storage
		unique_storage<string> st;
		using pointer = typename unique_storage<string>::shared_pointer;

		{
			pointer p1 = st.allocate("foo");
			pointer p2 = st.allocate("foo");

			// other constructors
			pointer p3 = st.allocate(string("foo"));
			string sref("foo");
			pointer p4 = st.allocate(sref);


			TS_ASSERT(p1 == p2);
			TS_ASSERT_EQUALS( *p1, string("foo"));
			TS_ASSERT_EQUALS(st.size(), 1);
		}
		TS_ASSERT_EQUALS(st.size(), 0);
	}


	void test_str_builder() {
		using M = str_builder;
		using std::endl;

		string s = M()<< "1+1=" << 1+1;

		TS_ASSERT_EQUALS(s,"1+1=2");

		TS_ASSERT_EQUALS( M()<< std::setfill('*') << std::setw(10) << 33 << M::end, "********33");

		try {
			throw instantiation_error(M() << "A silly result 1+1="<< 1+1 << M::end);
		} catch(instantiation_error e) {
			TS_ASSERT_EQUALS(e.what(), "A silly result 1+1=2");
		}
	}


};

