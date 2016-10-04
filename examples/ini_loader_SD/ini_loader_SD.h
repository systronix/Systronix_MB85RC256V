#ifndef		ini_loader_H_
#define		ini_loader_H_

//---------------------------< D E F A U L T   S E T T I N G S >----------------------------------------------
//
// these are the default setting.  If ever these are changed, make sure that the matching value in SALT.h is
// also changed so that the two have the same values
//

struct heat_settings							
	{
	char		day_temp [8];
	char		night_temp [8];
	char		watts [8];
	};

heat_settings habitat_A_heat_settings [13] =	// '1' indexed; [0] not used
	{
	{0, 0, 0},									// 0 - not used
	{{"85"}, {"80"}, {"25"}},	// 1: 85 day/80 night degrees F target, 25 watt heat lamp
	{{"85"}, {"80"}, {"25"}},	// 2
	{{"85"}, {"80"}, {"25"}},	// 3
	{{"85"}, {"80"}, {"25"}},	// 4
	{{"85"}, {"80"}, {"25"}},	// 5
	{{"85"}, {"80"}, {"25"}},	// 6
	{{"85"}, {"80"}, {"25"}},	// 7
	{{"85"}, {"80"}, {"25"}},	// 8
	{{"85"}, {"80"}, {"25"}},	// 9
	{{"85"}, {"80"}, {"25"}},	// 10
	{{"85"}, {"80"}, {"25"}},	// 11
	{{"85"}, {"80"}, {"25"}}	// 12
	};
heat_settings habitat_B_heat_settings [13] =	// '1' indexed; [0] not used
	{
	{0, 0, 0},									// 0 - not used
	{{"85"}, {"80"}, {"25"}},	// 1: 85 day/80 night degrees F target, 25 watt heat lamp
	{{"85"}, {"80"}, {"25"}},	// 2
	{{"85"}, {"80"}, {"25"}},	// 3
	{{"85"}, {"80"}, {"25"}},	// 4
	{{"85"}, {"80"}, {"25"}},	// 5
	{{"85"}, {"80"}, {"25"}},	// 6
	{{"85"}, {"80"}, {"25"}},	// 7
	{{"85"}, {"80"}, {"25"}},	// 8
	{{"85"}, {"80"}, {"25"}},	// 9
	{{"85"}, {"80"}, {"25"}},	// 10
	{{"85"}, {"80"}, {"25"}},	// 11
	{{"85"}, {"80"}, {"25"}}	// 12
	};
heat_settings habitat_EC_heat_settings [3] =	// '1' indexed; [0] not used
	{
	{0, 0, 0},									// 0 - not used
	{{"85"}, {"80"}, {"25"}},	// 1: 85 day/80 night degrees F target, 25 watt heat lamp
	{{"85"}, {"80"}, {"25"}}	// 2
	};

char overtemp_ignore_A [13][8] = 	// compartment overtemp alarm ignored when corresponding bit set here
	{
	{""}, {"no"}, {"no"}, {"no"}, {"no"}, {"no"}, {"no"},
	{"no"}, {"no"}, {"no"}, {"no"}, {"no"}, {"no"}
	};
	
char overtemp_ignore_B [13][8] = 	// compartment overtemp alarm ignored when corresponding bit set here
	{
	{""}, {"no"}, {"no"}, {"no"}, {"no"}, {"no"}, {"no"},
	{"no"}, {"no"}, {"no"}, {"no"}, {"no"}, {"no"}
	};

char overtemp_ignore_EC [3][8] = 	// compartment overtemp alarm ignored when corresponding bit set here
	{
	{""}, {"no"}, {"no"}
	};

char system_config [8] = {"SSWEC"};				// SS, SSWEC, B2B, B2BWEC, SBS
char system_dawn [8] = {"06:00"};				// minimal default: 06:00
char system_dusk [8] = {"18:00"};				// minimal default: 18:00
char system_auto_fan_1 [8] = {"80"};			// threshold temperatures 
char system_auto_fan_2 [8] = {"85"};
char system_auto_fan_3 [8] = {"90"};
char system_dhcp [8] = {"no"};					// does habitat use DHCP?
char system_ip [16];							// habitat's ip address
char system_mask [16];							// habitat's subnet mask
char system_server_ip [16];						// server's ip address
	
char system_dst [8] = {"yes"};					// true when this locale observes daylight saving time
char system_tz [8] = {"PST"};					// holds constant for this locale's UTC offset to standard time

