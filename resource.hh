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
	configures a resource by configuring the resource manager for the resource.
  */
template <typename Instance, typename Scope=GlobalScope , typename ... Tags>
struct resource
{
	/// the resource type
	typedef resource<Instance, Scope, Tags...> resource_type;

	/// The type of the contextual object type
	typedef Instance instance_type;

	/// The scope of the contextual object type
	typedef Scope scope;

	/// The tags of the contextual object type
	typedef tag_sequence<Tags...> tags;

	/// construct an instance 
	inline resource(const qualifiers& _q) : q(_q) { }
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

 	/// Return a resource manager for this resource
 	resource_manager<resource_type>* manager() const;

 	/**
		Return a resource instance for this resource.

		@return a resource instance for this resource, depending on the current
		        context.
		@throws instantiation_error if it failed to instantitate the resource

		This function returns a resource instance of type `instance_type` for the 
		given resource based on the current context.
 	  */
 	instance_type get() const { return scope::get(*this); }


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
public:
	/**
		Construct a new resource id.
		@param ti the type id of the resource
		@param q the qualifiers of the resource
	  */
	resourceid(std::type_index ti, const qualifiers& q = {})
	: sptr(std::make_shared<rid_impl>(ti,q))
	{ }

	template <typename Instance, typename Scope, typename ...Tags>
	resourceid(const resource<Instance,Scope,Tags...>& r)
	: resourceid(typeid(resource<Instance,Scope,Tags...>), r.quals()) 
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


template <typename T>
struct resource_map : std::unordered_map<resourceid, T, u::hash_code<resourceid> >
{
	typedef std::unordered_map<resourceid, T, u::hash_code<resourceid> > Super;
	using Super::find;
	using Super::end;

	inline bool contains(const resourceid& rid) const { return find(rid)!=end(); }

};


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

template <typename Resource>
inline auto getter(const Resource& r) { return &Resource::get; }


} // end namespace container

