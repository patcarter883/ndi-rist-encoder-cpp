# NDI RIST Encoder
Let's you select an NDI source, encodes it, sticks it in an MPEG Transport Stream, and pushes it out to a RIST receiver.

Will encode to H264, H265, or AV1 using software, Nvidia, AMD, or Intel encoding.

![NDI RIST Encoder Diagram](https://github.com/patcarter883/ndi-rist-encoder-cpp/assets/33055183/67ae0df2-b8f3-4aa5-9ae6-eab974de4587)

# Getting Started
## Installation
- Install the AV1 over MPEGTS patched [GStreamer](https://github.com/patcarter883/ndi-rist-encoder-cpp/releases/download/gstreamer/gstreamer-1.0-msvc-x86_64-1.23.0.1.msi) on both the encoder and server.
- Install the [encoder app](https://github.com/patcarter883/ndi-rist-encoder-cpp/releases/download/v0.2.0/NDI.Rist.Encoder-0.2.0-win64.exe).
- Download the [server app](https://github.com/patcarter883/ndi-rist-encoder-cpp/releases/download/v0.2.0/ndi-rist-server.exe) and run on your server. This currently requires an Nvidia GPU for video decoding and encoding. Support for Intel and AMD are planned. The server is being developed and tested on a Vultr Cloud GPU Server
- Make sure your server is accessible from you encoder computer, and has network ports 5000/UDP and 5999/TCP open.

## Encoder App
- Select your NDI source.
- Setup the Encode Section
  - Select the codec to be used for the stream being sent from the encoder computer to the server. If you select AV1 and have not installed the custom GStreamer above it will not work.
  - Select the encoder to be used.
    - Software encoding uses x264, and x265 encoders fast preset. If you have chosen AV1 as your codec and the software encoder the stream will start, but most likely won't work successfully unless you are running the encoder on some kind of magic beast of a system that can do this in realtime. The software AV1 combination is really only here for testing purposes.
    - The AMD encoder will use your AMD GPU to encode the video stream. To encode AV1 on an AMD GPU you need at least a RX7000 series card.
    - The Intel QSV encoder will use an integrated Intel graphics, or Intel Arc GPU to encode the video stream. To encode AV1 using the Intel QSV encoder you need an Intel Arc GPU.
    - The Nvidia NVENC encoder will use your Nvidia GPU to encode the video stream. To encode AV1 using the NVENC encoder you will need at least an RTX4000 series card.
  - Enter the bitrate you want to encode your video at in the Video Bitrate input. This must be below the maximum speed of the connection between the encoder and server, and below the speed of the RIST Settings bandwidth. This value is in kbps e.g. for 4Mbps enter 4000.
- Setup the Output Section
    - In the RIST address input enter the ip address and port of the system your are running the server on e.g. 127.0.0.1:5000
    - In the RTMP Server input enter the address of the RTMP server you want the video streamed to after it has been reencoded to a high bitrate H264 stream e.g. rtmp://a.rtmp.youtube.com/live2
    - In the RTMP Key input enter the stream key for the server you entered above.
    - In the Reencode Bitrate enter the bitrate you want to reencode the stream to for sending to the RTMP server. This value is in kbps e.g. for 18Mbps enter 18000. Keeping this as close to the maximum bitrate for the service you are sending to will ensure the highest quality stream.
    - Leave Use server remote control selected. If you wish to setup your own RIST receiver and stream handling on your server uncheck this box and the encoder app will act as a regular RIST sender.
- Setup the RIST Settings section.
  - In the bandwidth input enter either the maximum speed the connection between the encoder and server can maintain without encountering packet loss or latency increase. This must be higher than the encode bitrate. This value is in kbps e.g. for 6Mbps enter 6000.
  - LEAVE THE REST OF THESE SETTINGS ALONE. These are for fine tuning the connection to account for varying network conditions. If you are having problems the first thing to change is set Buffer Min to the same as Buffer Max. After that refer to at least [RIST Overview](https://code.videolan.org/rist/librist/-/wikis/6.-Appendix-RIST-Overview) to get an idea of how to go about further tuning.
- Click the Start Stream Button.
- Stream Stats
  - The stats are a per second snapshot of the stream. The important figure is the Link Quality, it accounts for lost and retransmitted packets. The encoder bitrate will be reduced as low as 1Mbps to keep the Link Quality at 100. If the encoder bitrate has been lowered due to the link quality it will be increased back up to the encode video bitrate if the link quality recovers. The encode bitrate can be manually increased or reduced while the stream is running by changing the Encode Video Bitrate.

AV1 works over MPEG Transport Stream if you use the [GStreamer](https://github.com/patcarter883/gstreamer.git) that I've included a pending merge request for AV1 MPEGTS support in. There is a built windows version available under releases to make it easy to get the client side up and running.

Can control the NDI RIST server to automatically start a RIST receiver and GStreamer pipeline that will transcode to H264 if needed, and send to an RTMP server.

![image](https://github.com/patcarter883/ndi-rist-encoder-cpp/assets/33055183/5e1ea5a9-eaa5-447f-8413-673757634c45)

# NDI RIST Server
Receives from the NDI RIST Encoder and transcodes to FLV over RTMP for sending to the usual suspects of live streaming destinations. This is currently a fixed GStreamer pipeline that uses Nvidia hardware decode and encode.

# Why
I needed to make the best use of limited bandwidth streaming over mobile data. I think RTMP should be shot and the 90's can come and get it's transport protocol back. I also wanted to ability to use bonded connections as a first class citizen of the the transport, no hacky borderline abuse of a VPN setup. So RTMP is out, and RIST is in.

# Building and installing

See the [BUILDING](BUILDING.md) document.

# Contributing

See the [CONTRIBUTING](CONTRIBUTING.md) document.

# Licensing

This project is licensed under GNU AGPLv3.
