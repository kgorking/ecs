#ifndef __CONTRACT
#define __CONTRACT

// Contracts. If they are violated, the program is an invalid state, so nuke it from orbit
#define Expects(cond) ((cond) ? static_cast<void>(0) : std::terminate())
#define Ensures(cond) ((cond) ? static_cast<void>(0) : std::terminate())

#endif // !__CONTRACT
