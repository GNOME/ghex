#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gi18n.h>

#define FMT_CMD				"(iv)"

#define FMT_NEW_OPEN		"s"
#define FMT_READ			"x"
#define FMT_WRITE			"(xy)"

#define RET_FMT_NEW_OPEN	"(imims)"
#define RET_FMT_WRITE		"(bmims)"
#define RET_FMT_READ		"(bmymxmims)"
#define RET_FMT_CLOSE		"(bmims)"

enum Command
{
	NEW,
	OPEN,
	CLOSE,
	READ,
	WRITE
};
