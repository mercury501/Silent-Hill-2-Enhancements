#include "bink.h"

class BinkModule
{
public:
	S32 (WINAPI* SetSoundOnOff)(HBINK bnk, int onoff) = nullptr;
	HBINK (WINAPI* Open)(const char* name, U32 flags) = nullptr;
	void (WINAPI* SetSoundTrack)(int total_tracks, U32* tracks) = nullptr;
	S32 (WINAPI* SetSoundSystem)(BINKSNDSYSOPEN open, U32 param) = nullptr;
	void (WINAPI* OpenDirectSound)(S32 param) = nullptr;
	void (WINAPI* Close)(HBINK bnk) = nullptr;
	S32 (WINAPI* CopyToBuffer)(HBINK bnk, void* dest, U32 destpitch, U32 destheight, U32 destx, U32 desty, U32 flags) = nullptr;
	void (WINAPI* NextFrame)(HBINK bnk) = nullptr;
	S32 (WINAPI* DoFrame)(HBINK bnk) = nullptr;
	S32 (WINAPI* Wait)(HBINK bnk) = nullptr;
	S32 (WINAPI* Pause)(HBINK bnk, S32 pause) = nullptr;

	BinkModule()
	{
		dll = LoadLibraryA("binkw32.dll");
		if (dll)
		{
			SetSoundOnOff = (S32(WINAPI*)(HBINK, int))GetProcAddress(dll, "_BinkSetSoundOnOff@8");
			Open = (HBINK(WINAPI*)(const char*, U32))GetProcAddress(dll, "_BinkOpen@8");
			SetSoundTrack = (void (WINAPI*)(int, U32*))GetProcAddress(dll, "_BinkSetSoundTrack@8");
			SetSoundSystem = (S32(WINAPI*)(BINKSNDSYSOPEN, U32))GetProcAddress(dll, "_BinkSetSoundSystem@8");
			OpenDirectSound = (void(WINAPI*)(S32))GetProcAddress(dll, "_BinkOpenDirectSound@4");
			Close = (void (WINAPI*)(HBINK))GetProcAddress(dll, "_BinkClose@4");
			CopyToBuffer = (S32(WINAPI * )(HBINK, void*, U32, U32, U32, U32, U32))GetProcAddress(dll, "_BinkCopyToBuffer@28");
			NextFrame = (void (WINAPI * )(HBINK))GetProcAddress(dll, "_BinkNextFrame@4");
			DoFrame = (S32(WINAPI * )(HBINK))GetProcAddress(dll, "_BinkDoFrame@4");
			Wait = (S32(WINAPI * )(HBINK))GetProcAddress(dll, "_BinkWait@4");
			Pause = (S32(WINAPI * )(HBINK, S32))GetProcAddress(dll, "_BinkPause@8");
		}
	}
	~BinkModule()
	{
		FreeLibrary(dll);
	}

	HMODULE dll;
};

static BinkModule m_hBink;
static BINKSNDSYSOPEN open_fn;
static U32 open_param;
static LPDIRECTSOUND DSoundPtr = nullptr;
static int track = 0, is_bink = 0;

S32 WINAPI BinkSetSoundOnOff(HBINK bnk, int onoff)
{
	if (is_bink)
		return m_hBink.SetSoundOnOff(bnk, onoff);

	return 0;
}

HBINK WINAPI BinkOpen(const char* name, U32 flags)
{
	auto b = new BinkMovie;
	DSoundPtr = (LPDIRECTSOUND)open_param;

	if (b->ff.open(name, track) == 0)
	{
		delete b;

		auto ret = m_hBink.SetSoundSystem(m_hBink.OpenDirectSound, open_param);
		m_hBink.SetSoundTrack(1, (U32*)&track);

		is_bink = 1;
		return m_hBink.Open(name, flags);
	}

	BINK* bnk = new BINK;
	memset(bnk, 0, sizeof(BINK));
	bnk->player = b;

	if(b->ff.vstream->nb_frames)
		bnk->Frames = (U32)b->ff.vstream->nb_frames;	// mp4
	else
		bnk->Frames = (U32)b->ff.vstream->duration;	// bink

	b->FPS = av_q2d(b->ff.vstream->avg_frame_rate);
	bnk->FrameRate = (U32)b->FPS;
	bnk->Width = b->ff.vctx->width;
	bnk->Height = b->ff.vctx->height;
	bnk->OpenFlags = flags;

	b->Init(DSoundPtr);

	return bnk;
}

void WINAPI BinkSetSoundTrack(int total_tracks, U32* tracks)
{
	UNREFERENCED_PARAMETER(total_tracks);

	track = *tracks;
}

S32  WINAPI BinkSetSoundSystem(BINKSNDSYSOPEN open, U32 param)
{
	open_fn = open;
	open_param = param;
	//open(param);

	return 1;
}

void WINAPI BinkOpenDirectSound(U32 param)
{
	DSoundPtr = (LPDIRECTSOUND)param;
}

void WINAPI BinkClose(HBINK bnk)
{
	if (is_bink)
		m_hBink.Close(bnk);
	else
	{
		bnk->player->Pause();
		bnk->player->Shut();
		bnk->player->ff.close();
		delete bnk->player;

		track = 0;

		delete bnk;
	}
}

S32  WINAPI BinkCopyToBuffer(HBINK bnk, void* dest, U32 destpitch, U32 destheight, U32 destx, U32 desty, U32 flags)
{
	if (is_bink)
		return m_hBink.CopyToBuffer(bnk, dest, destpitch, destheight, destx, desty, flags);

	UNREFERENCED_PARAMETER(flags);

	uint8_t* d = (uint8_t*)dest;
	
	LOCKED_RECT r;
	bnk->player->vid.disp->Lock(&r);

	uint8_t* s = (uint8_t*)r.pBits;

	d = &d[desty * destpitch + destx];

	for (UINT y = 0; y < destheight; y++)
	{
		memcpy(d, s, destpitch);
		d += destpitch;
		s += r.Pitch;
	}

	bnk->player->vid.disp->Unlock();

	return 0;	// return no frames skipped
}

void WINAPI BinkNextFrame(HBINK bnk)
{
	if (is_bink)
		m_hBink.NextFrame(bnk);
	else bnk->FrameNum++;
}

S32  WINAPI BinkDoFrame(HBINK bnk)
{
	if (is_bink)
		return m_hBink.DoFrame(bnk);

	if (bnk->FrameNum == 0)
		bnk->player->Start();

	return 0;	// return no frames skipped
}

S32  WINAPI BinkWait(HBINK bnk)
{
	if (is_bink)
		return m_hBink.Wait(bnk);

	UNREFERENCED_PARAMETER(bnk);

	return 0;	// it's okay to display frame
}

S32  WINAPI BinkPause(HBINK bnk, S32 pause)
{
	if (is_bink)
		return m_hBink.Pause(bnk, pause);

	// pause
	if (pause)
	{
		bnk->player->Pause();
		return 0;
	}
	// resume
	else
	{
		bnk->player->Resume();
		return 1;
	}
}
