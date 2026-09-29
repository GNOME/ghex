// vim: linebreak breakindent breakindentopt=shift\:4

#pragma once

/**
 * HexGroupType:
 * @HEX_GROUP_BYTE: group data by byte (8-bit)
 * @HEX_GROUP_WORD: group data by word (16-bit)
 * @HEX_GROUP_LONG: group data by long (32-bit)
 * @HEX_GROUP_QUAD: group data by quadword (64-bit)
 *
 * Specifies how hexadecimal data is to be grouped.
 */
typedef enum
{
	HEX_GROUP_INVALID =		0,
	HEX_GROUP_BYTE =		1,
	HEX_GROUP_WORD =		2,
	HEX_GROUP_LONG =		4,
	HEX_GROUP_QUAD =		8
} HexGroupType;
