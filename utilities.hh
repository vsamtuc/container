#pragma once

#include <cassert>
#include <memory>
#include <sstream>
#include <functional>
#include <unordered_set>

#include <boost/core/demangle.hpp>
#include <boost/functional/hash.hpp>


namespace cdi::utilities
{

using boost::hash_combine;
using boost::core::demangle;

/**
	Function class template for wrapping a `hash_code()` method.

	@tparam The type of the argument

	This wrapper can be used to provide function classes that wrap
	types which define a `size_t hash_code() const` method.
  */
template <typename T>
struct hash_code
{
	/**
		Return a hash code for the argument.

		@param val the value to be hashed
		@return the hash code

		The hash code is obtained by calling `val.hash_code()`.

	  */
	inline size_t operator()(const T& val) const {
		return val.hash_code();
	}
};


/**
	A storage provider for unique objects.

	@tparam StoredType the type of objects to store
	@tparam Hash a function class for hashing type `StoredType`
	@tparam Equal a function class for equivalence of objects of type `StoredType`

	Instances of this class can be used to allocate "unique" instances of `StoredType`
	objects. Uniqueness is defined by an eqivalence function and a compatible hash function.

	> CAVEAT: Although the objects stored in a unique storage are mutable, care must be 
	> taken never to change their state so that their hash code or equivalence to other
	> objects is changed.
	> Essentially, stored objects are assumed to have an immutable key (determining equivalence).

	The stored objects are accessed via shared pointers (`std::shared_ptr<StoredType>`).
	When all shared pointers pointing to an object are destroyed, the object is automatically
	deallocated.

	The main API method is `allocate(args...)`. 
 */
template <typename StoredType, typename Hash = std::hash<StoredType>, typename Equal = std::equal_to<StoredType> >
struct unique_storage
{
	/// The stored object type
	using stored_type = StoredType;

	/// The shared pointer type returned by `allocate()`
	using shared_pointer = std::shared_ptr<StoredType>;

	/**
		Return an object equivalent to `StoredType(args...)`

		@tparam Args a sequence of arguments to forward to a constuctor of `StoredType`.

		This call constructs an object by calling the constructor with the provided arguments.
		If this object is equivalent to some object already stored, it is destroyed and a new
		shared pointer to the old object is returned. Else, the new object is kept and a
		shared pointer to it is returned. 
	  */
	template <typename ... Args>
	shared_pointer allocate(Args&& ... args) 
	{
		auto ins = storage.emplace(std::forward<Args>(args)...);
		if(ins.second) {
			// item was inserted
			StoredType* ptr = & (* ins.first).object;
			//shared_pointer sptr(ptr, Deleter(this)); // make shared object
			shared_pointer sptr(ptr, Deleter(this)); // make shared object
			(* ins.first).wptr = sptr;  // store to weak object
			return sptr;
		} else {
			// item existed!
			std::weak_ptr<StoredType>& wptr = (* ins.first).wptr;
			assert(! wptr.expired());
			return wptr.lock();
		}
	}

	inline size_t size() const { return storage.size(); }

private:
	void free(StoredType* ptr)
	{
		// find it 
		auto it = storage.find(*ptr);
		assert(it != storage.end());
		assert( (*it).wptr.expired() );
		storage.erase(it);
	}

	struct value_type 
	{
		mutable StoredType object;
		mutable std::weak_ptr<StoredType> wptr;

		template< typename ... Args >
		value_type(Args&& ... args) : object(std::forward<Args>(args)...) { }
	};

	struct hasher : Hash {
		using Hash::Hash;
		inline size_t operator()(const value_type& val) const {
			return Hash::operator()(val.object);
		}
	};

	struct equal_to : Equal {
		using Equal::Equal;
		inline bool operator()(const value_type& val1, const value_type& val2) const {
			return Equal::operator()(val1.object, val2.object);
		}
	};

	struct Deleter {
		unique_storage* ust;
		Deleter(unique_storage* u) : ust(u) { }
		inline void operator()(StoredType* ptr) const {
			ust->free(ptr);
		}
	};
	friend class Deleter;

	std::unordered_set<value_type, hasher, equal_to> storage;

};

/**
	A utility class for building strings conveniently.

	This can be used to build strings as if writing to output streams;
	The usage is better demonstrated by example:
	```
	using SB = container::utilities::str_builder;

	std::string s = SB()<<"1+1=" << 1+1;

	throw my_exception(SB() << "Error in line "<<__LINE__);

	When used in contexts where automatic casting to std::string,
	just finish with  `<< SB::end`.

	```
  */
struct str_builder
{
	struct _end {};

	/// Used to return a string by ending a sequence of '<<' outputs.
	inline static const _end end;

	/// auto cast to string
	inline operator std::string () { return s.str(); }

	/// explicit return of string
	inline std::string str() { return s.str(); }

	/// overload of str_builder '<<' operator
	template <typename T>
	inline str_builder& operator<< (const T& val) { s << val; return *this; }
	/// overload of str_builder '<<' operator
	inline str_builder& operator<< (std::ostream& (*osm)(std::ostream&)) {
		s << osm; return *this;
	}
	/// overload of str_builder '<<' operator
	inline std::string operator<< (_end) { return str(); }

private:
	std::ostringstream s;
};

/// Macro to write location to stream (useful for exceptions and error messages)
#define ERRLOC "In " << __PRETTY_FUNCTION__ << " [" << __FILE__ << " (" << __LINE__ << ")]: "


} // end namespace container::utilities