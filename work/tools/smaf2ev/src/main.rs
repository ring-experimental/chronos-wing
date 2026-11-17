// snd/의 SMAF를 호스트가 읽는 이벤트 파일로 바꾼다. 빌드 때만 쓰며 제품에는 결과 데이터만 들어간다.
// 형식(리틀 엔디언): "CWEV", 파일 수 u32, 파일 이름순으로 파일마다 기록 하나(lib.rs)
use std::{env, fs, io::Write};

fn main() {
    let args: Vec<String> = env::args().collect();
    let mut out = Vec::new();
    let mut files: Vec<_> = fs::read_dir(&args[1]).unwrap().map(|e| e.unwrap().path()).collect();
    files.sort();
    out.extend_from_slice(b"CWEV");
    out.extend_from_slice(&(files.len() as u32).to_le_bytes());
    for path in &files {
        let raw = fs::read(path).unwrap();
        let events = smaf2ev::append_record(&raw, &mut out);
        println!("{} {} bytes {} events", path.display(), raw.len(), events);
    }
    fs::File::create(&args[2]).unwrap().write_all(&out).unwrap();
}
