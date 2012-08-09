// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

//****************************************************************
// Function that controls aileron/rudder, elevator, rudder (if 4 channel control) and throttle to produce desired attitude and airspeed.
//****************************************************************

static void stabilize()
{
	float ch1_inf = 1.0;
	float ch2_inf = 1.0;
	float ch4_inf = 1.0;

// ArduRocket is going to need real airspeed data, not scaled or interpolated throttle
// Right now this uses baro speed (pitot/static) data only, need to extend to use GPS and integrated accel data
	float aspeed = airspeed.get_airspeed();

	// Calculate desired servo output for the roll
	// Note: no speed scaling for now, this needs to be corrected
	// ---------------------------------------------
	g.channel_roll.servo_out = g.pidServoRoll.get_pid((nav_roll - ahrs.roll_sensor), 1.0);
	g.channel_pitch.servo_out = g.pidServoPitch.get_pid((nav_pitch - ahrs.pitch_sensor), 1.0);
	g.channel_yaw.servo_out = g.pidServoYaw.get_pid((nav_yaw - ahrs.yaw_sensor), 1.0);

	// Mix Stick input to allow users to override control surfaces
	// -----------------------------------------------------------
	if ( ENABLE_STICK_MIXING == 1 ) {

		// TODO: use RC_Channel control_mix function?
		ch1_inf = (float)g.channel_roll.radio_in - (float)g.channel_roll.radio_trim;
		ch1_inf = fabs(ch1_inf);
		ch1_inf = min(ch1_inf, 400.0);
		ch1_inf = ((400.0 - ch1_inf) /400.0);

		ch2_inf = (float)g.channel_pitch.radio_in - g.channel_pitch.radio_trim;
		ch2_inf = fabs(ch2_inf);
		ch2_inf = min(ch2_inf, 400.0);
		ch2_inf = ((400.0 - ch2_inf) /400.0);

		ch4_inf = (float)g.channel_yaw.radio_in - (float)g.channel_yaw.radio_trim;
		ch4_inf = fabs(ch4_inf);
		ch4_inf = min(ch4_inf, 400.0);
		ch4_inf = ((400.0 - ch4_inf) /400.0);

		// scale the sensor input based on the stick input
		// -----------------------------------------------
		g.channel_roll.servo_out *= ch1_inf;
		g.channel_pitch.servo_out *= ch2_inf;
		g.channel_yaw.servo_out *= ch4_inf;

		// Mix in stick inputs
		// -------------------
		g.channel_roll.servo_out +=	g.channel_roll.pwm_to_angle();
		g.channel_pitch.servo_out += g.channel_pitch.pwm_to_angle();
		g.channel_yaw.servo_out += g.channel_yaw.pwm_to_angle();

		//Serial.printf_P(PSTR(" servo_out[CH_ROLL] "));
		//Serial.println(servo_out[CH_ROLL],DEC);
	}
}

/*****************************************
 * Calculate desired roll/pitch/yaw angles (in medium freq loop)
 *****************************************/

static void calc_nav_pitch()
{
// Need entirely new pitch logic
}


static void calc_nav_roll()
{
// Need entirely new roll logic
}


/*****************************************
 * Roll servo slew limit
 *****************************************/
/*
float roll_slew_limit(float servo)
{
	static float last;
	float temp = constrain(servo, last-ROLL_SLEW_LIMIT * delta_ms_fast_loop/1000.f, last + ROLL_SLEW_LIMIT * delta_ms_fast_loop/1000.f);
	last = servo;
	return temp;
}*/


