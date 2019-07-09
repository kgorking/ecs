#include "helper.h"

#if 0		// big, medium, and small vectors
std::vector<int> const vector_1 = make_vector(set_size_1);
std::vector<int> const vector_2 = make_vector(set_size_2);
std::vector<int> const vector_3 = make_vector(set_size_2);
std::vector<int> const vector_4 = make_vector(set_size_3);
std::vector<int> const vector_5 = make_vector(set_size_3);
#elif 0		// small, medium, and big vectors
std::vector<int> const vector_1 = make_vector(set_size_3);
std::vector<int> const vector_2 = make_vector(set_size_3);
std::vector<int> const vector_3 = make_vector(set_size_2);
std::vector<int> const vector_4 = make_vector(set_size_2);
std::vector<int> const vector_5 = make_vector(set_size_1);
#elif 1		// big and medium vectors
std::vector<int> const vector_1 = make_vector(set_size_1);
std::vector<int> const vector_2 = make_vector(set_size_1);
std::vector<int> const vector_3 = make_vector(set_size_2);
std::vector<int> const vector_4 = make_vector(set_size_2);
std::vector<int> const vector_5 = make_vector(set_size_2);
#elif 1		// only big vectors
std::vector<int> const vector_1 = make_vector(set_size_1);
std::vector<int> const vector_2 = make_vector(set_size_1);
std::vector<int> const vector_3 = make_vector(set_size_1);
std::vector<int> const vector_4 = make_vector(set_size_1);
std::vector<int> const vector_5 = make_vector(set_size_1);
#elif 1		// only medium vectors
std::vector<int> const vector_1 = make_vector(set_size_2);
std::vector<int> const vector_2 = make_vector(set_size_2);
std::vector<int> const vector_3 = make_vector(set_size_2);
std::vector<int> const vector_4 = make_vector(set_size_2);
std::vector<int> const vector_5 = make_vector(set_size_2);
#elif 1		// only small vectors
std::vector<int> const vector_1 = make_vector(set_size_3);
std::vector<int> const vector_2 = make_vector(set_size_3);
std::vector<int> const vector_3 = make_vector(set_size_3);
std::vector<int> const vector_4 = make_vector(set_size_3);
std::vector<int> const vector_5 = make_vector(set_size_3);
#endif
