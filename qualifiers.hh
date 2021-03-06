#pragma once

#include <typeinfo>
#include <typeindex>
#include <ostream>
#include <string>
#include <algorithm>
#include <iterator>

#include "exceptions.hh"

namespace cdi {

namespace u = utilities;
using std::string;

#define QUAL_SUFFIX  "$qualifier"
#define QUAL_CLASS(qual) qual##$qualifier

/**
	Base class for qualifier state implementation.

	This is an abstract class, that requires two abstract virtual
	methods to be defined by a subclass. Every qualifier class must
	inherit publicly from this class.

	The qualifier class can contain additional state. In order to
	work correctly with `qual_base`, the subclass must provide
	three operations for the additional state:
	  1. a hash value for the additional state
	  2. equality comparison for the additional state, and
	  3. an output function for printing the additional state

	In the following example, qualifier type `Foo` is implemented by
	class `QUAL_CLASS(Foo)`.  QUAL_CLASS is a preprocessor macro that
	mangles the name `Foo` by adding a suffix. The default suffix is
	'$qualifier', which introduces a '$' into the name. If this is
	a problem, you can change the definition of the two macros
	QUAL_CLASS and QUAL_SUFFIX, to something your compiler will dig.

	QUAL_CLASS(Foo) and provides additional state (in this case,
	an integer field `x`). A possible implementation is the following:
	```
	struct QUAL_CLASS(Foo) : qual_base
	{
		// call constructor of qual_base
		QUAL_CLASS(Foo)(int _x)
		:	qual_base(typid(QUAL_CLASS(Foo)), std::hash<int>()(x))),
			x(_x)
		{ }

		// provide equality test including the value of x
		virtual bool equals(const qual_base& other) const override {
			return qual_base::equals(other) &&
				x == static_cast<const QUAL_CLASS(Foo)&>(other).x;
		}

		// provide output function
		virtual std::ostream& output(std::ostream& s) const override {
			qual_base::output(s) << "(" << x << ")";
			return s;
		}

		inline int xvalue() const { return x; }
	private:
		int x;
	};
	```

  */
struct qual_base
{

	/**
		Initialize the object.

		@param ti  the type index of the qualifier instance.
		@param vhash a hash value for the additional state of the object

		The implementation caches the hash value for the object, therefore
		this value must be provided to the constructor. This value is
		combined with the hash value of type(), to produce the final
		hash code for the object.

		If qualifier type (i.e., a subclass) does not wish to provide
		the hash value as a constructor argument `vhash`, then it is
		possible to set the value later (probably in the body of the
		subclass constructor) by calling method  set_value_hash().

	  */
	inline qual_base(const std::type_index& ti, size_t vhash)
	: _type(ti), hcode(compute_hash(ti, vhash)) { }

	/// Destructor
	virtual ~qual_base() { }

	/**
		The qualifier name suffix
	  */
	static inline const std::string qual_suffix = QUAL_SUFFIX;

	/**
		Return the name of the qualifier type.
		@return the qualifier type name.

		The qualifier type name is computed from the full class name
		of the qualifier type (e.g., `"QUAL_CLASS(Foo)"`), but dropping
		the suffix QUAL_SUFFIX, if it exists.
	  */
	virtual string name() const {
		string ret = u::demangle(type().name());
		// Remove a suffix if one is there! If not, leave the name as is.
		if(ret.size() > qual_suffix.size()
			&& std::equal(ret.end()-qual_suffix.size(), ret.end(), qual_suffix.begin()))
			ret.resize(ret.size()-qual_suffix.size());
		return ret;
	}

	/**
		Return the type index of the qualifier type.
	  */
	inline const std::type_index& type() const { return _type; }

	/**
		Return a hash code for this object.
	  */
	inline size_t hash_code() const { return hcode; }

	/**
		Compare two objects for equality.

		@return true if `*this` and `other` have equal hash_code and type

		A subclass containing more state should override this method, in
		order to check equality of the additional state.
	  */
	virtual inline bool equals(const qual_base& other) const {
		return (hcode==other.hcode) && (type()==other.type());
	}

