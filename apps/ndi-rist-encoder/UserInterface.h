// generated by Fast Light User Interface Designer (fluid) version 1.0308

#ifndef UserInterface_h
#define UserInterface_h
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Button.H>
#include "common.h"
extern void refreshSources_cb(Fl_Button*, void*);
extern void select_codec_cb(Fl_Menu_*, Codec);
extern void select_encoder_cb(Fl_Menu_*, Encoder);
#include <FL/Fl_Input.H>
extern void encoder_bitrate_cb(Fl_Input*, void*);
extern void rist_address_cb(Fl_Input*, void*);
extern void rtmp_address_cb(Fl_Input*, void*);
extern void rtmp_key_cb(Fl_Input*, void*);
#include <FL/Fl_Check_Button.H>
extern void use_rpc_cb(Fl_Check_Button*, void*);
extern void upscale_cb(Fl_Check_Button*, void*);
extern void reencodeBitrate_cb(Fl_Input*, void*);
extern void rist_bandwidth_cb(Fl_Input*, void*);
extern void rist_buffer_min_cb(Fl_Input*, void*);
extern void rist_buffer_max_cb(Fl_Input*, void*);
extern void rist_rtt_min_cb(Fl_Input*, void*);
extern void rist_rtt_max_cb(Fl_Input*, void*);
extern void rist_reorder_buffer_cb(Fl_Input*, void*);
extern void startStream_cb(Fl_Button*, void*);
extern void stopStream_cb(Fl_Button*, void*);
#include <FL/Fl_Output.H>
#include <FL/Fl_Text_Display.H>

class UserInterface {
public:
  UserInterface();
  Fl_Double_Window *mainWindow;
  Fl_Group *inputGroup;
  Fl_Choice *ndiSourceSelect;
  Fl_Button *previewSourceBtn;
  Fl_Button *btnRefreshSources;
  Fl_Group *encodeGroup;
  Fl_Choice *codecSelect;
  static Fl_Menu_Item menu_codecSelect[];
  static Fl_Menu_Item *h264CodecChoice;
  static Fl_Menu_Item *h265CodecChoice;
  static Fl_Menu_Item *av1CodecChoice;
  Fl_Choice *encoderSelect;
  static Fl_Menu_Item menu_encoderSelect[];
  static Fl_Menu_Item *softwareEncoderChoice;
  static Fl_Menu_Item *amdEncoderChoice;
  static Fl_Menu_Item *qsvEncoderChoice;
  static Fl_Menu_Item *nvencEncoderChoice;
  Fl_Input *encoderBitrateInput;
  Fl_Input *ristAddressInput;
  Fl_Input *rtmpAddressInput;
  Fl_Input *rtmpKeyInput;
  Fl_Check_Button *useRpcInput;
  Fl_Check_Button *upscaleInput;
  Fl_Input *reencodeBitrateInput;
  Fl_Input *ristBandwidthInput;
  Fl_Input *ristBufferMinInput;
  Fl_Input *ristBufferMaxInput;
  Fl_Input *ristRttMinInput;
  Fl_Input *ristRttMaxInput;
  Fl_Input *ristReorderBufferInput;
  Fl_Button *btnStartStream;
  Fl_Button *btnStopStream;
  Fl_Group *statsGroup;
  Fl_Output *bandwidthOutput;
  Fl_Output *linkQualityOutput;
  Fl_Output *retransmittedPacketsOutput;
  Fl_Output *rttOutput;
  Fl_Output *totalPacketsOutput;
  Fl_Output *encodeBitrateOutput;
  Fl_Text_Display *logDisplay;
  Fl_Text_Display *ristLogDisplay;
  void show(int argc, char **argv);
};
#endif
