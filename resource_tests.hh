#pragma once

#include <cxxtest/TestSuite.h>
#include <jsoncpp/json/json.h>

#include <memory>
#include <type_traits>
#include <functional>
#include <unordered_set>

#include "resource.hh"
#include "utilities.hh"

using namespace cdi;
using namespace cdi::utilities;
using namespace std;


class ResourceSuite : public CxxTest::TestSuite
{
public:


	void test_resource()
	{

		auto R = resource<string,void>({ Default }).id();
		TS_ASSERT_EQUALS(R.type(), type_index(typeid(resource<string,void>)));
		TS_ASSERT_EQUALS(R.quals(), qualifiers(Default));

		TS_ASSERT_EQUALS( (resource<string,void>({Default}).id()), (resource<string,void>(Default).id()) );
	}


};