	/**
		Match this qualifier to another qualifier.
	  */
	virtual inline bool matches(const qual_base& other) const {
		return equals(other);
	}

	/**
		Output the qualifier to a stream.

		@param s the stream to output to
		@return the stream s

		This method will output the name of the qualifer class `name()`,
		prefixed by the character '@'.

		A subclass containing more state should override this method to print
		the additional state.
	  */
	virtual inline std::ostream& output(std::ostream& s) const {
		s << "@" << name();
		return s;
	}

protected:
	/**
		Cache a hash_code for the object, given a hash for additional state.
		@param vhash a hash value for the additional state.

		A subclass must provide a hash value at initialization, as a parameter
		constructor. If this is inconvenient, or the hash value for additional state
		changes, this method can be used to recompute and cache the new hash value.
	*/
	void set_value_hash(size_t vhash) {
		hcode = compute_hash(_type, vhash);
	}

	/**
		Compute the hash_code for an object with the given type index and value hash.
		@param ti the type index of the qualifier class
		@param vhash a hash value for the additional state
	  */
	inline static size_t compute_hash(const std::type_index& ti, size_t vhash)
	{
		size_t seed = 0;
		u::hash_combine(seed, ti.hash_code());
		u::hash_combine(seed, vhash);
		return seed;
	}

private:
	std::type_index _type;
	size_t hcode;
};


namespace detail {

	using std::ostream;

	template<typename Value>
	struct ostream_printer {
		inline ostream& operator()(ostream& s, const Value& val) const {
			return (s << val);
		}
	};

	template <typename Value>
	struct qual_state_traits
	{
		typedef std::hash<Value> hasher;
		typedef std::equal_to<Value> equal_to;
		typedef ostream_printer<Value> printer;
	};


	template <typename Value>
	struct qual_state : qual_base
	{
		typedef Value value_type;
		typedef typename qual_state_traits<Value>::hasher hasher;
		typedef typename qual_state_traits<Value>::equal_to equal_to;
		typedef typename qual_state_traits<Value>::printer printer;

		qual_state(const std::type_index& ti, const value_type& v)
		: qual_base(ti, hasher()(v)), val(v) { }

		const value_type& value() const { return val; }

		bool equals(const qual_base& other) const override {
			equal_to eq;
			return qual_base::equals(other)
				&& eq(static_cast<const qual_state<value_type>&>(other).value(), value());
		}

		std::ostream& output(std::ostream& s) const override {
			printer pr;	 // default constructible
			qual_base::output(s) << "("; pr(s, value()); s << ")";
			return s;
		}
	private:
		const value_type val;
	};

	template <typename Qual, typename Value=void> struct qual_impl;

	template <typename Qual, typename Value>
	struct qual_impl : qual_state<Value>
	{
		typedef Qual qualifier_type;

		qual_impl(const Value& v)
		: qual_state<Value>(typeid(qualifier_type), v) { }
	};

	template <typename Qual>
	struct qual_impl<Qual, void>  : qual_base
	{
		typedef Qual qualifier_type;
		typedef void value_type;
		typedef void hasher;
		typedef void printer;

