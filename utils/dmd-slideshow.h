
typedef int64_t tmillis_t;

static const tmillis_t distant_future = (1LL<<40); // that is a while.

struct ImageParams {
    ImageParams() : anim_duration_ms(distant_future), wait_ms(1500),
    anim_delay_ms(-1), loops(-1), vsync_multiple(1) {}
    tmillis_t anim_duration_ms;  // If this is an animation, duration to show.
    tmillis_t wait_ms;           // Regular image: duration to show.
    tmillis_t anim_delay_ms;     // Animation delay override.
    int loops;
    int vsync_multiple;
};

struct    LoadedFile {
    LoadedFile(): is_multi_frame(0), currentFrameID(0), nextFrameTime(-distant_future){};
    const char *filename;
    std::vector<Magick::Image> frames;
    bool is_multi_frame;
    unsigned int    currentFrameID;
    unsigned int frameCount;
    //     int64_t delay_time_us;
    tmillis_t    nextFrameTime;
};

struct	FileCollection {
	const char *regex;
	std::vector<const char *>filePaths;
};

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
    interrupt_received = true;
}

static tmillis_t GetTimeInMillis() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

static void SleepMillis(tmillis_t milli_seconds) {
    if (milli_seconds <= 0) return;
    struct timespec ts;
    ts.tv_sec = milli_seconds / 1000;
    ts.tv_nsec = (milli_seconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}
