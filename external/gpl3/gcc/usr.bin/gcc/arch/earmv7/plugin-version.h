/* This file is automatically generated.  DO NOT EDIT! */
/* Generated from: NetBSD: mknative-gcc,v 1.78 2014/03/02 04:58:20 mrg Exp  */
/* Generated from: NetBSD: mknative.common,v 1.11 2014/02/17 21:39:43 christos Exp  */

#include "configargs.h"

#define GCCPLUGIN_VERSION_MAJOR   4
#define GCCPLUGIN_VERSION_MINOR   8
#define GCCPLUGIN_VERSION_PATCHLEVEL   3
#define GCCPLUGIN_VERSION  (GCCPLUGIN_VERSION_MAJOR*1000 + GCCPLUGIN_VERSION_MINOR)

static char basever[] = "4.8.3";
static char datestamp[] = "20140302";
static char devphase[] = "prerelease";
static char revision[] = "";

/* FIXME plugins: We should make the version information more precise.
   One way to do is to add a checksum. */

static struct plugin_gcc_version gcc_version = {basever, datestamp,
						devphase, revision,
						configuration_arguments};
