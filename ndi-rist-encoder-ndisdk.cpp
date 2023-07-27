// ndi-rist-encoder.cpp : Defines the entry point for the application.
//
#include <config.h>
#if defined(HAVE_PTHREAD) || defined(_WIN32)
#include "ndi-rist-encoder.h"
#include <FL/Fl.H>
#include "UserInterface.cxx"
#include <Processing.NDI.Lib.h>
#include <chrono>
#include <thread>
#include <fmt/core.h>
#include "threads.h"
#include <gst/gst.h>

UserInterface *ui = new UserInterface;
Fl_Text_Buffer *logBuff = new Fl_Text_Buffer();
Fl_Thread prime_thread;
const NDIlib_source_t *p_sources;

int main(int argc, char **argv)
{
	/* init GStreamer */
  	gst_init (&argc, &argv);

		// Not required, but "correct" (see the SDK documentation).
	if (!NDIlib_initialize())
	{
		return 0;
	}

	Fl::lock();
	ui->logDisplay->buffer(logBuff);
	ui->show(argc, argv);

	fl_create_thread(prime_thread, findNdiDevices, ui);

	int result = Fl::run();

	return result;
}

void* previewNDISource() {

	GstElement *pipeline, *appsrc, *conv, *videosink;

		/* setup pipeline */
	pipeline = gst_pipeline_new ("pipeline");
	appsrc = gst_element_factory_make ("appsrc", "source");
	conv = gst_element_factory_make ("videoconvert", "conv");
	videosink = gst_element_factory_make ("xvimagesink", "videosink");


	NDIlib_recv_instance_t pNDI_recv = NDIlib_recv_create_v3();
	if (!pNDI_recv)
		return 0L;

	// Connect to our sources
	NDIlib_recv_connect(pNDI_recv, p_sources + ui->ndiSourceSelect->value());

	// We are now going to use a frame-synchronizer to ensure that the audio is dynamically
	// resampled and time-based con
	NDIlib_framesync_instance_t pNDI_framesync = NDIlib_framesync_create(pNDI_recv);

	// Within NDI, it is the sender that decides when a frame of video and audio should be transmitted. This is natural
	// because it is normally senders (for instance a Camera or an Audio Microphone) that have a crystal in them that
	// represents the clock. In professional video and audio systems you might genlock all of your sources together so that
	// senders are in fact clocked to match another source (for instance a master clock) but those sources still push the
	// video and audio down-stream when they have it to send. This means that within NDI by default you receive frames 
	// shortly after they are sent.
	//
	// It is not uncommon that you do not wish to receive frames at times that are determined by the sender, for instance
	// if you wish to display video on a computer monitor that refreshes at 60Hz then you need to make display the correct
	// frames are available each 1/60th of a second to display. 
	//
	//		Side-note : It is very common that applications implement this as simply "displaying the last video frame that
	//					has been received each 60th of a second", but in practice this results in the potential for very 
	//					jittery video in timing edge-cases.
	// 
	// A frame synchronizer is designed to solve this problem in a way that should always look "as good as possible" and adding
	// the minimum theoretically possible latency.
	//
	// The code below shows how to receive a video clock and use a frame-sync to clock it to match the current system time.
	// This might be used to have many incoming video sources and lock them to the system clock so that they can be recorded
	// in sync with each-other. 

	// The current reference clock time at which the stream was ready to start.
	typedef std::chrono::high_resolution_clock clock_to_use;
	clock_to_use::time_point reference_time;

	// This is the number of clock ticks in known units. This works at all common frame-rates.
	static const int64_t timebase = 120000; // Assumed to be divisible by 1000 below.
	int64_t accumulated_time = 0;

	// The desired audio sample rate, you could actually listen to the source to get this (see framesync documentation), but this
	// is good enough for me in this example code.
	static const int audio_sample_rate = 48000;
	static const int audio_no_channels = 2;

	// Run for one minute
	for (const auto start = clock_to_use::now(); clock_to_use::now() - start < std::chrono::minutes(5);) {
		// We capture the a video frame. It is pretty debatable what the default desired field type is here
		// but if you do not know it is actually better to use interleaved since it will then always give you
		// a correct sequence of field_0 and field_1's for fielded video. The downside is that a fielded frame-sync
		// has one to one field of extra latency than a progressive frame-sync ... but latency here is not as
		// important since we are just re-clocking video.
		NDIlib_video_frame_v2_t video_frame = {};
		NDIlib_framesync_capture_video(pNDI_framesync, &video_frame, NDIlib_frame_format_type_progressive);

		// If we have not got the first video frame yet, we are going to wait until we do.
		if (reference_time == clock_to_use::time_point()) {
			// Do we have video yet ?
			if (!video_frame.p_data) {
				// For correctness
				NDIlib_framesync_free_video(pNDI_framesync, &video_frame);

				// We do not know how long to sleep, so lets just assume things are about 30Hz which is not going to
				// use much CPU time and allows us to pretty accurately start the moment that we have video.
				std::this_thread::sleep_for(std::chrono::milliseconds(33));

				// Just continue waiting for a video source
				continue;
			}

			// This represents the start time for the recording. We use this to correctly count-up at the correct frame-rate
			// without gradually losing precision.
			reference_time = clock_to_use::now();
		}

		// This is how long this video frame is in clock ticks. Note that we have to handle video fields correctly here because if there
		// is a single field then it is going to be half the actual frame-rate which is what the is_video_field value is for.
		const bool is_video_field = ((video_frame.frame_format_type == NDIlib_frame_format_type_field_0) || (video_frame.frame_format_type == NDIlib_frame_format_type_field_1));
		const int field_rate_N = video_frame.frame_rate_N * (is_video_field ? 2 : 1);
		const int field_rate_D = video_frame.frame_rate_D;
		const int64_t frame_length = (timebase * (int64_t)field_rate_D) / (int64_t)field_rate_N;

		// The start and end of this frame in time
		const int64_t frame_start = accumulated_time;
		const int64_t frame_end = accumulated_time + frame_length;

		// Get the number of needed audio samples to exactly match this video frame.
		const int64_t audio_sample_no_start = (frame_start * (int64_t)audio_sample_rate) / timebase;
		const int64_t audio_sample_no_end = (frame_end * (int64_t)audio_sample_rate) / timebase;
		const int     audio_samples_no = (int)(audio_sample_no_end - audio_sample_no_start);

		// Capture the audio samples.
		NDIlib_audio_frame_v2_t audio_frame;
		NDIlib_framesync_capture_audio(pNDI_framesync, &audio_frame, audio_sample_rate, audio_no_channels, audio_samples_no);

		// **************************************************************************
		// TODO : Do something with both audio_frame and video_frame here.
		// 
		// Please note that the video resolution or frame-rate can change on the fly,
		// if you are using this code to write to disk you might need to ensure that
		// you do something about dynamic video format changes.
		// 
		// **************************************************************************
		printf(
			"Captured video at %d%s%1.2f, %d audio samples.\n",
			video_frame.yres * (is_video_field ? 2 : 1), (is_video_field ? "i" : "p"),
			(float)field_rate_N / (float)field_rate_D,
			audio_samples_no
		);

		// Free the audio and video data
		NDIlib_framesync_free_video(pNDI_framesync, &video_frame);
		NDIlib_framesync_free_audio(pNDI_framesync, &audio_frame);

		// This is now the new accumulated time.
		accumulated_time = frame_end;

		// We are now going to clock to the incoming video source. We only need millisecond precision here since this is all
		// self correcting. This code does not assume any real accuracy in the sleep_until function.
		const clock_to_use::time_point wait_until = reference_time + std::chrono::milliseconds(accumulated_time / (timebase / 1000));

		// We wait until the desired time.
		std::this_thread::sleep_until(wait_until);
	}

	// Free the frame-sync
	NDIlib_framesync_destroy(pNDI_framesync);

	// Destroy the receiver
	NDIlib_recv_destroy(pNDI_recv);
}

void* findNdiDevices(void *p)
{
	UserInterface *ui = (UserInterface *)p;

	// We are going to create an NDI finder that locates sources on the network.
	NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v2();

	uint32_t no_sources = 0;
	while (!no_sources)
	{
		NDIlib_find_wait_for_sources(pNDI_find, 5000 /* milliseconds */);

		// Get the updated list of sources
		
		p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);

		// Display all the sources.
		Fl::lock();
		ui->logDisplay->insert(fmt::format("NDI Sources Updated ({} found).\n", no_sources).c_str());
		for (uint32_t i = 0; i < no_sources; i++) {
			ui->logDisplay->insert(fmt::format("{}. {}\n", i + 1, p_sources[i].p_ndi_name).c_str());
			ui->ndiSourceSelect->add(p_sources[i].p_ndi_name);
		}
		ui->logDisplay->insert("\n");
		Fl::unlock();
		Fl::awake();
	}

	// Destroy the NDI finder
	NDIlib_find_destroy(pNDI_find);

	// Finished
	NDIlib_destroy();

	return 0L;
}
#endif // HAVE_PTHREAD || _WIN32