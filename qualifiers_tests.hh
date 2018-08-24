#pragma once

#include <cxxtest/TestSuite.h>
#include <jsoncpp/json/json.h>

#include <memory>
#include <functional>
#include <array>

#include "cdi.hh"

//---------------------------------
// testing qualifier declarations
// these are also compile-time tests
//---------------------------------

DEFINE_VOID_QUALIFIER(Toplevel)
DEFINE_QUALIFIER(Toplevel2,int,int)

namespace qual_test {
	// declarations and definitions inside namespace
	namespace cc = cdi;

	DEFINE_VOID_QUALIFIER(Toplevel)
	DEFINE_QUALIFIER(Toplevel2,int,int)
}


using namespace cdi;

// ===============================
// A more complicated example
// ===============================

// Define traits


struct Point$qualifier : qual_base
{
	Point$qualifier(double x, double y)
	: qual_base(typeid(Point$qualifier), 0) , _x(x), _y(y)
	{ 
		std::hash<double> hf;
		size_t vhash = hf(x) ^ hf(y);
		set_value_hash(vhash);
	}

	virtual bool equals(const qual_base& other) const override {
		if (qual_base::equals(other)) {
			auto& o = static_cast<const Point$qualifier&>(other);
			return x()==o.x() && y()==o.y();
		}
		return false;
	}
	virtual std::ostream& output(std::ostream& s) const override {
		qual_base::output(s) << "(" << x() << "," << y() << ")";
		return s;
	}


	inline double x() const { return _x; }
	inline double y() const { return _y; }
private:
	double _x, _y;
};
inline qualifier Point(double x, double y) {
	return qualifier(new Point$qualifier(x,y));
}



using std::endl;
using std::cout;

class QualifierSuite : public CxxTest::TestSuite
{
public:


	DEFINE_QUALIFIER(Name, std::string, const std::string&)
	DEFINE_QUALIFIER(Size, size_t, size_t)

	void tearDown() override {
		providence().clear();
	}

	void test_qualifier_constructors()
	{
		qualifier q;
		TS_ASSERT_EQUALS(q, Null);
		qualifier q1 { All };
		TS_ASSERT_EQUALS(q1, All);

		std::array<qualifier, 1> arr;
		TS_ASSERT_EQUALS(arr[0], Null);

	}

	void test_qualifier_defmacros_void()
	{
		qualifier q1 = Toplevel;
		qualifier q2 = qual_test::Toplevel;

		TS_ASSERT_DIFFERS(q1,q2);
	}

	void test_qualifier_defmacros()
	{
		qualifier q1 = Toplevel2(10);
		qualifier q2 = qual_test::Toplevel2(10);

		TS_ASSERT_DIFFERS(q1,q2);
	}

	void test_qualifier() 
	{
		qualifier d = Default;
		qualifier n = Name("foo");
 
		using namespace std;
		TS_TRACE("printing some qualifiers");

		using M = utilities::str_builder;
		TS_ASSERT_EQUALS((M()<< All).str(), "@cdi::All");
		TS_ASSERT_EQUALS((M()<< Default).str(), "@cdi::Default");
		TS_ASSERT_EQUALS((M()<< n).str(), "@QualifierSuite::Name(foo)");

		TS_ASSERT(d == Default);
		TS_ASSERT_DIFFERS(d, All);
		TS_ASSERT_DIFFERS(All, d);
		TS_ASSERT_DIFFERS(d, n);
		TS_ASSERT(d != n);
		TS_ASSERT_EQUALS(n, Name("foo"));
		TS_ASSERT_DIFFERS(n, Name("bar"));

		TS_ASSERT_EQUALS(n.value<string>(), "foo");
		TS_ASSERT_THROWS(n.value<int>(), std::bad_cast);

		auto nimpl = n.get<Name$qualifier>();
		TS_ASSERT(nimpl);
		TS_ASSERT_EQUALS(nimpl->value(), "foo");

		TS_ASSERT_EQUALS(Default.get<Name$qualifier>(), nullptr);
		TS_ASSERT(! (n.get<Size$qualifier>()));
	}

	void test_point()
	{
		using M = utilities::str_builder;

		TS_ASSERT_EQUALS(Point(1,2), Point(1.0,2.0));
		TS_ASSERT_EQUALS((M()<<Point(1,0)).str(), "@Point(1,0)");
	}


};



class QualifiersSuite : public CxxTest::TestSuite
{
public:

	DEFINE_QUALIFIER(Name, std::string, const std::string&)
	DEFINE_QUALIFIER(Size, size_t, size_t)


	void test_qualifiers_constructor()
	{
		qualifiers q1 = {Name("foo"), Name("bar"), Name("baz")};
		TS_ASSERT_EQUALS(q1.size(), 1);

		TS_ASSERT_EQUALS(qualifiers({}).size(), 0);
		TS_ASSERT_EQUALS(qualifiers({Null}).size(), 1);
		TS_ASSERT_EQUALS(qualifiers(Default).size(), 1);
		TS_ASSERT_EQUALS(qualifiers({All, Null, Null, Default}).size(), 3);
	}

