// generated by Fast Light User Interface Designer (fluid) version 1.0400

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

UserInterface::UserInterface() {
  { mainWindow = new Fl_Double_Window(1172, 421, "NDI RIST Encoder");
    mainWindow->user_data((void*)(this));
    { inputGroup = new Fl_Group(25, 25, 258, 160, "Input");
      { previewSourceBtn = new Fl_Button(79, 161, 180, 24, "Preview Source");
        previewSourceBtn->callback((Fl_Callback*)preview_cb);
      } // Fl_Button* previewSourceBtn
      { ndiSourceSelect = new Fl_Choice(119, 53, 140, 22, "NDI Source");
        ndiSourceSelect->down_box(FL_BORDER_BOX);
      } // Fl_Choice* ndiSourceSelect
      { btnRefreshSources = new Fl_Button(79, 83, 185, 22, "Refresh Sources");
        btnRefreshSources->callback((Fl_Callback*)refreshSources_cb);
      } // Fl_Button* btnRefreshSources
      inputGroup->end();
    } // Fl_Group* inputGroup
    { encodeGroup = new Fl_Group(361, 25, 250, 192, "Encode");
      { codecSelect = new Fl_Choice(440, 50, 92, 22, "Codec");
        codecSelect->down_box(FL_BORDER_BOX);
        codecSelect->menu(menu_codecSelect);
      } // Fl_Choice* codecSelect
      encodeGroup->end();
    } // Fl_Group* encodeGroup
    { outputGroup = new Fl_Group(738, 29, 266, 195, "Output");
      { ristAddressInput = new Fl_Input(786, 56, 218, 22, "Address");
        ristAddressInput->callback((Fl_Callback*)rist_address_cb);
      } // Fl_Input* ristAddressInput
      { ristPortInput = new Fl_Input(786, 86, 92, 22, "Port");
        ristPortInput->callback((Fl_Callback*)rist_port_cb);
      } // Fl_Input* ristPortInput
      { ristBufferInput = new Fl_Input(786, 116, 92, 22, "Buffer");
        ristBufferInput->callback((Fl_Callback*)rist_buffer_cb);
      } // Fl_Input* ristBufferInput
      { ristBandwidthInput = new Fl_Input(786, 146, 92, 22, "Bandwidth");
        ristBandwidthInput->callback((Fl_Callback*)rist_bandwidth_cb);
      } // Fl_Input* ristBandwidthInput
      { btnStartStream = new Fl_Button(786, 176, 218, 22, "Start Stream");
        btnStartStream->callback((Fl_Callback*)startStream_cb);
      } // Fl_Button* btnStartStream
      { btnStopStream = new Fl_Button(786, 202, 218, 22, "Stop Stream");
        btnStopStream->callback((Fl_Callback*)stopStream_cb);
        btnStopStream->deactivate();
      } // Fl_Button* btnStopStream
      outputGroup->end();
    } // Fl_Group* outputGroup
    { logDisplay = new Fl_Text_Display(25, 242, 1132, 164, "Log");
    } // Fl_Text_Display* logDisplay
    mainWindow->end();
  } // Fl_Double_Window* mainWindow
}

void UserInterface::show(int argc, char **argv) {
  mainWindow->show(argc, argv);
}
