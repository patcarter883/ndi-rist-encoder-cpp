# NDI RIST Encoder
It's a little bit of a mess I'm sure. Let's you select an NDI source, encodes it, RTP payloads it, and pushes it out to a RIST receiver using stream ID 1000 and 2000 to multiplex the video and audio.

## Why
I needed a single button press solution to stream AV1 out of OBS that didn't use the Record feature, or need lovingly hand crafted changes to the OBS config.

## Example Receive Pipeline
udpsrc name=udpsrc1 ! tsparse name=tsparse2 ! tsdemux name=tsdemux3 tsdemux3. ! av1parse name=av1parse9 ! tee name=tee12 tee12. ! queue name=queue14 ! av1dec name=av1dec15 ! queue name=queue16 ! videoconvert name=videoconvert19 ! videoscale name=videoscale20 ! openh264enc name=openh264enc17 ! h264parse name=h264parse22 ! queue name=queue21 ! flvmux name=flvmux5 name=264flvmux streamable=true ! tee name=tee25 ! rtmp2sink name=rtmp2sink26 tee12. ! flvmux name=flvmux4 name=av1flvmux streamable=true ! queue name=queue24 ! rtmp2sink name=rtmp2sink23 tsdemux3. ! aacparse name=aacparse8 ! tee name=tee11 tee11. ! queue name=queue13 max-size-time=5000000000 ! flvmux5. tee11. ! flvmux4. 
