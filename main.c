#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <libv4l2.h>
#include <time.h>

typedef struct {
    void *start;
    uint length;
} maped_buffer_t;

typedef struct {
    char *dev_name;
    int dev_fd;
    uint buff_count;
    maped_buffer_t *maped_buffers;
    uint support_V4L2_CAP_VIDEO_CAPTURE;
    uint support_V4L2_CAP_STREAMING;
    uint support_V4L2_CAP_EXT_PIX_FORMAT;

    struct v4l2_format format;
    struct v4l2_requestbuffers requestbuffers;

} app_data_t;


void saveRgbFrameToJpeg(unsigned char *img, char *filename, uint Width, uint Height);

#define  mem_clear_struct(x) memset(&x, 0, sizeof(x))

// global variables

#define DEV_NAME  "/dev/video0"

app_data_t app_data = {
        .dev_name = DEV_NAME,
        .buff_count = 2,

};


static void xioctl(int fh, ulong request, void *arg) {
    int r;

    do {
        r = v4l2_ioctl(fh, request, arg);
    } while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));

    if (r == -1) {
        fprintf(stderr, "error %d, %s\\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void open_device(void) {
    int dev_fd = v4l2_open(app_data.dev_name, O_RDWR | O_NONBLOCK);
    if (dev_fd < 0) {
        printf("open device failed!\n");
        exit(-1);
    }
    app_data.dev_fd = dev_fd;
}


void query_cap(void) {

    // variable definition
    struct v4l2_capability cap = {0};

    // get pix format
    mem_clear_struct(cap);

    xioctl(app_data.dev_fd, VIDIOC_QUERYCAP, (void *) &cap);
    printf("v4l2_capability capability: %x\n", cap.capabilities);
    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        printf("\tsupport V4L2_CAP_VIDEO_CAPTURE\n");
        app_data.support_V4L2_CAP_VIDEO_CAPTURE = 1;
    }

    if (cap.capabilities & V4L2_CAP_STREAMING) {
        printf("\tsupport V4L2_CAP_STREAMING\n");
        app_data.support_V4L2_CAP_STREAMING = 1;
    }

    if (cap.capabilities & V4L2_CAP_EXT_PIX_FORMAT) {
        printf("\tsupport V4L2_CAP_EXT_PIX_FORMAT\n");
        app_data.support_V4L2_CAP_EXT_PIX_FORMAT = 1;
    }

    printf("v4l2_capability name: %s\n", cap.driver);
    printf("v4l2_capability bus_info: %s\n", cap.bus_info);
    printf("v4l2_capability card: %s\n", cap.card);
    printf("v4l2_capability Version: %u.%u.%u\n", (cap.version >> 16) & 0xFF, (cap.version >> 8) & 0xFF,
           cap.version & 0xFF);

}

