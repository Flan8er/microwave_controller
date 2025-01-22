pub const TARGET_VENDOR_ID: u16 = 8137;
pub const TARGET_PRODUCT_ID: u16 = 131;
pub const BAUD_RATE: u32 = 115_200;
pub const DATA_BITS: serialport::DataBits = serialport::DataBits::Eight;
pub const PARITY: serialport::Parity = serialport::Parity::None;
pub const FLOW_CONTROL: serialport::FlowControl = serialport::FlowControl::None;
pub const STOP_BITS: serialport::StopBits = serialport::StopBits::One;
pub const TIMEOUT: std::time::Duration = std::time::Duration::from_secs(10);