struct user_settings							// these initialize to empty strings or 0; the minimal default settings
	{
	char		name [16];						// 15 character name + null terminator
	char		pin [8];						// 5 digit number 00000-99999; TODO: 4 digits and a check digit? some values illegal?
	char		rights [16];					// associate, manager, service, developer, manufacturer
	};
	
user_settings user [11];						// '1' indexed; [0] not used


//---------------------------< K E Y / V A L U E   P A I R S >------------------------------------------------
//
// 
//

struct kv_pair
	{
	const char key [20];
	char* value;
	};

kv_pair	kv_system [13] =
	{
	{{""}, 0},			// 1 indexed; [0] not used
	{{CONFIG_KEY},				system_config},
	{{DAWN_KEY},				system_dawn},
	{{DUSK_KEY},				system_dusk},
	{{AUTO_FAN_1_KEY},			system_auto_fan_1},
	{{AUTO_FAN_2_KEY},			system_auto_fan_2},
	{{AUTO_FAN_3_KEY},			system_auto_fan_3},
	{{DHCP_KEY},				system_dhcp},
	{{IP_KEY},					system_ip},
	{{MASK_KEY},				system_mask},
	{{SERVER_IP_KEY},			system_server_ip},
	{{TZ_KEY},					system_tz},
	{{DST_KEY},					system_dst}
	};
 
kv_pair	kv_habitat_A [49] =
	{
	{{""}, 0},			// 1 indexed; [0] not used
	{{DAY_A_D1C1_KEY},	 		habitat_A_heat_settings [1].day_temp},
	{{NIGHT_A_D1C1_KEY},		habitat_A_heat_settings [1].night_temp},
	{{DAY_A_D1C2_KEY},			habitat_A_heat_settings [2].day_temp},
	{{NIGHT_A_D1C2_KEY},		habitat_A_heat_settings [2].night_temp},
	{{DAY_A_D1C3_KEY},			habitat_A_heat_settings [3].day_temp},
	{{NIGHT_A_D1C3_KEY},		habitat_A_heat_settings [3].night_temp},

	{{DAY_A_D2C1_KEY},			habitat_A_heat_settings [4].day_temp},
	{{NIGHT_A_D2C1_KEY},		habitat_A_heat_settings [4].night_temp},
	{{DAY_A_D2C2_KEY},			habitat_A_heat_settings [5].day_temp},
	{{NIGHT_A_D2C2_KEY},		habitat_A_heat_settings [5].night_temp},
	{{DAY_A_D2C3_KEY},			habitat_A_heat_settings [6].day_temp},
	{{NIGHT_A_D2C3_KEY},		habitat_A_heat_settings [6].night_temp},

	{{DAY_A_D3C1_KEY},			habitat_A_heat_settings [7].day_temp},
	{{NIGHT_A_D3C1_KEY},		habitat_A_heat_settings [7].night_temp},
	{{DAY_A_D3C2_KEY},			habitat_A_heat_settings [8].day_temp},
	{{NIGHT_A_D3C2_KEY},		habitat_A_heat_settings [8].night_temp},
	{{DAY_A_D3C3_KEY},			habitat_A_heat_settings [9].day_temp},
	{{NIGHT_A_D3C3_KEY},		habitat_A_heat_settings [9].night_temp},
	
	{{DAY_A_D4C1_KEY},			habitat_A_heat_settings [10].day_temp},
	{{NIGHT_A_D4C1_KEY},		habitat_A_heat_settings [10].night_temp},
	{{DAY_A_D4C2_KEY},			habitat_A_heat_settings [11].day_temp},
	{{NIGHT_A_D4C2_KEY},		habitat_A_heat_settings [11].night_temp},
	{{DAY_A_D4C3_KEY},			habitat_A_heat_settings [12].day_temp},
	{{NIGHT_A_D4C3_KEY},		habitat_A_heat_settings [12].night_temp},

	{{HLAMP_A_D1C1_KEY},		habitat_A_heat_settings [1].watts},
	{{HLAMP_A_D1C2_KEY},		habitat_A_heat_settings [2].watts},
	{{HLAMP_A_D1C3_KEY},		habitat_A_heat_settings [3].watts},
	{{HLAMP_A_D2C1_KEY},		habitat_A_heat_settings [4].watts},
	{{HLAMP_A_D2C2_KEY},		habitat_A_heat_settings [5].watts},
	{{HLAMP_A_D2C3_KEY},		habitat_A_heat_settings [6].watts},
	{{HLAMP_A_D3C1_KEY},		habitat_A_heat_settings [7].watts},
	{{HLAMP_A_D3C2_KEY},		habitat_A_heat_settings [8].watts},
	{{HLAMP_A_D3C3_KEY},		habitat_A_heat_settings [9].watts},
	{{HLAMP_A_D4C1_KEY},		habitat_A_heat_settings [10].watts},
	{{HLAMP_A_D4C2_KEY},		habitat_A_heat_settings [11].watts},
	{{HLAMP_A_D4C3_KEY},		habitat_A_heat_settings [12].watts},
	{{OT_IGNORE_A_D1C1_KEY},	overtemp_ignore_A [1]},
	{{OT_IGNORE_A_D1C2_KEY},	overtemp_ignore_A [2]},
	{{OT_IGNORE_A_D1C3_KEY},	overtemp_ignore_A [3]},
	{{OT_IGNORE_A_D2C1_KEY},	overtemp_ignore_A [4]},
	{{OT_IGNORE_A_D2C2_KEY},	overtemp_ignore_A [5]},
	{{OT_IGNORE_A_D2C3_KEY},	overtemp_ignore_A [6]},
	{{OT_IGNORE_A_D3C1_KEY},	overtemp_ignore_A [7]},
	{{OT_IGNORE_A_D3C2_KEY},	overtemp_ignore_A [8]},
	{{OT_IGNORE_A_D3C3_KEY},	overtemp_ignore_A [9]},
	{{OT_IGNORE_A_D4C1_KEY},	overtemp_ignore_A [10]},
	{{OT_IGNORE_A_D4C2_KEY},	overtemp_ignore_A [11]},
	{{OT_IGNORE_A_D4C3_KEY},	overtemp_ignore_A [12]}
	};
	
