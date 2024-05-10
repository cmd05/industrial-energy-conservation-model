/// Define Factory namespace to interface with different inputs and outputs

namespace Factory {
    // Analog ranges of components 
    const int ldr_low = 54, ldr_high = 974;
    const int led_low = 0, led_high = 255;
    int analog_tmp_low = 20, analog_tmp_high = 358;
    const int motor_low = 0, motor_high = 255;

    // Range of physical quantities
    const int temp_low = -40, temp_high = 125;
    const int lux_min = 10, lux_max = 5000;

   	//--------------------------- INPUT CONTROLLER FOR FACTORY -------------------------------------------
    
    /* Hold Standby Button Data */
    struct Stand_by {
        Stand_by(int btn_pin, int led_pin, int count_pin, int presence_led): 
        btn_pin{btn_pin}, led_pin{led_pin}, count_pin{count_pin}, presence_led{presence_led} {
            pinMode(btn_pin, INPUT);
            pinMode(led_pin, OUTPUT);
            pinMode(count_pin, OUTPUT);
            pinMode(presence_led, OUTPUT);
        }

        const int btn_pin;
        const int led_pin;
        const int count_pin;
        const int presence_led;

        int hours = 1;
        bool state = false;
        bool prev_state = false;
    };


    /* Weather Monitoring */
    struct Monitoring {
        Monitoring(int ldr_in, int ldr_out, int tmp_in, int tmp_out):
        ldr_in{ldr_in}, ldr_out{ldr_out}, tmp_in{tmp_in}, tmp_out{tmp_out} {
            pinMode(ldr_in, INPUT);
            pinMode(ldr_out, INPUT);
            pinMode(tmp_in, INPUT);
            pinMode(tmp_out, INPUT);
        }

        const int ldr_in;
        const int ldr_out;
        const int tmp_in;
        const int tmp_out;
    };

    /* Controller class for input methods */
    class Input {
        public:
            Input(int ultrasonic, Stand_by standby, Monitoring monitoring);
            
            /* Handle human presence */
            double read_ultrasonic_ping() const;
            double pulse_to_distance(double ping) const { return ping * 0.01723; }

            bool get_human_presence() const { return human_presence; }
            void set_human_presence(bool presence) { human_presence = presence; }
            void toggle_presence_led(bool state) const { digitalWrite(standby.presence_led, state); }

            /* Handle standby mode input */
            bool is_standby() const { return standby.state; }
            int get_standby_hours() const { return standby.hours; }
            void reset_standby();

            void toggle_standby_led(bool state) const { digitalWrite(standby.led_pin, state); }
            void check_standby_input(int max_hours);

            /* Read and Write weather information */
            int read_ldr_in() const { return analogRead(monitor.ldr_in); }
            int read_tmp_in() const { return analogRead(monitor.tmp_in); }
            int read_ldr_out() const { return analogRead(monitor.ldr_out); }
            int read_tmp_out() const { return analogRead(monitor.tmp_out); }
            
            int analog_to_celsius(int analog) const;
            int analog_to_lux(int analog) const;
            
            void weather_feedback() const;
        private:
            const int ultrasonic;
            
            // Standby Mode
            Stand_by standby;
            bool human_presence = false;

            // Weather
            const Monitoring monitor;
    };

    Input::Input(int ultrasonic, Stand_by standby, Monitoring monitor):
        ultrasonic{ultrasonic}, standby{standby}, monitor{monitor} 
    {
        // Initialize pins to arduino board
        pinMode(ultrasonic, INPUT);
    }
    
    int Input::analog_to_celsius(int analog) const {
        int value = (analog - 20) * 3.04;
        int celsius = map(value, 0, 1023, temp_low, temp_high);
        return celsius;
    }

    int Input::analog_to_lux(int analog) const {
        return map(analog, 0, 1023, lux_min, lux_max);
    }

    // Test ultrasonic ping and return distance
    double Input::read_ultrasonic_ping() const {
        pinMode(ultrasonic, OUTPUT); // Clear the trigger
        digitalWrite(ultrasonic, LOW);
        delayMicroseconds(2);

        digitalWrite(ultrasonic, HIGH);
        delayMicroseconds(10);
        
        digitalWrite(ultrasonic, LOW);
        pinMode(ultrasonic, INPUT);
        
        double distance = pulse_to_distance(pulseIn(ultrasonic, HIGH));
        return distance;
    }
    
