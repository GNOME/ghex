#!/bin/sh

xgettext --default-domain=ghex --directory=.. \
  --add-comments --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f ghex.po \
   || ( rm -f ./ghex.pot \
    && mv ghex.po ./ghex.pot )
