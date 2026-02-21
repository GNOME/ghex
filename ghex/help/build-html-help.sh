#!/bin/bash

# build-html-help.sh -- helper script to build static HTML help for GHex
# by: Logan Rathbone, (C) 2021, GPLv2+ (see COPYING)
#
# USAGE: copy to @datadir@ *after* ninja install, and run it from within
# @datadir@ as the CWD.
# Eg, `cd /tmp/DESTDIR/usr/share; ./build-html-help.sh`
#
# DEPENDENCIES:  `yelp-build` binary.
#
# README:
# Here's another instance of manual intervention being required in order
# to make our users' and distributors' lives easier.
#
# Since the translated mallard .page files don't get translated until
# the `ninja install` phase, it makes it difficult to generate static
# HTML help, fully translated, at build time. Since static HTML help is
# just a fallback anyway, we're not always going to remember to run this
# script, but we're going to make sure we remember before shipping
# a tarball!

CWD=`pwd`

if [ ! -d $CWD/help ]; then
	echo ERROR: No help found. Exiting.
	exit 1
fi

if [ -f $CWD/LINGUAS ]; then rm $CWD/LINGUAS; fi
(cd help; for i in *; do echo $i >> $CWD/LINGUAS; done)
if [ -d $CWD/HTML ]; then rm -rf $CWD/HTML; fi
mkdir $CWD/HTML

echo "Generating HTML help..."

for i in `cat $CWD/LINGUAS`; do
	mkdir -p $CWD/HTML/$i/ghex
	(cd $CWD/HTML/$i/ghex; yelp-build html $CWD/help/$i/ghex)
done

if [ ! -d $CWD/HTML/en ]; then
	cp -R $CWD/HTML/C/ $CWD/HTML/en
fi

# I have no idea what this .js file does, but it takes up a lot of space,
# and without it, help still displays fine and none of yelp, khelpcenter,
# chrome or firefox even spew an error. So guess what, if you don't have
# yelp and are using static HTML help, you're not going to get whatever
# razzly-dazzly effects it may provide.
find $CWD/HTML -name 'highlight.pack.js' |xargs rm

tar czf HTML.tar.gz HTML/

# cleanup
rm $CWD/LINGUAS
rm -rf $CWD/HTML

echo "DONE. You may now deploy the './HTML.tar.gz' tarball in the help/ directory of the GIT repo as static HTML help."
