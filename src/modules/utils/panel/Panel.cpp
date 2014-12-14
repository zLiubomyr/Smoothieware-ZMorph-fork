#include "Panel.h"
#include "ui/CompositeItem.h"
#include "SlowTicker.h"
#include "screen/Screen.h"
#include "ui/FileShiftRegister.h"
#include "ui/Layout.h"
#include "ui/Link.h"
#include "ui/Group.h"
#include "ui/Cell.h"
#include "ui/Item.h"
#include "ui/Widget.h"
#include "ui/Event.h"
#include "I18n.h"
#include "Icon.h"
#include "Logo.h"
#include "modules/robot/RobotPublicAccess.h"
#include "PlayerPublicAccess.h"
#include "PublicDataRequest.h"
#include "PublicData.h"
#include "TemperatureControlPublicAccess.h"
#include "NetworkPublicAccess.h"
#include "PublicData.h"
#include "checksumm.h"
#include <functional>
#include <tuple>
#include "Kernel.h"
#include "detail/CommandQueue.h"
#include "version.h"

#include "mri.h"
//FOR SENDING GCODES
#include "Gcode.h"

#define LOCATION __attribute__ ((section ("AHBSRAM0")))
#define hotend_temp_checksum 	 CHECKSUM("hotend_temperature")
#define bed_temp_checksum    	 CHECKSUM("bed_temperature")
#define hotend_temp_ABS_checksum CHECKSUM("hotend_temperature_ABS")
#define bed_temp_ABS_checksum    CHECKSUM("bed_temperature_ABS")
#define hotend_temp_PLA_checksum CHECKSUM("hotend_temperature_PLA")
#define bed_temp_PLA_checksum    CHECKSUM("bed_temperature_PLA")

namespace info
{
	static Version version; 
}

CommandQueue<1> command_buffer;

std::string get_network()
{
    void *returned_data;
    if(PublicData::get_value( network_checksum, get_ip_checksum, &returned_data ))
    {
        uint8_t *ipaddr = (uint8_t *)returned_data;
        char buf[20];
        int n = snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
        buf[n] = 0;
        return buf;
    }
    return "No network";
}

void send_gcode(std::string g)
{
    Gcode gcode(g, &(StreamOutput::NullStream));
    THEKERNEL->call_event(ON_GCODE_RECEIVED, &gcode);
}

template <size_t i>
float get_current_pos_()
{
	void *returned_data;

    bool ok = PublicData::get_value( robot_checksum, current_position_checksum, &returned_data );
    if(ok)
    {
    	float *p = static_cast<float *>(returned_data);
		return p[i];
	}	
	else
	{
		return 0;
	}
}

void set_x_position(float position)
{
	char buffer[12];
	snprintf(buffer, sizeof(buffer), "G0 X%4.0f", position);
	command_buffer.push(std::bind(send_gcode, std::string(buffer)));
}

void set_y_position(float position)
{
	char buffer[12];
	snprintf(buffer, sizeof(buffer), "G0 Y%4.0f", position);
	command_buffer.push(std::bind(send_gcode, std::string(buffer)));
}

void set_z_position(float position)
{
	char buffer[12];
	snprintf(buffer, sizeof(buffer), "G0 Z%4.0f", position);
	command_buffer.push(std::bind(send_gcode, std::string(buffer)));
}

void set_hotend_temperature(float temperature)
{
	// THEKERNEL->config->value( panel_checksum, hotend_temp_ABS_checksum )->by_default(235.0f)->as_number();
	// THEKERNEL->config->value( panel_checksum, bed_temp_ABS_checksum    )->by_default(100.0f)->as_number();
	PublicData::set_value( temperature_control_checksum, hotend_checksum, &temperature );
}

void set_hotbed_temperature(float temperature)
{
	// default_hotend_temperature_PLA = THEKERNEL->config->value( panel_checksum, hotend_temp_PLA_checksum )->by_default(185.0f )->as_number();
	// default_bed_temperature_PLA    = THEKERNEL->config->value( panel_checksum, bed_temp_PLA_checksum    )->by_default(60.0f  )->as_number();
	PublicData::set_value( temperature_control_checksum, bed_checksum, &temperature );
}

