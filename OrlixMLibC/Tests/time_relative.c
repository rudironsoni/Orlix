// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int check_tm(const char *name, const struct tm *actual,
		int year, int month, int day, int hour, int minute, int second)
{
	if (actual->tm_year == year - 1900 &&
			actual->tm_mon == month - 1 &&
			actual->tm_mday == day &&
			actual->tm_hour == hour &&
			actual->tm_min == minute &&
			actual->tm_sec == second)
		return 0;

	printf("# %s actual=%04d-%02d-%02dT%02d:%02d:%02d "
			"expected=%04d-%02d-%02dT%02d:%02d:%02d\n",
			name,
			actual->tm_year + 1900, actual->tm_mon + 1, actual->tm_mday,
			actual->tm_hour, actual->tm_min, actual->tm_sec,
			year, month, day, hour, minute, second);
	return 1;
}

static int set_timezone(const char *timezone_name)
{
	if (setenv("TZ", timezone_name, 1)) {
		perror("setenv");
		return 1;
	}
	tzset();
	return 0;
}

static int check_pre_epoch_utc(void)
{
	const time_t timestamp = -1;
	struct tm actual;

	if (!gmtime_r(&timestamp, &actual)) {
		perror("gmtime_r");
		return 1;
	}
	if (check_tm("pre_epoch_utc", &actual, 1969, 12, 31, 23, 59, 59))
		return 1;
	if (actual.tm_wday != 3 || actual.tm_yday != 364) {
		printf("# pre_epoch_utc wday=%d yday=%d "
				"expected_wday=3 expected_yday=364\n",
				actual.tm_wday, actual.tm_yday);
		return 1;
	}
	return 0;
}

static int check_pre_epoch_local(void)
{
	const time_t timestamp = -1;
	struct tm actual;

	if (set_timezone("PST8"))
		return 1;
	if (!localtime_r(&timestamp, &actual)) {
		perror("localtime_r");
		return 1;
	}
	if (check_tm("pre_epoch_local", &actual, 1969, 12, 31, 15, 59, 59))
		return 1;
	if (actual.tm_isdst != 0 || actual.tm_gmtoff != -8 * 60 * 60 ||
			!actual.tm_zone || strcmp(actual.tm_zone, "PST")) {
		printf("# pre_epoch_local isdst=%d gmtoff=%ld zone=%s\n",
				actual.tm_isdst, actual.tm_gmtoff,
				actual.tm_zone ? actual.tm_zone : "(null)");
		return 1;
	}
	return 0;
}

static int check_fixed_offset_mktime(void)
{
	struct tm local = {
		.tm_year = 1997 - 1900,
		.tm_mon = 7,
		.tm_mday = 1,
		.tm_hour = 6,
		.tm_isdst = -1,
	};
	struct tm expected_utc = {
		.tm_year = 1997 - 1900,
		.tm_mon = 7,
		.tm_mday = 1,
		.tm_hour = 10,
	};
	time_t expected;
	time_t actual;

	if (set_timezone("UTC+4"))
		return 1;
	expected = timegm(&expected_utc);
	actual = mktime(&local);
	if (actual != expected) {
		printf("# fixed_offset_mktime actual=%lld expected=%lld\n",
				(long long)actual, (long long)expected);
		return 1;
	}
	if (check_tm("fixed_offset_mktime", &local, 1997, 8, 1, 6, 0, 0))
		return 1;
	if (local.tm_isdst != 0 || local.tm_gmtoff != -4 * 60 * 60)
		return 1;
	return 0;
}

static int check_dst_mktime(void)
{
	struct tm local = {
		.tm_year = 2006 - 1900,
		.tm_mon = 3,
		.tm_mday = 23,
		.tm_hour = 12,
		.tm_isdst = -1,
	};
	struct tm round_trip;
	time_t timestamp;

	if (set_timezone("PST8PDT,M4.1.0,M10.5.0"))
		return 1;
	timestamp = mktime(&local);
	if (timestamp == (time_t)-1) {
		perror("mktime");
		return 1;
	}
	if (!localtime_r(&timestamp, &round_trip)) {
		perror("localtime_r");
		return 1;
	}
	if (check_tm("dst_mktime", &round_trip, 2006, 4, 23, 12, 0, 0))
		return 1;
	if (local.tm_isdst != 1 || round_trip.tm_isdst != 1 ||
			local.tm_gmtoff != -7 * 60 * 60 ||
			round_trip.tm_gmtoff != -7 * 60 * 60) {
		printf("# dst_mktime local_isdst=%d round_trip_isdst=%d "
				"local_gmtoff=%ld round_trip_gmtoff=%ld\n",
				local.tm_isdst, round_trip.tm_isdst,
				local.tm_gmtoff, round_trip.tm_gmtoff);
		return 1;
	}
	return 0;
}

static int check_utc_relative_day(void)
{
	const time_t reference = 1784473200;
	struct tm local;
	struct tm previous;
	time_t round_trip;
	time_t yesterday;

	if (set_timezone("UTC0"))
		return 1;
	if (!localtime_r(&reference, &local)) {
		perror("localtime_r");
		return 1;
	}

	previous = local;
	local.tm_isdst = -1;
	round_trip = mktime(&local);
	if (round_trip != reference) {
		printf("# time_relative round_trip=%lld expected=%lld\n",
				(long long)round_trip, (long long)reference);
		return 1;
	}

	previous.tm_mday--;
	previous.tm_isdst = -1;
	yesterday = mktime(&previous);
	if (yesterday != reference - 24 * 60 * 60) {
		printf("# time_relative yesterday=%lld expected=%lld\n",
				(long long)yesterday,
				(long long)(reference - 24 * 60 * 60));
		return 1;
	}

	return 0;
}

static int check_gregorian_yearday(void)
{
	static const struct {
		int year;
		int expected_yday;
	} cases[] = {
		{1900, 59},
		{2000, 60},
		{2100, 59},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct tm value = {
			.tm_year = cases[i].year - 1900,
			.tm_mon = 2,
			.tm_mday = 1,
		};

		if (timegm(&value) == (time_t)-1) {
			perror("timegm");
			return 1;
		}
		if (value.tm_yday != cases[i].expected_yday) {
			printf("# gregorian_yearday year=%d yday=%d expected=%d\n",
					cases[i].year, value.tm_yday,
					cases[i].expected_yday);
			return 1;
		}
	}

	return 0;
}

int main(void)
{
	int failures = 0;

	failures += check_pre_epoch_utc();
	failures += check_pre_epoch_local();
	failures += check_fixed_offset_mktime();
	failures += check_dst_mktime();
	failures += check_utc_relative_day();
	failures += check_gregorian_yearday();
	return failures != 0;
}
