/**
* Copyright (C) 2023 Gemini
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*/

#include "bink.h"
#include <chrono>
#include <thread>

#define THREAD_ADEC_PAUSE		(1 << 0)
#define THREAD_VDEC_PAUSE		(1 << 1)
#define THREAD_R_PAUSE			(1 << 2)
#define THREAD_D_CLOSE			(1 << 3)
#define THREAD_AREN_PAUSE
#define THREAD_VREN_PAUSE

#define MAX_FRAMES		3
#define MAX_AUDIO		16
#define AUDIO_SIZE		4096
#define BUF_MULT		4

void ADXD_SetLevel(int level);
void ADXD_Log(const char* fmt, ...);

/////////////////////////////////////////////////////////
// an actual microsleep function
void usleep(int64_t usec)
{
	std::this_thread::sleep_for(std::chrono::microseconds(usec));
}

/////////////////////////////////////////////////////////
// direct sound buffer interface for streaming
#define ADEV_COMPLETED
#define ADEV_PAUSE

DWORD WINAPI DsndBuffer::thread(LPVOID ctx)
{
	DsndBuffer* th = (DsndBuffer*)ctx;
	DWORD pos;
	DWORD add = 0, snd_dwOffset = 0, bytes1 = 0;
	const DWORD	snd_dwBytes = th->qsize,
		snd_dwTotal = th->size;
	void* ptr;

	while (th->loops)
	{
		if (th->paused)
		{
			usleep(20 * 1000);
			continue;
		}

		th->buf->GetCurrentPosition(&pos, nullptr);

		if (pos - snd_dwOffset < 0)
			add = th->size;
		if (pos + add - snd_dwOffset > 2 * snd_dwBytes + 16)
		{
			th->buf->Lock(snd_dwOffset, snd_dwBytes, &ptr, &bytes1, nullptr, nullptr, 0);
			memcpy(ptr, th->b->aud.data[th->b->aud.head].data(), bytes1);
			th->buf->Unlock(ptr, bytes1, nullptr, 0);

			if (th->b->aud.apts)
				*th->b->aud.apts = th->b->aud.ppts[th->b->aud.head];
			if (++th->b->aud.head == th->b->aud.bufnum) th->b->aud.head = 0;
			ReleaseSemaphore(th->b->aud.semw, 1, NULL);

			DWORD total = snd_dwBytes + snd_dwOffset;
			snd_dwOffset = total;
			if (snd_dwTotal <= total)
				snd_dwOffset = total - snd_dwTotal;
		}

		Sleep(0);
	}

	th->exited = 1;

	return 0;
}

void DsndBuffer::create(size_t _size, BinkMovie* bik, LPDIRECTSOUND lpDS)
{
	b = bik;

	// size stuff
	size = _size;
	hsize = size / 2;
	qsize = size / 4;

	// ffmpeg friendly buffer attributes
	WAVEFORMATEX fmt = { 0 };
	fmt.cbSize = sizeof(fmt);
	fmt.nSamplesPerSec = BINK_AUDIO_RATE;
	fmt.nBlockAlign = 4;
	fmt.nChannels = 2;
	fmt.nAvgBytesPerSec = BINK_AUDIO_RATE * 4;
	fmt.wBitsPerSample = 16;
	fmt.wFormatTag = WAVE_FORMAT_PCM;

	// the actual buffer
	DSBUFFERDESC desc = { 0 };
	desc.dwSize = sizeof(desc);
	desc.lpwfxFormat = &fmt;
	desc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS;
	desc.dwBufferBytes = size;
	lpDS->CreateSoundBuffer(&desc, &buf, nullptr);

	// fill thread
	loops = 1;
	exited = 0;
	paused = 0;
	th = CreateThread(nullptr, 0, thread, this, 0, nullptr);

	play();
}

void DsndBuffer::destroy()
{
	if (th) stop();

	// kill the DS buffer
	buf->Release();
	buf = nullptr;
}

void DsndBuffer::play()
{
	buf->Play(0, 0, DSBPLAY_LOOPING);
}

void DsndBuffer::stop()
{
	// stop buffer so thread doesn't crash
	if (!paused)
		buf->Stop();

	// kill thread
	loops = 0;
	//while (exited != 1);
	WaitForSingleObject(th, INFINITE);
	th = nullptr;
}