void negotiate_format(void) {
    // variable definition
    struct v4l2_format fmt = {0};

    if (!app_data.support_V4L2_CAP_VIDEO_CAPTURE || !app_data.support_V4L2_CAP_STREAMING) {
        printf("negotiate_format: not support!");
        exit(-1);
    }

    // get pix format
    mem_clear_struct(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;  //
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    xioctl(app_data.dev_fd, VIDIOC_G_FMT, (void *) &fmt);

    // set pix format
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;  //
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    xioctl(app_data.dev_fd, VIDIOC_S_FMT, (void *) &fmt);

    app_data.format = fmt;
}

void req_dev_buff(void) {
    struct v4l2_requestbuffers req_buff;
    uint req_count = app_data.buff_count;
    mem_clear_struct(req_buff);
    req_buff.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req_buff.count = req_count;
    req_buff.memory = V4L2_MEMORY_MMAP;

    xioctl(app_data.dev_fd, VIDIOC_REQBUFS, (void *) &req_buff);
    printf("v4l2_requestbuffers req count: %d\tactual count: %d\n", req_count, req_buff.count);

    app_data.requestbuffers = req_buff;
}


void set_mmap(void) {

    struct v4l2_buffer dev_buff;
    app_data.maped_buffers = (maped_buffer_t *) calloc(app_data.buff_count, sizeof(maped_buffer_t));
    for (unsigned int i = 0; i < app_data.buff_count; ++i) {

        dev_buff.memory = V4L2_MEMORY_MMAP;
        dev_buff.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        dev_buff.index = i;

        xioctl(app_data.dev_fd, VIDIOC_QUERYBUF, &dev_buff);

        app_data.maped_buffers[i].start = v4l2_mmap(NULL, dev_buff.length, PROT_READ | PROT_WRITE, MAP_SHARED, app_data.dev_fd,
                                           dev_buff.m.offset);
        app_data.maped_buffers[i].length = dev_buff.length;
    }
}


void queue_all_buff(void) {
    struct v4l2_buffer buff;

    for (uint i = 0; i < app_data.buff_count; ++i) {
        buff.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buff.memory = V4L2_MEMORY_MMAP;
        buff.index = i;
        xioctl(app_data.dev_fd, VIDIOC_QBUF, &buff);
    }

}

void setup(void) {
    // query driver capability
    query_cap();

    // set pix format
    negotiate_format();

    // request device buff
    req_dev_buff();

    set_mmap();

    queue_all_buff();
}

// loop: select io  ->  dequeue frame buff -> save as jpg -> queue frame  ->  sleep -> loop
void grab_loop(int times, int interval_msec) {
    struct v4l2_buffer buff;

    buff.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buff.memory = V4L2_MEMORY_MMAP;

    xioctl(app_data.dev_fd, VIDIOC_STREAMON, &app_data.requestbuffers.type);

    printf("start!\n");

    char filename[20] = {0};
    fd_set fds;

    struct timespec timespec1, timespec2;
    clock_gettime(CLOCK_REALTIME, &timespec1);
//    printf("%d, %d\n", timespec1.tv_sec, timespec1.tv_nsec);

    int ret = 0;
    for (int i = 0; i < times; ++i) {

        do {
            FD_ZERO(&fds);
            FD_SET(app_data.dev_fd, &fds);
            struct timeval tv = {
                    .tv_sec = 0,
                    .tv_usec = 1000,
            };

            ret = select(app_data.dev_fd + 1, &fds, NULL, NULL, &tv);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1) {
            printf("error: select\n");
            exit(-1);
        }

        xioctl(app_data.dev_fd, VIDIOC_DQBUF, &buff);

        printf("dequeue buff index: %d\n", buff.index);

        sprintf(filename, "t%02d.jpg", i);
        saveRgbFrameToJpeg(app_data.maped_buffers[buff.index].start, filename, app_data.format.fmt.pix.width, app_data.format.fmt.pix.height);


        xioctl(app_data.dev_fd, VIDIOC_QBUF, &buff);
        usleep(100 * (uint) interval_msec);
    }
    clock_gettime(CLOCK_REALTIME, &timespec2);
    printf("%d, %d\n", timespec2.tv_sec - timespec1.tv_sec, timespec2.tv_nsec - timespec1.tv_nsec);
    printf("end\n");

    xioctl(app_data.dev_fd, VIDIOC_STREAMOFF, &app_data.requestbuffers.type);
}


void clean_mmap(void) {
    for (int i = 0; i < app_data.buff_count; ++i) {
        v4l2_munmap(app_data.maped_buffers[i].start, app_data.maped_buffers[i].length);
    }
}

void cleanup(void) {

    clean_mmap();

    v4l2_close(app_data.dev_fd);
}

int main(int argc, char **argv) {
    printf("hey we are going grab picture from uvc camera!\n");

    // open device
    open_device();

    // set up driver
    setup();

    // loop: dequeue frame buff -> save as jpg -> queue frame  ->  sleep -> loop
    grab_loop(10, 100);

    // clear settings
    cleanup();

//    grab_rbg_frame();
    return 0;
}