    /* Check for standby toggle and hours input */
    void Input::check_standby_input(int max_hours) {
        // Detect standby toggle ON and OFF
        bool new_state = digitalRead(standby.btn_pin);
        if(standby.prev_state == false && new_state == true) {
            digitalWrite(standby.led_pin, !standby.state);
            standby.state = !standby.state;
            
            String message = "*** Standby mode is on ***";
            if(!standby.state) message.replace("on", "off");
            Serial.println(message);
        }

        // Save new state in a variable
        standby.prev_state = new_state;

        // Detect input for standby time
        bool count_state = digitalRead(standby.count_pin);
        if(count_state) {
            standby.hours++;

            // Reset counter to 1 hour, if value exceeds max hours
            if(standby.hours >= (max_hours + 1)) standby.hours = 1;

            Serial.println("Standby set to " + String(standby.hours) + " hours");
            delay(300);
        }
    }

    void Input::reset_standby() {
        standby.hours = 1;
        standby.state = false;
    }
    
    void Input::weather_feedback() const {
        // Lighting Feedback
        int analog_light = read_ldr_out();
        Serial.println("***************** WEATHER FEEDBACK *****************\n");

        if (analog_light >= 800) {
            Serial.println("It is bright outside. Utilize sunshine to minimize energy usage");
        } else if (analog_light >= 500) {
            Serial.println("Outdoor lighting is fading. It will be dark soon");
        } else {
            Serial.println("It is dark outside. Maximise sunlight usage during the day");
        }

        int lux = analog_to_lux(analog_light);
        Serial.println("Light Intensity: " + String(lux) + " lux\n\n");

        // Temperature feedback
        int analog_tmp = read_tmp_out();
        int temp_c = analog_to_celsius(analog_tmp);

        if (temp_c > 18 && temp_c < 28) Serial.println("Optimal temperature outside. Allow airflow");
        
        Serial.println("Temperature: " + String(temp_c) + " C");

        Serial.println("****************************************************\n\n");

        // Plot values
        Serial.println(temp_c);
    }

   	//--------------------------- OUTPUT CONTROLLER FOR FACTORY ------------------------------------------

    /* Store motor connections as an object */
    struct Motor {
        Motor(int in3, int in4, int pwm): in3{in3}, in4{in4}, pwm{pwm} {
            pinMode(in3, OUTPUT);
            pinMode(in4, OUTPUT);
            pinMode(pwm, OUTPUT);
        }

        void run(bool state_in3, bool state_in4, int pwm_analog) const {
            digitalWrite(in3, state_in3);
            digitalWrite(in4, state_in4);
            analogWrite(pwm, pwm_analog);
        }

        const int in3;
        const int in4;
        const int pwm;
    };

    /* Store LED connections as an object */
    struct LED_Set {
        LED_Set(int set_1, int set_2, int set_3): set_1{set_1}, set_2{set_2}, set_3{set_3} {
            pinMode(set_1, OUTPUT);
            pinMode(set_2, OUTPUT);
            pinMode(set_3, OUTPUT);
        }

        void analog_lighting(int analog) const {
            analogWrite(set_1, analog);
            analogWrite(set_2, analog);
            analogWrite(set_3, analog);
        }

        void lighting_digital(bool state) const {
            digitalWrite(set_1, state);
            digitalWrite(set_2, state);
            digitalWrite(set_3, state);
        }

        const int set_1;
        const int set_2;
        const int set_3;
    };

    class Output {
        public:
            Output(int relay, Motor m_cooling, Motor m_process, LED_Set led_set);
            void toggle_relay(bool state) const { digitalWrite(relay, state); }
            void adjust_lighting(int intensity) const; 
            void adjust_cooling(int analog) const; 
            void run_processing_unit() const;
            void lighting_digital(bool state) const { led_set.lighting_digital(state); };
        private:
            const int relay;
            const Motor m_cooling;
            const Motor m_process;
            const LED_Set led_set;
    };

