#pragma once

#include <cxxtest/TestSuite.h>

#include "cdi.hh"

using namespace cdi;
using namespace cdi::utilities;
using namespace std;


template <typename Instance>
struct minimal_resource
{
	//typedef minimal_resource<Instance> resource_type;
	typedef Instance instance_type;
	typedef Instance return_type;

	const scope_api& scope() const {
		static scope_proxy<GlobalScope> api;
		return api;
	}

	operator resourceid () const {
		return resourceid(typeid(minimal_resource<Instance>), {});
	}
};


class ResourceSuite : public CxxTest::TestSuite
{
public:

	void tearDown() {
		providence().clear();
	}

	struct foo : minimal_resource<foo>
	{
		int n;
		foo(int x) : n(x) { }
	};

	void test_fake()
	{
		foo r(2);

		declare(r);
		provide(r, []() { return foo(10); });

		TS_ASSERT_EQUALS(get(r).n, 10);
		TS_ASSERT( is_resource_type<foo> );
		TS_ASSERT( !is_resource_type<int> );
		TS_ASSERT( !is_resource_type<string> );
	}

	void test_resource_constructor()
	{
		auto R = resource<string>(Default).id();
		TS_ASSERT_EQUALS(R.type(), type_index(typeid(resource<string>)));
		TS_ASSERT_EQUALS(R.quals(), qualifiers({Default}));
		TS_ASSERT_EQUALS( (resource<string>(Default).id()), (resource<string>({Default}).id()) );

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

	void test_resourceid()
	{

	}

};
