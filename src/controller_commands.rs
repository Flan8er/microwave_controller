// Required functionality:
// Button - Command String - Comment
// Get Identity - $IDN,0
// Get Version - $VER,0
// Get Status - $ST,0
// Get Status (verbose) - $ST,0,1  - The extra argument tells the ISC board to return a verbose list of the errors instead of an error code.
// Clear Errors - $ERRC,0
// Get Frequency - $FCG,0
// Set Frequency - $FCS,0,? - Fills ? with value(s) from the adjacent lineEdit(s).
// Get PA Power (measurement) - $PPG,0
// Get Power (setpoint) - $PWRG,0
// Set Power - $PWRS,0,? - Fills ? with value(s) from the adjacent lineEdit(s).
// Configure DLL - $DLES,0,?,?,?,?,?,? - Fills ? with value(s) from the adjacent lineEdit(s).
// DLL Enable - $DLES,0,1
// DLL Disable - $DLES,0,0
// RF Enable - $ECS,0,1
// RF Disable - $ECS,0,0
// Sweep (dBm) - $SWPD,0,?,?,?,?,0 - Fills ? with value(s) from the adjacent lineEdit(s).

pub enum Command {
    GetIdentity,
    GetVersion,
    GetStatus {
        verbose: bool,
    },
    ClearErrors,
    GetFrequency,
    SetFrequency(f32),
    GetPaPower,
    GetPowerSetpoint,
    SetPower(f32),
    ConfigureDll {
        param1: f32,
        param2: f32,
        param3: f32,
        param4: f32,
        param5: f32,
        param6: f32,
    },
    DllEnable,
    DllDisable,
    RfEnable,
    RfDisable,
    SweepDbm {
        start: f32,
        stop: f32,
        step: f32,
        dwell: f32,
    },
}

impl Command {
    pub fn to_string(&self) -> String {
        match self {
            Command::GetIdentity => "$IDN,0".to_string(),
            Command::GetVersion => "$VER,0".to_string(),
            Command::GetStatus { verbose } => {
                if *verbose {
                    "$ST,0,1".to_string()
                } else {
                    "$ST,0".to_string()
                }
            }
            Command::ClearErrors => "$ERRC,0".to_string(),
            Command::GetFrequency => "$FCG,0".to_string(),
            Command::SetFrequency(value) => format!("$FCS,0,{:.2}", value),
            Command::GetPaPower => "$PPG,0".to_string(),
            Command::GetPowerSetpoint => "$PWRG,0".to_string(),
            Command::SetPower(value) => format!("$PWRS,0,{:.2}", value),
            Command::ConfigureDll {
                param1,
                param2,
                param3,
                param4,
                param5,
                param6,
            } => format!(
                "$DLES,0,{:.2},{:.2},{:.2},{:.2},{:.2},{:.2}",
                param1, param2, param3, param4, param5, param6
            ),
            Command::DllEnable => "$DLES,0,1".to_string(),
            Command::DllDisable => "$DLES,0,0".to_string(),
            Command::RfEnable => "$ECS,0,1".to_string(),
            Command::RfDisable => "$ECS,0,0".to_string(),
            Command::SweepDbm {
                start,
                stop,
                step,
                dwell,
            } => format!(
                "$SWPD,0,{:.2},{:.2},{:.2},{:.2},0",
                start, stop, step, dwell
            ),
        }
    }
}
