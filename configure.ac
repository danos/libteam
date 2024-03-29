#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([libteam], [1.11], [jiri@resnulli.us])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES(yes)], [])
AM_PROG_AR

# Here are a set of rules to help you update your library version information:
# 1. Start with version information of ‘0:0:0’ for each libtool library.
# 2. Update the version information only immediately before a public release
#    of your software. More frequent updates are unnecessary, and only guarantee
#    that the current interface number gets larger faster.
# 3. If the library source code has changed at all since the last update,
#    then increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
# 4. If any interfaces have been added, removed, or changed since the last
#    update, increment current, and set revision to 0.
# 5. If any interfaces have been added since the last public release, then
#    increment age.
# 6. If any interfaces have been removed or changed since the last public
#    release, then set age to 0.

AC_SUBST(LIBTEAM_CURRENT, 7)
AC_SUBST(LIBTEAM_REVISION, 0)
AC_SUBST(LIBTEAM_AGE, 2)

AC_SUBST(LIBTEAMDCTL_CURRENT, 1)
AC_SUBST(LIBTEAMDCTL_REVISION, 1)
AC_SUBST(LIBTEAMDCTL_AGE, 1)

CFLAGS="$CFLAGS -Wall"

# Checks for programs.
AC_PROG_CC
LT_INIT

PKG_CHECK_MODULES([LIBNL], [libnl-3.0 libnl-genl-3.0 libnl-route-3.0 libnl-cli-3.0])
	TMP_CFLAGS="$CFLAGS"
	TMP_LIBS="$LIBS"
	CFLAGS="$CPPFLAGS $LIBNL_CFLAGS"
	LIBS="$LIBS $LIBNL_LIBS"
	AC_CHECK_LIB([nl-route-3], [rtnl_link_get_phys_port_id],
		     AC_DEFINE(HAVE_RTNL_LINK_GET_PHYS_ID, [1], [Define to 1 if you have rtnl_link_get_phys_port_id function.]))
	AC_CHECK_LIB([nl-route-3], [rtnl_link_set_carrier],
		     AC_DEFINE(HAVE_RTNL_LINK_SET_CARRIER, [1], [Define to 1 if you have rtnl_link_set_carrier.]))
	AC_CHECK_LIB([nl-route-3], [rtnl_link_get_carrier],
		     AC_DEFINE(HAVE_RTNL_LINK_GET_CARRIER, [1], [Define to 1 if you have rtnl_link_get_carrier.]))
	CFLAGS="$TMP_CFLAGS"
	LIBS="$TMP_LIBS"

PKG_CHECK_MODULES([LIBDAEMON], [libdaemon])
PKG_CHECK_MODULES([JANSSON], [jansson])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE

# Checks for library functions.
AC_FUNC_MALLOC

AC_ARG_ENABLE([logging],
	AS_HELP_STRING([--disable-logging], [disable system logging @<:@default=enabled@:>@]),
	[], enable_logging=yes)
AS_IF([test "x$enable_logging" = "xyes"], [
	AC_DEFINE(ENABLE_LOGGING, [1], [System logging.])
])

AC_ARG_ENABLE([debug],
	AS_HELP_STRING([--enable-debug], [enable debug messages @<:@default=disabled@:>@]),
	[], [enable_debug=no])
AS_IF([test "x$enable_debug" = "xyes"], [
	AC_DEFINE(ENABLE_DEBUG, [1], [Debug messages.])
])

have_dbus=no
AC_ARG_ENABLE([dbus],
	AS_HELP_STRING([--disable-dbus], [disable D-Bus API @<:@default=enabled@:>@]))
	if test "x$enable_dbus" != "xno"; then
		PKG_CHECK_MODULES([DBUS], [dbus-1],
				  [AC_DEFINE(ENABLE_DBUS, [1], [D-Bus API.]) have_dbus=yes],
				  have_dbus=no)
	if test "x$have_dbus$enable_dbus" = xnoyes; then
		AC_MSG_ERROR([*** D-Bus support requested but libraries not found])
	fi
fi

have_zmq=no
AC_ARG_ENABLE([zmq],
	AS_HELP_STRING([--disable-zmq], [disable ZeroMQ API @<:@default=enabled@:>@]))
	if test "x$enable_zmq" != "xno"; then
		PKG_CHECK_MODULES([ZMQ], [libzmq >= 3.2.0],
				  [AC_DEFINE(ENABLE_ZMQ, [1], [ZMQ API.]) have_zmq=yes],
				  have_zmq=no)
	if test "x$have_zmq$enable_zmq" = xnoyes; then
		AC_MSG_ERROR([*** ZeroMQ support requested but libraries not found])
	fi
fi

AC_CONFIG_FILES([Makefile
include/Makefile \
libteam/Makefile \
libteamdctl/Makefile \
utils/Makefile \
teamd/Makefile \
man/Makefile \
libteam/libteam.pc \
libteamdctl/libteamdctl.pc \
binding/Makefile \
binding/python/Makefile \
binding/python/setup.py \
binding/python/team/Makefile \
binding/python/team/capi.i \
examples/Makefile])
AC_OUTPUT