kv_pair	kv_habitat_B [49] =
	{
	{{""}, 0},			// 1 indexed; [0] not used
	{{DAY_B_D1C1_KEY},			habitat_B_heat_settings [1].day_temp},
	{{NIGHT_B_D1C1_KEY},		habitat_B_heat_settings [1].night_temp},
	{{DAY_B_D1C2_KEY},			habitat_B_heat_settings [2].day_temp},
	{{NIGHT_B_D1C2_KEY},		habitat_B_heat_settings [2].night_temp},
	{{DAY_B_D1C3_KEY},			habitat_B_heat_settings [3].day_temp},
	{{NIGHT_B_D1C3_KEY},		habitat_B_heat_settings [3].night_temp},
	{{DAY_B_D2C1_KEY},			habitat_B_heat_settings [4].day_temp},
	{{NIGHT_B_D2C1_KEY},		habitat_B_heat_settings [4].night_temp},
	{{DAY_B_D2C2_KEY},			habitat_B_heat_settings [5].day_temp},
	{{NIGHT_B_D2C2_KEY},		habitat_B_heat_settings [5].night_temp},
	{{DAY_B_D2C3_KEY},			habitat_B_heat_settings [6].day_temp},
	{{NIGHT_B_D2C3_KEY},		habitat_B_heat_settings [6].night_temp},
	{{DAY_B_D3C1_KEY},			habitat_B_heat_settings [7].day_temp},
	{{NIGHT_B_D3C1_KEY},		habitat_B_heat_settings [7].night_temp},
	{{DAY_B_D3C2_KEY},			habitat_B_heat_settings [8].day_temp},
	{{NIGHT_B_D3C2_KEY},		habitat_B_heat_settings [8].night_temp},
	{{DAY_B_D3C3_KEY},			habitat_B_heat_settings [9].day_temp},
	{{NIGHT_B_D3C3_KEY},		habitat_B_heat_settings [9].night_temp},
	{{DAY_B_D4C1_KEY},			habitat_B_heat_settings [10].day_temp},
	{{NIGHT_B_D4C1_KEY},		habitat_B_heat_settings [10].night_temp},
	{{DAY_B_D4C2_KEY},			habitat_B_heat_settings [11].day_temp},
	{{NIGHT_B_D4C2_KEY},		habitat_B_heat_settings [11].night_temp},
	{{DAY_B_D4C3_KEY},			habitat_B_heat_settings [12].day_temp},
	{{NIGHT_B_D4C3_KEY},		habitat_B_heat_settings [12].night_temp},
	{{HLAMP_B_D1C1_KEY},		habitat_B_heat_settings [1].watts},
	{{HLAMP_B_D1C2_KEY},		habitat_B_heat_settings [2].watts},
	{{HLAMP_B_D1C3_KEY},		habitat_B_heat_settings [3].watts},
	{{HLAMP_B_D2C1_KEY},		habitat_B_heat_settings [4].watts},
	{{HLAMP_B_D2C2_KEY},		habitat_B_heat_settings [5].watts},
	{{HLAMP_B_D2C3_KEY},		habitat_B_heat_settings [6].watts},
	{{HLAMP_B_D3C1_KEY},		habitat_B_heat_settings [7].watts},
	{{HLAMP_B_D3C2_KEY},		habitat_B_heat_settings [8].watts},
	{{HLAMP_B_D3C3_KEY},		habitat_B_heat_settings [9].watts},
	{{HLAMP_B_D4C1_KEY},		habitat_B_heat_settings [10].watts},
	{{HLAMP_B_D4C2_KEY},		habitat_B_heat_settings [11].watts},
	{{HLAMP_B_D4C3_KEY},		habitat_B_heat_settings [12].watts},
	{{OT_IGNORE_B_D1C1_KEY},	overtemp_ignore_B [1]},
	{{OT_IGNORE_B_D1C2_KEY},	overtemp_ignore_B [2]},
	{{OT_IGNORE_B_D1C3_KEY},	overtemp_ignore_B [3]},
	{{OT_IGNORE_B_D2C1_KEY},	overtemp_ignore_B [4]},
	{{OT_IGNORE_B_D2C2_KEY},	overtemp_ignore_B [5]},
	{{OT_IGNORE_B_D2C3_KEY},	overtemp_ignore_B [6]},
	{{OT_IGNORE_B_D3C1_KEY},	overtemp_ignore_B [7]},
	{{OT_IGNORE_B_D3C2_KEY},	overtemp_ignore_B [8]},
	{{OT_IGNORE_B_D3C3_KEY},	overtemp_ignore_B [9]},
	{{OT_IGNORE_B_D4C1_KEY},	overtemp_ignore_B [10]},
	{{OT_IGNORE_B_D4C2_KEY},	overtemp_ignore_B [11]},
	{{OT_IGNORE_B_D4C3_KEY},	overtemp_ignore_B [12]}
	};

	