std::tuple<float, float> get_hotend_temperature()
{
	void *returned_data;
	if(PublicData::get_value( temperature_control_checksum, hotend_checksum, current_temperature_checksum, &returned_data ))
	{
		pad_temperature t =  *static_cast<struct pad_temperature *>(returned_data);
		return std::make_tuple(t.target_temperature, t.current_temperature);
	}
	else
	{
		return std::make_tuple(0, 0);
	}
}

std::tuple<float, float> get_hotbed_temperature()
{
	void *returned_data;
	if(PublicData::get_value( temperature_control_checksum, bed_checksum, current_temperature_checksum, &returned_data ))
	{
		pad_temperature t =  *static_cast<struct pad_temperature *>(returned_data);
		return std::make_tuple(t.target_temperature, t.current_temperature);
	}
	else
	{
		return std::make_tuple(0, 0);
	}
}

bool is_file_being_played()
{
	void* returned_data;
	if(PublicData::get_value(player_checksum, is_playing_checksum, NULL, &returned_data) )
	{
		return *static_cast<bool*>(returned_data);
	}
	else
	{
		return false;
	}
}

void abort_playing_file()
{
	PublicData::set_value(player_checksum, abort_play_checksum, 0);
}

std::tuple<uint32_t, std::string> get_progress()
{
    void *returned_data;
    if (PublicData::get_value( player_checksum, get_progress_checksum, &returned_data ))
    {
    	pad_progress p =  *static_cast<struct pad_progress *>(returned_data);
    	size_t name_start = p.filename.find_last_of("/") + 1;
    	size_t name_end   = p.filename.find_last_of(".");
        return(std::make_tuple(p.percent_complete, p.filename.substr(name_start, name_end-name_start) ));
    } else {
		return(std::make_tuple(0, i18n::no_file_caption));
    }
}

uint32_t estimate_remaining(uint32_t seconds_elapsed, uint32_t percent_complete)
{
	if(percent_complete > 3)
	{
		return (seconds_elapsed * 100)/(percent_complete) - seconds_elapsed;
	}
	else
	{
		return 0;
	}
}

std::tuple<uint32_t, uint32_t> get_time_progress()
{
    void *returned_data;
    if (PublicData::get_value( player_checksum, get_progress_checksum, &returned_data )) 
    {
    	pad_progress p =  *static_cast<struct pad_progress *>(returned_data);
        return(std::make_tuple(p.elapsed_secs, estimate_remaining(p.elapsed_secs, p.percent_complete) ));
    } else {
		return(std::make_tuple(0, 0));
    }
}

ui::Cell const main_menu_cells[] = 
{
	ui::Cell{/*x*/  0,  /*y*/   1, /*w*/ 43, /*h*/ 30},
	ui::Cell{/*x*/  0,  /*y*/  33, /*w*/ 43, /*h*/ 30},
	ui::Cell{/*x*/ 43,  /*y*/   1, /*w*/ 43, /*h*/ 30},
	ui::Cell{/*x*/ 43,  /*y*/  33, /*w*/ 43, /*h*/ 30},
	ui::Cell{/*x*/ 86,  /*y*/   1, /*w*/ 42, /*h*/ 30},
	ui::Cell{/*x*/ 86,  /*y*/  33, /*w*/ 42, /*h*/ 30}
};

ui::Cell const status_menu_cells[] = 
{
// row 1
	ui::Cell{/*x*/  0,    /*y*/   0, /*w*/ 128, /*h*/ 16 }, // hnotification
// row 2
	ui::Cell{/*x*/  0,    /*y*/   16, /*w*/ 128, /*h*/ 16}, // hotend
// row 3
	ui::Cell{/*x*/  0,    /*y*/ 32, /*w*/ 128, /*h*/ 16}, // hotbed
// row 4
	ui::Cell{/*x*/ 0,    /*y*/   48, /*w*/ 128, /*h*/ 16}, // Abort
};

