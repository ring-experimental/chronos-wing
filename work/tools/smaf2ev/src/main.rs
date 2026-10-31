// snd/의 SMAF를 호스트가 읽는 이벤트 파일로 바꾼다. 빌드 때만 쓰며 제품에는 결과 데이터만 들어간다.
// 형식(리틀 엔디언): "CWEV", 파일 수 u32, [FNV-1a 해시 u64, 이벤트 수 u32, [시각(ms) u32, 종류 u8, ...]]
// 종류 0 = MIDI(길이 u32, 바이트), 1 = 파형(채널 u8, 샘플링 u32, 샘플 수 u32, i16...)
use smaf_player::{parse_smaf, SmafEvent};
use std::{env, fs, io::Write};

fn fnv(data: &[u8]) -> u64 {
    data.iter().fold(0xcbf29ce484222325u64, |h, b| (h ^ *b as u64).wrapping_mul(0x100000001b3))
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let mut out = Vec::new();
    let mut files: Vec<_> = fs::read_dir(&args[1]).unwrap().map(|e| e.unwrap().path()).collect();
    files.sort();
    out.extend_from_slice(b"CWEV");
    out.extend_from_slice(&(files.len() as u32).to_le_bytes());
    for path in &files {
        let raw = fs::read(path).unwrap();
        let events = parse_smaf(&raw);
        println!("{} {} bytes {} events", path.display(), raw.len(), events.len());
        out.extend_from_slice(&fnv(&raw).to_le_bytes());
        out.extend_from_slice(&(events.len() as u32).to_le_bytes());
        for (time, ev) in events {
            out.extend_from_slice(&(time as u32).to_le_bytes());
            let midi: Vec<u8> = match ev {
                SmafEvent::Wave { channel, sampling_rate, data } => {
                    out.push(1);
                    out.push(channel);
                    out.extend_from_slice(&sampling_rate.to_le_bytes());
                    out.extend_from_slice(&(data.len() as u32).to_le_bytes());
                    for s in data {
                        out.extend_from_slice(&s.to_le_bytes());
                    }
                    continue;
                }
                SmafEvent::MidiNoteOn { channel, note, velocity } => vec![0x90 | channel, note, velocity],
                SmafEvent::MidiNoteOff { channel, note, velocity } => vec![0x80 | channel, note, velocity],
                SmafEvent::MidiProgramChange { channel, program } => vec![0xc0 | channel, program],
                SmafEvent::MidiControlChange { channel, control, value } => vec![0xb0 | channel, control, value],
                SmafEvent::MidiPitchBend { channel, value } => vec![0xe0 | channel, (value & 0x7f) as u8, ((value >> 7) & 0x7f) as u8],
                SmafEvent::MidiSysEx(data) => data,
                SmafEvent::End => vec![],
            };
            out.push(0);
            out.extend_from_slice(&(midi.len() as u32).to_le_bytes());
            out.extend_from_slice(&midi);
        }
    }
    fs::File::create(&args[2]).unwrap().write_all(&out).unwrap();
}
