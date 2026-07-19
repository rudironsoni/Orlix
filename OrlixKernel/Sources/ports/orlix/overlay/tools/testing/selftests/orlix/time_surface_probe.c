// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define ORLIX_EARLIEST_SUPPORTED_REALTIME 1577836800LL
#define ORLIX_LATEST_SUPPORTED_REALTIME 4102444800LL

static bool timespec_is_normalized(const struct timespec *value)
{
	return value->tv_nsec >= 0 && value->tv_nsec < 1000000000L;
}

static bool realtime_is_sane(const struct timespec *value)
{
	return timespec_is_normalized(value) &&
	       value->tv_sec >= ORLIX_EARLIEST_SUPPORTED_REALTIME &&
	       value->tv_sec < ORLIX_LATEST_SUPPORTED_REALTIME;
}

static bool timeval_matches_realtime(const struct timeval *value,
				      const struct timespec *realtime)
{
	time_t delta;

	if (value->tv_usec < 0 || value->tv_usec >= 1000000)
		return false;
	delta = value->tv_sec - realtime->tv_sec;
	return delta >= -1 && delta <= 1;
}

static bool timespec_not_before(const struct timespec *after,
				const struct timespec *before)
{
	return after->tv_sec > before->tv_sec ||
	       (after->tv_sec == before->tv_sec &&
		after->tv_nsec >= before->tv_nsec);
}

static bool explicit_file_timestamp_round_trips(time_t realtime)
{
	static const char path[] = "/tmp/orlix-time-surface";
	struct timespec expected[2];
	struct stat st;
	int fd;
	bool passed;

	fd = open(path, O_CREAT | O_RDWR | O_TRUNC | O_CLOEXEC, 0600);
	if (fd < 0)
		return false;
	close(fd);

	expected[0].tv_sec = realtime - 24 * 60 * 60;
	expected[0].tv_nsec = 123456789;
	expected[1] = expected[0];
	errno = 0;
	passed = utimensat(AT_FDCWD, path, expected, 0) == 0 &&
		 stat(path, &st) == 0 &&
		 st.st_atim.tv_sec == expected[0].tv_sec &&
		 st.st_atim.tv_nsec == expected[0].tv_nsec &&
		 st.st_mtim.tv_sec == expected[1].tv_sec &&
		 st.st_mtim.tv_nsec == expected[1].tv_nsec;
	unlink(path);
	return passed;
}

int main(void)
{
	struct timespec realtime_before;
	struct timespec realtime_after;
	struct timespec monotonic_before;
	struct timespec monotonic_after;
	struct timeval wall_time;
	bool realtime_before_read;
	bool realtime_after_read;
	bool monotonic_before_read;
	bool monotonic_after_read;

	realtime_before_read =
		clock_gettime(CLOCK_REALTIME, &realtime_before) == 0;
	monotonic_before_read =
		clock_gettime(CLOCK_MONOTONIC, &monotonic_before) == 0;
	realtime_after_read =
		clock_gettime(CLOCK_REALTIME, &realtime_after) == 0;
	monotonic_after_read =
		clock_gettime(CLOCK_MONOTONIC, &monotonic_after) == 0;

	orlix_test_plan(7);
	orlix_test_result(realtime_before_read &&
			  realtime_is_sane(&realtime_before),
			  "CLOCK_REALTIME returns a normalized supported time");
	orlix_test_result(gettimeofday(&wall_time, NULL) == 0 &&
			  realtime_before_read &&
			  timeval_matches_realtime(&wall_time,
						     &realtime_before),
			  "gettimeofday agrees with CLOCK_REALTIME");
	orlix_test_result(realtime_before_read && realtime_after_read &&
			  timespec_not_before(&realtime_after,
					      &realtime_before),
			  "CLOCK_REALTIME does not move backwards");
	orlix_test_result(monotonic_before_read &&
			  timespec_is_normalized(&monotonic_before),
			  "CLOCK_MONOTONIC returns a normalized time");
	orlix_test_result(monotonic_before_read && monotonic_after_read &&
			  timespec_not_before(&monotonic_after,
					      &monotonic_before),
			  "CLOCK_MONOTONIC does not move backwards");
	orlix_test_result(realtime_before_read &&
			  explicit_file_timestamp_round_trips(
				  realtime_before.tv_sec),
			  "utimensat preserves an explicit prior-day timestamp");
	orlix_test_result(time(NULL) >= realtime_before.tv_sec - 1,
			  "time agrees with CLOCK_REALTIME");

	orlix_test_exit();
}
