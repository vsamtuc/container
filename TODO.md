# TO DO

## Immediate
  - add resource<...> API for put()

  - make system check for existing provider... disallow re-setting a provider,
    add provider_override
    Same for initializer, disposer, etc?

## Next steps

-	Resource managers could be initialized from other RMs.
  The idea is to share configuration between multiple resources that
  are "similar", no need to repeat same code. Thus, we need an
  "inherits" semantics, and possible modifications/overrides.


- Qualifiers for the resource table affecting provision at run time.
  An example is to have a set of resources R1,...,Rn and export one
  of them as "default", depending on configuration. One idea in
  "pseudo code"
  ```
  resource<qualifier,...>  my_choice;

  resource<T...> chosen_resource = switch(my_choice) // inject qualifier
    .alternative(q1, R1)  // match my_choice==q1
    .alternative(q2, R2)  //   ... and so on ...
       ...
    .default(Rd)
  ```
  where selection should be done at __instantiation time__, instead
  of at configuration time. This could be done in the provider naturally,
  if not for issues related to cycles...

  N.B. this is an advanced form of aliasing, so maybe look at above issue
  first, or together.


## Future/open
  - support multithreading (maybe hard)

  - faster qualifiers equality (cache?)

  - how to leverage qualifier matching?

  - what is the purpose of 'class resource_manager'? All the info is in
    'class contextual'

  - Scopes could not use static variables (including context). They
    could use the container. Problem: what about thread_local (see
    multithreading) ?