void DsndBuffer::pause()
{
	if (!paused)
	{
		buf->Stop();
		paused = 1;
	}
}

void DsndBuffer::resume()
{
	if (paused)
	{
		buf->Play(0, 0, DSBPLAY_LOOPING);
		paused = 0;
	}
}

/////////////////////////////////////////////////////////
// generic texture buffer
void Texture::Create(int _w, int _h)
{
	if (buffer)
		delete[] buffer;

	w = _w;
	h = _h;
	buffer = new DWORD[_w * _h];
	memset(buffer, 0, _w * h * 4);
	pitch = w * sizeof(*buffer);
}

void Texture::Release()
{
	if (buffer)
	{
		//while (lock_cnt)
		//	usleep(20 * 1000);
		delete[] buffer;
		buffer = nullptr;
	}
	w = 0;
	h = 0;
	pitch = 0;
}

void Texture::Lock(LOCKED_RECT* r)
{
	lock_cnt++;
	r->pBits = buffer;
	r->Pitch = pitch;
}

void Texture::Unlock()
{
	lock_cnt--;
}

/////////////////////////////////////////////////////////
#define COPY_FRAME		1

#define VDEV_CLOSE		(1 << 0)
#define VDEV_PAUSE		(1 << 1)
#define VDEV_COMPLETED	(1 << 2)

DWORD WINAPI Video::thread(LPVOID ctx)
{
	Video* v = (Video*)ctx;
	v->decode();

	return 0;
}

void Video::init(int video_w, int video_h, int bufs, BinkMovie* bik)
{
	tickavdiff = tickframe = ticksleep = 0;
	ticklast = 0;
	completed_counter = 0;
	completed_apts = completed_vpts = 0;
	start_pts = end_pts = start_tick = 0;

	tickframe = (int32_t)(1000. / bik->FPS);
	ticksleep = tickframe;
	start_pts = AV_NOPTS_VALUE;

	apts = vpts = -1;
	end_pts = 0;

	w = video_w;
	h = video_h;

	bufnum = bufs ? bufs : MAX_FRAMES;

	// textures and synch points
	bmp = std::vector<Texture*>(bufnum);
	for (int i = 0; i < bufnum; i++)
	{
		bmp[i] = new Texture;
		bmp[i]->Create(w, h);
	}

#if COPY_FRAME
	disp = new Texture;
	disp->Create(w, h);
#else
	disp = nullptr;
#endif
	ppts = std::vector<int64_t>(bufnum);

	head = tail = 0;

	// semaphores for write/read
	semr = CreateSemaphoreA(nullptr, 0,      1024, nullptr);
	semw = CreateSemaphoreA(nullptr, bufnum, 1024, nullptr);

	// thread init
	status = 0;
	th = CreateThread(nullptr, 0, thread, this, 0, nullptr);
}

void Video::shut()
{
	if (th) stop();

	// release data structures
	for (int i = 0; i < bufnum; i++)
	{
		if (bmp[i])
		{
			bmp[i]->Release();
			delete bmp[i];
		}
	}
	bmp.clear();
#if COPY_FRAME
	disp->Release();
	delete disp;
#endif
	disp = nullptr;

	// release semaphores
	CloseHandle(semr); semr = nullptr;
	CloseHandle(semw); semw = nullptr;
	ppts.clear();
}

void Video::stop()
{
	// kill thread
	status = VDEV_CLOSE;
	ReleaseSemaphore(semr, 1, nullptr);
	WaitForSingleObject(th, INFINITE);

	th = nullptr;
}

void Video::decode()
{
	while (1)
	{
		WaitForSingleObject(semr, INFINITE);

		if (status & VDEV_CLOSE)
			break;

		int64_t cvpts = vpts = ppts[head];
		if (cvpts != -1)
		{
#if COPY_FRAME
			LOCKED_RECT rd, rs;
			disp->Lock(&rd);
			bmp[head]->Lock(&rs);

			memcpy(rd.pBits, rs.pBits, rd.Pitch * h);

			disp->Unlock();
			bmp[head]->Unlock();
#else
			disp = bmp[head];
#endif
		}

		if (++head == bufnum)
			head = 0;
		ReleaseSemaphore(semw, 1, nullptr);

		if (end_pts)
		{
			if (vpts + (30 * tickframe) > end_pts)
			{
				vpts = AV_NOPTS_VALUE;
				apts = AV_NOPTS_VALUE;
			}
		}

		sync();
	}
}