ui::Cell const stacked_menu_cells[] = 
{
	ui::Cell{/*x*/ 2,  /*y*/   0, /*w*/ 124, /*h*/ 21},
	ui::Cell{/*x*/ 2,  /*y*/  21, /*w*/	124, /*h*/ 21},
	ui::Cell{/*x*/ 2,  /*y*/  42, /*w*/	124, /*h*/ 22}
};

ui::Cell const modal_menu_cells[] = 
{
	ui::Cell{/*x*/ 0,  /*y*/   0,  /*w*/ 128, /*h*/ 32},
	ui::Cell{/*x*/ 0,  /*y*/   32, /*w*/ 128, /*h*/ 32},
};

ui::Cell const splash_cell[] = 
{
	ui::Cell{/*x*/ 0,  /*y*/   0, /*w*/ 128, /*h*/ 64},
};

ui::Layout default_layout(main_menu_cells);
ui::Layout stacked_layout(stacked_menu_cells);
ui::Layout status_layout(status_menu_cells);
ui::Layout splash_layout(splash_cell);
ui::Layout modal_layout(modal_menu_cells);

enum class MainMenu : size_t
{
	Move,
	Heat,
	Print,
	Maintenance,
	Status,
	Options
};

enum class MoveMenu : size_t
{
	Back,
	Home,
	Z,
	X,
	Y
};

enum class ManualHeatMenu : size_t
{
	Back,
	Hotend,
	Hotbed
};

enum class SetupMenu : size_t
{
	HotendTemperature,
	HotbedTemperature,
	ZPosition,
	PlayerStatus
};


enum class HeatMenu : size_t
{
	Back,
	PreheatAbs,
	PreheatPla,
	ManualPreheat,
	CoolDown
};

enum class HomeMenu : size_t
{
	Back,
	Z,
	XY,
	XYZ
};

enum class StatusMenu : size_t
{
	HotendTemperature,
	HotbedTemperature,
	Z,
	Progress	
};

enum class MaintenanceMenu : size_t
{
	Back,
	ManualExtrusion,
	PrimePrinthead,
	FilamentChange,
	LevelBed
};

enum class ExtrudeMenu : size_t
{
	Back,
	Extrude,
	Retract
};

enum class OptionsMenu : size_t
{
	Back,
	Ip,
	Version
};

CompositeItem LOCATION logo_menu_items[] = 
{
	ui::LogoItem(i18n::back_caption, picture::logo, 18)
};

CompositeItem LOCATION status_menu_items[] = 
{
	ui::ProgressInfo(i18n::progress_caption, get_progress),
	ui::TimeInfo(i18n::progress_caption, get_time_progress),
	ui::FloatFloatInfo(i18n::hotend_temperature_caption, get_hotend_temperature),
	ui::FloatFloatInfo(i18n::hotbed_temperature_caption, get_hotbed_temperature),
};

CompositeItem LOCATION heat_menu_items[] = 
{
	ui::Item(i18n::back_caption),
	ui::Command(i18n::preheat_abs_caption, []{
		set_hotend_temperature(245); 
		set_hotbed_temperature(100);
	}),
	ui::Command(i18n::preheat_pla_caption, []{
		set_hotend_temperature(220); 
		set_hotbed_temperature(60);
	}),
	ui::Item(i18n::manual_preheat_caption),
	ui::Command(i18n::cool_down_caption, []{set_hotend_temperature(0); set_hotbed_temperature(0);})
};

CompositeItem LOCATION manual_heat_menu_items[] = 
{
	ui::Item(i18n::back_caption),
	ui::HeatControl(i18n::hotend_temperature_caption, get_hotend_temperature, set_hotend_temperature, 150),
	ui::HeatControl(i18n::hotbed_temperature_caption, get_hotbed_temperature, set_hotbed_temperature, 0)
};