kv_pair	kv_habitat_EC [9] =
	{
	{{""}, 0},			// 1 indexed; [0] not used
	{{DAY_ECT_KEY},				habitat_EC_heat_settings [1].day_temp},
	{{NIGHT_ECT_KEY},			habitat_EC_heat_settings [1].night_temp},
	{{DAY_ECB_KEY},				habitat_EC_heat_settings [2].day_temp},
	{{NIGHT_ECB_KEY},			habitat_EC_heat_settings [2].night_temp},
	{{HLAMP_ECT_KEY},			habitat_EC_heat_settings [1].watts},
	{{HLAMP_ECB_KEY},			habitat_EC_heat_settings [2].watts},
	{{OT_IGNORE_ECT_KEY},		overtemp_ignore_EC [1]},
	{{OT_IGNORE_ECB_KEY},		overtemp_ignore_EC [2]}
	};

kv_pair	kv_users [31] =
	{
	{{""}, 0},			// 1 indexed; [0] not used
	{{NAME_1_KEY},				user [1].name},
	{{PIN_1_KEY},				user [1].pin},
	{{RIGHTS_1_KEY},			user [1].rights},
	{{NAME_2_KEY},				user [2].name},
	{{PIN_2_KEY},				user [2].pin},
	{{RIGHTS_2_KEY},			user [2].rights},
	{{NAME_3_KEY},				user [3].name},
	{{PIN_3_KEY},				user [3].pin},
	{{RIGHTS_3_KEY},			user [3].rights},
	{{NAME_4_KEY},				user [4].name},
	{{PIN_4_KEY},				user [4].pin},
	{{RIGHTS_4_KEY},			user [4].rights},
	{{NAME_5_KEY},				user [5].name},
	{{PIN_5_KEY},				user [5].pin},
	{{RIGHTS_5_KEY},			user [5].rights},
	{{NAME_6_KEY},				user [6].name},
	{{PIN_6_KEY},				user [6].pin},
	{{RIGHTS_6_KEY},			user [6].rights},
	{{NAME_7_KEY},				user [7].name},
	{{PIN_7_KEY},				user [7].pin},
	{{RIGHTS_7_KEY},			user [7].rights},
	{{NAME_8_KEY},				user [8].name},
	{{PIN_8_KEY},				user [8].pin},
	{{RIGHTS_8_KEY},			user [8].rights},
	{{NAME_9_KEY},				user [9].name},
	{{PIN_9_KEY},				user [9].pin},
	{{RIGHTS_9_KEY},			user [9].rights},
	{{NAME_10_KEY},				user [10].name},
	{{PIN_10_KEY},				user [10].pin},
	{{RIGHTS_10_KEY},			user [10].rights},
	};
#endif		// ini_loader_H_
