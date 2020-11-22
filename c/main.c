
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>             /* getopt_long() */
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <stdlib.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

struct buffer {
	void   *start;
	size_t  length;
};

static char *dev1_name; //dev_name
static char *dev2_name;
static char *dev3_name;
static char *dev4_name;
static int file_dev1 = -1; //fd
static int file_dev2 = -1;
static int file_dev3 = -1;
static int file_dev4 = -1;
static int force_format;
struct buffer *buffers[4];




static void errno_exit(const char *s)
{
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
	exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
	int r;

	do {
		r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

static void init_mmap(char* dev,int *fd)
{
	
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(*fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support memory mapping\n", dev);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n", dev);
		exit(EXIT_FAILURE);
	}

	// buffers = calloc(req.count, sizeof(*buffers));

	// if (!buffers) {
	// 	fprintf(stderr, "Out of memory\n");
	// 	exit(EXIT_FAILURE);
	// }

	// for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
	// 	struct v4l2_buffer buf;

	// 	CLEAR(buf);

	// 	buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	// 	buf.memory      = V4L2_MEMORY_MMAP;
	// 	buf.index       = n_buffers;

	// 	if (-1 == xioctl(*fd, VIDIOC_QUERYBUF, &buf))
	// 		errno_exit("VIDIOC_QUERYBUF");

	// 	buffers[n_buffers].length = buf.length;
	// 	buffers[n_buffers].start = mmap(NULL /* start anywhere */,
	// 					buf.length,
	// 					PROT_READ | PROT_WRITE /* required */,
	// 					MAP_SHARED /* recommended */,
	// 					*fd, buf.m.offset);

	// 	if (MAP_FAILED == buffers[n_buffers].start)
	// 		errno_exit("mmap");
	// }
}

static void init_device(char* dev, int *fd)
{
	printf("init_device\n");
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;

	if (-1 == xioctl(*fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n", dev);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n", dev);
		exit(EXIT_FAILURE);
	}


	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n", dev);
		exit(EXIT_FAILURE);
	}

	/* Select video input, video standard and tune here. */

	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(*fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(*fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
			case EINVAL:
				/* Cropping not supported. */
				break;
			default:
				/* Errors ignored. */
				break;
			}
		}
	} else {
		/* Errors ignored. */
	}

	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (force_format) {
		fprintf(stderr, "Set mpeg\r\n");
		fmt.fmt.pix.width       = 320; //replace
		fmt.fmt.pix.height      = 240; //replace
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG; //replace
		fmt.fmt.pix.field       = V4L2_FIELD_ANY;

		if (-1 == xioctl(*fd, VIDIOC_S_FMT, &fmt))
			errno_exit("VIDIOC_S_FMT");

		/* Note VIDIOC_S_FMT may change width and height. */
	} else {
		/* Preserve original settings as set by v4l2-ctl for example */
		if (-1 == xioctl(*fd, VIDIOC_G_FMT, &fmt))
			errno_exit("VIDIOC_G_FMT");
	}

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	printf("IO_METHOD_MMAP\n");
	//init_mmap();
}

static void close_device(int *fd)
{
	if (-1 == close(*fd))
		printf("close error\n"); //errno_exit("close");
    *fd = -1;
}

static void open_device(char* dev, int *fd)
{
	struct stat st;
	printf("a t: %s i: %d\n", dev, *fd);
	if (-1 == stat(dev, &st)) {
		fprintf(stderr, "Cannot identify '%s': %d, %s\n", dev, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, "%s is no device\n", dev);
		exit(EXIT_FAILURE);
	}

	*fd = open(dev, O_RDWR /* required */ | O_NONBLOCK, 0);
	printf("d t: %s i: %d\n", dev, *fd);
	if (-1 == *fd) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n", dev, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	dev1_name = "/dev/video0";
	dev2_name = "/dev/video2";
	dev3_name = "/dev/video4";
	dev4_name = "/dev/video6";

	force_format++; //formar formato

	open_device(dev1_name, &file_dev1);
	open_device(dev2_name, &file_dev2);
	open_device(dev3_name, &file_dev3);
	open_device(dev4_name, &file_dev4);

	printf("d t: %s i: %d\n", dev1_name, file_dev1);
	printf("d t: %s i: %d\n", dev2_name, file_dev2);
	printf("d t: %s i: %d\n", dev3_name, file_dev3);
	printf("d t: %s i: %d\n", dev4_name, file_dev4);

	init_device(dev1_name, &file_dev1);
	init_device(dev2_name, &file_dev2);
	init_device(dev3_name, &file_dev3);
	init_device(dev4_name, &file_dev4);

	close_device(&file_dev1);
	close_device(&file_dev2);
	close_device(&file_dev3);
	close_device(&file_dev4);

	printf("i: %d\n", file_dev1);
	printf("i: %d\n", file_dev2);
	printf("i: %d\n", file_dev3);
	printf("i: %d\n", file_dev4);


	fprintf(stderr, "\n");
	return 0;

}