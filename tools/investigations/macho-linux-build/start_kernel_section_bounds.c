typedef int (*orlix_probe_initcall_t)(void);

__attribute__((used, section("__ORLIX,__init"))) char __init_begin[1];
__attribute__((used, section("__ORLIX,__init"))) char __init_end[1];

__attribute__((used, section("__ORLIX,__initcall"))) orlix_probe_initcall_t __initcall_start[1];
__attribute__((used, section("__ORLIX,__initcall"))) orlix_probe_initcall_t __initcall0_start[1];
__attribute__((used, section("__ORLIX,__initcall"))) orlix_probe_initcall_t __initcall1_start[1];
__attribute__((used, section("__ORLIX,__initcall"))) orlix_probe_initcall_t __initcall2_start[1];
__attribute__((used, section("__ORLIX,__initcall"))) orlix_probe_initcall_t __initcall3_start[1];
__attribute__((used, section("__ORLIX,__initcall"))) orlix_probe_initcall_t __initcall4_start[1];
__attribute__((used, section("__ORLIX,__initcall"))) orlix_probe_initcall_t __initcall5_start[1];
__attribute__((used, section("__ORLIX,__initcall"))) orlix_probe_initcall_t __initcall6_start[1];
__attribute__((used, section("__ORLIX,__initcall"))) orlix_probe_initcall_t __initcall7_start[1];
__attribute__((used, section("__ORLIX,__initcall"))) orlix_probe_initcall_t __initcall_end[1];

__attribute__((used, section("__ORLIX,__setup"))) char __setup_start[1];
__attribute__((used, section("__ORLIX,__setup"))) char __setup_end[1];

__attribute__((used, section("__ORLIX,__param"))) char __start___param[1];
__attribute__((used, section("__ORLIX,__param"))) char __stop___param[1];
