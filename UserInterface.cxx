// generated by Fast Light User Interface Designer (fluid) version 1.0308

#include "UserInterface.h"

Fl_Menu_Item UserInterface::menu_codecSelect[] = {
 {"H264", 0,  (Fl_Callback*)select_codec_cb, (void*)("h264"), 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"H265", 0,  (Fl_Callback*)select_codec_cb, (void*)("h265"), 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"AV1", 0,  (Fl_Callback*)select_codec_cb, (void*)("av1"), 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {0,0,0,0,0,0,0,0,0}
};
Fl_Menu_Item* UserInterface::h264CodecChoice = UserInterface::menu_codecSelect + 0;
Fl_Menu_Item* UserInterface::h265CodecChoice = UserInterface::menu_codecSelect + 1;
Fl_Menu_Item* UserInterface::av1CodecChoice = UserInterface::menu_codecSelect + 2;

Fl_Menu_Item UserInterface::menu_encoderSelect[] = {
 {"Software", 0,  (Fl_Callback*)select_encoder_cb, (void*)("software"), 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"AMD", 0,  (Fl_Callback*)select_encoder_cb, (void*)("amd"), 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Intel QSV", 0,  (Fl_Callback*)select_encoder_cb, (void*)("qsv"), 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"Nvidia NVENC", 0,  (Fl_Callback*)select_encoder_cb, (void*)("nvenc"), 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {0,0,0,0,0,0,0,0,0}
};
Fl_Menu_Item* UserInterface::softwareEncoderChoice = UserInterface::menu_encoderSelect + 0;
Fl_Menu_Item* UserInterface::amdEncoderChoice = UserInterface::menu_encoderSelect + 1;
Fl_Menu_Item* UserInterface::qsvEncoderChoice = UserInterface::menu_encoderSelect + 2;
Fl_Menu_Item* UserInterface::nvencEncoderChoice = UserInterface::menu_encoderSelect + 3;

Fl_Menu_Item UserInterface::menu_transportSelect[] = {
 {"MPEG TS", 0,  (Fl_Callback*)select_transport_cb, (void*)("m2ts"), 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {"RTP", 0,  (Fl_Callback*)select_transport_cb, (void*)("rtp"), 0, (uchar)FL_NORMAL_LABEL, 0, 14, 0},
 {0,0,0,0,0,0,0,0,0}
};
Fl_Menu_Item* UserInterface::m2tsTransportChoice = UserInterface::menu_transportSelect + 0;
Fl_Menu_Item* UserInterface::rtpTransportChoice = UserInterface::menu_transportSelect + 1;

UserInterface::UserInterface() {
  { mainWindow = new Fl_Double_Window(1175, 724, "NDI RIST Encoder");
    mainWindow->user_data((void*)(this));
    { inputGroup = new Fl_Group(10, 25, 266, 199, "Input");
      { previewSourceBtn = new Fl_Button(64, 113, 185, 22, "Preview Source");
        previewSourceBtn->callback((Fl_Callback*)preview_cb);
      } // Fl_Button* previewSourceBtn
      { ndiSourceSelect = new Fl_Choice(104, 53, 140, 22, "NDI Source");
        ndiSourceSelect->down_box(FL_BORDER_BOX);
      } // Fl_Choice* ndiSourceSelect
      { btnRefreshSources = new Fl_Button(64, 83, 185, 22, "Refresh Sources");
        btnRefreshSources->callback((Fl_Callback*)refreshSources_cb);
      } // Fl_Button* btnRefreshSources
      { Fl_Button* o = new Fl_Button(63, 164, 190, 21, "Stream Source");
        o->callback((Fl_Callback*)streamSource_cb);
      } // Fl_Button* o
      { Fl_Button* o = new Fl_Button(63, 193, 190, 22, "Stream Standby");
        o->callback((Fl_Callback*)streamStandby_cb);
      } // Fl_Button* o
      inputGroup->end();
    } // Fl_Group* inputGroup
    { Fl_Group* o = new Fl_Group(293, 23, 252, 212);
      { encodeGroup = new Fl_Group(294, 23, 250, 97, "Encode");
        { codecSelect = new Fl_Choice(365, 48, 92, 22, "Codec");
          codecSelect->down_box(FL_BORDER_BOX);
          codecSelect->menu(menu_codecSelect);
        } // Fl_Choice* codecSelect
        { encoderSelect = new Fl_Choice(366, 73, 92, 22, "Encoder");
          encoderSelect->down_box(FL_BORDER_BOX);
          encoderSelect->menu(menu_encoderSelect);
        } // Fl_Choice* encoderSelect
        { encoderBitrateInput = new Fl_Input(366, 96, 90, 24, "Video Bitrate");
          encoderBitrateInput->callback((Fl_Callback*)encoder_bitrate_cb);
        } // Fl_Input* encoderBitrateInput
        encodeGroup->end();
      } // Fl_Group* encodeGroup
      { transportGroup = new Fl_Group(294, 156, 250, 79, "Transport");
        { transportSelect = new Fl_Choice(301, 175, 156, 22);
          transportSelect->down_box(FL_BORDER_BOX);
          transportSelect->menu(menu_transportSelect);
        } // Fl_Choice* transportSelect
        transportGroup->end();
      } // Fl_Group* transportGroup
      o->end();
    } // Fl_Group* o
    { outputGroup = new Fl_Group(562, 20, 266, 195, "Output");
      { ristAddressInput = new Fl_Input(610, 47, 218, 22, "Address");
        ristAddressInput->callback((Fl_Callback*)rist_address_cb);
      } // Fl_Input* ristAddressInput
      { ristPortInput = new Fl_Input(610, 77, 92, 22, "Port");
        ristPortInput->callback((Fl_Callback*)rist_port_cb);
      } // Fl_Input* ristPortInput
      { ristBufferInput = new Fl_Input(610, 107, 92, 22, "Buffer");
        ristBufferInput->callback((Fl_Callback*)rist_buffer_cb);
      } // Fl_Input* ristBufferInput
      { ristBandwidthInput = new Fl_Input(610, 137, 92, 22, "Bandwidth");
        ristBandwidthInput->callback((Fl_Callback*)rist_bandwidth_cb);
      } // Fl_Input* ristBandwidthInput
      { btnStartStream = new Fl_Button(610, 167, 218, 22, "Start Stream");
        btnStartStream->callback((Fl_Callback*)startStream_cb);
      } // Fl_Button* btnStartStream
      { btnStopStream = new Fl_Button(610, 193, 218, 22, "Stop Stream");
        btnStopStream->callback((Fl_Callback*)stopStream_cb);
        btnStopStream->deactivate();
      } // Fl_Button* btnStopStream
      outputGroup->end();
    } // Fl_Group* outputGroup
    { statsGroup = new Fl_Group(845, 20, 266, 200, "Stats");
      { bandwidthOutput = new Fl_Output(940, 45, 165, 24, "Bandwidth");
      } // Fl_Output* bandwidthOutput
      { linkQualityOutput = new Fl_Output(940, 71, 165, 24, "Link Quality");
      } // Fl_Output* linkQualityOutput
      { retransmittedPacketsOutput = new Fl_Output(940, 96, 165, 24, "Retransmitted Packets");
      } // Fl_Output* retransmittedPacketsOutput
      { rttOutput = new Fl_Output(940, 146, 165, 24, "RTT");
      } // Fl_Output* rttOutput
      { totalPacketsOutput = new Fl_Output(940, 121, 165, 24, "Total Packets");
      } // Fl_Output* totalPacketsOutput
      { encodeBitrateOutput = new Fl_Output(940, 196, 165, 24, "Encode Bitrate");
      } // Fl_Output* encodeBitrateOutput
      statsGroup->end();
    } // Fl_Group* statsGroup
    { logDisplay = new Fl_Text_Display(21, 358, 1132, 164, "Log");
    } // Fl_Text_Display* logDisplay
    { ristLogDisplay = new Fl_Text_Display(22, 547, 1132, 164, "RIST Log");
    } // Fl_Text_Display* ristLogDisplay
    mainWindow->end();
    mainWindow->resizable(mainWindow);
  } // Fl_Double_Window* mainWindow
}

void UserInterface::show(int argc, char **argv) {
  mainWindow->show(argc, argv);
}