CompositeItem LOCATION home_menu_items[] = 
{
	ui::Item(i18n::back_caption),
	ui::Command(i18n::home_z_caption, []{
		command_buffer.push( std::bind(send_gcode, "G28 Z") );
	}
	),
	ui::Command(i18n::home_xy_caption, []{
		command_buffer.push( std::bind(send_gcode, "G28 XY") );
	}
	),
	ui::Command(i18n::home_xyz_caption, []{
		command_buffer.push( std::bind(send_gcode, "G28 XYZ") );
	}
	)
};

CompositeItem LOCATION move_menu_items[] = 
{
	ui::Item(i18n::back_caption),
	ui::Item(i18n::home_caption),
	ui::PositionControl(i18n::z_caption, get_current_pos_<2>, set_z_position),
	ui::PositionControl(i18n::x_caption, get_current_pos_<0>, set_x_position),
	ui::PositionControl(i18n::y_caption, get_current_pos_<1>, set_y_position)
};

CompositeItem LOCATION main_menu_items[] = 
{
	ui::GraphicalItem(i18n::move_caption, icon::move),
	ui::GraphicalItem(i18n::heat_caption, icon::heat),
	ui::GraphicalItem(i18n::print_caption, icon::print),
	ui::GraphicalItem(i18n::maintenance_caption, icon::service),
	ui::GraphicalItem(i18n::status_caption, icon::status),
	ui::GraphicalItem(i18n::settings_caption, icon::setup)
};

CompositeItem LOCATION abort_menu_items[] = 
{
	ui::Command(i18n::abort_print_caption, []{command_buffer.push(abort_playing_file);}),
	ui::Item(i18n::not_abort_print_caption)
};

ui::FileShiftRegister file_browser;

CompositeItem LOCATION file_menu_items[] = 
{
	ui::File(file_browser, 0),
	ui::File(file_browser, 1),
	ui::File(file_browser, 2)
};

CompositeItem LOCATION maintenance_menu_items[] = 
{
	ui::Item(i18n::back_caption),
	ui::Item(i18n::manual_extrusion_caption),
	ui::Command(i18n::prime_printhead_caption, []{send_gcode("G91");send_gcode("G1 E100 F100");send_gcode("G90");}),
//	ui::Item(i18n::filament_change_caption),
//	ui::Item(i18n::level_bed_caption)
};

CompositeItem LOCATION extrude_menu_items[] = 
{
	ui::Item(i18n::back_caption),
	ui::Command(i18n::extrude_caption, []{ 
		command_buffer.push([]{send_gcode("G91");send_gcode("G1 E5 F100");send_gcode("G90");});
	}
	),
	ui::Command(i18n::retract_caption, []{ 
		command_buffer.push([]{send_gcode("G91");send_gcode("G1 E-5 F100");send_gcode("G90");});
	}
	)
};

CompositeItem LOCATION options_menu_items[] = 
{
	ui::Item(i18n::back_caption),
	ui::CharInfo(i18n::ip_caption, get_network),
	ui::CharInfo(i18n::version_caption, []{return info::version.get_build();})
};

CompositeItem LOCATION init_menu_items[] = 
{
	ui::Command(i18n::init_home, []{
		command_buffer.push( std::bind(send_gcode, "G28 XYZ") );
	}
	),
	ui::Item(i18n::dont_home),
};

ui::Widget logo_menu_widget(&splash_layout);
ui::Widget init_menu_widget(&modal_layout);
ui::Widget main_menu_widget(&default_layout);
ui::Widget move_menu_widget(&stacked_layout); // correctness is not checked during compilation
ui::Widget file_menu_widget(&stacked_layout);
ui::Widget abort_menu_widget(&modal_layout);
ui::Widget heat_menu_widget(&stacked_layout);
ui::Widget home_menu_widget(&stacked_layout);
ui::Widget manual_heat_menu_widget(&stacked_layout);
ui::Widget status_menu_widget(&status_layout);
ui::Widget maintenance_menu_widget(&stacked_layout);
ui::Widget extrude_menu_widget(&stacked_layout);
ui::Widget options_menu_widget(&stacked_layout);

