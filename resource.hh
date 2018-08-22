#pragma once

#include <typeindex>
#include <unordered_set>

#include "utilities.hh"
#include "exceptions.hh"
#include "qualifiers.hh"

namespace cdi {

/**
	Empty template used to hold a sequence of tag types.
	@tparam Tags the sequence of tag types.
  */
template <typename ... Tags> struct tag_sequence  { };

// =====================
// forward
// =====================

template <typename Resource>
struct resource_manager; 	// the basic class resouce operations
class GlobalScope; 			// the default scope 
class resourceid;  			// type-erased resource id

std::ostream& operator<<(std::ostream&, const resourceid&);

/**
	A resource is a descriptor combining the type, scope and qualifiers of
	a contextual object type.

	@tparam Value the c++ type of the contextual object
	@tparam Scope the scope of the contextual object
	@tparam Tags a sequence of types, which are uninterpreted

	A contextual object type is an extension of a C++ type, which provides
	interface information about C+ objects created and/or managed by the container.

	The interface of contextual object type has a compile-time and a run-time
	component. The compile-time part consists of the object type, the object
	scope and a sequence of tags (which can be any types).
	The run-time part consists of a set of qualifiers.
  */
template <typename Value, typename Scope=GlobalScope , typename ... Tags>
struct resource
{
	/// the resource type
	typedef resource<Value, Scope, Tags...> resource_type;

	/// The type of the contextual object type
	typedef Value provided_type;

	/// The scope of the contextual object type
	typedef Scope scope;

	/// The tags of the contextual object type
	typedef tag_sequence<Tags...> tags;

	/// construct an instance 
	inline resource(const qualifiers& _q) : q(_q) { }

 	/// The qualifiers of the contextual object type
 	inline const qualifiers& quals() const { return q; }

 	/// A resourceid for this resource
 	resourceid id() const;

 	/// Return a resource manager for this resource
 	resource_manager<resource_type>* manager() const;

private:
	const qualifiers q;
};



/**
	Implementation of a resource id.

	A resourceid is an untyped descriptor for resources. It can be
	thought of as a pair of a type (described by a std::type_index) and a 
	set of qualifiers.

	@section perf Performance

	Copying and/or moving a resourceid is the same as doing so on a std::shared_ptr.
  */
class resourceid
{
public:
	/**
		Construct a new resource id.
		@param ti the type id of the resource
		@param q the qualifiers of the resource
	  */
	resourceid(std::type_index ti, const qualifiers& q = {})
	: sptr(std::make_shared<rid_impl>(ti,q))
	{ }

	// Equality comparison
	inline bool operator==(const resourceid& other) const {
		if(sptr == other.sptr)
			return true;
		if( sptr->hcode == other.sptr->hcode &&
			sptr->type == other.sptr->type &&
			sptr->quals == other.sptr->quals)
		{
			if(sptr.use_count() > other.sptr.use_count()) 
				sptr = other.sptr;
			else
				other.sptr = sptr;
			return true;
		}
		return false;
	}

	/// Inequality
	inline bool operator!=(const resourceid& other) const {  return !operator==(other); }

	/// hash code 
	inline size_t hash_code() const { return sptr->hcode; }

	/// the type id of the resource
	inline const std::type_index& type() const { return sptr->type; }

	/// the qualifiers of the resource
	inline const qualifiers& quals() const { return sptr->quals; }

private:
	struct rid_impl
	{
		// cache the hcode for speed
		const std::type_index type;
		const qualifiers quals;
		const size_t hcode;

		inline rid_impl(std::type_index ti, const qualifiers& q) 
		: type(ti), quals(q), hcode(compute_hash(ti, q)) { }

		static size_t compute_hash(std::type_index ti, const qualifiers& q)
		{
			using u::hash_combine;
			size_t seed = 0;
			hash_combine(seed, ti.hash_code());
			hash_combine(seed, q.hash_code());
			return seed;
		}
	};

