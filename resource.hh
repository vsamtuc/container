#pragma once

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
class resource_manager; 	// the basic class resouce operations
class GlobalScope; 			// the default scope
class resourceid;  			// type-erased resource id

std::ostream& operator<<(std::ostream&, const resourceid&);

/**
	A resource is a descriptor combining the type, scope and qualifiers of
	a contextual object type.

	@tparam Instance the c++ type of the contextual object
	@tparam Scope the scope of the contextual object
	@tparam Tags a sequence of types, which are uninterpreted

	A contextual object type is an extension of a C++ type, which provides
	interface information about C+ objects created and/or managed by the container.

	Contextual objects are likely to be pointers (normal, smart, etc) to other
	objects, although this is not necessary. The only requirement imposed is that
	they be copyable. As pointer-like objects, they usually point to an actual
	object. The lifecycle API provided by the resource allows contextual objects
	to be initialized, configured and/or finalized.

	The interface of contextual object type has a compile-time and a run-time
	component. The compile-time part consists of the object type, the object
	scope and a sequence of tags (which can be any types).
	The run-time part consists of a set of qualifiers.

	A resource can be instantiated by the container, depending on the current
	context. At each point in time, and for each thread, a scope is associated
	with at most one context (if it is not associated with a context, it is
	inactive). Therefore, the same resource can instantiate different instances,
	depending on the context.

	Resource instances are managed (for lifecycle operations like creation and disposal)
	by an associated resource_manager object (each resource has one). The user
	configures a resource by configuring the resource manager for the resource,
	although this is seldom done directly on the resource manager. More conveniently,
	the resouce API is used.

  */
template <typename Instance, typename Scope=GlobalScope , typename ... Tags>
struct resource
{
	static_assert( std::is_same_v<Instance, std::decay_t<Instance>>,
	"Resource instance types must not be references and/or cv-qualified.\n"
	"Please use std::decay<...> to pass the base type"
	);

	/// the resource type
	typedef resource<Instance, Scope, Tags...> resource_type;

	/// The type of the contextual object type
	typedef Instance instance_type;

	/// The type of return for getting a value of an instance
	typedef std::conditional_t<
		std::is_scalar_v<Instance>,
			Instance,
			std::add_lvalue_reference_t< std::add_const_t<Instance> >
			>
		return_type;

	/// The scope of the contextual object type
	typedef Scope scope;

	/// The tags of the contextual object type
	typedef tag_sequence<Tags...> tags;

	/// Construct an instance
	inline resource(const qualifiers& _q) : q(_q) { }

	/// Construct an instance
	inline resource(qualifiers&& _q) : q(_q) { }

 	/// The qualifiers of the contextual object type
 	inline const qualifiers& quals() const { return q; }

 	//===================================================
 	// The following methods are all defined externally
 	// and constitute the main user entries to the
 	// container API.
 	//=================================================

 	/// A resourceid for this resource
 	resourceid id() const;

 	/**
 		Return a resource manager for this resource.

 		If a resource did not have one, it will be created;
 	  */
 	resource_manager<resource_type>* manager() const;

 	/**
		Return a resource instance for this resource.

		@return a resource instance for this resource, depending on the current
		        context.
		@throws instantiation_error if it failed to instantitate the resource

		This function returns a resource instance of type `instance_type` for the
		given resource based on the current context.
 	  */
 	return_type get() const;

 	/// Declare a resource as having a resource manager
 	inline const resource_type& declare() const { manager(); return (*this); }


	template <typename Callable, typename...Args>
	const resource_type& provide(Callable func, Args&& ... args ) const;

	/**
		Register a new injector for a resource.

		@tparam Callable the callable injector
		@tparam Args a sequence of argument types to pass to the callable
		@param func the function called by the new disposer
		@param args a sequence of arguments to be given to func at invocation
		@throws config_error if a disposer already exists for this resource

		This call registers a new inject function for this resource. When an
		instance is created, first the provider is invoked, followed by every
		injector (of which there can be several), in the order of their declaration.

		When the created injector is invoked, the `func` is called with a non-const
		reference to `obj` as the first argument, followed by the same number of
		parameters as passed to `args`. The actual parameters following obj call
		are computed as for provide(). the function's return value is ignored.

		The usual function of an injector is to provide to the constructed instance
		some value obtained by another resource. Injectors are very important in
		breaking dependency cycles between resources, because the container can
		schedule their execution in the right order, when instantiating mutually
		dependent resources.

		It is the intention of the API to not hide these dependencies, so that the
		user may gen better information. For example, consider two cases. In the
		first (bad) case, the resource to be injected is hidden in the injector code:
		```
		resource<T*, ...> R({});
		resource<U*, ...> Injected({});
		R.provide(...).inject([](auto x){ x->field = Injected.get(); });
		```
		In this case the dependency of `R` on `Injected` is hidden from the container.
		A much better approach is to replace the last line with
		```
		R.provide(...).inject([](auto x, auto y){ x->field = y; }, Injected);
		```

		@see provide()
		@see dispose()
	 */
	template <typename Callable, typename...Args>
	const resource_type& inject(Callable func, Args&& ... args ) const;


