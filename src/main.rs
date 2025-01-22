use serialport::{available_ports, SerialPort, SerialPortInfo};
use std::{
    process,
    time::{Duration, Instant},
};

mod controller_properites;
use controller_properites::*;

mod controller_commands;
use controller_commands::Command;

fn main() {
    let mut port = match connect_to_signal_generator() {
        Some(port) => port,
        None => {
            eprintln!("Exiting program: No valid connection.");
            return;
        }
    };

    // // Example commands
    // let command = Command::SetPower(30.);
    // let command_string = command.to_string();

    // match write_read(&mut *port, command_string) {
    //     Ok(response) => println!("Response: {}", response),
    //     Err(e) => eprintln!("Error: {}", e),
    // }

    let command = Command::SetFrequency(50.);
    let command_string = command.to_string();
    match write_read(&mut *port, command_string) {
        Ok(response) => println!("Response: {}", response),
        Err(e) => eprintln!("Error: {}", e),
    }

    let command = Command::GetFrequency;
    let command_string = command.to_string();
    match write_read(&mut *port, command_string) {
        Ok(response) => println!("Response: {}", response),
        Err(e) => eprintln!("Error: {}", e),
    }

    std::thread::sleep(Duration::from_secs(20));
    disconnect(port);
}

fn connect_to_signal_generator() -> Option<Box<dyn SerialPort>> {
    let signal_generators = autodetect_sg_port();

    if signal_generators.is_empty() {
        eprintln!("No signal generator boards detected.");
        return None;
    }

    let first_signal_generator = &signal_generators[0];
    println!(
        "Connecting to signal generator: {:?}",
        first_signal_generator.port_name
    );

    match serialport::new(&first_signal_generator.port_name, BAUD_RATE)
        .data_bits(DATA_BITS)
        .parity(PARITY)
        .flow_control(FLOW_CONTROL)
        .stop_bits(STOP_BITS)
        .timeout(CONNECTION_TIMEOUT)
        .open()
    {
        Ok(port) => {
            println!(
                "Successfully connected to {}",
                first_signal_generator.port_name
            );
            Some(port)
        }
        Err(e) => {
            eprintln!(
                "Failed to connect to {}: {:?}",
                first_signal_generator.port_name, e
            );
            None
        }
    }
}

fn autodetect_sg_port() -> Vec<SerialPortInfo> {
    let available_ports = match available_ports() {
        Ok(ports) => ports,
        Err(e) => {
            eprintln!("Failed to list serial ports: {:?}", e);
            process::exit(1);
        }
    };
    println!("Available ports to connect to:\n{:#?}\n", available_ports);

    available_ports
        .into_iter()
        .filter(|port| {
            if let serialport::SerialPortType::UsbPort(usb_info) = &port.port_type {
                usb_info.vid == TARGET_VENDOR_ID && usb_info.pid == TARGET_PRODUCT_ID
            } else {
                false
            }
        })
        .collect()
}

fn write_read(port: &mut dyn SerialPort, tx: String) -> Result<String, String> {
    let command = format!("{}\r\n", tx);
    println!("TX:\t{}", command);

    if let Err(e) = port.write_all(command.as_bytes()) {
        return Err(format!("Failed to write to the port: {:?}", e));
    }

    if let Err(e) = port.flush() {
        return Err(format!("Failed to flush the port: {:?}", e));
    }

    let mut buffer = String::new();
    let mut temp_buffer = [0; 256];
    let timeout = Duration::from_millis(500);
    let start_time = Instant::now();

    while !buffer.contains("\r\n") {
        if start_time.elapsed() >= timeout {
            return Err("Timeout while waiting for response.".to_string());
        }

        match port.read(&mut temp_buffer) {
            Ok(bytes_read) => {
                if bytes_read > 0 {
                    buffer.push_str(&String::from_utf8_lossy(&temp_buffer[..bytes_read]));
                }
            }
            Err(ref e) if e.kind() == std::io::ErrorKind::TimedOut => continue,
            Err(e) => return Err(format!("Failed to read from the port: {:?}", e)),
        }
    }

    println!("RX:\t{}", buffer);
    Ok(buffer)
}

fn disconnect(port: Box<dyn serialport::SerialPort>) {
    let port_name = port.name().unwrap_or_else(|| "Unknown".to_string());
    println!("Disconnecting from port: {}", port_name);
    drop(port); // Explicitly drop the port
    println!("Disconnected from port: {}", port_name);
}