#define COMPLETE_COUNTER 30

void Video::sync()
{
	if (!(status & VDEV_PAUSE))
	{
		if (completed_apts != apts || completed_vpts != vpts)
		{
			completed_apts = apts;
			completed_vpts = vpts;
			completed_counter = 0;
			status &= ~VDEV_COMPLETED;
		}
		else if ((vpts == -1 || vpts == AV_NOPTS_VALUE) && ++completed_counter == COMPLETE_COUNTER)
		{
			status |= VDEV_COMPLETED;
		}

		int64_t tickcur = av_gettime_relative() / 1000;
		int tickdiff = (int)(tickcur - ticklast);
		ticklast = tickcur;

		if (start_pts == AV_NOPTS_VALUE)
		{
			start_pts = vpts;
			start_tick = tickcur;
		}

		int avdiff = (int)(apts - vpts - tickavdiff);								// diff between audio and video pts
		int scdiff = (int)(start_pts + tickcur - start_tick - vpts - tickavdiff);	// diff between system clock and video pts
		if (apts <= 0)
			avdiff = scdiff;	// if apts is invalid, sync vpts to system clock

		if (tickdiff - tickframe > 5) ticksleep--;
		if (tickdiff - tickframe < -5) ticksleep++;
		if (vpts >= 0)
		{
			if (avdiff > 500) ticksleep -= 3;
			else if (avdiff > 50) ticksleep -= 1;
			else if (avdiff < -500) ticksleep += 3;
			else if (avdiff < -50) ticksleep += 1;
		}
		if (ticksleep < 0) ticksleep = 0;
	}
	else ticksleep = tickframe;

	if (ticksleep > 0)
		usleep(ticksleep * 1000);
}

void Video::getavpts(int64_t** ppapts, int64_t** ppvpts)
{
	if (ppapts) *ppapts = &apts;
	if (ppvpts) *ppvpts = &vpts;
}

void Video::lock(uint8_t* buffer[], int linesize[])
{
	WaitForSingleObject(semw, INFINITE);

	LOCKED_RECT r;
	bmp[tail]->Lock(&r);

	if (buffer) buffer[0] = (uint8_t*)r.pBits;
	if (linesize) linesize[0] = r.Pitch;
}

void Video::unlock(int64_t pts)
{
	bmp[tail]->Unlock();
	ppts[tail] = pts;
	if (++tail == bufnum)
		tail = 0;
	ReleaseSemaphore(semr, 1, nullptr);
}

void Video::pause()
{
	status |= VDEV_PAUSE;
	start_pts = AV_NOPTS_VALUE;
}

void Video::resume()
{
	status &= ~VDEV_PAUSE;
	start_pts = AV_NOPTS_VALUE;
}

/////////////////////////////////////////////////////////
void Audio::init(int _bufnum, int _buflen, BinkMovie* bik, LPDIRECTSOUND lpDS)
{
	bufnum = _bufnum ? _bufnum : MAX_AUDIO;
	buflen = _buflen ? _buflen : AUDIO_SIZE;
	head = 0;
	tail = 0;
	ppts = std::vector<int64_t>(bufnum);
	semw = CreateSemaphoreA(nullptr, bufnum, bufnum, nullptr);

	data = std::vector<std::vector<int16_t>>(bufnum);
	for (int i = 0; i < bufnum; i++)
		data[i] = std::vector<int16_t>(buflen / 2);

	AudioBuf = std::vector<BUF>(bufnum);

	dssnd.create(buflen * BUF_MULT, bik, lpDS);
	speed = 0.;
}

void Audio::shut()
{
	reset();
	CloseHandle(semw); semw = nullptr;

	AudioBuf.clear();
	dssnd.destroy();
	data.clear();
	ppts.clear();
}

void Audio::reset()
{
	head = tail = 0;
	ReleaseSemaphore(semw, bufnum, 0);
}

void Audio::stop()
{
	dssnd.stop();
}

