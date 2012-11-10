#ifndef UTILS_H_
#define UTILS_H_

template <typename T>
static inline T div_round_up(T a, T b) {
	return (a + b - 1) / b;
}

template <typename T>
static inline T align(T a, T b) {
	while (a % b) {
		++a;
	}
	return a;
}

#endif
