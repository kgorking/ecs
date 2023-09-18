#ifndef ECS_CONTRACT
#define ECS_CONTRACT

// Contracts. If they are violated, the program is in an invalid state and is terminated.

#define Assert(cond)                                               \
	do {                                                           \
		((cond) ? static_cast<void>(0) : std::terminate());        \
	} while (false)
#define Pre(cond)                                                  \
	do {                                                           \
		((cond) ? static_cast<void>(0) : std::terminate());        \
	} while (false)
#define Post(cond)                                                 \
	do {                                                           \
		((cond) ? static_cast<void>(0) : std::terminate());        \
	} while (false)

#endif // !ECS_CONTRACT
