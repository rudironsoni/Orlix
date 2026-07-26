// SPDX-License-Identifier: GPL-2.0
#include <stdint.h>

#include "orlix_kselftest_user.h"

struct vector128 {
	uint64_t low;
	uint64_t high;
};

static void execute_sha512h(struct vector128 *result)
{
	const struct vector128 destination = {
		0x0123456789abcdefULL, 0xfedcba9876543210ULL,
	};
	const struct vector128 source_n = {
		0x13579bdf2468ace0ULL, 0x0badf00ddeadbeefULL,
	};
	const struct vector128 source_m = {
		0x1111111122222222ULL, 0x3333333344444444ULL,
	};

	asm volatile(
		"ldr q0, [%[destination]]\n"
		"ldr q1, [%[source_n]]\n"
		"ldr q2, [%[source_m]]\n"
		".inst 0xce628020\n" /* sha512h q0, q1, v2.2d */
		"str q0, [%[result]]\n"
		:
		: [result] "r" (result), [destination] "r" (&destination),
		  [source_n] "r" (&source_n), [source_m] "r" (&source_m)
		: "v0", "v1", "v2", "memory");
}

static void execute_eor3(struct vector128 *result)
{
	const struct vector128 source_n = {
		0x0123456789abcdefULL, 0xfedcba9876543210ULL,
	};
	const struct vector128 source_m = {
		0x1111222233334444ULL, 0x5555666677778888ULL,
	};
	const struct vector128 source_a = {
		0x00ff00ff00ff00ffULL, 0xff00ff00ff00ff00ULL,
	};

	asm volatile(
		"ldr q1, [%[source_n]]\n"
		"ldr q2, [%[source_m]]\n"
		"ldr q3, [%[source_a]]\n"
		".inst 0xce020c20\n" /* eor3 v0.16b, v1.16b, v2.16b, v3.16b */
		"str q0, [%[result]]\n"
		:
		: [result] "r" (result), [source_n] "r" (&source_n),
		  [source_m] "r" (&source_m), [source_a] "r" (&source_a)
		: "v0", "v1", "v2", "v3", "memory");
}

static void execute_sm3partw1(struct vector128 *result)
{
	const struct vector128 destination = {
		0x89abcdef01234567ULL, 0x76543210fedcba98ULL,
	};
	const struct vector128 source_n = {
		0x2468ace013579bdfULL, 0xdeadbeef0badf00dULL,
	};
	const struct vector128 source_m = {
		0x2222222211111111ULL, 0x4444444433333333ULL,
	};

	asm volatile(
		"ldr q0, [%[destination]]\n"
		"ldr q1, [%[source_n]]\n"
		"ldr q2, [%[source_m]]\n"
		".inst 0xce62c020\n" /* sm3partw1 v0.4s, v1.4s, v2.4s */
		"str q0, [%[result]]\n"
		:
		: [result] "r" (result), [destination] "r" (&destination),
		  [source_n] "r" (&source_n),
		  [source_m] "r" (&source_m)
		: "v0", "v1", "v2", "memory");
}

static void execute_sm4e(struct vector128 *result)
{
	const struct vector128 destination = {
		0x89abcdef01234567ULL, 0x76543210fedcba98ULL,
	};
	const struct vector128 source = {
		0x2468ace013579bdfULL, 0xdeadbeef0badf00dULL,
	};

	asm volatile(
		"ldr q0, [%[destination]]\n"
		"ldr q1, [%[source]]\n"
		".inst 0xcec08420\n" /* sm4e v0.4s, v1.4s */
		"str q0, [%[result]]\n"
		:
		: [result] "r" (result), [destination] "r" (&destination),
		  [source] "r" (&source)
		: "v0", "v1", "memory");
}

static void execute_aese(struct vector128 *result)
{
	const struct vector128 destination = {
		0x0706050403020100ULL, 0x0f0e0d0c0b0a0908ULL,
	};
	const struct vector128 round_key = {
		0xbfc6cdd4dbe2e9f0ULL, 0x878e959ca3aab1b8ULL,
	};

	asm volatile(
		"ldr q0, [%[destination]]\n"
		"ldr q1, [%[round_key]]\n"
		".inst 0x4e284820\n" /* aese v0.16b, v1.16b */
		"str q0, [%[result]]\n"
		:
		: [result] "r" (result), [destination] "r" (&destination),
		  [round_key] "r" (&round_key)
		: "v0", "v1", "memory");
}

static void execute_pmull(struct vector128 *result)
{
	const struct vector128 source_n = {
		0x8000000000000001ULL, 0,
	};
	const struct vector128 source_m = {
		3, 0,
	};

	asm volatile(
		"ldr q1, [%[source_n]]\n"
		"ldr q2, [%[source_m]]\n"
		".inst 0x0ee2e020\n" /* pmull v0.1q, v1.1d, v2.1d */
		"str q0, [%[result]]\n"
		:
		: [result] "r" (result), [source_n] "r" (&source_n),
		  [source_m] "r" (&source_m)
		: "v0", "v1", "v2", "memory");
}

