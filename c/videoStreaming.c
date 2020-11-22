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
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define SIGHUP  1   /* Hangup the process */ 
#define SIGINT  2   /* Interrupt the process */ 
#define SIGQUIT 3   /* Quit the process */ 
#define SIGILL  4   /* Illegal instruction. */ 
#define SIGTRAP 5   /* Trace trap. */ 
#define SIGABRT 6   /* Abort. */

typedef int bool;
#define true 1
#define false 0

struct buffer {
	void   *start;
	size_t  length;
};

static char *ip;
static char *dev_name[4]; //dev_name
static int port[4]; //port
static int file_dev[4] = {-1,-1,-1,-1};
struct buffer *buffers[4];

static int force_format;
static bool streaming = false;
static unsigned int n_buffers;
static double fps = 20;
double rate = 1;

static int sock[4];
//unsigned short port;
struct sockaddr_in server[4];

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

static void process_image(const void *p, int size, int i)
{
		char filename[15];
		//sprintf(filename, "frame%d.raw", i);
		//FILE *fp=fopen(filename,"wb");
		//printf("------------------- {%d}\n",size);

		//char *hello = "Hello from client"; 
		if (sendto(sock[i], p, size, MSG_CONFIRM, (const struct sockaddr *) &server[i], sizeof(server[i])) < 0)
		{
		 	fprintf(stderr, "sendto error\n");
		 	exit(EXIT_FAILURE);
		}
		//fwrite(p, size, 1, fp);

		//fflush(fp);
		//fclose(fp);
}


static int read_frame(char* dev, int fd, int i){
	struct v4l2_buffer buf;

	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
			case EAGAIN:
				return 0;
			case EIO:
				/* Could ignore EIO, see spec. */
				/* fall through */
			default:
				errno_exit("VIDIOC_DQBUF");
		}
	}
	assert(buf.index < n_buffers);
	process_image(buffers[i][buf.index].start, buf.bytesused, i);
	if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
		errno_exit("VIDIOC_QBUF");
}

static void get_frame(char* dev, int fd, int i){
	//printf("frame i %d d: %s fd: %d --- ",i,dev, fd);
	
	fd_set fds;
	struct timeval tv;
	int r;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	/* Timeout. */
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	//printf("-%d\n", fd);
	r = select(fd + 1, &fds, NULL, NULL, &tv);
	//printf("%d\n", r);
	if (-1 == r) {
		if (EINTR == errno)
			return;
		errno_exit("select");
	}

	if (0 == r) {
		fprintf(stderr, "select timeout\n");
		exit(EXIT_FAILURE);
	}

	read_frame(dev, fd , i);
	/* EAGAIN - continue select loop. */
	return;
}

static void mainloop(char* dev[], int *fd){
	while (streaming) {
		clock_t t,d,g; 
		t = clock() / (CLOCKS_PER_SEC / 1000);
		for(int i =0 ; i < 4 ; i++){
			get_frame( dev[i], fd[i] , i);
		}
		d = clock() / (CLOCKS_PER_SEC / 1000);
		(d - t);
		
		double time_taken = (double)(d - t);
		unsigned int s = ((int)rate - (int)time_taken) * 1000;
		usleep(s);
		printf("%d fun() took %f miliseconds to execute, sleep %d\n",d, time_taken, s); 
	}
}

static void start_capturing(char* dev[], int *fd)
{
	enum v4l2_buf_type type;
	for(int i =0 ; i < 4 ; i++){
		//printf("i %d d: %s fd: %d\n",i,dev[i], fd[i]);
		for (int j = 0; j < n_buffers; ++j) {
			struct v4l2_buffer buf;

			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = j;

			if (-1 == xioctl(fd[i], VIDIOC_QBUF, &buf))
				errno_exit("VIDIOC_QBUF");
		}
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl(fd[i], VIDIOC_STREAMON, &type))
			errno_exit("VIDIOC_STREAMON");
	}
}