		qual_impl() : qual_base(typeid(qualifier_type), (size_t)0) { }
	};

} // end namespace detail


class qualifier;
/// Standard qualifiers
extern qualifier Default, All, Null;


/**
	A tagging object used to annotate resources.

	A qualifier is essentially a run-time enum value. Sets of qualifiers
	can be used to semantically annotate other objects (most notably, resources).
	They are similar to Java annotations, but contrary to Java, these are runtime objects.
	Qualifiers can also take a value as attribute.

	Instances of class qualifier are std::shared_ptr wrappers, that point to an underlying
	implementation object.

	There are some standard qualifiers: `Default` and 'All'.

	Each qualifier is associated with a distinct class. The class for
	`Default` is `QUAL_CLASS(cdi::Default)` and for All it is
	`QUAL_CLASS(cdi::All)`.

	To define a custom qualifier type, say `Foo`, you declare a new class
	as follows:
	```
	use cdi::detail::qual_impl;

	struct QUAL_CLASS(Foo) : qual_impl<QUAL_CLASS(Foo)> {
		using qual_impl<QUAL_CLASS(Foo)>::qual_impl;
	};
	extern qualifier Foo;
	```
	Then you can define `Foo` as
	```
	qualifier Foo { new QUAL_CLASS(Foo); }
	```

	To define a qualifier with an argument of some type, e.g. string, do
	```
	// Declare
	struct QUAL_CLASS(Foo) : qual_impl<QUAL_CLASS(Foo), string> {
		using qual_impl<QUAL_CLASS(Foo)>::qual_impl;
	};
	qualifier Foo(const string& val) { return qualifier(new QUAL_CLASS(Foo)(val)); }

	```

	There are two convenience macros `DEFINE_VOID_QUALIFIER` and `DEFINE_QUALIFIER`
	that help with the above boilerplate.

	@see qualifiers
	@see resource
	@see detail::qual_impl
  */
class qualifier
{
public:

	/// Constructor. This should not be used in user code.
	inline qualifier(qual_base* p=nullptr)
	: sptr(p)
	{
		if(p==nullptr)
			(*this) = Null;
	}

	template <typename QualBase>
	inline qualifier(const std::shared_ptr<QualBase>& p)
	: sptr(p)
	{
		if(sptr==nullptr)
			(*this) = Null;
	}

	inline qualifier(std::shared_ptr<qual_base>&& p)
	: sptr(p)
	{
		if(sptr==nullptr)
			(*this) = Null;
	}

	/// Compare two qualifiers for equality
	inline bool operator==(const qualifier& other) const {
		if(sptr == other.sptr)
			return true;
		else if(sptr->equals(*other.sptr)) {
			// combine into one state, speeding future compares
			if(sptr.use_count() > other.sptr.use_count()) {
				sptr = other.sptr;
			} else {
				other.sptr = sptr;
			}
			return true;
		}
		return false;
	}
	/// Inequality operator
	inline bool operator!=(const qualifier& other) const {
		return !((*this)==other);
	}

	inline bool matches(const qualifier& other) const {
		return sptr->matches( * other.sptr );
	}

	/// Qualifier name.
	string name() const { return sptr->name(); }

	/// Type index of the qualifier type
	std::type_index type() const { return sptr->type(); }

	/// A hash code for this qualifier
	size_t hash_code() const { return sptr->hash_code(); }

	/**
		Retrieve the value of this qualifier (if any)

		@tparam Value the type of the value
		@return the value
		@throw std::bad_cast if the type of the underlying value is different

		Retrieve the qualifier value given its type. For example,
		```
		qualifier q = Name("foo");
		string name = q.value<string>(q);
		```
	  */
	template <typename Value>
	const Value& value() const {
		const detail::qual_state<Value>* ptr = dynamic_cast<const detail::qual_state<Value>*>(sptr.get());
		if(ptr)
			return ptr->value();
		else
			throw std::bad_cast();
	}