void Audio::sync(int64_t* papts)
{
	apts = papts;
}

void Audio::lock(BUF** ppab)
{
	WaitForSingleObject(semw, INFINITE);
	*ppab = &AudioBuf[tail];

	AudioBuf[tail].data = data[tail].data();
	AudioBuf[tail].size = buflen;
}

void Audio::unlock(int64_t pts)
{
	ppts[tail] = pts;

	if (++tail == bufnum)
		tail = 0;
}

void Audio::pause()
{
	dssnd.pause();
}

void Audio::resume()
{
	dssnd.resume();
}

/////////////////////////////////////////////////////////
PktQueue::PktQueue()
{
	fsize = asize = vsize = 0;
	fhead = ftail = ahead = 0;
	atail = vhead = vtail = 0;
	fsem = asem = vsem = nullptr;
	lock = nullptr;
}

void PktQueue::init(size_t size)
{
	fsize = size ? size : 256;
	asize = vsize = fsize;

	// packets
	bpkts = std::vector<AVPacket>(fsize);
	fpkts = std::vector<AVPacket*>(fsize);
	apkts = std::vector<AVPacket*>(asize);
	vpkts = std::vector<AVPacket*>(vsize);
	// semaphores
	fsem = CreateSemaphoreA(nullptr, fsize, 1024, nullptr);
	asem = CreateSemaphoreA(nullptr, 0, 1024, nullptr);
	vsem = CreateSemaphoreA(nullptr, 0, 1024, nullptr);
	// mutex
	lock = CreateMutexA(nullptr, false, nullptr);

	for (size_t i = 0; i < fsize; i++)
		fpkts[i] = &bpkts[i];
}

void PktQueue::reset()
{
	AVPacket* packet = nullptr;

	while ((packet = audio_dequeue()) != nullptr)
	{
		av_packet_unref(packet);
		free_enqueue(packet);
	}

	while ((packet = video_dequeue()) != nullptr)
	{
		av_packet_unref(packet);
		free_enqueue(packet);
	}

	fhead = ftail = 0;
	ahead = atail = 0;
	vhead = vtail = 0;
}

void PktQueue::shut()
{
	// free all queues
	reset();
	// delete semaphores
	CloseHandle(fsem); fsem = nullptr;
	CloseHandle(asem); asem = nullptr;
	CloseHandle(vsem); vsem = nullptr;
	// delete mutex
	CloseHandle(lock); lock = nullptr;
	// deallocate buffers
	bpkts.clear();
	fpkts.clear();
	apkts.clear();
	vpkts.clear();
}

void PktQueue::audio_enqueue(AVPacket* pkt)
{
	apkts[atail++ & (asize - 1)] = pkt;
	ReleaseSemaphore(asem, 1, nullptr);
}

AVPacket* PktQueue::audio_dequeue()
{
	if (WaitForSingleObject(asem, 0) != 0)
		return nullptr;
	return apkts[ahead++ & (asize - 1)];
}

AVPacket* PktQueue::audio_next()
{
	return apkts[ahead];
}

void PktQueue::video_enqueue(AVPacket* pkt)
{
	vpkts[vtail++ & (vsize - 1)] = pkt;
	ReleaseSemaphore(vsem, 1, nullptr);
}

AVPacket* PktQueue::video_dequeue()
{
	if (WaitForSingleObject(vsem, 0) != 0)
		return nullptr;
	return vpkts[vhead++ & (vsize - 1)];
}

inline void PktQueue::free_enqueue(AVPacket* pkt)
{
	WaitForSingleObject(lock, INFINITE);
	fpkts[ftail++ & (fsize - 1)] = pkt;
	ReleaseMutex(lock);
	ReleaseSemaphore(fsem, 1, nullptr);
}

AVPacket* PktQueue::free_dequeue()
{
	if (WaitForSingleObject(fsem, 0) != 0)
		return nullptr;
	return fpkts[fhead++ & (fsize - 1)];
}

void PktQueue::free_cancel(AVPacket* pkt)
{
	WaitForSingleObject(lock, INFINITE);
	fpkts[ftail++ & (fsize - 1)] = pkt;
	ReleaseMutex(lock);
	ReleaseSemaphore(fsem, 1, NULL);
}

