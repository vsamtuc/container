#pragma once

#include <cxxtest/TestSuite.h>

#include "cdi.hh"

using namespace cdi;
using namespace cdi::utilities;
using namespace std;


class ResourceSuite : public CxxTest::TestSuite
{
public:

	void tearDown() {
		providence().clear();
	}

	void test_resource()
	{

		auto R = resource<string,void>(Default).id();
		TS_ASSERT_EQUALS(R.type(), type_index(typeid(resource<string,void>)));
		TS_ASSERT_EQUALS(R.quals(), qualifiers(Default));

		TS_ASSERT_EQUALS( (resource<string,void>(Default).id()), (resource<string,void>(Default).id()) );
	}


};