	/**
		Get a shared pointer to the underlying implementation object.

		If the underlying implementation object cannot be cast to `QualType` the
		returned std::smart_ptr is null.

		Example:
		```
		qualifier q = Default;
		auto qimpl = q.get<QUAL_CLASS(cdi::Default)>();
		```
	  */
	template <typename QualType>
	std::shared_ptr<const QualType> get() const {
		return std::dynamic_pointer_cast<const QualType>(sptr);
	}

private:
	mutable std::shared_ptr<const qual_base> sptr;
	friend std::ostream& operator<<(std::ostream& , const qualifier& q);
};


/// Output the qualifier
inline std::ostream& operator<<(std::ostream& s, const qualifier& q)
{
	return q.sptr->output(s);
}


#define DEFINE_VOID_QUALIFIER_CUSTOM(qname, body) \
struct QUAL_CLASS(qname) : cdi::detail::qual_impl< QUAL_CLASS(qname) > { \
	using cdi::detail::qual_impl< QUAL_CLASS(qname) >::qual_impl; \
	body \
};\
inline cdi::qualifier qname { new QUAL_CLASS(qname)() };\


/**
	Macro to easily define a qualifier type with no additional state.
  */
#define DEFINE_VOID_QUALIFIER(qname) DEFINE_VOID_QUALIFIER_CUSTOM(qname, )


/**
	Macro to easily define a qualifier type with additional state.
  */
#define DEFINE_QUALIFIER_CUSTOM(qname, qvalue_type, arg_type, body) \
struct QUAL_CLASS(qname) : cdi::detail::qual_impl< QUAL_CLASS(qname), qvalue_type> { \
	using cdi::detail::qual_impl< QUAL_CLASS(qname), qvalue_type>::qual_impl; \
	body \
};\
inline cdi::qualifier qname(arg_type val) { return cdi::qualifier(new QUAL_CLASS(qname)(val)); }\

#define DEFINE_QUALIFIER(qname, qvalue_type, arg_type) DEFINE_QUALIFIER_CUSTOM(qname,qvalue_type,arg_type,)


DEFINE_VOID_QUALIFIER(Default);
DEFINE_VOID_QUALIFIER_CUSTOM(All,
	inline bool matches(const qual_base& other) const override { return true; }
	);
DEFINE_VOID_QUALIFIER(Null);




/**
	A set of qualifiers with a unique instance for each qualifier type.

	At most one instance of each qualifier is allowed to be in the set.
	In general, two qualifiers of the same type are called similar.
	Void-value qualifiers are similar if and only if they are equal.
	However, two qualifiers with a value can be similar but not equal
	(e.g. `Name("foo")` and `Name("bar")` are similar but not equal )
  */
class qualifiers
{
public:

	/// Create a singleton set containing `Q`.
	qualifiers(const qualifier& Q)
	: qset({Q})
	{
		compute_hash();
	}

	/// Create a set from an initialization list
	qualifiers(const std::initializer_list<qualifier>& init)
	: qset(init)
	{
		compute_hash();
	}

	qualifiers(const qualifiers&) = default;
	qualifiers(qualifiers&&) = default;

	/**
		Check membership in the set of a qualifier with the same type.

		@param q the qualifier whose type is used to check membership
		@return true if a qualifier of this type belongs to the set
		@see contains()

		This membership test ignores the value of the qualifier.
 	  */
	inline bool contains_similar(const qualifier& q) const {
		return qset.count(q)!=0;
	}

	/**
		Return a subset of qualifiers with underlying implementation
		being an instance of a class.

		@tparam QualClass base class for the implementations of collected
			qualifiers.
		@tparam OutputIter iterator type
		@param iter an output iterator used to collect the result

		When a set of qualifier types are subclasses of a base class,
		this call can select it.
		Passing `qual_base` as the base class, will select every element.
		*/
	template <typename QualClass, typename OutputIter>
	void collect(OutputIter iter) const {
		std::copy_if(qset.begin(), qset.end(), iter,
			[](const qualifier& q) -> bool {
				return bool(q.get<QualClass>());
			});
	}

	/**
		Check membership in the set of a qualifier.
		@param q the qualifier to check for membership
		@return true if `q` belongs to the set
		@see contains_similar()
	  */
	inline bool contains(const qualifier& q) const {
		auto it = qset.find(q);
		return it!=qset.end() && (*it)==q;
	}

	/// Return the number of elements of the set
	inline size_t size() const { return qset.size(); }

	/**
		Return true if this set matches another.

		@param other the other set
		@return true if this matches other

		The definition is
		```
		every qual in this matches something in other
		and for every qual in other, something in this matches it
		```

		Since mathcing between qualifiers is an arbitrary relation,
		this relation is also arbitrary.
	  */
	bool matches(const qualifiers& other) const
	{
		// check that everything in this matches something in other
		for(auto& a : qset) {
			bool matched=false;
			for(auto& b : other.qset)
				if(a.matches(b)) { matched=true; break; }
			if(! matched) return false;
		}
		for(auto& b : other.qset) {
			bool matched=false;
			for(auto& a: qset)
				if(a.matches(b)) { matched=true; break; }
			if(! matched) return false;
		}
		return true;
	}



