# NDI RIST Encoder
Let's you select an NDI source, encodes it, sticks it in an MPEG Transport Stream, and pushes it out to a RIST receiver.

Will encode to H264, H265, or AV1 using software, Nvidia, AMD, or Intel encoding.

AV1 really works over the MPEG Transport Stream if you use the [GStreamer](https://github.com/patcarter883/gstreamer.git) that I've included a pending merge request for AV1 MPEGTS support in. There is a built windows version available under releases to make it easy to get the client side up and running.

Can control the NDI RIST server to automatically start a RIST receiver and GStreamer pipeline that will transcode to H264 if needed, and send to an RTMP server.

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