/////////////////////////////////////////////////////////
// decoding for threads
void BinkMovie::process_video(AVFrame* vframe)
{
	AVFrame pic = { 0 };

	vid.lock(pic.data, pic.linesize);
	if (vframe->data[0] && vframe->pts != -1)
		sws_scale(ff.sws, vframe->data, vframe->linesize, 0, vframe->height, pic.data, pic.linesize);
	vid.unlock(vframe->pts);
}

void BinkMovie::process_audio(AVFrame* aframe)
{
	int sampnum = 0;
	int64_t apts = aframe->pts;

	do
	{
		if (aux_buf_avail == 0)
		{
			aud.lock(&aux_hdr_cur);
			apts += (int64_t)(aud.speed / FPS);
			if (aud.speed == 0.)
				aud.speed = 1000.;
			aux_buf_avail = (int)aux_hdr_cur->size;
			aux_buf_cur = (uint8_t*)aux_hdr_cur->data;
		}

		//++ do resample audio data ++//
		sampnum = swr_convert(ff.swr,
			(uint8_t**)&aux_buf_cur, aux_buf_avail / 4,
			(const uint8_t**)aframe->extended_data, aframe->nb_samples);
		aframe->extended_data = NULL;
		aframe->nb_samples = 0;
		aux_buf_avail -= sampnum * 4;
		aux_buf_cur += sampnum * 4;
		//-- do resample audio data --//

		if (aux_buf_avail == 0)
			aud.unlock(apts);

	} while (sampnum > 0);
}

/////////////////////////////////////////////////////////
// threaded interface for ffmpeg decoding
HANDLE hVid, hAud, hDem;
volatile BYTE is_suspended,
	is_asafe,
	is_vsafe,
	is_dsafe;

DWORD WINAPI BinkMovie::video_thread(LPVOID ctx)
{
	BinkMovie* b = (BinkMovie*)ctx;
	static const AVRational TIMEBASE_MS = { 1, 1000 };

	while (!(b->status & THREAD_D_CLOSE))
	{
		if (b->status & THREAD_ADEC_PAUSE)
		{
			usleep(20 * 1000);
			continue;
		}

		auto pkt = b->queue.video_dequeue();
		if (pkt == nullptr)
			usleep(20 * 1000);
		else
		{
			while (b->status & THREAD_ADEC_PAUSE);

			if (b->ff.decode_video(b->ff.vctx, b->ff.vframe, pkt))
			{
				b->ff.vframe->pts = av_rescale_q(b->ff.vframe->pts, b->ff.vstream->time_base, TIMEBASE_MS);

				b->process_video(b->ff.vframe);
			}

			av_packet_unref(pkt);
			b->queue.free_enqueue(pkt);
		}
	}

	return 0;
}

DWORD WINAPI BinkMovie::audio_thread(LPVOID ctx)
{
	BinkMovie* b = (BinkMovie*)ctx;
	int64_t apts = AV_NOPTS_VALUE;
	static const AVRational TIMEBASE_MS = { 1, 1000 };
	b->ff.aframe->pts = -1;

	AVFrame frame = { 0 };
	size_t samples = 0;
	AVPacket* apkt = nullptr;

	while (!(b->status & THREAD_D_CLOSE))
	{
		if (b->status & THREAD_ADEC_PAUSE)
		{
			usleep(20 * 1000);
			continue;
		}

		auto pkt = b->queue.audio_dequeue();
		if (pkt == nullptr)
			usleep(20 * 1000);
		else
		{
			while (b->status & THREAD_ADEC_PAUSE);

			// complete packets
retry:
			if (b->ff.decode_audio(b->ff.actx, &frame, pkt))
			{
				AVRational tb_sample_rate{ 1, b->ff.actx->sample_rate };

#if 0
				if (apts == AV_NOPTS_VALUE)
					apts = frame.pts;
				frame.pts = av_rescale_q(apts, b->ff.actx->time_base, tb_sample_rate);
#else
				if (frame.pts != AV_NOPTS_VALUE)
					frame.pts = av_rescale_q(frame.pts, b->ff.actx->time_base, tb_sample_rate);
				else if (apts != AV_NOPTS_VALUE)	// failsafe case for packets with no markers
					frame.pts = av_rescale_q(pkt->pts, b->ff.actx->time_base, tb_sample_rate);
#endif

				apts = frame.pts + frame.nb_samples;

				printf("apts pkt %lld\n", frame.pts);
				frame.pts = av_rescale_q(frame.pts, tb_sample_rate, TIMEBASE_MS);

				b->process_audio(&frame);

				av_packet_unref(pkt);
				b->queue.free_enqueue(pkt);
			}
			// incomplete packet
			else
			{
				apkt = pkt;
				auto head = b->queue.ahead;

				int ret = 0;
				do
				{
					pkt = b->queue.audio_dequeue();
					printf("filling gaps %lld\n", pkt->pts);
					ret = avcodec_send_packet(b->ff.actx, pkt);
					avcodec_receive_frame(b->ff.actx, &frame);
				} while (ret < 0);

				b->queue.ahead = head;
				pkt = apkt;
				goto retry;
			}
		}
	}

	return 0;
}

