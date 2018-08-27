#pragma once

#include <cxxtest/TestSuite.h>

#include "cdi.hh"

using namespace cdi;
using namespace cdi::utilities;
using namespace std;


class ContainerSuite : public CxxTest::TestSuite
{
public:

	void tearDown() {
		providence().clear();
	}


	void test_get_declared()
	{
		resource<int> r({});

		TS_ASSERT_EQUALS(providence().get_declared(r), nullptr);

		r.declare();
		TS_ASSERT_EQUALS(providence().get_declared(r), r.manager());

		TS_ASSERT_EQUALS(providence().resource_managers().size(),1);
	}

};
