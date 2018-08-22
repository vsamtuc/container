#pragma once

#include <unordered_set>
#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include <functional>
#include <cassert>
#include <any>

#include "provider.hh"

namespace cdi {

/**
	Class that provides storage for objects inside context.

	The implementation is based on std:any.
  */
class asset 
{
public:
	/**
		Set the asset to a value.

		@tparam Value the type of the value, which must be CopyConstructible
		@param o the value to store
	  */
	template <typename Value>
	asset(const Value& o) : obj(o) { }

	/**
		Get an object of the provided value stored inside the asset
		@tparam Value the type of the value, which must be CopyConstructible
		@return the stored value
		@throw std::bad_any_cast 
	  */
	template <typename Value>
	Value get_object() const { return std::any_cast<Value>(obj); }

	/**
		Get a reference to the std::any object within the asset.
	  */
	std::any& object() { return obj; }

	/**
		Get a const reference to the std::any object within the asset.
	  */
	const std::any& object() const { return obj; }
private:
	std::any obj;
};


/**
	A container that can materialize resources on demand, providing storage.

	Internally the context holds a map mapping `resourceid`s to `asset`s.
	The main method of a context is `get()`. When the context is asked for a
	resource instance, it first performs a lookup in the map. If not found, it
	initiates a process of instantiation, and stores the result in the map
	(also returning it).
	
	This class is meant to be used in the implementation of scopes.
  */
class context 
{
public:

	/**
		Get a resource instance from this context.

		@tparam Resource the resource type
		@param r the resource
		@return the provided type for the resource
		@throw instantiation_error if something went wrong

		If a resource instance is not already in the context, 
		this method calls create() to create one.
	  */
	template <typename Resource>
	typename Resource::provided_type get(const Resource& r) {
		using M = utilities::str_builder;
		using provided_type = typename Resource::provided_type;
		try {
			const asset& ass = asset_map.lookup(r);
			return ass.get_object<provided_type>();
		} catch(std::out_of_range) {
			return create(r);
		} catch(std::bad_any_cast) {
			throw instantiation_error(M() << "Cyclic instantiation for " << r.id());
		}
	}

	/**
		Create a resource instance in this context.

		@tparam Resource the resource type
		@param r the resource
		@return the cretaed resource instance
		@throw instantiation_error if something went wrong

		Currently this is only meant to be called by get()
	  */
	template <typename Resource>
	typename Resource::provided_type create(const Resource& r) {
		using provided_type = typename Resource::provided_type;

		resource_manager<Resource>* rm = declare(r);
		auto [iter, succ] = asset_map.bind_emplace(r, rm->provide());
		assert(succ);
		const asset& ass = (*iter).second;
		return ass.get_object<provided_type>();
	}

	/**
	   Empty the context, disposing all resource instances.
	  */
	void clear() {
		for(auto& [ti, qmap] : asset_map.get_map()) {
			for(auto& [quals, ass] : qmap) {
				basic_resource_manager* rm = prov_map.at(ti,quals);
				rm->dispose_any(ass.object());
			}
			qmap.clear();
		}
		asset_map.clear();
	}

private:
	resource_map<asset> asset_map;
};


//==================================
//
//  Standard scopes
//
//==================================


/**
	A scope that always returns new resource instances.

	This scope is not associated with a context; every time
	it is asked for an object, it accesses the resource manager
	to provide a new value.	

	It is not clear that this scope is useful in applications,
	but it is quite useful in testing.
  */
struct NewScope 
{
	/**
		Create a resource instance in this scope.

		@tparam Resource the resource type
		@param r the resource
		@return the new resource instance
		@throw instantiation_error if something went wrong
	  */
	template <typename Value, typename ... Tags>
	inline static Value get(const resource<Value, NewScope, Tags...>& r) {
		return declare(r)->provide();
	}
};


/**
	Implementation of scopes that are guarded by the existence of 
	at least one object instance.

	@tparam Tag For each different type, a new scope class is defined.

	The Tag type is only used to instantiate different scopes.

	A scope defined by class instances of this template contains an internal 
	context where objects are stored. 

	A scope defined by a (concrete) class GuardedScope<T> is activatable in a turnstile
	fashion. Each instance of the type GuardScope<T> (on the stack, on the heap, as
	a subclass inside another class, in an array etc.)  increases the turnstile `count()`.
	The scope is active only if the turnstile count in non-zero.

	This class is non-copyable but it is movable (moving it does not affect the turnstile count)
  */
template <typename Tag>
struct GuardedScope
{
	/**
		Constructor, increases the turnstile count.
	  */
	inline GuardedScope() noexcept {  ++_n; }

	/**
		Destructor, decreases the turnstile count.
	  */
	inline ~GuardedScope() { 
		-- _n; 
		if(_n==0) ctx.clear(); 
	}

	GuardedScope(const GuardedScope<Tag>&)=delete;
	GuardedScope& operator=(const GuardedScope<Tag>&)=delete;

	/** Move operator (has no effect) */
	inline GuardedScope(GuardedScope&& _other) noexcept { }

	/**
		Static method that returns a resource instance in this scope.
		@tparam Resource the resource type
		@param r the resource
		@return a resource instance from this scope
		@throws inactive_scope_error if the scope is not active
		@throws instantiation_error if contextual instantiation fails 
	  */
	template <typename Resource>
	static inline typename Resource::provided_type get(const Resource& r) {
		if(! is_active()) throw inactive_scope_error(M() << "Trying to allocate " 
			<< r.id() << " while scope is inactive");
		return ctx.get(r);
	}	

	/**
		Returns true if the scope is active
	  */
	static inline bool is_active() { return _n>0; }

	/**
		Returns the current turnstile count.
	  */	
	static inline size_t count() { return _n; }
private:
	static inline size_t _n=0;
	static inline context ctx;
};



/**
	A class that implements the global scope.

	The global scope is always active. Also, it is the default scope
	for resources. E.g.,
	```
	resource<string> r;
	```
	declares a resource in GlobalScope.
  */
struct GlobalScope {

	/**
		Static method that returns a resource instance in this scope.
		@tparam Resource the resource type
		@param r the resource
		@return a resource instance from this scope
		@throws instantiation_error if contextual instantiation fails 
	  */
	template <typename Resource>
	static inline typename Resource::provided_type get(const Resource& r) {
		return global_context.get(r);
	}

	/**
		Clears the global context, disposing of all assets.
	  */
	static void clear() { global_context.clear(); }
private:
	inline static context global_context;
};

/**
	Convenience declaration for resources in the global scope.
  */
template <typename Value, typename ... Tags>
using Global = resource<Value, GlobalScope, Tags...>;


} // end namespace container

