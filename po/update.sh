#!/bin/sh

xgettext --default-domain=gnome-utils --directory=.. \
  --add-comments --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f gnome-utils.po \
   || ( rm -f ./gnome-utils.pot \
    && mv gnome-utils.po ./gnome-utils.pot )
