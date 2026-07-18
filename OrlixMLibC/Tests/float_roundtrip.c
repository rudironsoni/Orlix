// SPDX-License-Identifier: MIT
#include <errno.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int decimal_exponent(const char *text, int *exponent)
{
	const char *marker = strchr(text, 'e');
	char *end;
	long value;

	if (!marker)
		marker = strchr(text, 'E');
	if (!marker) {
		*exponent = 0;
		return 0;
	}

	value = strtol(marker + 1, &end, 10);
	if (end == marker + 1 || *end || value < -100000 || value > 100000)
		return -1;
	*exponent = (int)value;
	return 0;
}

static int check_float(const char *name, float value)
{
	char text[128];
	int exponent;

	for (int precision = FLT_DIG; precision <= FLT_DECIMAL_DIG; precision++) {
		if (snprintf(text, sizeof(text), "%.*g", precision,
				(double)value) < 0)
			return 1;
		if (decimal_exponent(text, &exponent) ||
		    exponent < FLT_MIN_10_EXP - FLT_DECIMAL_DIG ||
		    exponent > FLT_MAX_10_EXP + FLT_DECIMAL_DIG) {
			printf("# float_roundtrip %s invalid text=%s\n", name, text);
			return 1;
		}
		if (strtof(text, NULL) == value)
			return 0;
	}

	printf("# float_roundtrip %s did not round trip text=%s source=%a parsed=%a\n",
	       name, text, (double)value, (double)strtof(text, NULL));
	return 1;
}

static int check_double(const char *name, double value)
{
	char text[128];
	int exponent;

	for (int precision = DBL_DIG; precision <= DBL_DECIMAL_DIG; precision++) {
		if (snprintf(text, sizeof(text), "%.*g", precision, value) < 0)
			return 1;
		if (decimal_exponent(text, &exponent) ||
		    exponent < DBL_MIN_10_EXP - DBL_DECIMAL_DIG ||
		    exponent > DBL_MAX_10_EXP + DBL_DECIMAL_DIG) {
			printf("# float_roundtrip %s invalid text=%s\n", name, text);
			return 1;
		}
		if (strtod(text, NULL) == value)
			return 0;
	}

	printf("# float_roundtrip %s did not round trip text=%s source=%a parsed=%a\n",
	       name, text, value, strtod(text, NULL));
	return 1;
}

static int check_long_double(const char *name, long double value)
{
	char text[128];
	int exponent;
	long double parsed = 0.0L;

	for (int precision = LDBL_DIG; precision <= LDBL_DECIMAL_DIG; precision++) {
		if (snprintf(text, sizeof(text), "%.*Lg", precision, value) < 0)
			return 1;
		if (decimal_exponent(text, &exponent) ||
		    exponent < LDBL_MIN_10_EXP - LDBL_DECIMAL_DIG ||
		    exponent > LDBL_MAX_10_EXP + LDBL_DECIMAL_DIG) {
			printf("# float_roundtrip %s invalid text=%s\n", name, text);
			return 1;
		}
		parsed = strtold(text, NULL);
		if (parsed == value)
			return 0;
	}
	if (__builtin_isinf(parsed))
		printf("# float_roundtrip %s parsed as infinity text=%s\n", name, text);
	else if (parsed < value)
		printf("# float_roundtrip %s parsed below source text=%s source=%La parsed=%La\n",
		       name, text, value, parsed);
	else
		printf("# float_roundtrip %s parsed above source text=%s source=%La parsed=%La\n",
		       name, text, value, parsed);

	return 1;
}

static int check_bounded_exponents(void)
{
	const char *overflow = "1e2147483647tail";
	const char *underflow = "1e-2147483647tail";
	char *end;
	int failures = 0;

	errno = 0;
	if (!__builtin_isinf(strtold(overflow, &end)) || strcmp(end, "tail")) {
		printf("# float_roundtrip huge positive exponent was not consumed as infinity errno=%d\n",
		       errno);
		failures++;
	}
	if (strtold(underflow, &end) != 0.0L || strcmp(end, "tail")) {
		printf("# float_roundtrip huge negative exponent was not consumed as zero\n");
		failures++;
	}

	return failures;
}

static int check_basic_decimal(void)
{
	static const struct {
		const char *text;
		long double expected;
	} cases[] = {
		{ "1", 1.0L },
		{ "1.5", 1.5L },
		{ "1e1", 10.0L },
	};
	int failures = 0;

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		char *end;
		long double parsed;

		errno = 0;
		parsed = strtold(cases[i].text, &end);
		if (parsed == cases[i].expected && !*end)
			continue;
		printf("# float_roundtrip basic text=%s expected=%La parsed=%La end=%s errno=%d\n",
		       cases[i].text, cases[i].expected, parsed, end, errno);
		failures++;
	}
	return failures;
}

int main(void)
{
	int failures = 0;

	failures += check_basic_decimal();
	failures += check_bounded_exponents();
	failures += check_float("FLT_MIN", FLT_MIN);
	failures += check_float("FLT_MAX", FLT_MAX);
	failures += check_double("DBL_MIN", DBL_MIN);
	failures += check_double("DBL_MAX", DBL_MAX);
	failures += check_long_double("LDBL_MIN", LDBL_MIN);
	failures += check_long_double("LDBL_MAX", LDBL_MAX);
	return failures ? 1 : 0;
}
