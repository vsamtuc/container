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

	void test_resource_constructor()
	{
		auto R = resource<string,void>(Default).id();
		TS_ASSERT_EQUALS(R.type(), type_index(typeid(resource<string,void>)));
		TS_ASSERT_EQUALS(R.quals(), qualifiers(Default));

		TS_ASSERT_EQUALS( (resource<string,void>(Default).id()), (resource<string,void>(Default).id()) );
	}

	void test_resource_return()
	{
		resource<string> r({});
		provide(r, [](){ return "hello world"; });

		auto s = r.get();
		s.append(" and bye");

		TS_ASSERT_DIFFERS(s, r.get());
	}

	void test_resource_intset()
	{
		// define as the decayed type of a func
		resource< std::decay_t<const set<int>> > s({});
		s	.provide([]() { return set<int>(); })
			.initialize([](auto& self){
				for(int i=1; i<=3; i++) {
					self.insert(i);
				}
			});

		auto x = s.get();
		TS_ASSERT_EQUALS(x, (set<int>{1,2,3}));
		x.insert(5);
		TS_ASSERT_EQUALS(s.get(), (set<int>{1,2,3}));
	}

};