ui::Link logo_menu_links[1];
ui::Link init_menu_links[2];
ui::Link main_menu_links[6];
ui::Link move_menu_links[5];
ui::Link file_menu_links[3];
ui::Link heat_menu_links[5];
ui::Link home_menu_links[4];
ui::Link abort_menu_links[2];
ui::Link manual_heat_menu_links[3];
ui::Link status_menu_links[4];
ui::Link maintenance_menu_links[3];
ui::Link extrude_menu_links[3];
ui::Link options_menu_links[3];

ui::Group logo(logo_menu_items, logo_menu_links, logo_menu_widget);
ui::Group init_menu(init_menu_items, init_menu_links, init_menu_widget);
ui::Group main_menu(main_menu_items, main_menu_links, main_menu_widget);
ui::Group move_menu(move_menu_items, move_menu_links, move_menu_widget);
ui::Group file_menu(file_menu_items, file_menu_links, file_menu_widget);
ui::Group abort_menu(abort_menu_items, abort_menu_links, abort_menu_widget);
ui::Group heat_menu(heat_menu_items, heat_menu_links, heat_menu_widget);
ui::Group home_menu(home_menu_items, home_menu_links, home_menu_widget);
ui::Group manual_heat_menu(manual_heat_menu_items, manual_heat_menu_links, manual_heat_menu_widget);
ui::Group status_menu(status_menu_items, status_menu_links, status_menu_widget);
ui::Group maintenance_menu(maintenance_menu_items, maintenance_menu_links, maintenance_menu_widget);
ui::Group extrude_menu(extrude_menu_items, extrude_menu_links, extrude_menu_widget);
ui::Group options_menu(options_menu_items, options_menu_links, options_menu_widget);

Panel::Panel()
:up_button(10, 5), down_button(10, 5), select_button(10, 5), user_interface(logo.get_link_to(0), screen)
{
	
	init_menu.set_link_for(index(0), status_menu.get_link_to(0));
	init_menu.set_link_for(index(1), status_menu.get_link_to(0));
	file_browser.open_directory("/");
	main_menu.set_link_for(index(MainMenu::Move), move_menu.get_link_to(index(MoveMenu::Back)));
	main_menu.set_link_for(index(MainMenu::Heat), heat_menu.get_link_to(index(HeatMenu::Back)));
	main_menu.set_link_for(index(MainMenu::Print), ui::Link(is_file_being_played, 0, &abort_menu, 0, &file_menu) );
	main_menu.set_link_for(index(MainMenu::Status), status_menu.get_link_to(0));
	main_menu.set_link_for(index(MainMenu::Maintenance), maintenance_menu.get_link_to(index(MaintenanceMenu::Back)) );
	main_menu.set_link_for(index(MainMenu::Options), options_menu.get_link_to(index(OptionsMenu::Back)) );

	move_menu.set_link_for(index(MoveMenu::Back), main_menu.get_link_to(index(MainMenu::Move)));
	move_menu.set_link_for(index(MoveMenu::Home), home_menu.get_link_to(index(HomeMenu::Back)));
	home_menu.set_link_for(index(HomeMenu::Back), move_menu.get_link_to(index(MoveMenu::Back)));
	
	file_menu.set_link_for(0, status_menu.get_link_to(0));
	file_menu.set_link_for(1, status_menu.get_link_to(0));
	file_menu.set_link_for(2, status_menu.get_link_to(0));

	heat_menu.set_link_for(index(HeatMenu::Back), main_menu.get_link_to(index(MainMenu::Heat)));
	heat_menu.set_link_for(index(HeatMenu::ManualPreheat), manual_heat_menu.get_link_to(index(ManualHeatMenu::Hotend)));
	
	manual_heat_menu.set_link_for(index(ManualHeatMenu::Back), heat_menu.get_link_to(index(HeatMenu::Back) ));
	maintenance_menu.set_link_for(index(MaintenanceMenu::Back), main_menu.get_link_to(index(MainMenu::Maintenance)) );
	maintenance_menu.set_link_for(index(MaintenanceMenu::ManualExtrusion), extrude_menu.get_link_to(index(ExtrudeMenu::Back)) );
	extrude_menu.set_link_for(index(ExtrudeMenu::Back), maintenance_menu.get_link_to(index(MaintenanceMenu::Back)));
	
	status_menu.set_link_for(0, main_menu.get_link_to(index(MainMenu::Status)));
	status_menu.set_link_for(1, main_menu.get_link_to(index(MainMenu::Status)));
	status_menu.set_link_for(2, main_menu.get_link_to(index(MainMenu::Status)));
	status_menu.set_link_for(3, main_menu.get_link_to(index(MainMenu::Status)));

	abort_menu.set_link_for(0, status_menu.get_link_to(0) );
	abort_menu.set_link_for(1, main_menu.get_link_to(index(MainMenu::Print)) );

	options_menu.set_link_for(index(OptionsMenu::Back), main_menu.get_link_to(index(MainMenu::Options)));

	logo.set_link_for(0, init_menu.get_link_to(0));
 //    default_hotend_temperature 	   = THEKERNEL->config->value( panel_checksum, hotend_temp_checksum )->by_default(185.0f )->as_number();
 //    default_bed_temperature    	   = THEKERNEL->config->value( panel_checksum, bed_temp_checksum    )->by_default(60.0f  )->as_number();
	// default_hotend_temperature_ABS = THEKERNEL->config->value( panel_checksum, hotend_temp_ABS_checksum )->by_default(235.0f )->as_number();
 //    default_bed_temperature_ABS    = THEKERNEL->config->value( panel_checksum, bed_temp_ABS_checksum    )->by_default(100.0f  )->as_number();
	// default_hotend_temperature_PLA = THEKERNEL->config->value( panel_checksum, hotend_temp_PLA_checksum )->by_default(185.0f )->as_number();
 //    default_bed_temperature_PLA    = THEKERNEL->config->value( panel_checksum, bed_temp_PLA_checksum    )->by_default(60.0f  )->as_number();

}

