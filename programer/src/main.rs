extern crate portmidi as pm;

extern crate sysexprog;

use sysexprog::command::*;

fn main() {
    //  let context = pm::PortMidi::new().unwrap();

    /*  let device_id = 3;
  hallo();
  context
    .device(device_id)
    .and_then(|dev| context.output_port(dev, 1024))
    .and_then(|port| port.write_sysex(0, &[240, 0, 112, 1, 1, 0, 1, 0, 247])).unwrap();
*/

    let ping = Ping {};
    print!("{:?}", ping.to_sysex());
}