DWORD WINAPI BinkMovie::demux_thread(LPVOID ctx)
{
	BinkMovie* b = (BinkMovie*)ctx;

	while (!(b->status & THREAD_D_CLOSE))
	{
		auto pkt = b->queue.free_dequeue();
		if (pkt == nullptr)
			usleep(20 * 1000);
		else
		{
			auto retv = av_read_frame(b->ff.ctx, pkt);
			if (retv < 0)
			{
				av_packet_unref(pkt);
				b->queue.free_cancel(pkt);
				usleep(20 * 1000);
			}
			else
			{
				if (pkt->stream_index == b->ff.video_stream)
					b->queue.video_enqueue(pkt);
				else if (pkt->stream_index == b->ff.audio_stream)
					b->queue.audio_enqueue(pkt);
				else
				{
					av_packet_unref(pkt);
					b->queue.free_cancel(pkt);
				}
			}
		}
	}

	return 0;
}

void BinkMovie::CreateThreads()
{
	is_suspended = 0;
	status = 0;
	hDem = CreateThread(nullptr, 0, demux_thread, this, 0, nullptr);
	hAud = CreateThread(nullptr, 0, audio_thread, this, 0, nullptr);
	hVid = CreateThread(nullptr, 0, video_thread, this, 0, nullptr);
}

void BinkMovie::KillThreads()
{
	status |= THREAD_D_CLOSE;

	WaitForSingleObject(hDem, INFINITE); hDem = nullptr;
	WaitForSingleObject(hAud, INFINITE); hAud = nullptr;
	WaitForSingleObject(hVid, INFINITE); hVid = nullptr;
}

void BinkMovie::Init(LPDIRECTSOUND lpDS)
{
	aux_buf_avail = 0;
	aux_buf_cur = nullptr;
	aux_hdr_cur = nullptr;

	status = THREAD_ADEC_PAUSE | THREAD_VDEC_PAUSE | THREAD_R_PAUSE;

	queue.init(0);
	vid.init(ff.vctx->width, ff.vctx->height, MAX_FRAMES, this);
	aud.init(MAX_AUDIO, AUDIO_SIZE, this, lpDS);

	int64_t* papts = nullptr;
	vid.getavpts(&papts, nullptr);
	aud.sync(papts);

	CreateThreads();
}

void BinkMovie::Start()
{
	status &= ~(THREAD_ADEC_PAUSE | THREAD_VDEC_PAUSE | THREAD_R_PAUSE);
}

void BinkMovie::Shut()
{
	vid.resume();
	aud.resume();

	KillThreads();

	aud.shut();
	vid.shut();
	queue.shut();
}

void BinkMovie::Pause()
{
	if (is_suspended == 0)
	{
		status &= ~(THREAD_ADEC_PAUSE | THREAD_VDEC_PAUSE | THREAD_R_PAUSE);
		is_suspended = 1;
	}

	aud.pause();
	vid.pause();
}

void BinkMovie::Resume()
{
	aud.resume();
	vid.resume();

	if (is_suspended == 1)
	{
		status |= THREAD_ADEC_PAUSE | THREAD_VDEC_PAUSE | THREAD_R_PAUSE;
		is_suspended = 0;
		SwitchToThread();
	}
}