	void test_qualifiers_contains()
	{
		qualifiers dflt(Default);
		TS_ASSERT_EQUALS(dflt.size(), 1);
		TS_ASSERT(dflt.contains(Default));

		qualifiers foo({Name("foo")});
		TS_ASSERT_EQUALS(foo.size(), 1);
		TS_ASSERT(foo.contains(Name("foo")));

		qualifiers dfoo({Default, Name("foo")});
		TS_ASSERT_EQUALS(dfoo.size(), 2);
		TS_ASSERT(dfoo.contains(Default));
		TS_ASSERT(dfoo.contains(Name("foo")));
		TS_ASSERT(!dfoo.contains(Name("bar")));
		TS_ASSERT(dfoo.contains_similar(Name("bar")));

		qualifiers bar({Name("bar")}); 
		qualifiers dbar({Default, Name("bar")});
	}

	void test_qualifiers_match()
	{
		qualifiers dflt(Default);
		qualifiers foo({Name("foo")});
		qualifiers dfoo({Default, Name("foo")});
		qualifiers bar({Name("bar")}); 
		qualifiers dbar({Default, Name("bar")});

		TS_ASSERT(dflt.matches(dflt));
		TS_ASSERT(! dflt.matches(foo));
		TS_ASSERT_DIFFERS(Default, Name("foo"));
		TS_ASSERT(! dflt.matches(dfoo));

		TS_ASSERT(foo.matches(foo));
		TS_ASSERT(! foo.matches(dflt));
		TS_ASSERT(! dfoo.matches(dflt));

		TS_ASSERT(! foo.matches(bar));
		TS_ASSERT(! dfoo.matches(foo));
		TS_ASSERT(dfoo.matches(dfoo));
		TS_ASSERT(! dfoo.matches(dbar));
		TS_ASSERT(! dfoo.matches(bar));

	}

	void test_qualifiers_match_empty()
	{
		qualifiers dflt(Default);
		qualifiers foo({Name("foo")});
		qualifiers dfoo({Default, Name("foo")});
		qualifiers bar({Name("bar")}); 
		qualifiers dbar({Default, Name("bar")});

		qualifiers qe {};
		TS_ASSERT_EQUALS(qe.size(), 0);

		TS_ASSERT(qe.matches(qe));
		TS_ASSERT(!qe.matches(dflt));
		TS_ASSERT(!qe.matches(foo));
		TS_ASSERT(!qe.matches(dfoo));

	}

	void test_qualifiers_match_all()
	{
		qualifiers dflt(Default);
		qualifiers foo({Name("foo")});
		qualifiers dfoo({Default, Name("foo")});
		qualifiers bar({Name("bar")}); 
		qualifiers dbar({Default, Name("bar")});
		qualifiers qe {};

		qualifiers all({All});
		qualifiers adflt({All, Default});
		qualifiers afoo({All, Name("foo")});		
		qualifiers abar({All, Name("bar")});

		TS_ASSERT(all.matches(qe));
		TS_ASSERT(all.matches(dflt));
		TS_ASSERT(all.matches(dfoo));
		TS_ASSERT(all.matches(foo));
		TS_ASSERT(all.matches(bar));
		TS_ASSERT(all.matches(dbar));

		TS_ASSERT(!adflt.matches(qe));
		TS_ASSERT(adflt.matches(dflt));
		TS_ASSERT(adflt.matches(dfoo));
		TS_ASSERT(!adflt.matches(foo));
		TS_ASSERT(!adflt.matches(bar));
		TS_ASSERT(adflt.matches(dbar));

		TS_ASSERT(!afoo.matches(qe));
		TS_ASSERT(!afoo.matches(dflt));
		TS_ASSERT(afoo.matches(dfoo));
		TS_ASSERT(afoo.matches(foo));
		TS_ASSERT(!afoo.matches(bar));
		TS_ASSERT(!afoo.matches(dbar));

	}

	void check_hash(const qualifiers& Q)
	{
		size_t chash = 0;
		for(auto& q: Q)
			chash ^= q.hash_code();
		TS_ASSERT_EQUALS(Q.hash_code(), chash);
	}

	void test_hash()
	{

		qualifiers dflt(Default);
		qualifiers foo({Name("foo")});
		qualifiers dfoo({Default, Name("foo")});
		qualifiers bar({Name("bar")}); 
		qualifiers dbar({Default, Name("bar")});
		qualifiers qe {};

		qualifiers all({All});
		qualifiers adflt({All, Default});
		qualifiers afoo({All, Name("foo")});		
		qualifiers abar({All, Name("bar")});

		check_hash(dflt);
		check_hash(foo);
		check_hash(bar);
		check_hash(dfoo);
		check_hash(dbar);
		check_hash(all);
		check_hash(afoo);
		check_hash(abar);
	}

	void test_insert()
	{

	}


};

