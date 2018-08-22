#pragma once

#include <stdexcept>

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



}