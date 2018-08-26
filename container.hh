#pragma once

#include "contextual.hh"

//=================================
//
//  container
//
//=================================


namespace cdi {


/**
	A container is the holder of all resource-related information.

	The container is the only library object that is allocated
	statically  (note: this is currently FALSE!)

	It is the main point of entry for most-all operations
	on resources.
  */

class container : resource_map<contextual_base*> 
{
public:

	using resource_map<contextual_base*>::at;
	using resource_map<contextual_base*>::begin;
	using resource_map<contextual_base*>::end;
	using resource_map<contextual_base*>::size;
	using resource_map<contextual_base*>::contains;

	template <typename Resource>
	inline resource_manager<Resource>* get(const Resource& r) {
		try {
			return static_cast<resource_manager<Resource>*>(this->at(r));
		} catch(std::out_of_range) {
			auto rm = new resource_manager<Resource>(r); 
			bool succ [[maybe_unused]];
			std::tie(std::ignore, succ) = emplace(r, rm);
			assert(succ); // since we just failed the lookup!
			// call recursively!
			return get(r);
		}
	}

	void clear();

private:
};


inline container& providence() { 
	static container c;
	return c;
}

template <typename Resource>
inline resource_manager<Resource>* resource_manager<Resource>::get(const Resource& r)
{
	return providence().get(r);
}



} // end namespace cdi