    /* Controller class for output methods */
    Output::Output(int relay, Motor m_cooling, Motor m_process, LED_Set led_set):
        relay{relay}, m_cooling{m_cooling}, m_process{m_process}, led_set{led_set} {
            // Initialize pins to arduino board
            pinMode(relay, OUTPUT);
    }

    /* Adjust lighting based on light intensity */
    void Output::adjust_lighting(int intensity) const {
        int mapped = map(intensity, ldr_low, ldr_high, led_low, led_high);
        
        // Higher intensity will require lesser lighting
        mapped = led_high - mapped;
        led_set.analog_lighting(mapped);
    }

    /* Adjust cooling units based on temperature analog */
    void Output::adjust_cooling(int analog) const {
        // RPM of motor should be directly proportional to temperature
        int m_out = map(analog, analog_tmp_low, analog_tmp_high, motor_low, motor_high);
        m_cooling.run(false, true, m_out);
    }

    void Output::run_processing_unit() const {
        m_process.run(false, true, motor_high);
        delay(100);
    }
};

/* Input Components */

const int ultrasonic = 3;

// btn_pin, led_pin, count_btn_pin, presence_led
const Factory::Stand_by standby {4, 8, 7, A4};

// ldr_in, ldr_out, tmp_in, tmp_out
const Factory::Monitoring monitoring{A3, A0, A2, A1};

Factory::Input in_fct{ultrasonic, standby, monitoring};

/* Output Components */

const int relay_pin = 2;

// in3, in4, pwm
Factory::Motor m_cooling {13, 12, 10};
Factory::Motor m_process {11, 0, 1};
// set_1, set_2, set_3
Factory::LED_Set led_set{5, 6, 9};

Factory::Output out_fct{relay_pin, m_cooling, m_process, led_set};

void setup() {
    Serial.begin(9600);
    Serial.println("*** Program Start ***\n\n");
}

unsigned long prev_feedback_time;

void loop()
{
    using namespace Factory;
    double ping_distance = in_fct.read_ultrasonic_ping();
    const int min_distance = 20;

    // Detect human presence if ping distance is less than or equal to minimum distance
    if(ping_distance <= min_distance) {
        // If human was already present => set state to false
        // If human was not present => set state to true
        bool state = !in_fct.get_human_presence();
        in_fct.set_human_presence(state);
        in_fct.toggle_presence_led(state);
        
        String message = "*****************\nHuman has entered\n*****************\n\n";
        if(!state) message.replace("entered", "exited");
        Serial.println(message);

        delay(500);
    }

    if(in_fct.get_human_presence()) {
        // Human is present 

        // Note: Relay is ON for LOW state and OFF for HIGH state
        out_fct.toggle_relay(false);

        // Check input for standby mode
        int max_standby_hrs = 10;
        in_fct.check_standby_input(max_standby_hrs);
        
        // Adjust lighting to save energy
        int light_intensity = in_fct.read_ldr_in();
        out_fct.adjust_lighting(light_intensity);

        // Adjust cooling to save energy
        int analog_tmp_in = in_fct.read_tmp_in();
        out_fct.adjust_cooling(analog_tmp_in);

        // Run required processing units
        out_fct.run_processing_unit();

        // Periodically provide weather feedback with energy saving-suggestions   
        int feedback_period = 5000;
        if(millis() - prev_feedback_time >= feedback_period) {
            prev_feedback_time = millis();
            in_fct.weather_feedback();
        }
    } else {
        // Human is not present 

        // Turn of all lighting
        out_fct.lighting_digital(false);

        // Check whether standby mode is enabled
        if(in_fct.is_standby()) {
            out_fct.toggle_relay(false); // Turn on
            Serial.println("********* Entering standby mode *********");
            int run_hours = in_fct.get_standby_hours();

            // Run units for specified hours
            for(int i = 0; i <= run_hours; i++) {
                out_fct.run_processing_unit();
                delay(1000);
                Serial.println("=== Standby - " + String(i+1) + "hour(s) elapsed ===");
            }

            // Reset systems
            Serial.println("********* Standby period is over *********");
            in_fct.toggle_standby_led(false);
            out_fct.toggle_relay(true);

            in_fct.reset_standby();
        } else {
            // Standby mode is not enabled 
            out_fct.toggle_relay(true); // turn off
        }
    }
}