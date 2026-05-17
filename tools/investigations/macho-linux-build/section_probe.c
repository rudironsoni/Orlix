__attribute__((used, section("__TEXT,__init")))
void orlix_macho_section_probe_init(void)
{
}

__attribute__((used, section("__DATA,__initdata")))
unsigned long orlix_macho_section_probe_initdata = 1;

__attribute__((used, section("__DATA,__percpu")))
unsigned long orlix_macho_section_probe_percpu = 2;