	/// Checks set equality
	inline bool operator==(const qualifiers& other) const
	{
		if(size()!=other.size())
			return false;
		for(auto& q : qset) {
			if(! other.contains(q))
				return false;
		}
		return true;
	}



	/// Checks set inequality
	inline bool operator!=(const qualifiers& other) const {
		return !( *this == other );
	}

	/// A hash code for the set
	inline size_t hash_code() const { return hcode; }

private:
	struct hash_types {
		inline size_t operator()(const qualifier& q) const {
			return q.type().hash_code();
		}
	};
	struct equal_types {
		inline bool operator()(const qualifier& q1, const qualifier& q2) const {
			return q1.type()==q2.type();
		}
	};
public:
	/// The underlying set type used as a cdi
	typedef std::unordered_set<qualifier, hash_types, equal_types> set_type;

	/// Iterator type
	typedef typename set_type::const_iterator iterator;

	/// Beginning of range
	iterator begin() const { return qset.cbegin(); }

	/// End of range
	iterator end() const { return qset.cend(); }

	//-----------------------------
	// Mutating operations
	//-----------------------------

	/**
		Remove a qualifier similar to a given one

		@param q the qualifier whose type is removed
		@return `true` if a removal was peroformed, `false` if not
	  */
	bool delete_similar(const qualifier& q) {
		auto it = qset.find(q);
		if(it != qset.end()) {
			hcode ^= (*it).hash_code();
			qset.erase(it);
			return true;
		}
		return false;
	}

	/**
		Remove a qualifier equal to a given one

		@param q the qualifier whose type is removed
		@return `true` if a removal was peroformed, `false` if not
	  */
	bool delete_equal(const qualifier& q) {
		auto it = qset.find(q);
		if(it != qset.end() and (*it)==q) {
			hcode ^= (*it).hash_code();
			qset.erase(it);
			return true;
		}
		return false;
	}

	/**
		Add the given qualifier to the set, possibly removing a similar one.
		@param The qualifier to add to the set.
	  */
	void update(const qualifier& q) {
		delete_similar(q);
		auto ret [[maybe_unused]] = qset.insert(q);
		assert(ret.second);
		hcode ^= q.hash_code();
	}

	/**
		Update from a range of iterators
	  */
	template <typename Iterator>
	void update(Iterator from, Iterator to)
	{
		for(Iterator it = from; it!=to; ++it)
			update(*it);
	}

	/**
		Update from another set of qualifiers.
	  */
	void update(const qualifiers& qs) {
		update(qs.begin(), qs.end());
	}

	/**
		Empty the set.
	  */
	void clear() {
		qset.clear(); hcode=0;
	}


private:
	set_type qset;
	size_t hcode;
	void compute_hash() {
		// q: initialize the hash code of the empty set to a
		// weird value (doesn't matter which)?
		size_t seed = 0;
		for(auto& q : qset)
			seed ^= q.hash_code();
		hcode = seed;
	}

	auto find_exact(const qualifier& q) {
		auto it = qset.find(q);
		if(it!=qset.end() && (*it)==q) return it;
		else return qset.end();
	}

};

/// Output streaming for qualifiers
inline std::ostream& operator<<(std::ostream& s, const qualifiers& q)
{
	std::copy(q.begin(), q.end(),
		std::ostream_iterator<qualifier>(s, " "));
	return s;
}



/**
	An unordered map type with qualifiers sets as keys.
  */
template <typename T>
struct qualifiers_map : std::unordered_map<qualifiers, T, utilities::hash_code<qualifiers> >
{
	/// the unordered map type used to implement this class
	typedef typename std::unordered_map<qualifiers, T, utilities::hash_code<qualifiers> > impl_map;

	//using impl_map::unordered_map;
	using impl_map::find;
	using impl_map::end;
	//using impl_map::operator[];

	/// Check if this map contains given key
	bool contains(const qualifiers& key) const {
		return find(key)!=end();
	}

};


} //end namespace cdi
