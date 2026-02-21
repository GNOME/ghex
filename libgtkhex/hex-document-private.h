// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

G_BEGIN_DECLS

/*
 * @start: start offset of the payload, in bytes
 * @end: end offset of the payload, in bytes
 * @rep_len: amount of data to replace at @start, or 0 for data to be inserted
 *   without any overwriting
 * @lower_nibble: %TRUE if targetting the lower nibble (2nd hex digit) %FALSE
 *   if targetting the upper nibble (1st hex digit)
 * @insert: %TRUE if the operation should be insert mode, %FALSE if in
 *   overwrite mode
 * @type: [enum@Hex.ChangeType] representing the type of change (ie, a string
 *   or a single byte)
 * @v_string: string of the data representing a change, or %NULL
 * @v_byte: character representing a single byte to be changed, if applicable
 * @external_file_change: whether the change came externally (typically from
 *   another program) (Since: 4.10)
 */
struct _HexChangeData
{
	gint64 start, end;
	/* `replace length`: length to replace (overwrite); (0 to insert without
	 * overwriting) */
	size_t rep_len;
	gboolean lower_nibble;
	gboolean insert;
	HexChangeType type;
	char *v_string;
	char v_byte;
	gboolean external_file_change;
};

G_END_DECLS
