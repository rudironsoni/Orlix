"""Keep one guest package's configure inputs off every other package and rootfs."""

_PACKAGE_SOURCE_MARKERS = {
    "acl": "orlix_acl_source/",
    "attr": "orlix_attr_source/",
    "bash": "orlix_bash_source/",
    "coreutils": "orlix_coreutils_source/",
    "curl": "orlix_curl_source/",
    "e2fsprogs": "orlix_e2fsprogs_source/",
    "findutils": "orlix_findutils_source/",
    "grep": "orlix_grep_source/",
    "jq": "orlix_jq_source/",
    "libcap": "orlix_libcap_source/",
    "libselinux": "orlix_libselinux_source/",
    "libsepol": "orlix_libsepol_source/",
    "musl-fts": "orlix_musl_fts_source/",
    "ncurses": "orlix_ncurses_source/",
    "pcre2": "orlix_pcre2_source/",
    "zsh": "orlix_zsh_source/",
}

def guest_configure_inputs(package_name, files):
    """Return configure inputs for one package.

    A source file from another guest package, or the rootfs assembly policy,
    fails the action instead of becoming a configure input.
    """
    owner = _PACKAGE_SOURCE_MARKERS.get(package_name)
    if owner == None:
        fail("unknown guest package configure owner: %s" % package_name)
    selected = []
    for file in files:
        path = file.path
        if path.endswith("assembly_policy.txt"):
            fail("rootfs policy is not a %s configure input: %s" % (package_name, path))
        for other in _PACKAGE_SOURCE_MARKERS:
            marker = _PACKAGE_SOURCE_MARKERS[other]
            if other != package_name and marker in path:
                fail("foreign package source is not a %s configure input: %s" % (package_name, path))
        selected.append(file)
    return selected
