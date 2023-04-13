#pragma once

#define BINK_AUDIO_RATE		44100

class BinkMovie;

typedef struct LOCKED_RECT
{
	void* pBits;
	size_t Pitch;
} LOCKED_RECT;

class Texture
{
public:
	Texture() : w(0), h(0), pitch(0),
		buffer(nullptr),
		lock_cnt(0)
	{}

	void Create(int w, int h);
	void Release();

	void Lock(LOCKED_RECT* r);
	void Unlock();

	DWORD *buffer;
	int w, h;
	int pitch;
	int lock_cnt;
};

class Video
{
public:
	std::vector<Texture*> bmp;
	Texture* disp;
	int head, tail, bufnum,
		w, h;
	volatile DWORD status;
	int64_t vpts, apts,
		completed_vpts,
		completed_apts,
		start_pts,
		end_pts,
		start_tick;
	int32_t completed_counter;
	int32_t tickavdiff,
		tickframe,
		ticksleep;
	int64_t ticklast;
	std::vector<int64_t> ppts;
	BinkMovie* b;

	// semaphores
	HANDLE semr,	// read
		semw;		// write
	// thread
	HANDLE th;

	static DWORD WINAPI thread(LPVOID ctx);

	void init(int video_w, int video_h, int bufs, BinkMovie* bik);
	void shut();
	void stop();

	void decode();
	void sync();
	void getavpts(int64_t** ppapts, int64_t** ppvpts);

	void lock(uint8_t* buffer[], int linesize[]);
	void unlock(int64_t pts);

	void pause();
	void resume();
};

class DsndBuffer
{
	LPDIRECTSOUNDBUFFER buf;
	HANDLE th;
	size_t size, hsize, qsize;
	volatile unsigned int loops : 1,
		exited : 1,
		paused : 1;
	BinkMovie* b;

public:
	static DWORD WINAPI thread(LPVOID ctx);
	void create(size_t _size, BinkMovie* bik, LPDIRECTSOUND lpDS);
	void destroy();
	void play();
	void stop();

	void pause();
	void resume();
};

class Audio
{
public:
	typedef struct BUF
	{
		int16_t* data;
		int32_t  size;
	} BUF;

	std::vector<int64_t> ppts;
	int bufnum;
	int buflen;
	int head;
	int tail;
	int64_t* apts;
	HANDLE semw;
	std::vector<BUF> AudioBuf;
	DsndBuffer dssnd;
	std::vector<std::vector<int16_t>> data;
	double speed;

	void init(int _bufnum, int _buflen, BinkMovie* bik, LPDIRECTSOUND DSoundPtr);
	void shut();
	void reset();
	void stop();
	void sync(int64_t* papts);

	void lock(BUF** ppab);
	void unlock(int64_t pts);

	void pause();
	void resume();
};

class PktQueue
{
public:
	HANDLE fsem, asem, vsem, lock;
	std::vector<AVPacket> bpkts;
	std::vector<AVPacket*> fpkts,
		apkts,
		vpkts;
	size_t fsize, vsize, asize,
		fhead, ftail,
		vhead, vtail,
		ahead, atail;

	PktQueue();

	void init(size_t size);
	void reset();
	void shut();

	void audio_enqueue(AVPacket* pkt);
	AVPacket* audio_dequeue();
	AVPacket* audio_next();

	void video_enqueue(AVPacket* pkt);
	AVPacket* video_dequeue();

	void free_enqueue(AVPacket* pkt);
	AVPacket* free_dequeue();

	void free_cancel(AVPacket* pkt);
};

class BinkMovie
{
public:
	FFmpeg ff;
	PktQueue queue;
	Audio aud;
	Video vid;

	size_t aux_buf_avail;
	uint8_t* aux_buf_cur;
	Audio::BUF* aux_hdr_cur;

	DWORD status;
	int THE_END;
	double FPS;

	static DWORD WINAPI demux_thread(LPVOID ctx);
	static DWORD WINAPI audio_thread(LPVOID ctx);
	static DWORD WINAPI video_thread(LPVOID ctx);

	void process_video(AVFrame* vframe);
	void process_audio(AVFrame* aframe);

	void CreateThreads();
	void KillThreads();

	void Init(LPDIRECTSOUND lpDS);
	void Shut();

	void Start();
	void Pause();
	void Resume();
};