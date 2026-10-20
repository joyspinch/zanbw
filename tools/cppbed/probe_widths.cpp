// Probe: the exact width of the C++ fixed_point raw types on this toolchain.
// This decides whether fp8 arithmetic wraps at 32 or 64 bits, and whether
// direction_t's from_raw()/wrap() truncate to 8 bits.
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>

int main() {
	printf("int_fast8_t  = %d bytes\n", (int)sizeof(int_fast8_t));
	printf("int_fast16_t = %d bytes\n", (int)sizeof(int_fast16_t));
	printf("int_fast32_t = %d bytes\n", (int)sizeof(int_fast32_t));
	printf("int_fast64_t = %d bytes\n", (int)sizeof(int_fast64_t));
	printf("uint_fast32_t = %d bytes\n", (int)sizeof(uint_fast32_t));

	// int_bits<T> = numeric_limits<T>::digits + is_signed
	printf("int_bits<int_fast8_t>  = %d\n",
		(int)std::numeric_limits<int_fast8_t>::digits + 1);
	printf("int_bits<int_fast32_t> = %d\n",
		(int)std::numeric_limits<int_fast32_t>::digits + 1);

	// direction_t::from_raw(200) with raw_type = int_fastn_t<8>
	{
		using int_fastn_t = std::conditional<8 <= 8, std::int_fast8_t, int>::type;
		int_fastn_t v = (int_fastn_t)200;
		printf("direction from_raw(200) via int_fast8_t = %d\n", (int)v);
	}
	{
		// emulate from_raw for a 4-byte raw type: (raw << 24) >> 24
		int raw = 200;
		printf("direction from_raw(200) via 4-byte raw = %d\n", (int)((raw << 24) >> 24));
	}
	return 0;
}
