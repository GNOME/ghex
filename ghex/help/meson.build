#(cd C; for i in *.{page,xml}; do echo "  '$i',"; done;)
help_pages = [
  'advanced-clipboard.page',
  'advanced-marks.page',
  'basics-create-new-file.page',
  'basics-edit-file.page',
  'basics-find-replace.page',
  'basics-open-file.page',
  'basics-save-file.page',
  'conf-grouptype.page',
  'conf-preferences.page',
  'index.page',
  'tools-chartable.page',
  'tools-converter.page',
  'legal.xml',
]
# TODO: figures

gnome.yelp(
  meson.project_name(),
  sources: help_pages,
)