static void init_mmap(char* dev, int fd, int i)
{
	//printf("init_mmap\n");
	//printf("i: %d d: %s fd: %d\n", i ,dev, fd);
	struct v4l2_requestbuffers req;

	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
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

	buffers[i] = calloc(req.count, sizeof(*buffers[i]));

	if (!buffers[i]) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers;

		if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
			errno_exit("VIDIOC_QUERYBUF");
		
		buffers[i][n_buffers].length = buf.length;
		buffers[i][n_buffers].start = mmap(NULL /* start anywhere */, buf.length, PROT_READ | PROT_WRITE /* required */, MAP_SHARED /* recommended */, fd, buf.m.offset);
		//printf("length: %d\n", buffers[i][n_buffers].length);
		if (MAP_FAILED == buffers[i][n_buffers].start)
			errno_exit("mmap");
	}
}

static void init_devices(char* dev[], int *fd)
{
	for(int i =0 ; i < 4 ; i++){
		//printf("i %d d: %s fd: %d\n",i,dev[i], fd[i]);
		struct v4l2_capability cap;
		struct v4l2_cropcap cropcap;
		struct v4l2_crop crop;
		struct v4l2_format fmt;
		unsigned int min;
		if (-1 == xioctl(fd[i], VIDIOC_QUERYCAP, &cap)) {
			if (EINVAL == errno) {
				fprintf(stderr, "%s is no V4L2 device\n", dev[i]);
				exit(EXIT_FAILURE);
			} else {
				errno_exit("VIDIOC_QUERYCAP");
			}
		}
		if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
			fprintf(stderr, "%s is no video capture device\n", dev[i]);
			exit(EXIT_FAILURE);
		}
		if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
			fprintf(stderr, "%s does not support streaming i/o\n", dev[i]);
			exit(EXIT_FAILURE);
		}
		/* Select video input, video standard and tune here. */
		CLEAR(cropcap);
		cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (0 == xioctl(fd[i], VIDIOC_CROPCAP, &cropcap)) {
			crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			crop.c = cropcap.defrect; /* reset to default */

			if (-1 == xioctl(fd[i], VIDIOC_S_CROP, &crop)) {
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
			//fprintf(stderr, "Set mpeg\r\n");
			fmt.fmt.pix.width       = 320; //replace
			fmt.fmt.pix.height      = 240; //replace
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG; //replace
			fmt.fmt.pix.field       = V4L2_FIELD_ANY;

			if (-1 == xioctl(fd[i], VIDIOC_S_FMT, &fmt))
				errno_exit("VIDIOC_S_FMT");

			/* Note VIDIOC_S_FMT may change width and height. */
		} else {
			/* Preserve original settings as set by v4l2-ctl for example */
			if (-1 == xioctl(fd[i], VIDIOC_G_FMT, &fmt))
				errno_exit("VIDIOC_G_FMT");
		}
		init_mmap( dev[i], fd[i] , i);
	}
}

static void close_devices(int *fd)
{
	for(int i =0 ; i < 4 ; i++){
		//printf("i %d fd: %d\n",i, fd[i]);
		if (-1 == close(fd[i]))
			errno_exit("close");
		fd[i] = -1;
		close(sock[i]);
	}
}

