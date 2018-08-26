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

class container
{
public:

	inline auto at(const resourceid& r) { return rms.at(r); }

	template <typename Resource>
	inline resource_manager<Resource>* get_declared(const Resource& r) {
		try {
			return static_cast<resource_manager<Resource>*>(rms.at(r));
		} catch(std::out_of_range) {
			return nullptr;
		}
	}

	template <typename Resource>
	inline resource_manager<Resource>* get(const Resource& r) {
		try {
			return static_cast<resource_manager<Resource>*>(rms.at(r));
		} catch(std::out_of_range) {
			auto rm = new resource_manager<Resource>(r); 
			bool succ [[maybe_unused]];
			std::tie(std::ignore, succ) = rms.emplace(r, rm);
			assert(succ); // since we just failed the lookup!
			return rm;
		}
	}

	inline auto resource_managers() const { return rms; }

	void check_container() {

	}

	void clear();

private:
	resource_map<contextual_base*> rms;
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