void Panel::on_module_loaded()
{
	this->register_for_event(ON_IDLE);
    this->register_for_event(ON_MAIN_LOOP);
    this->register_for_event(ON_SECOND_TICK);

    THEKERNEL->slow_ticker->attach( 23U,  this,  &Panel::button_tick );
    THEKERNEL->slow_ticker->attach( 50,   this,  &Panel::refresh_tick );
}

uint32_t Panel::button_tick(uint32_t dummy)
{
	this->button_state = this->screen.read_buttons();
	if(this->button_state & BUTTON_UP)
	{
		up_button.press();
	}
	else
	{
		up_button.tick();
	}
	if(this->button_state & BUTTON_DOWN)
	{
		down_button.press();
	}
	else
	{
		down_button.tick();
	}
	if(this->button_state & BUTTON_SELECT)
	{
		select_button.press();
	}
	else
	{
		select_button.tick();
	}
	return 0;
}

uint32_t Panel::refresh_tick(uint32_t dummy)
{
	this->refresh_flag = true;
	return 0;
}

void Panel::on_main_loop(void* argument)
{
	while(execute_next(command_buffer)){};
	if(refresh_flag)
	{
		refresh_flag = false;
		user_interface.render();
		user_interface.refresh();
	}
}

void Panel::on_second_tick(void* argument)
{
	this->tick_flag = true;
}

void Panel::on_idle(void* argument)
{
	if(tick_flag)
	{
		tick_flag = false;
		user_interface.dispatch(ui::TickEvent());
	}
	if(up_button.read())
	{
		user_interface.dispatch(ui::UpEvent());
	}
	if(down_button.read())
	{
		user_interface.dispatch(ui::DownEvent());
	}
	if(select_button.read())
	{
		screen.clear();
		user_interface.dispatch(ui::OkEvent());
	}
	if(refresh_flag)
	{
		refresh_flag = false;
		user_interface.render();
		user_interface.refresh();
	}
}