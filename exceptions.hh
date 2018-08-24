#pragma once

#include <stdexcept>

#include "utilities.hh"

namespace cdi {

//-------------------------
//
// Exceptions
//
//-------------------------

/**
	Base class for all container exceptions.
  */
struct exception : std::runtime_error { using std::runtime_error::runtime_error; };

/**
	Thrown on an illegal operation on a resource manager,
	such as giving multiple providers to a manager
  */
struct config_error : exception { using exception::exception; }; 

/**
	Thrown when an instantiation of an instance failed.
  */
struct instantiation_error : exception { using exception::exception; };

/**
	Thrown when disposal of an instance failed.
 */
struct disposal_error : exception { using exception::exception; };

/**
	Thrown whenever an operation is called on an inactive scope.
  */
struct inactive_scope_error : exception { using exception::exception; };

inline std::ostream& 
output_exception(std::ostream& stream, const std::exception& e, int level =  0)
{
    stream << std::string(level, ' ') << "exception: " << e.what() << '\n';
    try {
        std::rethrow_if_nested(e);
    } catch(const std::exception& e) {
        return output_exception(stream, e, level+1);
    } 
    return stream;
}

}