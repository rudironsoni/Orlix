COREUTILS_VERSION ?= 9.11
COREUTILS_GIT_URL ?= https://github.com/coreutils/coreutils.git
COREUTILS_GIT_REF ?= v$(COREUTILS_VERSION)
COREUTILS_GIT_COMMIT ?= c01fd163a47468a8296fb369f5233853bb551bb6
COREUTILS_GNULIB_GIT_URL ?= https://github.com/coreutils/gnulib.git

ORLIX_COREUTILS_PROGRAMS := [ b2sum base32 base64 basenc basename cat chcon chgrp chmod chown chroot cksum comm cp csplit cut date dd df dir dircolors dirname du echo env expand expr factor false fmt fold groups head hostid id install join link ln logname ls md5sum mkdir mkfifo mknod mktemp mv nice nl nohup nproc numfmt od paste pathchk pinky pr printenv printf ptx pwd readlink realpath rm rmdir runcon seq sha1sum sha224sum sha256sum sha384sum sha512sum shred shuf sleep sort split stat stty sum sync tac tail tee test timeout touch tr true truncate tsort tty uname unexpand uniq unlink users vdir wc who whoami yes
