// SMAF 하나를 호스트가 읽는 이벤트 기록으로 바꾼다. 빌드 도구(main.rs)와 웹 빌드(브라우저에서 가져온 게임 데이터를 변환)가 함께 쓴다.
// 기록: FNV-1a 해시 u64, 이벤트 수 u32, [시각(ms) u32, 종류 u8, ...]
// 종류 0 = MIDI(길이 u32, 바이트), 1 = 파형(채널 u8, 샘플링 u32, 샘플 수 u32, i16...)
use smaf_player::{parse_smaf, SmafEvent};

fn fnv(data: &[u8]) -> u64 {
    data.iter().fold(0xcbf29ce484222325u64, |h, b| (h ^ *b as u64).wrapping_mul(0x100000001b3))
}

/// 기록을 out에 덧붙이고 이벤트 수를 돌려준다.
pub fn append_record(raw: &[u8], out: &mut Vec<u8>) -> usize {
    let events = parse_smaf(raw);
    let count = events.len();
    out.extend_from_slice(&fnv(raw).to_le_bytes());
    out.extend_from_slice(&(count as u32).to_le_bytes());
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
    count
}

// 웹: JS가 ev_alloc으로 받은 자리에 파일을 써 넣고 ev_add를 부른다. 결과는 ev_ptr, ev_len으로 읽는다. 파일 머리("CWEV", 파일 수)는 JS가 붙인다.
#[cfg(target_arch = "wasm32")]
mod web {
    static mut INPUT: Vec<u8> = Vec::new();
    static mut OUTPUT: Vec<u8> = Vec::new();

    #[no_mangle]
    pub unsafe extern "C" fn ev_alloc(len: usize) -> *mut u8 {
        let input = &mut *core::ptr::addr_of_mut!(INPUT);
        input.clear();
        input.resize(len, 0);
        input.as_mut_ptr()
    }

    #[no_mangle]
    pub unsafe extern "C" fn ev_add() {
        let input = &*core::ptr::addr_of!(INPUT);
        let output = &mut *core::ptr::addr_of_mut!(OUTPUT);
        super::append_record(input, output);
    }

    #[no_mangle]
    pub unsafe extern "C" fn ev_ptr() -> *const u8 {
        (*core::ptr::addr_of!(OUTPUT)).as_ptr()
    }

    #[no_mangle]
    pub unsafe extern "C" fn ev_len() -> usize {
        (*core::ptr::addr_of!(OUTPUT)).len()
    }
}