	template <typename Callable, typename...Args>
	const resource_type& initialize(Callable func, Args&& ... args ) const;

	/**
		Register a new disposer for a resource.

		@tparam Callable the callable disposer
		@tparam Args a sequence of argument types to pass to the callable
		@param func the function called by the new disposer
		@param args a sequence of arguments to be given to func at invocation
		@throws config_error if a disposer already exists for this resource

		This call registers a dispose function for this resource. When method
		dispose(obj) is invoked on an instance `obj` of this resource,
		`func` is called with a non-const reference to `obj` as the first argument,
		followed by the same number of parameters as passed to `args`.
		The actual parameters following obj call are computed as for provide()

		@see provide()
		@see dispose()
	 */
	template <typename Callable, typename...Args>
	const resource_type& dispose(Callable func, Args&& ... args ) const;

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
	/**
		Construct a new resource id.
		@param ti the type id of the resource
		@param q the qualifiers of the resource
	  */
	resourceid(std::type_index ti, const qualifiers& q = {})
	: sptr(std::make_shared<rid_impl>(ti,q))
	{ }

public:

	/**
		Construct a new resource id.
		@param ti the type id of the resource
		@param q the qualifiers of the resource
	  */
	template <typename Instance, typename Scope, typename ...Tags>
	resourceid(const resource<Instance,Scope,Tags...>& r)
	: resourceid(typeid(resource<Instance,Scope,Tags...>), r.quals())
	{ }

	/// Equality comparison
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
	return resourceid(*this);
}


/**
	An unordered map template mapping resourceid to some type T.

	@tparam T the value type that each resourceid is mapped to
  */
template <typename T>
class resource_map : public std::unordered_map<resourceid, T, u::hash_code<resourceid> >
{
public:
	/// The type of the underlying container
	typedef std::unordered_map<resourceid, T, u::hash_code<resourceid>> container_type;

	using container_type::find;
	using container_type::end;


	/**
		Return true if the map contains an entry for given resourceid
		@param rid the resource id to look up
		@return true if the rid was mapped by this map
	  */
	inline bool contains(const resourceid& rid) const { return find(rid)!=end(); }
};


//==================================================
//
// Functional resource API
//
//==================================================


/**
	Return a resource instance for the given resource.

	@tparam Resource the resource type
	@param r the resource to inject
	@throws instantiation_error if it failed to retrieve the resource

	This function retrieves a resource instance for the given resource
	based on the current context.
  */
template <typename Instance, typename Scope, typename...Tags>
inline Instance get(const resource<Instance, Scope, Tags...>& r) {
	return r.get();
}

/**
	Return the resource manager for a resource
	@tparam Resource the resource type
	@param r the resource
	@return the resource manager for this resource
  */
template <typename Resource>
inline resource_manager<Resource>* declare(const Resource& r) { return r.manager(); }

/**
	Register a new provider for a resource.

	@tparam Resource the resource type
	@tparam Callable the callable provider
	@tparam Args a sequence of argument types to pass to the callable
	@param r the resource to be provisioned with a provider
	@param func the function called by the new provider
	@param args a sequence of arguments to be given to func at invocation
	@throws config_error if a provider already exists for this resource

	This call registers a new provider for resource `r`. When method
	provide() is invoked on the provider, `func` is called with the same
	number of parameters as passed to `args`. The actual parameters to
	the call are computed as follows:
	* if an argument is a resource instance, the resource will be
	  injected at the time `func` is called.
	* if an argument is any other type, the argument is stored and
	  provided to `func` when it is called.
	The requirements for (non-resource) arguments are the same as in std::bind().
 */
template <typename Resource, typename Callable,	typename ... Args>
inline auto provide(const Resource& r, Callable&& func, Args&& ... args  )
{
	return r.provide(std::forward<Callable>(func), std::forward<Args>(args)...);
}

template <typename Resource, typename Callable, typename...Args>
inline auto initialize(const Resource& r, Callable&& func, Args&& ... args )
{
	return r.initialize(std::forward<Callable>(func), std::forward<Args>(args)...);
}


/**
	Register a new disposer for a resource.

	@tparam Resource the resource type
	@tparam Callable the callable disposer
	@tparam Args a sequence of argument types to pass to the callable
	@param r the resource to be provisioned with a disposer
	@param func the function called by the new disposer
	@param args a sequence of arguments to be given to func at invocation
	@throws config_error if a disposer already exists for this resource

	This is a wrapper function for r.dispose(...).

	@see resource::dispose()
	@see provide()
 */
template <typename Resource, typename Callable, typename...Args>
inline auto dispose(const Resource& r, Callable&& func, Args&& ... args )
{
	return r.dispose(std::forward<Callable>(func), std::forward<Args>(args)...);
}



} // end namespace container
