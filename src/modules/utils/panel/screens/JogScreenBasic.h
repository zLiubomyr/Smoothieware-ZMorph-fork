/*
      This file is part of Smoothie (http://smoothieware.org/). The motion control part is heavily based on Grbl (https://github.com/simen/grbl).
      Smoothie is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
      Smoothie is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
      You should have received a copy of the GNU General Public License along with Smoothie. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef JOGSCREENBASIC_H
#define JOGSCREENBASIC_H

#include "PanelScreen.h"

class ControlScreen;
class JogScreen;


class JogScreenBasic : public PanelScreen {
    public:
        JogScreenBasic();
        void on_main_loop();
        void on_refresh();
        void on_enter();
        void display_menu_line(uint16_t line);
        void set_jog_increment(float i) { jog_increment = i;}
        void clicked_menu_entry(uint16_t line);
        int idle_timeout_secs() { return 60; }

    private:
        void display_axis_line(char axis);
        void enter_axis_control(char axis);
        void enter_menu_control();
        void get_current_pos(float *p);
        void set_current_pos(char axis, float p);
        char control_mode;
        char controlled_axis;
        bool pos_changed;
        float pos[3];
        float jog_increment;
        ControlScreen *control_screen;
        void preheat();
        void cooldown();
        void setup_temperature_screen();
        PanelScreen *extruder_screen;
        JogScreen *jog_screen;
        const char *command;
};






#endif
