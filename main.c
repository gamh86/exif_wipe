#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "exif.h"
#include "file.h"
#include "logging.h"

static int get_options(int, char *[]) __wur;
static void usage(int) __attribute__ ((__noreturn__));

int
main(int argc, char *argv[])
{
	int							fd;
	unsigned char		*p = NULL, *q = NULL;
	unsigned char		*p1 = NULL, *p2 = NULL;
	struct stat			statb;
	void						*data = NULL;
	void						*map_end = NULL;
	size_t					new_len;
	int							*int_ptr = NULL;
	char						*char_ptr = NULL;
	long						*long_ptr = NULL;
	int							endianness;
	int							cnt;

	prog_name = argv[0];

	fprintf(stdout, "    >>> eXif Wipe %s <<<%s%s",
					BUILD, _EOL, _EOL);

	if (get_options(argc, argv) < 0)
		goto fail;

	if (!file_ok(argv[1]))
		goto fail;

	memset(&statb, 0, sizeof(statb));
	lstat(argv[1], &statb);

	infile.size = statb.st_size;

	fprintf(stdout, "\e[5;01m\e[35;5;2m  File: %s%s"
						"  Size: %lu bytes\e[m%s%s%s",
						argv[1], _EOL,
						statb.st_size, _EOL, _EOL, _EOL);

	fd = open(argv[1], O_RDWR);

	assert(strlen(argv[1]) < MAX_PATH_LEN);
	strncpy(infile.fullpath, argv[1], strlen(argv[1]));
	infile.fullpath[strlen(argv[1])] = 0;
	infile.fd = fd;
	
	if ((data = mmap(NULL, statb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED)
		goto fail;

	infile.map = data;
	infile.map_end = ((unsigned char *)data + statb.st_size);
	infile.new_end = infile.map_end;

	lim = get_limit(&infile);
	if (!lim)
	  {
			log_error("Failed to find limit");
			goto fail;
	  }

	p = (unsigned char *)data;
	while (strncmp((char *)"Exif", (char *)p, 4) != 0 && p < (unsigned char *)data + statb.st_size)
		++p;
	if (p == ((unsigned char *)data + statb.st_size))
		{
			fprintf(stdout, "No EXIF data in %s\n", argv[1]);
			goto end;
	  }

	while (strncmp((char *)"MM", (char *)p, 2) != 0
			&& strncmp((char *)"II", (char *)p, 2) != 0
			&& p < ((unsigned char *)data + statb.st_size))
		++p;

	EXIF_DATA_OFFSET = (off_t)(p - (unsigned char *)data);
#ifdef DEBUG
	printf("EXIF_DATA_OFFSET: %lu\n", (size_t)EXIF_DATA_OFFSET);
#endif

	if (p == ((unsigned char *)data + statb.st_size))
	  {
			errno = EPROTO;
			log_error("No endianness marker in file");
			goto fail;
	  }
	else
	if (strncmp((char *)"MM", (char *)p, 2) == 0)
		endianness = 1;
	else
		endianness = 0;

	map_end = (void *)((unsigned char *)data + statb.st_size);

	fprintf(stdout, "\e[38;5;8m  ----EXIF Data----\e[m%s%s", _EOL, _EOL);

#ifdef DEBUG
	cnt = get_test(&infile, (const int)endianness);
	if (!cnt)
		printf("Found no test data...\n");
	else
		printf("Found test data...\n");
#else
	fprintf(stdout, "%s\tDate/Time Data\e[m%s", SECTION_COL, _EOL);
	cnt = get_date_time(&infile, (const int)endianness);
	if (!cnt)
		printf("(None)%s", _EOL);
	fprintf(stdout, "%s\tDevice Data\e[m%s", SECTION_COL, _EOL);
	cnt = get_make_model(&infile, (const int)endianness);
	if (!cnt)
		printf("(None)%s", _EOL);
	fprintf(stdout, "%s\tOther Data\e[m%s", SECTION_COL, _EOL);
	cnt = get_unique_id(&infile, (const int)endianness);
	cnt += get_image_comment(&infile, (const int)endianness);
	if (!cnt)
		printf("(None)%s", _EOL);
	//get_latitude(data, map_end, endianness);
#endif

	end:
	if (data)
		{ munmap(data, statb.st_size); data = NULL; }
	exit(EXIT_SUCCESS);

	no_exif:
	fprintf(stdout, "Found no EXIF data\n");
	if (data)
		{ munmap(data, statb.st_size); data = NULL; }
	exit(EXIT_SUCCESS);

	fail:
	fprintf(stderr, "An error occurred: %s\n", strerror(errno));
	if (data)
		{ munmap(data, statb.st_size); data = NULL; }
	exit(EXIT_FAILURE);
}

int
get_options(int argc, char *argv[])
{
	int					i;

	FLAGS = 0;
	for (i = 1; i < argc; ++i)
	  {
			if (strncmp("--help", argv[i], 6) == 0)
				usage(EXIT_SUCCESS);
			else
			if (strcmp("--wipe-all", argv[i]) == 0)
				FLAGS |= WIPE_ALL;
			else
			if (strcmp("--wipe-date", argv[i]) == 0)
				FLAGS |= WIPE_DATE;
			else
			if (strcmp("--wipe-device", argv[i]) == 0)
				FLAGS |= WIPE_DEVICE;
			else
			if (strcmp("--wipe-location", argv[i]) == 0)
				FLAGS |= WIPE_LOCATION;
			else
			if (strcmp("--wipe-uid", argv[i]) == 0)
				FLAGS |= WIPE_UID;
			else
			if (strcmp("--wipe-comment", argv[i]) == 0)
				FLAGS |= WIPE_COMMENT;
	  }

	if ((FLAGS & WIPE_ALL)
			&& (FLAGS & WIPE_DATE || FLAGS & WIPE_DEVICE || FLAGS & WIPE_LOCATION))
	  {
			fprintf(stderr, "--wipe-all cannot be specified with other options\n");
			errno = EINVAL;
			return -1;
	  }

	return 0;
}

void
usage(int exit_type)
{
	fprintf(stdout,
				"Usage:%s"
				"%s"
				"%s <image> [options]%s"
				"%s"
				"  --wipe-all\t\tWipe all EXIF data%s"
				"  --wipe-date\t\tWipe Date/Time data%s"
				"  --wipe-device\t\tWipe Make/Model data%s"
				"  --wipe-location\tWipe GPS data%s"
				"  --wipe-uid\t\tWipe the Unique Image ID%s"
				"  --wipe-comment\t\tWipe Image Comment%s"
				"%s"
				"(No options specified = just view data)%s",
				_EOL,
				_EOL,
				prog_name, _EOL,
				_EOL,
				_EOL, _EOL, _EOL, _EOL, _EOL, _EOL,
				_EOL, _EOL);

	exit(exit_type);
}