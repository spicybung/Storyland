-----BEGIN PGP SIGNED MESSAGE-----
Hash: SHA512

Format: 3.0 (quilt)
Source: espeak-ng
Binary: espeak-ng, espeak-ng-espeak, espeak-ng-data, espeak-ng-data-udeb, libespeak-ng1, libespeak-ng-libespeak1, libespeak-ng-dev, libespeak-ng-libespeak-dev
Architecture: any all
Version: 1.52.0+dfsg-5
Maintainer: Debian Accessibility Team <pkg-a11y-devel@alioth-lists.debian.net>
Uploaders: Samuel Thibault <sthibault@debian.org>
Homepage: https://github.com/espeak-ng/espeak-ng
Standards-Version: 4.6.1
Vcs-Browser: https://salsa.debian.org/a11y-team/espeak-ng
Vcs-Git: https://salsa.debian.org/a11y-team/espeak-ng.git
Testsuite: autopkgtest
Testsuite-Triggers: @builddeps@, clang, libclang-rt-dev
Build-Depends: debhelper-compat (= 13), dh-exec, libpcaudio-dev, libsonic-dev, ronn, kramdown
Package-List:
 espeak-ng deb sound optional arch=any
 espeak-ng-data deb sound optional arch=any
 espeak-ng-data-udeb udeb debian-installer optional arch=any
 espeak-ng-espeak deb sound optional arch=all
 libespeak-ng-dev deb libdevel optional arch=any
 libespeak-ng-libespeak-dev deb libdevel optional arch=any
 libespeak-ng-libespeak1 deb libs optional arch=any
 libespeak-ng1 deb libs optional arch=any
Checksums-Sha1:
 5f7cbff586612db6f22bec47d6beb83d401c26fe 16464682 espeak-ng_1.52.0+dfsg.orig.tar.gz
 0e3bf35a3a25959b819757ae2f8c7204af938448 50116 espeak-ng_1.52.0+dfsg-5.debian.tar.xz
Checksums-Sha256:
 d2a886bae3547a15c52801cb37e415230834a7ee7f9a57352b98d60454d97cea 16464682 espeak-ng_1.52.0+dfsg.orig.tar.gz
 d253d81d2815194e2c13c1af77b6761c43db48848ca423430b4a8c416359b813 50116 espeak-ng_1.52.0+dfsg-5.debian.tar.xz
Files:
 c19f54ac5d9d53b4b0a3454848893c1d 16464682 espeak-ng_1.52.0+dfsg.orig.tar.gz
 d488e7f898aa71982e93ab4920388103 50116 espeak-ng_1.52.0+dfsg-5.debian.tar.xz

-----BEGIN PGP SIGNATURE-----

iQIzBAEBCgAdFiEEZTSF1IMOAGwT71n/aHTOWK4tfj8FAmfrBb4ACgkQaHTOWK4t
fj8ysQ/+MTZTO+qVI9OEP4ReNLTzYTcFbWr4uASfYonaCcQe2AnEraPs1nHyFyOM
4AiGbs2Z4FZ08L2lkHIAhpGeWSx8oIsdvPNZrpXml6wREx+HUehGG7/E8L9f5/6n
t/NVV3Hc4JIXQUE2diJ1E3G7Ny8d8mPRivGPKQDxzbtY78tVYXDEaXO6u9IGRen+
jPicsmUHgWtsy84aLciLpX1PqG+s2qtZlEaTh840P/NwmBFRojrldyPCO4vPAdfh
Hod5wjPXrPcHM26JTAlEU9hGD0+5wW1SNCBllkW2RJ3lAk3vUJeU2qCB+WunjLqk
4oY6m+xAV6NCLS15sOaQYPCSNfA2DYC1Tfao8D/WjE3pxLjfh3IuqLqGumsVIAU2
ojnB4GBwdPX4IoCvIQhqUMpC1Rc0sY+3yz0SClm167i9lsM5KStRBpNKjQ/TRlgy
GoVAU2nI5FTdv2a9s25KiStWt+kOy1/anPi5umwLEjSkYb4gtQ75dhHvupvlSLw6
HU9rTzI+FbjfXIsE1recIAP0iZAaEk+2BUxNklPvDGkGVPfeoetLkvIzDq4OVyLq
Cz1vR4kRT/9wXgLqeAeGdivyUhJd49ZrKlIr78Ay2ZqmEQ1a+5AcH3mFWPyEg/Tw
QV/Vw/2XkR/HNu7b5bhOZqgiuqSPWHhTBYUmpcbgEoaPQ5GPIto=
=1ieQ
-----END PGP SIGNATURE-----
