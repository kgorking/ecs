#ifndef __CONTRACT
#define __CONTRACT

// Contracts. If they are violated, the program is an invalid state, so nuke it from orbit
#define Expects(cond) do { ((cond) ? static_cast<void>(0) : std::terminate()); } while(false)
#define Ensures(cond) do { ((cond) ? static_cast<void>(0) : std::terminate()); } while(false)

#endif // !__CONTRACT