static void open_devices(char* dev[], int *fd)
{
	struct stat st;
	for(int i =0 ; i < 4 ; i++){
		if (-1 == stat(dev[i], &st)) {
			fprintf(stderr, "Cannot identify '%s': %d, %s\n", dev[i], errno, strerror(errno));
			exit(EXIT_FAILURE);
		}

		if (!S_ISCHR(st.st_mode)) {
			fprintf(stderr, "%s is no device\n", dev);
			exit(EXIT_FAILURE);
		}
		fd[i] = open(dev[i], O_RDWR /* required */ | O_NONBLOCK, 0);
		if (-1 == fd[i]) {
			fprintf(stderr, "Cannot open '%s': %d, %s\n", dev[i], errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		//printf("i %d d: %s fd: %d\n",i,dev[i], fd[i]);
	}
}

void handle_sigint(int sig) 
{ 
	printf("Caught signal %d\n", sig); 
	streaming = false;
} 

static void print_usage(FILE *fp, int argc, char **argv)
{
		fprintf(fp,
				 "Usage: %s [options]\n\n"
				 "Version 1.0\n"
				 "Options:\n"
				 "-1 | --dev1   Video device name\n"
				 "-2 | --dev2   Video device name\n"
				 "-3 | --dev3   Video device name\n"
				 "-4 | --dev4   Video device name\n"
				 "",
				 argv[0]);
}

static const struct option long_options[] = {
	{ "help",   no_argument,       NULL, 'h' },
	{ "ip",   	required_argument, NULL, 'i' },
	{ "dev1", 	required_argument, NULL, '1' },
	{ "dev2",   required_argument, NULL, '2' },
	{ "dev3",   required_argument, NULL, '3' },
	{ "dev4",   required_argument, NULL, '4' },
	{ "port1",  required_argument, NULL, 'a' },
	{ "port2",  required_argument, NULL, 'b' },
	{ "port3",  required_argument, NULL, 'c' },
	{ "port4",  required_argument, NULL, 'd' },
	{ "format", no_argument      , NULL, 'f' },
	{ 0, 0, 0, 0 }
};

int main(int argc, char *argv[])
{
	streaming = true;
	signal(SIGINT, handle_sigint); 
	int opt= 0;
	int long_index =0;
	ip = "192.168.1.128";
	dev_name[0] = "/dev/video0";
	dev_name[1] = "/dev/video2";
	dev_name[2] = "/dev/video4";
	dev_name[3] = "/dev/video6";
	port[0]= 5001;
	port[1]= 5002;
	port[2]= 5003;
	port[3]= 5004;

	rate = (rate/fps)*((double)1000);

	while ((opt = getopt_long_only(argc, argv,"", long_options, &long_index )) != -1) {
		switch (opt) {
			case 'h': print_usage(stdout, argc, argv);
				exit(EXIT_SUCCESS);
			case 'i': ip = optarg;
				break;
			case '1' : dev_name[0] = optarg;
				break;
			case '2' : dev_name[1] = optarg;
				break;
			case '3' : dev_name[2] = optarg;
				break;
			case '4' : dev_name[3] = optarg;
				break;
			case 'a' : port[0] = atoi(optarg);
				break;
			case 'b' : port[1] = atoi(optarg);
				break;
			case 'c' : port[2] = atoi(optarg);
				break;
			case 'd' : port[3] = atoi(optarg);
				break;
			case 'f': force_format++;
				break;
			default: print_usage(stderr, argc, argv); 
				exit(EXIT_FAILURE);
		}
	}
	printf("ip: %s\n",ip);
	printf("rate: %f\n",rate);
	for(int i =0 ; i < 4 ; i++){
		printf("d: %s p: %d\n",dev_name[i], port[i]);
		if ((sock[i] = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		{
			fprintf(stderr, "socket %d\n",i);
			exit(EXIT_FAILURE);
		}
		/* Set up the server name */
		server[i].sin_family      = AF_INET;
		server[i].sin_port        = htons(port[i]);
		server[i].sin_addr.s_addr = inet_addr(ip);
	}
	printf("start...\n");
	for(int i =0 ; i < 4 ; i++){
		//printf("i %d d: %s fd: %d\n",i,dev_name[i], file_dev[i]);
	}

	printf("open_device\n");
	open_devices(dev_name, file_dev);

	printf("init_devices\n");
	init_devices(dev_name, file_dev);

	printf("start_capturing\n");
	start_capturing(dev_name, file_dev);

	printf("loop\n");
	mainloop(dev_name, file_dev);

	printf("close_devices\n");
	close_devices(file_dev);

	printf("Finish\n");
	//for(int i =0 ; i < 4 ; i++){
		//printf("i %d d: %s fd: %d\n",i,dev_name[i], file_dev[i]);
	//}

	fprintf(stderr, "\n");
	return 0;

}
