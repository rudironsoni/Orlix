/* Test-fixture helper for TinyCC. OrlixMLibC compiler-rt has no __clear_cache. */
void __clear_cache(void *start, void *end)
{
	(void)start;
	(void)end;
}
