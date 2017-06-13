const VERSION: u8 = 0x01;
const HEADER: [u8; 4] = [0xf0, 0x00, 0x70, VERSION];
const FOOTER: [u8; 1] = [0xf7];

pub trait Command {
    fn to_sysex(&self) -> Vec<u8> {
        let mut payload = self.payload();
        let checksum = payload.iter().fold(0, |acc, val| acc ^ val);
        payload.push(checksum);
        let mut message = Vec::new();
        message.extend(HEADER.to_vec());
        message.extend(self.to_nibbles(payload));
        message.extend(FOOTER.to_vec());
        message
    }

    fn to_nibbles(&self, vec: Vec<u8>) -> Vec<u8> {
        vec.iter()
            .flat_map(|byte| vec![byte >> 4, byte & 0x0f])
            .collect()
    }

    fn payload(&self) -> Vec<u8>;
}

pub struct Ping {}

impl Command for Ping {
    fn payload(&self) -> Vec<u8> {
        vec![0x10]
    }
}

pub struct Write {
    page_no: u8,
    page_data: Vec<u8>,
}

impl Command for Write {
    fn payload(&self) -> Vec<u8> {
        let mut payload = vec![0x11, self.page_no];
        payload.extend(self.page_data.iter());
        payload
    }
}

pub struct Read {
    page_no: u8,
}

impl Command for Read {
    fn payload(&self) -> Vec<u8> {
        vec![0x12, self.page_no]
    }
}

pub struct Verify {
    page_no: u8,
}

impl Command for Verify {
    fn payload(&self) -> Vec<u8> {
        vec![0x13, self.page_no]
    }
}

pub struct Quit {}

impl Command for Quit {
    fn payload(&self) -> Vec<u8> {
        vec![0x14]
    }
}