	mutable std::shared_ptr<rid_impl> sptr;
};

/// Alias for set of `resourceid`s
using resource_set = std::unordered_set<resourceid, utilities::hash_code<resourceid>>;

/// Output streaming for resourceid
inline std::ostream& operator<<(std::ostream& s, const resourceid& r)
{
	s << "RESOURCE(";
	s << r.quals() << " ";
	s << boost::core::demangle(r.type().name()) <<" )";
	return s;
}



template <typename V, typename S, typename ... T>
resourceid resource<V,S,T...>::id() const { 
	return resourceid(typeid(resource<V,S,T...>), quals()); 
}

/**
	A map container mapping resources to T.

	This map is implemented internally as a map-of-maps, that is,
	MAP< typeid, MAP<qualifiers, T> >, using std::unordered_map
	at both levels. This may change in the future.
  */
template <typename T>
struct resource_map 
{
	/// The type of the second-level maps
	typedef qualifiers_map<T> qual_map;

	/// The type of the top-level map
	typedef std::unordered_map<std::type_index, qual_map> type_map;

	/// Iterator for top-level map
	typedef typename type_map::iterator type_iter;

	/// Iterator for second-level maps
	typedef typename qual_map::iterator qual_iter;

	//-------------------------------------------
	// Split API
	//-------------------------------------------

	inline bool contains_type(const std::type_index& ti) const {
		return map.find(ti)!=map.end();
	}

	inline bool contains(const std::type_index& ti, const qualifiers& q) const {
		return contains_type(ti) && map.at(ti).contains(q);
	}

	[[nodiscard]] auto bind(const std::type_index& ti, const qualifiers& q, const T& val) {
		return map[ti].insert(std::make_pair(q, val));
	}

	inline const qual_map& at(const std::type_index& ti) const {
		return map.at(ti);
	}

	inline const T& at(const std::type_index& ti, const qualifiers& q) const {
		return map.at(ti).at(q);
	}

	auto operator[](const std::type_index& ti) { return map[ti]; }

	type_map& get_map() { return map; }
	const type_map& get_map() const { return map; }

	//-------------------------------------------
	// untyped interface (based on resourceid)
	//-------------------------------------------

	inline bool contains(const resourceid& res) const {
		return contains_type(res.type()) 
			&& map.at(res.type()).contains(res.quals());
	}

	[[nodiscard]] auto bind(const resourceid& res, const T& val) {
		return bind(res.type(), res.quals(), val);
	}

	inline const T& at(const resourceid& res) const {
		return map.at(res.type(), res.quals());
	}

	auto operator[](const resourceid& res) { return map[res.type()][res.quals()]; }


	//
	// templated interface (based on Resource)
	//

	template <typename Resource>
	inline bool contains_type() const { return contains_type(typeid(Resource)); }

	template <typename Resource>
	inline bool contains(const Resource& r) const {
		return contains_type(typeid(Resource)) && 
			map.at(typeid(Resource)).contains(r.quals());
	}

	template <typename Resource>
	[[nodiscard]] auto bind(const Resource& r, const T& val) {
		return bind(typeid(Resource), r.quals(), val);
	} 

	template <typename Resource, typename ...Args>
	auto bind_emplace(const Resource& r, Args&& ... args) {
		return 	map[typeid(Resource)].emplace(
				std::piecewise_construct, 
				std::forward_as_tuple(r.quals()), 
				std::forward_as_tuple(std::forward<Args>(args)...));
	}

	template <typename Resource>
	const T& lookup(const Resource& r) const {
		return map.at(typeid(Resource)).at(r.quals());
	}

	template <typename Resource>
	T& operator[](const Resource& r) { return map[typeid(Resource)][r.quals()]; }


	/**
		Completely empty the container
	  */
	void clear() { map.clear(); }

private:
	type_map map;
};

/**
	Return a resource instance for the given resource.

	@tparam Resource the resource type
	@param r the resource to inject
	@throws instantiation_error if it failed to retrieve the resource

	This function retrieves a resource instance for the given resource
	based on the current context.
  */
template <typename Resource>
inline typename Resource::provided_type inject(const Resource& r) {
	using scope_type = typename Resource::scope;
	return scope_type::template get< Resource >(r);
}



} // end namespace container