/*****************************************
* Set the flight control servos based on the current calculated values
*****************************************/
static void set_servos(void)
{
	// vectorize the rc channels
	RC_Channel_aux* rc_array[NUM_CHANNELS];
	rc_array[CH_1] = NULL;
	rc_array[CH_2] = NULL;
	rc_array[CH_3] = NULL;
	rc_array[CH_4] = NULL;
	rc_array[CH_5] = &g.rc_5;
	rc_array[CH_6] = &g.rc_6;
	rc_array[CH_7] = &g.rc_7;
	rc_array[CH_8] = &g.rc_8;

	if(control_mode == MANUAL)
	{
		// do a direct pass through of radio values
		if (g.mix_mode == 0){
			g.channel_roll.radio_out 		= g.channel_roll.radio_in;
			g.channel_pitch.radio_out 		= g.channel_pitch.radio_in;
		} else {
			g.channel_roll.radio_out 		= APM_RC.InputCh(CH_ROLL);
			g.channel_pitch.radio_out 		= APM_RC.InputCh(CH_PITCH);
		}
		g.channel_throttle.radio_out 	= g.channel_throttle.radio_in;
		g.channel_yaw.radio_out 		= g.channel_yaw.radio_in;
		// FIXME To me it does not make sense to control the aileron using radio_in in manual mode
		// Doug could you please take a look at this ?
		if (g_rc_function[RC_Channel_aux::k_aileron]) {
			if (g_rc_function[RC_Channel_aux::k_aileron] != rc_array[g.flight_mode_channel-1]) {
				g_rc_function[RC_Channel_aux::k_aileron]->radio_out	= g_rc_function[RC_Channel_aux::k_aileron]->radio_in;
			}
		}
		// only use radio_in if the channel is not used as flight_mode_channel
		if (g_rc_function[RC_Channel_aux::k_flap_auto]) {
			if (g_rc_function[RC_Channel_aux::k_flap_auto] != rc_array[g.flight_mode_channel-1]) {
				g_rc_function[RC_Channel_aux::k_flap_auto]->radio_out	= g_rc_function[RC_Channel_aux::k_flap_auto]->radio_in;
			} else {
				g_rc_function[RC_Channel_aux::k_flap_auto]->radio_out	= g_rc_function[RC_Channel_aux::k_flap_auto]->radio_trim;
			}
		}
	} else
	{
		if (g.mix_mode == 0) {
			g.channel_roll.calc_pwm();
			g.channel_pitch.calc_pwm();
			if (g_rc_function[RC_Channel_aux::k_aileron]) {
				g_rc_function[RC_Channel_aux::k_aileron]->servo_out = g.channel_roll.servo_out;
				g_rc_function[RC_Channel_aux::k_aileron]->calc_pwm();
			}
		} else {
			/*Elevon mode*/
			float ch1;
			float ch2;
			ch1 = g.channel_pitch.servo_out - (BOOL_TO_SIGN(g.reverse_elevons) * g.channel_roll.servo_out);
			ch2 = g.channel_pitch.servo_out + (BOOL_TO_SIGN(g.reverse_elevons) * g.channel_roll.servo_out);
			g.channel_roll.radio_out =	elevon1_trim + (BOOL_TO_SIGN(g.reverse_ch1_elevon) * (ch1 * 500.0/ SERVO_MAX));
			g.channel_pitch.radio_out =	elevon2_trim + (BOOL_TO_SIGN(g.reverse_ch2_elevon) * (ch2 * 500.0/ SERVO_MAX));
		}
		g.channel_yaw.calc_pwm();
	}


#if HIL_MODE == HIL_MODE_DISABLED || HIL_SERVOS
	// send values to the PWM timers for output
	// ----------------------------------------
	// Rocket uses 4 servos on canards.
	// Channel 1 and 2- opposite pair for pitch
	// Channel 3 and 4 - opposite pair for yaw
	// All 4 move together for roll
	APM_RC.OutputCh(CH_1, g.channel_pitch.radio_out + (g.channel_roll.radio_out - 1500) ); // send to Servos
	APM_RC.OutputCh(CH_2, 3000 - g.channel_pitch.radio_out + (g.channel_roll.radio_out - 1500) ); // send to Servos
	APM_RC.OutputCh(CH_3, g.channel_yaw.radio_out + (g.channel_roll.radio_out - 1500) ); // send to Servos
	APM_RC.OutputCh(CH_4, 3000 - g.channel_yaw.radio_out + (g.channel_roll.radio_out - 1500) ); // send to Servos
	// Route configurable aux. functions to their respective servos
	g.rc_5.output_ch(CH_5);
	g.rc_6.output_ch(CH_6);
	g.rc_7.output_ch(CH_7);
	g.rc_8.output_ch(CH_8);
#endif
}

static void demo_servos(byte i) {

	while(i > 0){
		gcs_send_text_P(SEVERITY_LOW,PSTR("Demo Servos!"));
#if HIL_MODE == HIL_MODE_DISABLED || HIL_SERVOS
		APM_RC.OutputCh(1, 1400);		// pitch up
		APM_RC.OutputCh(2, 1600);
		APM_RC.OutputCh(3, 1500);
		APM_RC.OutputCh(4, 1500);
		mavlink_delay(400);
		APM_RC.OutputCh(1, 1600);		//pitch down
		APM_RC.OutputCh(1, 1400);
		APM_RC.OutputCh(1, 1500);
		APM_RC.OutputCh(1, 1500);
		mavlink_delay(400);
		APM_RC.OutputCh(1, 1500);		// yaw left
		APM_RC.OutputCh(1, 1500);
		APM_RC.OutputCh(1, 1400);
		APM_RC.OutputCh(1, 1600);
		mavlink_delay(400);
		APM_RC.OutputCh(1, 1500);		// yaw right
		APM_RC.OutputCh(1, 1500);
		APM_RC.OutputCh(1, 1600);
		APM_RC.OutputCh(1, 1400);
		mavlink_delay(400);
		APM_RC.OutputCh(1, 1400);		// roll clockwise
		APM_RC.OutputCh(1, 1400);
		APM_RC.OutputCh(1, 1400);
		APM_RC.OutputCh(1, 1400);
		mavlink_delay(400);
		APM_RC.OutputCh(1, 1600);		// roll counterclockwise
		APM_RC.OutputCh(1, 1600);
		APM_RC.OutputCh(1, 1600);
		APM_RC.OutputCh(1, 1600);
		mavlink_delay(400);
#endif
		mavlink_delay(400);
		i--;
	}
}
