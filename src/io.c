/*
 * io.c - block-wise IO routines for GHex
 * written by Jaka Mocnik <jaka.mocnik@kiss.uni-lj.si>
 */

#include <stdio.h>
#include <sys/stat.h>

#include "ghex.h"

FileEntry *open_file(gchar *name) {
  FileEntry *fe;
  struct stat stats;
  int i;

  /* hopefully using stat() works for all flavours of UNIX...
     don't know for sure, though */
  if(!stat(name, &stats)) {
    if(fe = (FileEntry *)malloc(sizeof(FileEntry))) {
      fe->len = stats.st_size;
      fe->hexedit = NULL;

      if(fe->contents = (guchar *)malloc(fe->len)) {
	if(fe->file_name = (gchar *)strdup(name)) {
	  for(i = strlen(fe->file_name); (i >= 0) && (fe->file_name[i] != '/'); i--)
	    ;
	  if(fe->file_name[i] == '/')
	    fe->path_end = &fe->file_name[i+1];
	  else
	    fe->path_end = fe->file_name;

	  if(fe->file = fopen(name, "r")) {
	    fe->cursor_pos = 0;
	    add_buffer_entry(fe);
	    g_slist_append(buffer_list, fe);
	    fe->len = fread(fe->contents, 1, fe->len, fe->file);
	    fclose(fe->file);
	    return fe;
	  }
	  free(fe->file_name);
	}
	free(fe->contents);
      }
      free(fe);
    }
  }

  return NULL;
}

void close_file(FileEntry *fe) {
  if(fe) {
#ifdef DEBUG
    printf("closing file %s\n", fe->file_name);
#endif
    remake_buffer_menu();
    if(fe->hexedit)
      remove_view(fe);

    if(fe->file_name)
      free(fe->file_name);
    if(fe->file)
      fclose(fe->file);
    if(fe->contents)
      free(fe->contents);

    free(fe);
  }
}

gint read_file(FileEntry *fe) {
  if(fe->file = fopen(fe->file_name, "r")) {
    fe->len = fread(fe->contents, 1, fe->len, fe->file);
    fclose(fe->file);
    return 0;
  }
  return 1;
}

gint write_file(FileEntry *fe) {
  if(fe->file = fopen(fe->file_name, "w")) {
    fwrite(fe->contents, 1, fe->len, fe->file);
    fclose(fe->file);
    return 0;
  }
  return 1;
}

gint compare_data(guchar *s1, guchar *s2, gint len) {
  while(len > 0) {
    if((*s1) != (*s2))
      return ((*s1) - (*s2));
    s1++;
    s2++;
    len--;
  }

  return 0;
}

/*
 * search functions return 0 if the string was successfully found and != 0 otherwise;
 * the offset of the first occurence of the searched-for string is returned in *found
 */
gint find_string_forward(FileEntry *fe, guint start, guchar *what, gint len, guint *found) {
  guint pos;

  pos = start;
  while(pos < fe->len) {
    if(compare_data(&fe->contents[pos], what, len) == 0) {
      *found = pos;
      return 0;
    }
    pos++;
  }

  return 1;
}

gint find_string_backward(FileEntry *fe, guint start, guchar *what, gint len, guint *found) {
  guint pos;

  pos = start;
  while(pos >= 0) {
    if(compare_data(&fe->contents[pos], what, len) == 0) {
      *found = pos;
      return 0;
    }
    pos--;
  }

  return 1;
}

