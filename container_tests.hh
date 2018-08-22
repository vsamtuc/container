#pragma once

#include <cxxtest/TestSuite.h>
#include <jsoncpp/json/json.h>

#include <memory>
#include <functional>

#include "container.hh"

using namespace container;


struct foo
{
	int a;
};

using container::detail::qual_impl;


DEFINE_QUALIFIER(Name, std::string, const std::string&)

class ContainerSuite : public CxxTest::TestSuite
{
public:


	void test_scope_default()
	{
		auto pld = new foo { 11 };
		scope S;

		S.provide(pld);

		auto req = S.require<foo*>();

		TS_ASSERT_EQUALS(req, pld);
		TS_ASSERT_EQUALS(req->a, 11);

		TS_ASSERT_THROWS( S.require<foo*>({Name("foo")}), instantiation_error);
	}

	void test_scope_set()
	{
		scope S;
		auto foo1 = new foo{1};
		auto foo2 = new foo{2};

		S.provide(foo1, {Name("foo1")});
		S.provide(foo2, {Name("foo2")});

		instances<foo*> q1 = S.inquire<foo*>({ Name("foo1")});
		TS_ASSERT_EQUALS(q1.size(), 1);
		TS_ASSERT_EQUALS(q1.count(foo1), 1);

		instances<foo*> q2 = S.inquire<foo*>();
		TS_ASSERT_EQUALS(q2.size(), 0);

		TS_ASSERT_EQUALS(S.inquire<foo*>({Name("foo1")}).count(foo1), 1);
		TS_ASSERT_EQUALS(S.inquire<foo*>({Name("foo1")}).size(), 1);
		
		TS_ASSERT_EQUALS(S.inquire<foo*>({Name("foo2")}).count(foo2), 1);
		TS_ASSERT_EQUALS(S.inquire<foo*>({Name("foo2")}).size(), 1);

		instances<foo*> q3 = S.inquire<foo*>(All);
		TS_ASSERT_EQUALS(q3.size(), 2);

	}

	static foo* make_foo(int a) { return new foo { a }; }

	void test_scope_factory()
	{
		scope S;
		auto fasset = S.provide_factory(make_foo);

		TS_ASSERT(fasset != nullptr);
		auto foo10 = S.require<foo*>(Default,10);
		auto foo15 = S.require<foo*>({Default},15);
		auto foo20 = S.require<foo*>({Default},20);

		TS_ASSERT_EQUALS(foo10->a, 10);
		TS_ASSERT_EQUALS(foo15->a, 15);
		TS_ASSERT_EQUALS(foo20->a, 20);

		TS_ASSERT_EQUALS(foo10, S.require<foo*>(Default,10));
		TS_ASSERT_EQUALS(foo15, S.require<foo*>({Default},15));
		TS_ASSERT_EQUALS(foo20, S.require<foo*>(Default,20));
	}

};

