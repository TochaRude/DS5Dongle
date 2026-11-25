//
// BOOTSEL button gestures, split out of bt.cpp.
//

#ifndef DS5_BRIDGE_BUTTON_FUNCTIONS_H
#define DS5_BRIDGE_BUTTON_FUNCTIONS_H

// Poll the BOOTSEL button at 10 Hz and dispatch gestures:
//   single click  -> disconnect / start pairing inquiry (bt_bootsel_click_action)
//   double click  -> reboot the Pico
//   triple click  -> reboot into BOOTSEL (USB mass storage)
//   ~1.5 s hold   -> clear all pairings                 (bt_bootsel_hold_action)
void button_check();

#endif // DS5_BRIDGE_BUTTON_FUNCTIONS_H