static void execute_sha1h(struct vector128 *result)
{
	const struct vector128 source = { 0x12345678ULL, 0 };

	asm volatile(
		"ldr q1, [%[source]]\n"
		".inst 0x5e280820\n" /* sha1h s0, s1 */
		"str q0, [%[result]]\n"
		:
		: [result] "r" (result), [source] "r" (&source)
		: "v0", "v1", "memory");
}

static void execute_sha256h(struct vector128 *result)
{
	const struct vector128 destination = {
		0x89abcdef01234567ULL, 0x76543210fedcba98ULL,
	};
	const struct vector128 source_n = {
		0x13579bdfdeadbeefULL, 0x0badf00d2468ace0ULL,
	};
	const struct vector128 source_m = {
		0x2222222211111111ULL, 0x4444444433333333ULL,
	};

	asm volatile(
		"ldr q0, [%[destination]]\n"
		"ldr q1, [%[source_n]]\n"
		"ldr q2, [%[source_m]]\n"
		".inst 0x5e024020\n" /* sha256h q0, q1, v2.4s */
		"str q0, [%[result]]\n"
		:
		: [result] "r" (result), [destination] "r" (&destination),
		  [source_n] "r" (&source_n), [source_m] "r" (&source_m)
		: "v0", "v1", "v2", "memory");
}

static uint64_t execute_crc32x(void)
{
	uint64_t result;

	asm volatile(
		"mov w1, %w[accumulator]\n"
		"mov x2, %[value]\n"
		".inst 0x9ac24c20\n" /* crc32x w0, w1, x2 */
		"mov %[result], x0\n"
		: [result] "=r" (result)
		: [accumulator] "r" (0x12345678U),
		  [value] "r" (0x8877665544332211ULL)
		: "x0", "x1", "x2");
	return result;
}

int main(void)
{
	static const struct vector128 sha512_expected = {
		0xf0f47a5b26e1bdb2ULL, 0xb4206fda37564a94ULL,
	};
	static const struct vector128 sha3_expected = {
		0x10cd67baba678954ULL, 0x548923fefe234598ULL,
	};
	static const struct vector128 sm3_expected = {
		0x030bcfc73030fcfcULL, 0x6f1f3c6e38e32aaaULL,
	};
	static const struct vector128 sm4_expected = {
		0xb349299b5c86dacaULL, 0xee5ae58e5f9dc081ULL,
	};
	static const struct vector128 aes_expected = {
		0x61cd6c70c4e0e88cULL, 0xc2ba9b606ce146e7ULL,
	};
	static const struct vector128 pmull_expected = {
		0x8000000000000003ULL, 1,
	};
	static const struct vector128 sha1_expected = { 0x048d159eULL, 0 };
	static const struct vector128 sha2_expected = {
		0x0f260be68877d7f6ULL, 0x7373c2c6750180f4ULL,
	};
	struct vector128 result;

	orlix_test_plan(9);

	execute_sha512h(&result);
	orlix_test_result(orlix_memcmp(&result, &sha512_expected,
				       sizeof(result)) == 0,
			  "SHA512 executes through OrlixTCTI");

	execute_eor3(&result);
	orlix_test_result(orlix_memcmp(&result, &sha3_expected,
				       sizeof(result)) == 0,
			  "SHA3 executes through OrlixTCTI");

	execute_sm3partw1(&result);
	orlix_test_result(orlix_memcmp(&result, &sm3_expected,
				       sizeof(result)) == 0,
			  "SM3 executes through OrlixTCTI");

	execute_sm4e(&result);
	orlix_test_result(orlix_memcmp(&result, &sm4_expected,
				       sizeof(result)) == 0,
			  "SM4 executes through OrlixTCTI");

	execute_aese(&result);
	orlix_test_result(orlix_memcmp(&result, &aes_expected,
				       sizeof(result)) == 0,
			  "AES executes through OrlixTCTI");

	execute_pmull(&result);
	orlix_test_result(orlix_memcmp(&result, &pmull_expected,
				       sizeof(result)) == 0,
			  "PMULL executes through OrlixTCTI");

	execute_sha1h(&result);
	orlix_test_result(orlix_memcmp(&result, &sha1_expected,
				       sizeof(result)) == 0,
			  "SHA1 executes through OrlixTCTI");

	execute_sha256h(&result);
	orlix_test_result(orlix_memcmp(&result, &sha2_expected,
				       sizeof(result)) == 0,
			  "SHA2 executes through OrlixTCTI");

	orlix_test_result(execute_crc32x() == 0x4e41e95aU,
			  "CRC32 executes through OrlixTCTI");

	orlix_test_exit();
}
